#include "fs.hpp"

#include <fstream>

#define DECLARE_READ_FILE(__T)                          \
    template vgi::unique_span<__T> vgi::read_file<__T>( \
            const std::filesystem::path::value_type* VGI_RESTRICT)

namespace vgi {
    template<class T>
        requires(std::is_trivially_copy_constructible_v<T> && std::is_trivially_destructible_v<T>)
    unique_span<T> read_file(const std::filesystem::path::value_type* VGI_RESTRICT path) {
        std::ifstream file;
        file.exceptions(std::ifstream::failbit | std::ifstream::badbit);
        file.open(path, std::ios_base::in | std::ios_base::binary | std::ios_base::ate);
        VGI_ASSERT(file.is_open());

        std::streamoff file_size = static_cast<std::streamoff>(file.tellg());
        VGI_ASSERT(file_size >= 0);
        if (file_size >= SIZE_MAX) throw std::length_error{"file is too big"};
        if (file_size % sizeof(T) != 0)
            throw std::length_error{"file size is not a multiple of element size"};
        file.seekg(0, std::ios_base::beg);

        unique_span<T> buffer = make_unique_span_for_overwrite<T>(file_size / sizeof(T));
        char* VGI_RESTRICT ptr = reinterpret_cast<char * VGI_RESTRICT>(buffer.data());
        size_t size = file_size;

        while (size > 0) {
            if (file.eof()) throw std::runtime_error{"unexpected eof"};
            file.read(ptr, size);
            std::streamoff bytes_read = static_cast<std::streamoff>(file.gcount());
            VGI_ASSERT(bytes_read >= 0 && bytes_read <= size);
            ptr += bytes_read;
            size -= bytes_read;
        }

        file.close();
        return buffer;
    }

}  // namespace vgi

DECLARE_READ_FILE(std::byte);
DECLARE_READ_FILE(char);
DECLARE_READ_FILE(signed char);
DECLARE_READ_FILE(signed short);
DECLARE_READ_FILE(signed int);
DECLARE_READ_FILE(signed long);
DECLARE_READ_FILE(signed long long);
DECLARE_READ_FILE(unsigned char);
DECLARE_READ_FILE(unsigned short);
DECLARE_READ_FILE(unsigned int);
DECLARE_READ_FILE(unsigned long);
DECLARE_READ_FILE(unsigned long long);
