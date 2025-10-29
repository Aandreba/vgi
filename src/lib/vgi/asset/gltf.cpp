#include "gltf.hpp"

#include <bit>
#include <concepts>
#include <fastgltf/core.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/math.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>
#include <glm/glm.hpp>
#include <memory>
#include <ranges>
#include <type_traits>
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

template<class T, glm::qualifier Q = glm::defaultp>
constexpr glm::vec<2, T, Q> to_glm(const fastgltf::math::vec<T, 2>& lhs) noexcept {
    return glm::vec<2, T, Q>{lhs[0], lhs[1]};
}
template<class T, glm::qualifier Q = glm::defaultp>
constexpr glm::vec<3, T, Q> to_glm(const fastgltf::math::vec<T, 3>& lhs) noexcept {
    return glm::vec<3, T, Q>{lhs[0], lhs[1], lhs[2]};
}
template<class T, glm::qualifier Q = glm::defaultp>
constexpr glm::vec<4, T, Q> to_glm(const fastgltf::math::vec<T, 4>& lhs) noexcept {
    return glm::vec<4, T, Q>{lhs[0], lhs[1], lhs[2], lhs[3]};
}
template<class T, glm::qualifier Q = glm::defaultp>
constexpr glm::qua<T, Q> to_glm(const fastgltf::math::quat<T>& lhs) noexcept {
    return glm::qua<T, Q>{lhs.w(), lhs.x(), lhs.y(), lhs.z()};
}

namespace vgi::asset::gltf {
    static material parse_material(const fastgltf::Material& mat) {
        material result;

        if (mat.normalTexture) {
            if (mat.normalTexture->texCoordIndex != 0) {
                throw vgi_error{"Invalid texture coordinate index"};
            } else if (mat.normalTexture->transform != nullptr) {
                throw vgi_error{"'KHR_texture_transform' is not supported"};
            } else {
                result.normal = normal_texture{.texture = mat.normalTexture->textureIndex,
                                               .scale = mat.normalTexture->scale};
            }
        }

        if (mat.occlusionTexture) {
            if (mat.occlusionTexture->texCoordIndex != 0) {
                throw vgi_error{"Invalid texture coordinate index"};
            } else if (mat.occlusionTexture->transform != nullptr) {
                throw vgi_error{"'KHR_texture_transform' is not supported"};
            } else {
                result.occlusion = occlusion_texture{.texture = mat.occlusionTexture->textureIndex,
                                                     .strength = mat.occlusionTexture->strength};
            }
        }

        if (mat.emissiveTexture) {
            if (mat.emissiveTexture->texCoordIndex != 0) {
                throw vgi_error{"Invalid texture coordinate index"};
            } else if (mat.emissiveTexture->transform != nullptr) {
                throw vgi_error{"'KHR_texture_transform' is not supported"};
            } else {
                result.emissive = emissive_texture{.texture = mat.emissiveTexture->textureIndex,
                                                   .factor = to_glm(mat.emissiveFactor)};
            }
        }

        switch (mat.alphaMode) {
            case fastgltf::AlphaMode::Opaque:
                result.alpha_mode = alpha_mode::opaque;
                break;
            case fastgltf::AlphaMode::Mask:
                result.alpha_mode = alpha_mode::mask;
                break;
            case fastgltf::AlphaMode::Blend:
                result.alpha_mode = alpha_mode::blend;
                break;
        }

        result.alpha_cutoff = mat.alphaCutoff;
        result.double_sided = mat.doubleSided;
        return result;
    }

    static node parse_node(const fastgltf::Node& n) {
        const fastgltf::TRS* trs = std::get_if<fastgltf::TRS>(std::addressof(n.transform));
        VGI_ASSERT(trs != nullptr);
        node result{
                .local_origin = to_glm(trs->translation),
                .local_rotation = to_glm(trs->rotation),
                .local_scale = to_glm(trs->scale),
                .mesh = n.meshIndex,
                .skin = n.skinIndex,
                .name = std::string{n.name},
        };

        result.children.insert(result.children.cend(), n.children.cbegin(), n.children.cend());
        return result;
    }

    static scene parse_scene(const fastgltf::Scene& s) {
        scene result{.name = std::string{s.name}};
        result.roots.insert(result.roots.cend(), s.nodeIndices.cbegin(), s.nodeIndices.cend());
        return result;
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
        std::vector<scene> scenes;
        std::vector<node> nodes;
        std::vector<std::string> skins;
        std::vector<animation> animations;
        std::vector<std::shared_ptr<surface>> images;
        std::vector<std::shared_ptr<material>> materials;
        std::vector<size_t> transfer_buffers;
        size_t transfer_size = 0;

        asset_parser(window& parent, fastgltf::Parser& parser, fastgltf::Asset& asset) :
            parent(parent), parser(parser), asset(asset) {
            this->scenes.reserve(asset.scenes.size());
            for (fastgltf::Scene& scene: asset.scenes) {
                if (scene.name.empty()) {
                    vgi::log_dbg("Found anonymous scene");
                } else {
                    vgi::log_dbg("Found scene '{}'", scene.name);
                }
                this->scenes.push_back(parse_scene(scene));
            }

            this->nodes.reserve(asset.nodes.size());
            for (fastgltf::Node& node: asset.nodes) {
                if (node.name.empty()) {
                    vgi::log_dbg("Found anonymous node");
                } else {
                    vgi::log_dbg("Found node '{}'", node.name);
                }
                this->nodes.push_back(parse_node(node));
            }

            this->skins.reserve(asset.skins.size());
            for (size_t i = 0; i < asset.skins.size(); ++i) {
                if (asset.skins[i].name.empty()) {
                    vgi::log_dbg("Found anonymous skin");
                } else {
                    vgi::log_dbg("Found skin '{}'", asset.skins[i].name);
                }
                this->skins.push_back(this->parse_skin(i));
            }

            this->animations.reserve(asset.skins.size());
            for (size_t i = 0; i < asset.animations.size(); ++i) {
                if (asset.animations[i].name.empty()) {
                    vgi::log_dbg("Found anonymous animation");
                } else {
                    vgi::log_dbg("Found animation '{}'", asset.animations[i].name);
                }
                this->animations.push_back(this->parse_animation(i));
            }

            this->images.reserve(asset.images.size());
            for (fastgltf::Image& img: asset.images) {
                if (img.name.empty()) {
                    vgi::log_dbg("Found anonymous image");
                } else {
                    vgi::log_dbg("Found image '{}'", img.name);
                }
                this->images.push_back(std::make_shared<surface>(load_image(img.data)));
            }

            this->materials.reserve(asset.images.size());
            for (fastgltf::Material& mat: asset.materials) {
                if (mat.name.empty()) {
                    vgi::log_dbg("Found anonymous material");
                } else {
                    vgi::log_dbg("Found material '{}'", mat.name);
                }
                this->materials.push_back(std::make_shared<material>(parse_material(mat)));
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

        std::string parse_skin(size_t index) {
            const fastgltf::Skin& skin = this->asset.skins[index];

            if (skin.inverseBindMatrices) {
                auto range = fastgltf::iterateAccessor<glm::mat4>(
                        this->asset, this->asset.accessors[*skin.inverseBindMatrices]);

                auto it = range.begin();
                for (size_t i = 0; i < skin.joints.size(); ++i, ++it) {
                    if (it == range.end()) throw vgi_error{"Invalid skin data"};
                    this->nodes[skin.joints[i]].attachments.emplace_back(index, i, *it);
                }
            } else {
                for (size_t i = 0; i < skin.joints.size(); ++i) {
                    this->nodes[skin.joints[i]].attachments.emplace_back(index, i);
                }
            }

            return std::string{skin.name};
        }

        template<std::ranges::range R>
            requires(std::is_same_v<std::ranges::range_value_t<R>, glm::mat4>)
        std::string parse_skin(std::string_view name, std::span<const size_t> joints, R&& r) {
            return name;
        }

        animation_sampler parse_animation_sampler(const fastgltf::AnimationSampler& sampler) {
            animation_sampler result;
            switch (sampler.interpolation) {
                case fastgltf::AnimationInterpolation::Step:
                    result.interpolation = interpolation::step;
                    break;
                case fastgltf::AnimationInterpolation::Linear:
                    result.interpolation = interpolation::linear;
                    break;
                case fastgltf::AnimationInterpolation::CubicSpline:
                    result.interpolation = interpolation::cubic_spline;
                    break;
            }

            const fastgltf::Accessor& input = this->asset.accessors[sampler.inputAccessor];
            unique_span<float> keyframes = make_unique_span_for_overwrite<float>(input.count);
            fastgltf::copyFromAccessor<float>(this->asset, input, keyframes.data());

            const fastgltf::Accessor& output = this->asset.accessors[sampler.outputAccessor];
            unique_span<float> values = make_unique_span_for_overwrite<float>(
                    fastgltf::getNumComponents(output.type) * output.count);

            switch (output.componentType) {
                case fastgltf::ComponentType::Float:
                    fastgltf::copyComponentsFromAccessor<float>(this->asset, output, values.data());
                    break;
                case fastgltf::ComponentType::Byte: {
                    float* ptr = values.data();
                    for (auto val: fastgltf::iterateAccessor<glm::i8vec4>(this->asset, output)) {
                        glm::vec4 norm = glm::max(glm::vec4{val} / 127.0f, -1.0f);
                        ptr[0] = norm.x;
                        ptr[1] = norm.y;
                        ptr[2] = norm.z;
                        ptr[3] = norm.w;
                        ptr += 4;
                    }
                    break;
                }
                case fastgltf::ComponentType::UnsignedByte: {
                    float* ptr = values.data();
                    for (auto val: fastgltf::iterateAccessor<glm::u8vec4>(this->asset, output)) {
                        glm::vec4 norm = glm::vec4{val} / 255.0f;
                        ptr[0] = norm.x;
                        ptr[1] = norm.y;
                        ptr[2] = norm.z;
                        ptr[3] = norm.w;
                        ptr += 4;
                    }
                    break;
                }
                case fastgltf::ComponentType::Short: {
                    float* ptr = values.data();
                    for (auto val: fastgltf::iterateAccessor<glm::i16vec4>(this->asset, output)) {
                        glm::vec4 norm = glm::max(glm::vec4{val} / 32767.0f, -1.0f);
                        ptr[0] = norm.x;
                        ptr[1] = norm.y;
                        ptr[2] = norm.z;
                        ptr[3] = norm.w;
                        ptr += 4;
                    }
                    break;
                }
                case fastgltf::ComponentType::UnsignedShort: {
                    float* ptr = values.data();
                    for (auto val: fastgltf::iterateAccessor<glm::u16vec4>(this->asset, output)) {
                        glm::vec4 norm = glm::vec4{val} / 65535.0f;
                        ptr[0] = norm.x;
                        ptr[1] = norm.y;
                        ptr[2] = norm.z;
                        ptr[3] = norm.w;
                        ptr += 4;
                    }
                    break;
                }
                default:
                    throw vgi_error{"Animation sampler value type is not supported"};
            }

            result.keyframes = std::move(keyframes);
            result.values = std::move(values);
            return result;
        }

        animation parse_animation(size_t index) {
            const fastgltf::Animation& anim = this->asset.animations[index];
            animation result{.name = std::string{anim.name}};

            result.samplers.reserve(anim.samplers.size());
            for (const fastgltf::AnimationSampler& sampler: anim.samplers) {
                result.samplers.push_back(this->parse_animation_sampler(sampler));
            }

            for (const fastgltf::AnimationChannel& channel: anim.channels) {
                if (!channel.nodeIndex.has_value()) continue;
                node& node = this->nodes[*channel.nodeIndex];
                node_animation& result = node.animations[index];
                switch (channel.path) {
                    case fastgltf::AnimationPath::Translation: {
                        if (result.origin.has_value()) {
                            throw vgi_error{
                                    "Node translation has multiple samplers for the same "
                                    "animation"};
                        }
                        result.origin = channel.samplerIndex;
                        break;
                    }
                    case fastgltf::AnimationPath::Rotation: {
                        if (result.rotation.has_value()) {
                            throw vgi_error{
                                    "Node rotation has multiple samplers for the same "
                                    "animation"};
                        }
                        result.rotation = channel.samplerIndex;
                        break;
                    }
                    case fastgltf::AnimationPath::Scale: {
                        if (result.scale.has_value()) {
                            throw vgi_error{
                                    "Node scale has multiple samplers for the same "
                                    "animation"};
                        }
                        result.scale = channel.samplerIndex;
                        break;
                    }
                    case fastgltf::AnimationPath::Weights:
                        log_warn("Morph target animations are not supported");
                        break;
                }
            }

            return result;
        }
    };

    struct asset_uploader {
        fastgltf::Asset& asset;
        command_buffer cmdbuf;
        std::vector<transfer_buffer> transfer_buffers;

        asset_uploader(window& win, asset_parser& parser) : asset(parser.asset), cmdbuf(win) {
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
        std::shared_ptr<material> material;
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
            weights(find_accessor(asset, primitive, "WEIGHTS_0")),
            material(primitive.materialIndex ? asset.materials[*primitive.materialIndex]
                                             : nullptr) {
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
            result.material = this->material;
            result.topology = this->topology;

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

    asset::asset(window& win, const std::filesystem::path& path,
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
        asset_uploader uploader{win, parser};

        this->textures.reserve(textures.size());
        for (texture_parser& tex: textures) {
            this->textures.push_back(tex.upload(uploader));
        }

        this->meshes.reserve(meshes.size());
        for (mesh_parser& mesh: meshes) {
            this->meshes.push_back(mesh.upload(uploader));
        }

        // Wait for uploads to complete
        std::move(uploader.cmdbuf).submit_and_wait();
        this->scenes = std::move(parser.scenes);
        this->nodes = std::move(parser.nodes);
        this->skins = std::move(parser.skins);
        this->animations = std::move(parser.animations);
    }

    template<>
    glm::vec3 animation_sampler::sample<glm::vec3>(duration_type time) const {
        VGI_ASSERT(this->values.size() > 0);
        VGI_ASSERT(this->values.size() % 3 == 0);

        size_t lower_bound =
                std::ranges::lower_bound(this->keyframes, time.count()) - this->keyframes.data();
        size_t upper_bound = math::sat_add<size_t>(lower_bound, 1);
        float dur, t;

        if (upper_bound >= this->keyframes.size()) {
            upper_bound = this->keyframes.size() - 1;
            lower_bound = math::sat_sub<size_t>(upper_bound, 1);
            dur = this->keyframes[upper_bound] - this->keyframes[lower_bound];
            t = 1.0f;
        } else {
            dur = this->keyframes[upper_bound] - this->keyframes[lower_bound];
            t = (time.count() - this->keyframes[lower_bound]) / dur;
        }

        switch (this->interpolation) {
            case interpolation::step: {
                const float* xyz = this->values.data() + 3 * lower_bound;
                return glm::vec3{xyz[0], xyz[1], xyz[2]};
            }
            case interpolation::linear: {
                const float* lhs = this->values.data() + 3 * lower_bound;
                const float* rhs = this->values.data() + 3 * upper_bound;
                return glm::mix(glm::vec3{lhs[0], lhs[1], lhs[2]},
                                glm::vec3{rhs[0], rhs[1], rhs[2]}, t);
            }
            case interpolation::cubic_spline: {
                const float* in = this->values.data() + 0 * 3 * this->keyframes.size();
                const float* val = this->values.data() + 1 * 3 * this->keyframes.size();
                const float* out = this->values.data() + 2 * 3 * this->keyframes.size();

                const float* lhs = in + 3 * lower_bound;
                glm::vec3 lhs_in{lhs[0], lhs[1], lhs[2]};
                lhs = val + 3 * lower_bound;
                glm::vec3 lhs_val{lhs[0], lhs[1], lhs[2]};
                lhs = out + 3 * lower_bound;
                glm::vec3 lhs_out{lhs[0], lhs[1], lhs[2]};

                const float* rhs = in + 3 * upper_bound;
                glm::vec3 rhs_in{rhs[0], rhs[1], rhs[2]};
                rhs = val + 3 * upper_bound;
                glm::vec3 rhs_val{rhs[0], rhs[1], rhs[2]};
                rhs = out + 3 * upper_bound;
                glm::vec3 rhs_out{rhs[0], rhs[1], rhs[2]};

                const float t3 = t * t * t;
                const float t2 = t * t;

                glm::vec3 res = (2.0f * t3 - 3.0f * t2 + 1.0f) * lhs_val;
                res += dur * (t3 - 2.0f * t2 + t) * lhs_out;
                res += (-2.0f * t3 + 3.0f * t2) * rhs_val;
                res += dur * (t3 - t2) * rhs_in;

                return res;
            }
            default:
                VGI_UNREACHABLE;
        }
    }

    template<>
    glm::quat animation_sampler::sample<glm::quat>(duration_type time) const {
        VGI_ASSERT(this->values.size() > 0);
        VGI_ASSERT(this->values.size() % 4 == 0);

        size_t lower_bound =
                std::ranges::lower_bound(this->keyframes, time.count()) - this->keyframes.data();
        size_t upper_bound = math::sat_add<size_t>(lower_bound, 1);
        float dur, t;

        if (upper_bound >= this->keyframes.size()) {
            upper_bound = this->keyframes.size() - 1;
            lower_bound = math::sat_sub<size_t>(upper_bound, 1);
            dur = this->keyframes[upper_bound] - this->keyframes[lower_bound];
            t = 1.0f;
        } else {
            dur = this->keyframes[upper_bound] - this->keyframes[lower_bound];
            t = (time.count() - this->keyframes[lower_bound]) / dur;
        }

        switch (this->interpolation) {
            case interpolation::step: {
                const float* xyzw = this->values.data() + 4 * lower_bound;
                return glm::quat{xyzw[3], xyzw[0], xyzw[1], xyzw[2]};
            }
            case interpolation::linear: {
                const float* lhs = this->values.data() + 4 * lower_bound;
                const float* rhs = this->values.data() + 4 * upper_bound;
                return glm::slerp(glm::quat{lhs[3], lhs[0], lhs[1], lhs[2]},
                                  glm::quat{rhs[3], rhs[0], rhs[1], rhs[2]}, t);
            }
            case interpolation::cubic_spline: {
                const float* in = this->values.data() + 0 * 4 * this->keyframes.size();
                const float* val = this->values.data() + 1 * 4 * this->keyframes.size();
                const float* out = this->values.data() + 2 * 4 * this->keyframes.size();

                const float* lhs = in + 4 * lower_bound;
                glm::quat lhs_in{lhs[3], lhs[0], lhs[1], lhs[2]};
                lhs = val + 4 * lower_bound;
                glm::quat lhs_val{lhs[3], lhs[0], lhs[1], lhs[2]};
                lhs = out + 4 * lower_bound;
                glm::quat lhs_out{lhs[3], lhs[0], lhs[1], lhs[2]};

                const float* rhs = in + 4 * upper_bound;
                glm::quat rhs_in{rhs[3], rhs[0], rhs[1], rhs[2]};
                rhs = val + 4 * upper_bound;
                glm::quat rhs_val{rhs[3], rhs[0], rhs[1], rhs[2]};
                rhs = out + 4 * upper_bound;
                glm::quat rhs_out{rhs[3], rhs[0], rhs[1], rhs[2]};

                const float t3 = t * t * t;
                const float t2 = t * t;

                glm::quat res = (2.0f * t3 - 3.0f * t2 + 1.0f) * lhs_val;
                res += dur * (t3 - 2.0f * t2 + t) * lhs_out;
                res += (-2.0f * t3 + 3.0f * t2) * rhs_val;
                res += dur * (t3 - t2) * rhs_in;

                return glm::normalize(res);
            }
            default:
                VGI_UNREACHABLE;
        }
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
