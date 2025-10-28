#include "gltf.hpp"

#include <bit>
#include <fastgltf/core.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/math.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>
#include <glm/glm.hpp>
#include <memory>
#include <ranges>
#include <vgi/buffer/index.hpp>
#include <vgi/buffer/transfer.hpp>
#include <vgi/buffer/vertex.hpp>
#include <vgi/cmdbuf.hpp>
#include <vgi/fs.hpp>
#include <vgi/log.hpp>
#include <vgi/math.hpp>
#include <vgi/vgi.hpp>
#include <vgi/window.hpp>

#define COPY_VERTEX_FIELD(__field, ...)                                \
    this->template copy_vertex_field<decltype(::vgi::vertex::__field), \
                                     offsetof(::vgi::vertex, __field)>(asset, __VA_ARGS__, dest)
#define FILL_VERTEX_FIELD(__field, ...)                                \
    this->template fill_vertex_field<decltype(::vgi::vertex::__field), \
                                     offsetof(::vgi::vertex, __field)>(__VA_ARGS__, dest)

constexpr fastgltf::Options LOAD_OPTIONS =
        fastgltf::Options::DecomposeNodeMatrices | fastgltf::Options::GenerateMeshIndices;

static const char* mime_type_to_img_type(fastgltf::MimeType mime_type) noexcept {
    switch (mime_type) {
        case fastgltf::MimeType::JPEG:
            return "JPG";
        case fastgltf::MimeType::PNG:
            return "PNG";
        case fastgltf::MimeType::WEBP:
            return "WEBP";
        default:
            return nullptr;
    }
}

static vgi::sampler_options parse_sampler(const fastgltf::Sampler* sampler) noexcept {
    auto raw_mag_filter = sampler ? sampler->magFilter.value_or(fastgltf::Filter::Linear)
                                  : fastgltf::Filter::Linear;
    auto raw_min_filter = sampler ? sampler->minFilter.value_or(fastgltf::Filter::Linear)
                                  : fastgltf::Filter::Linear;
    auto raw_wrap_u = sampler ? sampler->wrapS : fastgltf::Wrap::Repeat;
    auto raw_wrap_v = sampler ? sampler->wrapT : fastgltf::Wrap::Repeat;

    vgi::sampler_options options;
    switch (raw_mag_filter) {
        case fastgltf::Filter::Linear:
        case fastgltf::Filter::LinearMipMapLinear:
        case fastgltf::Filter::LinearMipMapNearest:
            options.mag_filter = vk::Filter::eLinear;
            break;
        case fastgltf::Filter::Nearest:
        case fastgltf::Filter::NearestMipMapLinear:
        case fastgltf::Filter::NearestMipMapNearest:
            options.mag_filter = vk::Filter::eNearest;
            break;
    }
    // TODO Mipmaps
    switch (raw_min_filter) {
        case fastgltf::Filter::Linear:
        case fastgltf::Filter::LinearMipMapLinear:
        case fastgltf::Filter::LinearMipMapNearest:
            options.min_filter = vk::Filter::eLinear;
            break;
        case fastgltf::Filter::Nearest:
        case fastgltf::Filter::NearestMipMapLinear:
        case fastgltf::Filter::NearestMipMapNearest:
            options.min_filter = vk::Filter::eNearest;
            break;
    }
    switch (raw_wrap_u) {
        case fastgltf::Wrap::ClampToEdge:
            options.address_mode_u = vk::SamplerAddressMode::eClampToEdge;
            break;
        case fastgltf::Wrap::MirroredRepeat:
            options.address_mode_u = vk::SamplerAddressMode::eMirroredRepeat;
            break;
        case fastgltf::Wrap::Repeat:
            options.address_mode_u = vk::SamplerAddressMode::eRepeat;
            break;
    }
    switch (raw_wrap_v) {
        case fastgltf::Wrap::ClampToEdge:
            options.address_mode_v = vk::SamplerAddressMode::eClampToEdge;
            break;
        case fastgltf::Wrap::MirroredRepeat:
            options.address_mode_v = vk::SamplerAddressMode::eMirroredRepeat;
            break;
        case fastgltf::Wrap::Repeat:
            options.address_mode_v = vk::SamplerAddressMode::eRepeat;
            break;
    }

    return options;
}

namespace vgi::asset::gltf {
    static material parse_material(const fastgltf::Material& mat) noexcept {
        return material{.name = mat.name};
    }

    struct TransferOffset {
        size_t buffer;
        size_t offset;
        size_t byte_size;
    };

    struct asset_parser {
        window& parent;
        fastgltf::Parser& parser;
        fastgltf::Asset& asset;
        std::vector<std::shared_ptr<surface>> images;
        std::vector<size_t> transfer_buffers;
        size_t transfer_size = 0;

        asset_parser(window& parent, fastgltf::Parser& parser, fastgltf::Asset& asset) :
            parent(parent), parser(parser), asset(asset) {
            this->images.reserve(asset.images.size());
            for (fastgltf::Image& img: asset.images) {
                if (img.name.empty()) {
                    vgi::log_dbg("Found anonymous image");
                } else {
                    vgi::log_dbg("Found image '{}'", img.name);
                }
                this->images.push_back(std::make_shared<surface>(load_image(img.data)));
            }
        }

        inline fastgltf::Asset* operator->() noexcept { return std::addressof(this->asset); }
        inline const fastgltf::Asset* operator->() const noexcept {
            return std::addressof(this->asset);
        }

        /// @brief Reserves device mapped memory for a later upload
        /// @param count Number of elements to reserve memory for
        /// @return The transfer information of the reserved memory
        template<class T>
        TransferOffset reserve(size_t count) {
            std::optional<size_t> byte_size = math::check_mul<size_t>(sizeof(T), count);
            if (!byte_size) throw vgi_error{"arithmetic overflow"};
            if (auto new_transfer_size = math::check_add(this->transfer_size, *byte_size)) {
                const size_t offset = this->transfer_size;
                this->transfer_size = *new_transfer_size;
                return {this->transfer_buffers.size(), offset, *byte_size};
            } else {
                this->transfer_buffers.push_back(std::exchange(this->transfer_size, *byte_size));
                return {this->transfer_buffers.size(), 0, *byte_size};
            }
        }

        /// @brief Reserves device mapped memory for a later upload
        /// @param accessor GLTF accessor describing the memory to be reserved
        /// @return The transfer information of the reserved memory
        TransferOffset reserve(const fastgltf::Accessor& accessor) {
            size_t element_size =
                    fastgltf::getElementByteSize(accessor.type, accessor.componentType);
            std::optional<size_t> byte_size = math::check_mul(element_size, accessor.count);
            if (!byte_size) throw vgi_error{"arithmetic overflow"};
            if (auto new_transfer_size = math::check_add(this->transfer_size, *byte_size)) {
                const size_t offset = this->transfer_size;
                this->transfer_size = *new_transfer_size;
                return {this->transfer_buffers.size(), offset, *byte_size};
            } else {
                this->transfer_buffers.push_back(std::exchange(this->transfer_size, *byte_size));
                return {this->transfer_buffers.size(), 0, *byte_size};
            }
        }

        surface load_image(fastgltf::DataSource& source) {
            if (fastgltf::sources::BufferView* buf_view =
                        std::get_if<fastgltf::sources::BufferView>(&source)) {
                return load_image(this->asset.bufferViews[buf_view->bufferViewIndex],
                                  buf_view->mimeType);
            } else if (fastgltf::sources::URI* uri = std::get_if<fastgltf::sources::URI>(&source)) {
                return load_image(uri->uri, uri->fileByteOffset, uri->mimeType);
            } else if (fastgltf::sources::Array* array =
                               std::get_if<fastgltf::sources::Array>(&source)) {
                return surface{array->bytes, mime_type_to_img_type(array->mimeType)};
            } else if (fastgltf::sources::Vector* vec =
                               std::get_if<fastgltf::sources::Vector>(&source)) {
                return surface{vec->bytes, mime_type_to_img_type(vec->mimeType)};
            } else if (fastgltf::sources::ByteView* buf =
                               std::get_if<fastgltf::sources::ByteView>(&source)) {
                return surface{buf->bytes, mime_type_to_img_type(buf->mimeType)};
            } else {
                throw vgi_error{"Could not import data from image"};
            }
        }

        surface load_image(fastgltf::BufferView& buf_view,
                           fastgltf::MimeType mime_type = fastgltf::MimeType::None) {
            if (buf_view.byteStride.has_value()) {
                throw vgi_error{"Invalid image data"};
            } else if (buf_view.meshoptCompression != nullptr) {
                throw vgi_error{"'EXT_meshopt_compression' is not supported for image data"};
            }

            fastgltf::Buffer& buf = asset.buffers[buf_view.bufferIndex];
            VGI_ASSERT(!std::holds_alternative<fastgltf::sources::BufferView>(buf.data));
            unique_span<std::byte> byte_buf;
            std::span<std::byte> bytes;

            if (fastgltf::sources::URI* uri = std::get_if<fastgltf::sources::URI>(&buf.data)) {
                if (uri->uri.isLocalPath()) {
                    byte_buf = read_file<std::byte>(uri->uri.fspath());
                    bytes = byte_buf;
                    bytes = bytes.subspan(std::min(byte_buf.size(), uri->fileByteOffset));
                } else {
                    throw vgi_error{"Data URIs are not supported yet"};
                }
            } else if (fastgltf::sources::Array* array =
                               std::get_if<fastgltf::sources::Array>(&buf.data)) {
                bytes = array->bytes;
            } else if (fastgltf::sources::Vector* vec =
                               std::get_if<fastgltf::sources::Vector>(&buf.data)) {
                bytes = array->bytes;
            } else if (fastgltf::sources::ByteView* view =
                               std::get_if<fastgltf::sources::ByteView>(&buf.data)) {
                bytes = array->bytes;
            } else {
                throw vgi_error{"Could not import data from image"};
            }

            return surface{bytes.subspan(std::min(bytes.size(), buf_view.byteOffset),
                                         std::min(bytes.size(), buf_view.byteLength)),
                           mime_type_to_img_type(mime_type)};
        }

        surface load_image(fastgltf::URI& uri, size_t offset = 0,
                           fastgltf::MimeType mime_type = fastgltf::MimeType::None) {
            if (uri.isLocalPath()) {
                return surface{uri.fspath(), mime_type_to_img_type(mime_type)};
            } else {
                throw vgi_error{"Data URIs are not supported yet"};
            }
        }
    };

    struct asset_uploader {
        fastgltf::Asset& asset;
        command_buffer cmdbuf;
        std::vector<transfer_buffer> transfer_buffers;

        asset_uploader(window& win, asset_parser&& parser) : asset(parser.asset), cmdbuf(win) {
            if (parser.transfer_size > 0) parser.transfer_buffers.push_back(parser.transfer_size);
            this->transfer_buffers.reserve(parser.transfer_buffers.size());
            for (size_t size: parser.transfer_buffers) {
                this->transfer_buffers.emplace_back(win, size);
            }
        }

        //! @copydoc command_buffer::window
        inline window& parent() const noexcept { return cmdbuf.window(); }

        /// @brief Registers the transfer on the command buffer.
        /// @param transfer Transfer information
        /// @param dest Destination buffer
        /// @param dest_offset Destination buffer offset
        /// @return Memory region where the upload data must be written
        std::span<std::byte> upload(TransferOffset transfer, vk::Buffer dest,
                                    size_t dest_offset = 0) noexcept {
            transfer_buffer& buf = this->transfer_buffers[transfer.buffer];
            cmdbuf->copyBuffer(buf, dest,
                               vk::BufferCopy{.srcOffset = transfer.offset,
                                              .dstOffset = dest_offset,
                                              .size = transfer.byte_size});
            return buf->subspan(transfer.offset, transfer.byte_size);
        }

        /// @brief Registers the transfer on the command buffer.
        /// @param transfer Transfer information
        /// @param src Surface to upload
        /// @return The new texture with it's data upload already set up
        vgi::texture upload(TransferOffset transfer, const surface& src) noexcept {
            transfer_buffer& buf = this->transfer_buffers[transfer.buffer];
            return vgi::texture{this->parent(),
                                this->cmdbuf,
                                buf,
                                src,
                                vk::ImageUsageFlagBits::eSampled,
                                vk::SampleCountFlagBits::e1,
                                vk::ImageLayout::eShaderReadOnlyOptimal,
                                transfer.offset};
        }

        ~asset_uploader() {
            for (auto& buf: this->transfer_buffers) {
                std::move(buf).destroy(cmdbuf.window());
            }
        }
    };

    struct primitive_parser {
        fastgltf::Accessor* indices = nullptr;
        fastgltf::Accessor* position = nullptr;
        fastgltf::Accessor* normal = nullptr;
        fastgltf::Accessor* tangent = nullptr;
        fastgltf::Accessor* texcoord = nullptr;
        fastgltf::Accessor* color = nullptr;
        fastgltf::Accessor* joints = nullptr;
        fastgltf::Accessor* weights = nullptr;
        vk::PrimitiveTopology topology;
        TransferOffset index_transfer;
        TransferOffset vertex_transfer;

        primitive_parser(asset_parser& asset, fastgltf::Primitive& primitive) :
            indices(find_accessor(asset, primitive.indicesAccessor)),
            position(find_accessor(asset, primitive, "POSITION")),
            normal(find_accessor(asset, primitive, "NORMAL")),
            tangent(find_accessor(asset, primitive, "TANGENT")),
            texcoord(find_accessor(asset, primitive, "TEXCOORD_0")),
            color(find_accessor(asset, primitive, "COLOR_0")),
            joints(find_accessor(asset, primitive, "JOINTS_0")),
            weights(find_accessor(asset, primitive, "WEIGHTS_0")) {
            if (this->indices) {
                if (this->indices->count > static_cast<size_t>(UINT32_MAX)) {
                    throw vgi_error{"Primitive has too many indices"};
                } else {
                    this->index_transfer = asset.reserve(*this->indices);
                }
            }

            if (this->position) {
                this->vertex_transfer = asset.template reserve<vertex>(this->position->count);
            }

            switch (primitive.type) {
                case fastgltf::PrimitiveType::Points:
                    this->topology = vk::PrimitiveTopology::ePointList;
                    break;
                case fastgltf::PrimitiveType::Lines:
                    this->topology = vk::PrimitiveTopology::eLineList;
                    break;
                case fastgltf::PrimitiveType::LineStrip:
                    this->topology = vk::PrimitiveTopology::eLineStrip;
                    break;
                case fastgltf::PrimitiveType::Triangles:
                    this->topology = vk::PrimitiveTopology::eTriangleList;
                    break;
                case fastgltf::PrimitiveType::TriangleStrip:
                    this->topology = vk::PrimitiveTopology::eTriangleStrip;
                    break;
                case fastgltf::PrimitiveType::TriangleFan:
                    this->topology = vk::PrimitiveTopology::eTriangleFan;
                    break;
                default:
                    throw vgi_error{"Unsupported primitive topology"};
            }
        }

        primitive upload(asset_uploader& asset) {
            VGI_ASSERT(this->indices != nullptr);
            VGI_ASSERT(this->position != nullptr);
            primitive result;

            // Upload indices
            vertex_buffer* vertices = nullptr;
            switch (this->indices->componentType) {
                case fastgltf::ComponentType::UnsignedShort: {
                    auto& value = result.mesh.template emplace<vgi::mesh<uint16_t>>(
                            asset.parent(), this->position->count,
                            static_cast<uint32_t>(this->indices->count));
                    std::span<std::byte> dest = asset.upload(this->index_transfer, value.indices);
                    fastgltf::copyFromAccessor<uint16_t>(asset.asset, *this->indices,
                                                         static_cast<void*>(dest.data()));
                    vertices = &value.vertices;
                    break;
                }
                case fastgltf::ComponentType::UnsignedInt: {
                    auto& value = result.mesh.template emplace<vgi::mesh<uint32_t>>(
                            asset.parent(), this->position->count,
                            static_cast<uint32_t>(this->indices->count));
                    std::span<std::byte> dest = asset.upload(this->index_transfer, value.indices);
                    fastgltf::copyFromAccessor<uint32_t>(asset.asset, *this->indices,
                                                         static_cast<void*>(dest.data()));
                    vertices = &value.vertices;
                    break;
                }
                default:
                    throw vgi_error{"Invalid index type"};
            }

            // Upload vertices
            VGI_ASSERT(vertices != nullptr);
            std::byte* dest = asset.upload(this->vertex_transfer, *vertices).data();
            COPY_VERTEX_FIELD(origin, *this->position);

            if (this->normal) {
                COPY_VERTEX_FIELD(normal, *this->normal);
            } else {
                FILL_VERTEX_FIELD(normal, {0.0f, 1.0f, 0.0f});
            }
            if (this->texcoord) {
                COPY_VERTEX_FIELD(tex, *this->texcoord);
            } else {
                FILL_VERTEX_FIELD(tex, {0.0f, 0.0f});
            }
            if (this->color) {
                COPY_VERTEX_FIELD(color, *this->color);
            } else {
                FILL_VERTEX_FIELD(color, {1.0f, 1.0f, 1.0f, 1.0f});
            }
            if (this->joints) {
                COPY_VERTEX_FIELD(joints, *this->joints);
            } else {
                FILL_VERTEX_FIELD(joints, {0, 0, 0, 0});
            }
            if (this->weights) {
                COPY_VERTEX_FIELD(weights, *this->weights);
            } else {
                FILL_VERTEX_FIELD(weights, {1.0f, 0.0f, 0.0f, 0.0f});
            }

            return result;
        }

        static fastgltf::Accessor* find_accessor(asset_parser& asset,
                                                 std::optional<size_t> index) noexcept {
            if (!index.has_value()) return nullptr;
            return std::addressof(asset->accessors[*index]);
        }

        static fastgltf::Accessor* find_accessor(asset_parser& asset,
                                                 fastgltf::Primitive& primitive,
                                                 std::string_view name) noexcept {
            auto prim = primitive.findAttribute(name);
            if (prim == primitive.attributes.end()) return nullptr;
            return std::addressof(asset->accessors[prim->accessorIndex]);
        }

        template<class T, size_t offset>
        void copy_vertex_field(asset_uploader& asset, fastgltf::Accessor& accessor,
                               std::byte* vertices) {
            for (size_t i = 0; i < (std::min)(accessor.count, this->position->count); ++i) {
                std::byte* dest = vertices + i * sizeof(vertex) + offset;
                T value = fastgltf::getAccessorElement<T>(asset.asset, accessor, i);
                std::memcpy(dest, std::addressof(value), sizeof(T));
            }
        }

        template<class T, size_t offset>
        void fill_vertex_field(const T& value, std::byte* vertices) {
            for (size_t i = 0; i < this->position->count; ++i) {
                std::byte* dest = vertices + i * sizeof(vertex) + offset;
                std::memcpy(dest, std::addressof(value), sizeof(T));
            }
        }
    };

    struct mesh_parser {
        std::string name;
        std::vector<primitive_parser> primitives;

        mesh_parser(asset_parser& asset, fastgltf::Mesh& mesh) : name(mesh.name) {
            if (mesh.name.empty()) {
                vgi::log_dbg("Found anonymous mesh");
            } else {
                vgi::log_dbg("Found mesh '{}'", mesh.name);
            }

            this->primitives.reserve(mesh.primitives.size());
            for (fastgltf::Primitive& primitive: mesh.primitives) {
                primitive_parser parser{asset, primitive};
                if (!parser.indices || !parser.position)
                    throw vgi_error{"Mesh has invalid primitive"};
                this->primitives.push_back(parser);
            }
        }

        mesh upload(asset_uploader& asset) {
            mesh result{.name = std::move(this->name)};
            result.primitives.reserve(this->primitives.size());
            for (primitive_parser& primitive: this->primitives) {
                result.primitives.push_back(primitive.upload(asset));
            }
            return result;
        }
    };

    struct texture_parser {
        std::string name;
        std::shared_ptr<surface> image;
        sampler_options sampler;
        TransferOffset transfer;

        texture_parser(asset_parser& asset, fastgltf::Texture& tex) :
            name(tex.name),
            sampler(parse_sampler(tex.samplerIndex ? &asset->samplers[*tex.samplerIndex]
                                                   : nullptr)) {
            if (tex.name.empty()) {
                vgi::log_dbg("Found anonymous texture");
            } else {
                vgi::log_dbg("Found texture '{}'", tex.name);
            }

            std::optional<size_t> image_index = tex.imageIndex;
            if (!image_index) image_index = tex.webpImageIndex;
            if (!image_index) image_index = tex.ddsImageIndex;
            if (!image_index) image_index = tex.basisuImageIndex;
            if (!image_index) throw vgi_error{"Texure has no valid image"};
            this->image = asset.images[*image_index];
            this->transfer = asset.template reserve<std::byte>(
                    vgi::texture::transfer_size(asset.parent, *this->image));
        }

        texture upload(asset_uploader& asset) {
            vgi::texture tex = asset.upload(this->transfer, *this->image);
            if (!this->name.empty()) vmaSetAllocationName(asset.parent(), tex, this->name.c_str());

            return texture{
                    .texture = texture_sampler{asset.parent(), std::move(tex), this->sampler},
                    .name = std::move(this->name),
            };
        }
    };

    asset import(window& win, const std::filesystem::path& path,
                 const std::filesystem::path& directory) {
        fastgltf::Parser gltf_parser;
        fastgltf::GltfFileStream stream{path};
        if (!stream.isOpen()) throw vgi_error{"Error opening file"};

        auto asset = gltf_parser.loadGltf(stream, directory, LOAD_OPTIONS);
        if (auto error = asset.error(); error != fastgltf::Error::None) {
            throw vgi_error{fastgltf::getErrorMessage(error).data()};
        }
        vgi::log_dbg(VGI_OS("Importing GLTF asset at '{}'"), path.native());

        asset_parser parser{win, gltf_parser, asset.get()};
        std::vector<mesh_parser> meshes;
        std::vector<texture_parser> textures;

        // Setup uploaders
        textures.reserve(asset->textures.size());
        for (fastgltf::Texture& tex: asset->textures) {
            textures.emplace_back(parser, tex);
        }

        meshes.reserve(asset->meshes.size());
        for (fastgltf::Mesh& mesh: asset->meshes) {
            meshes.emplace_back(parser, mesh);
        }

        // Register uploads
        struct asset result;
        asset_uploader uploader{win, std::move(parser)};

        result.textures.reserve(textures.size());
        for (texture_parser& tex: textures) {
            result.textures.push_back(tex.upload(uploader));
        }

        result.meshes.reserve(meshes.size());
        for (mesh_parser& mesh: meshes) {
            result.meshes.push_back(mesh.upload(uploader));
        }

        // Wait for uploads to complete
        std::move(uploader.cmdbuf).submit_and_wait();
        return result;
    }

    void primitive::destroy(window& parent) && {
        std::visit([&](auto& mesh) { std::move(mesh).destroy(parent); }, this->mesh);
    }

    void mesh::destroy(window& parent) && {
        for (primitive& primitive: this->primitives) std::move(primitive).destroy(parent);
    }

    void texture::destroy(window& parent) && { std::move(this->texture).destroy(parent); }

    void asset::destroy(window& parent) && {
        for (mesh& mesh: this->meshes) std::move(mesh).destroy(parent);
        for (texture& tex: this->textures) std::move(tex).destroy(parent);
    }
}  // namespace vgi::asset::gltf
