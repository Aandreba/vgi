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

#define COPY_VERTEX_FIELD(__field, ...)                                \
    this->template copy_vertex_field<decltype(::vgi::vertex::__field), \
                                     offsetof(::vgi::vertex, __field)>(asset, __VA_ARGS__, dest)
#define FILL_VERTEX_FIELD(__field, ...)                                \
    this->template fill_vertex_field<decltype(::vgi::vertex::__field), \
                                     offsetof(::vgi::vertex, __field)>(__VA_ARGS__, dest)

constexpr fastgltf::Options LOAD_OPTIONS =
        fastgltf::Options::DecomposeNodeMatrices | fastgltf::Options::GenerateMeshIndices;

namespace vgi::asset::gltf {
    struct TransferOffset {
        size_t buffer;
        size_t offset;
        size_t byte_size;
    };

    struct AssetParser {
        fastgltf::Asset& asset;
        std::vector<size_t> transfer_buffers;
        size_t transfer_size = 0;

        inline fastgltf::Asset* operator->() noexcept { return std::addressof(this->asset); }
        inline const fastgltf::Asset* operator->() const noexcept {
            return std::addressof(this->asset);
        }

        /// @brief Reserves device mapped memory for a later upload
        /// @param count Number of elements to reserve memory for
        /// @return The transfer information of the reserved memory
        template<class T>
        inline TransferOffset reserve(size_t count) {
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
        inline TransferOffset reserve(const fastgltf::Accessor& accessor) {
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

        void parse() {
            // TODO
        }
    };

    struct AssetUploader {
        fastgltf::Asset& asset;
        command_buffer cmdbuf;
        std::vector<transfer_buffer> transfer_buffers;

        AssetUploader(window& win, AssetParser&& parser) : asset(parser.asset), cmdbuf(win) {
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

        ~AssetUploader() {
            for (auto& buf: this->transfer_buffers) {
                std::move(buf).destroy(cmdbuf.window());
            }
        }
    };

    struct PrimitiveParser {
        fastgltf::Accessor* indices = nullptr;
        fastgltf::Accessor* position = nullptr;
        fastgltf::Accessor* normal = nullptr;
        fastgltf::Accessor* tangent = nullptr;
        fastgltf::Accessor* texcoord = nullptr;
        fastgltf::Accessor* color = nullptr;
        fastgltf::Accessor* joints = nullptr;
        fastgltf::Accessor* weights = nullptr;
        TransferOffset index_transfer;
        TransferOffset vertex_transfer;

        PrimitiveParser(AssetParser& asset, fastgltf::Primitive& primitive) :
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
        }

        primitive upload(AssetUploader& asset) {
            VGI_ASSERT(this->indices != nullptr);
            VGI_ASSERT(this->position != nullptr);
            primitive result;

            // Upload indices
            vertex_buffer* vertices = nullptr;
            switch (this->indices->componentType) {
                case fastgltf::ComponentType::UnsignedShort: {
                    auto& value = result.mesh.template emplace<mesh<uint16_t>>(
                            asset.parent(), this->position->count, this->indices->count);
                    std::span<std::byte> dest = asset.upload(this->index_transfer, value.indices);
                    fastgltf::copyFromAccessor<uint16_t>(asset.asset, *this->indices,
                                                         static_cast<void*>(dest.data()));
                    vertices = &value.vertices;
                    break;
                }
                case fastgltf::ComponentType::UnsignedInt: {
                    auto& value = result.mesh.template emplace<mesh<uint32_t>>(
                            asset.parent(), this->position->count, this->indices->count);
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

        static fastgltf::Accessor* find_accessor(AssetParser& asset,
                                                 std::optional<size_t> index) noexcept {
            if (!index.has_value()) return nullptr;
            return std::addressof(asset->accessors[*index]);
        }

        static fastgltf::Accessor* find_accessor(AssetParser& asset, fastgltf::Primitive& primitive,
                                                 std::string_view name) noexcept {
            auto prim = primitive.findAttribute(name);
            if (prim == primitive.attributes.end()) return nullptr;
            return std::addressof(asset->accessors[prim->accessorIndex]);
        }

        template<class T, size_t offset>
        void copy_vertex_field(AssetUploader& asset, fastgltf::Accessor& accessor,
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

    struct MeshParser {
        std::vector<PrimitiveParser> primitives;

        MeshParser(AssetParser& asset, fastgltf::Mesh& mesh) {
            if (mesh.name.empty()) {
                vgi::log_dbg("Found anonymous mesh");
            } else {
                vgi::log_dbg("Found mesh '{}'", mesh.name);
            }

            this->primitives.reserve(mesh.primitives.size());
            for (fastgltf::Primitive& primitive: mesh.primitives) {
                PrimitiveParser parser{asset, primitive};
                if (!parser.indices || !parser.position)
                    throw vgi_error{"Mesh has invalid primitive"};
                this->primitives.push_back(parser);
            }
        }

        void upload(AssetUploader& asset) {
            for (PrimitiveParser& primitive: this->primitives) {
                primitive.upload(asset);
            }
        }
    };

    static void importGltf(const std::filesystem::path& path,
                           const std::filesystem::path& directory) {
        fastgltf::Parser parser;
        fastgltf::GltfFileStream stream{path};
        if (!stream.isOpen()) throw vgi_error{"Error opening file"};

        auto asset = parser.loadGltf(stream, directory, LOAD_OPTIONS);
        if (auto error = asset.error(); error != fastgltf::Error::None) {
            throw vgi_error{fastgltf::getErrorMessage(error).data()};
        }

        vgi::log_dbg(VGI_OS("Importing GLTF asset at '{}'"), path.native());
    }
}  // namespace vgi::asset::gltf
