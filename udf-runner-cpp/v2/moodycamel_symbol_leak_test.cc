#include <elf.h>

#include <cassert>
#include <cstring>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <exasol/udf/v2/mpmc_queue.hpp>

namespace {

constexpr std::string_view kGlobalNamespacePrefix = "_ZN10moodycamel";
constexpr std::string_view kIsolatedNamespacePrefix =
    "_ZN6exasol3udf2v211third_party10moodycamel";

[[noreturn]] void fail(const std::string& message) {
    throw std::runtime_error(message);
}

template <typename T>
T read_object(const std::vector<char>& file, std::size_t offset) {
    if (offset > file.size() || file.size() - offset < sizeof(T)) {
        fail("ELF file is truncated");
    }
    T result;
    std::memcpy(&result, file.data() + offset, sizeof(result));
    return result;
}

std::string read_string(const std::vector<char>& file, std::size_t offset,
                        std::size_t maximum_size) {
    if (offset > file.size() || file.size() - offset < maximum_size) {
        fail("ELF string table is truncated");
    }
    const char* begin = file.data() + offset;
    const void* end = std::memchr(begin, '\0', maximum_size);
    if (end == nullptr) {
        fail("ELF symbol name is not terminated");
    }
    return std::string(begin, static_cast<const char*>(end));
}

void verify_symbols(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        fail("cannot read queue library: " + path);
    }
    const std::vector<char> file{std::istreambuf_iterator<char>(input),
                                 std::istreambuf_iterator<char>()};
    const Elf64_Ehdr header = read_object<Elf64_Ehdr>(file, 0);
    if (std::memcmp(header.e_ident, ELFMAG, SELFMAG) != 0 ||
        header.e_ident[EI_CLASS] != ELFCLASS64 ||
        header.e_ident[EI_DATA] != ELFDATA2LSB ||
        header.e_shentsize != sizeof(Elf64_Shdr)) {
        fail("queue library is not a little-endian ELF64 file");
    }

    Elf64_Shdr dynamic_symbols{};
    bool found_dynamic_symbols = false;
    for (std::size_t i = 0; i < header.e_shnum; ++i) {
        const Elf64_Shdr section = read_object<Elf64_Shdr>(
            file, header.e_shoff + i * sizeof(Elf64_Shdr));
        if (section.sh_type == SHT_DYNSYM) {
            dynamic_symbols = section;
            found_dynamic_symbols = true;
            break;
        }
    }
    if (!found_dynamic_symbols || dynamic_symbols.sh_link >= header.e_shnum ||
        dynamic_symbols.sh_entsize != sizeof(Elf64_Sym)) {
        fail("ELF dynamic symbol table is invalid");
    }

    const Elf64_Shdr string_table = read_object<Elf64_Shdr>(
        file, header.e_shoff + dynamic_symbols.sh_link * sizeof(Elf64_Shdr));
    bool found_isolated_symbol = false;
    for (std::size_t offset = 0; offset < dynamic_symbols.sh_size;
         offset += sizeof(Elf64_Sym)) {
        const Elf64_Sym symbol = read_object<Elf64_Sym>(
            file, dynamic_symbols.sh_offset + offset);
        if (symbol.st_name >= string_table.sh_size) {
            continue;
        }
        const std::string name = read_string(
            file, string_table.sh_offset + symbol.st_name,
            string_table.sh_size - symbol.st_name);
        if (name.starts_with(kGlobalNamespacePrefix)) {
            fail("queue library exports a global moodycamel symbol: " + name);
        }
        if (name.starts_with(kIsolatedNamespacePrefix)) {
            found_isolated_symbol = true;
        }
    }
    if (!found_isolated_symbol) {
        fail("queue library does not export an isolated moodycamel symbol");
    }
}

}  // namespace

int main(int argc, char** argv) {
    assert(argc == 2);
    verify_symbols(argv[1]);
}
