#include <elf.h>
#include <link.h>

#include <cassert>
#include <cstring>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <exasol/udf/v2/json_schema.hpp>

namespace isolated_nlohmann = exasol::udf::v2::third_party::nlohmann;

namespace {

constexpr std::string_view kGlobalNamespacePrefix = "_ZN8nlohmann";
constexpr std::string_view kIsolatedNamespacePrefix =
    "_ZN6exasol3udf2v211third_party8nlohmann";

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

std::vector<char> read_file(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        fail("cannot read validator library: " + path);
    }
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

bool is_exported(const Elf64_Sym& symbol) {
    const unsigned char binding = ELF64_ST_BIND(symbol.st_info);
    const unsigned char visibility = ELF64_ST_VISIBILITY(symbol.st_other);
    return symbol.st_shndx != SHN_UNDEF &&
           (binding == STB_GLOBAL || binding == STB_WEAK) &&
           (visibility == STV_DEFAULT || visibility == STV_PROTECTED);
}

void verify_symbols(const std::string& library_path) {
    const std::vector<char> file = read_file(library_path);
    const Elf64_Ehdr header = read_object<Elf64_Ehdr>(file, 0);
    if (std::memcmp(header.e_ident, ELFMAG, SELFMAG) != 0 ||
        header.e_ident[EI_CLASS] != ELFCLASS64 ||
        header.e_ident[EI_DATA] != ELFDATA2LSB ||
        header.e_shentsize != sizeof(Elf64_Shdr)) {
        fail("validator library is not a little-endian ELF64 file");
    }

    const std::size_t section_count = header.e_shnum;
    const std::size_t section_table = header.e_shoff;
    if (section_count == 0 ||
        section_table > file.size() ||
        section_count > (file.size() - section_table) / sizeof(Elf64_Shdr)) {
        fail("ELF section table is invalid");
    }

    Elf64_Shdr dynamic_symbols{};
    bool found_dynamic_symbols = false;
    for (std::size_t index = 0; index < section_count; ++index) {
        const Elf64_Shdr section = read_object<Elf64_Shdr>(
            file, section_table + index * sizeof(Elf64_Shdr));
        if (section.sh_type == SHT_DYNSYM) {
            dynamic_symbols = section;
            found_dynamic_symbols = true;
            break;
        }
    }
    if (!found_dynamic_symbols || dynamic_symbols.sh_link >= section_count ||
        dynamic_symbols.sh_entsize != sizeof(Elf64_Sym) ||
        dynamic_symbols.sh_size % sizeof(Elf64_Sym) != 0) {
        fail("ELF dynamic symbol table is invalid");
    }

    const Elf64_Shdr string_table = read_object<Elf64_Shdr>(
        file, section_table + dynamic_symbols.sh_link * sizeof(Elf64_Shdr));
    if (dynamic_symbols.sh_offset > file.size() ||
        dynamic_symbols.sh_size > file.size() - dynamic_symbols.sh_offset ||
        string_table.sh_offset > file.size() ||
        string_table.sh_size > file.size() - string_table.sh_offset) {
        fail("ELF dynamic symbol data is truncated");
    }

    bool found_isolated_symbol = false;
    for (std::size_t offset = 0; offset < dynamic_symbols.sh_size;
         offset += sizeof(Elf64_Sym)) {
        const Elf64_Sym symbol = read_object<Elf64_Sym>(
            file, dynamic_symbols.sh_offset + offset);
        if (!is_exported(symbol) || symbol.st_name >= string_table.sh_size) {
            continue;
        }

        const std::string name = read_string(
            file, string_table.sh_offset + symbol.st_name,
            string_table.sh_size - symbol.st_name);
        if (name.compare(0, kGlobalNamespacePrefix.size(),
                         kGlobalNamespacePrefix) == 0) {
            fail("validator exports a global nlohmann symbol: " + name);
        }
        if (name.compare(0, kIsolatedNamespacePrefix.size(),
                         kIsolatedNamespacePrefix) == 0) {
            found_isolated_symbol = true;
        }
    }

    if (!found_isolated_symbol) {
        fail("validator does not export an isolated nlohmann symbol");
    }
}

void verify_loaded_validator() {
    Dl_info library{};
    const auto anchor = &isolated_nlohmann::json_schema::default_string_format_check;
    if (dladdr(reinterpret_cast<const void*>(anchor), &library) == 0 ||
        library.dli_fname == nullptr) {
        fail("cannot locate validator library");
    }
    verify_symbols(library.dli_fname);
}

}  // namespace

int main() {
    verify_loaded_validator();
}
