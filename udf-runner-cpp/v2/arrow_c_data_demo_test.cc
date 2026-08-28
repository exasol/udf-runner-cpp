#include <elf.h>
#include <link.h>

#include <cassert>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <arrow/api.h>
#include <arrow/c/bridge.h>

#include <dlfcn.h>

namespace {

using export_fn_t = int (*)(ArrowArray*, ArrowSchema*);
using consume_fn_t = int (*)(ArrowArray*, ArrowSchema*, int64_t*, int64_t*);
using error_fn_t = const char* (*)(void);

constexpr std::string_view kArrowMangledPrefix = "_ZN5arrow";
constexpr std::string_view kDemoExportedPrefix = "udf_runner_cpp_v2_demo_";

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
        fail("cannot read shared library: " + path);
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
        fail("shared library is not a little-endian ELF64 file");
    }

    const std::size_t section_count = header.e_shnum;
    const std::size_t section_table = header.e_shoff;
    if (section_count == 0 || section_table > file.size() ||
        section_count > (file.size() - section_table) / sizeof(Elf64_Shdr)) {
        fail("ELF section table is invalid");
    }

    Elf64_Shdr dynamic_symbols{};
    bool found_dynamic_symbols = false;
    for (std::size_t index = 0; index < section_count; ++index) {
        const Elf64_Shdr section =
            read_object<Elf64_Shdr>(file, section_table + index * sizeof(Elf64_Shdr));
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

    bool found_demo_symbol = false;
    for (std::size_t offset = 0; offset < dynamic_symbols.sh_size;
         offset += sizeof(Elf64_Sym)) {
        const Elf64_Sym symbol =
            read_object<Elf64_Sym>(file, dynamic_symbols.sh_offset + offset);
        if (!is_exported(symbol) || symbol.st_name >= string_table.sh_size) {
            continue;
        }

        const std::string name = read_string(
            file, string_table.sh_offset + symbol.st_name, string_table.sh_size - symbol.st_name);
        if (name.compare(0, kArrowMangledPrefix.size(), kArrowMangledPrefix) == 0) {
            fail("shared library exports an Arrow C++ symbol: " + name);
        }
        if (name.compare(0, kDemoExportedPrefix.size(), kDemoExportedPrefix) == 0) {
            found_demo_symbol = true;
        }
    }

    if (!found_demo_symbol) {
        fail("shared library does not export the demo API");
    }
}

std::string find_library_path(void* exported_symbol) {
    Dl_info library{};
    if (dladdr(exported_symbol, &library) == 0 || library.dli_fname == nullptr) {
        fail("cannot locate the shared library");
    }
    return library.dli_fname;
}

std::shared_ptr<arrow::RecordBatch> import_record_batch(ArrowArray* array, ArrowSchema* schema) {
    auto maybe_batch = arrow::ImportRecordBatch(array, schema);
    if (!maybe_batch.ok()) {
        fail(maybe_batch.status().ToString());
    }
    return *std::move(maybe_batch);
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        fail("expected the shared library path as argv[1]");
    }

    void* handle = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (handle == nullptr) {
        fail(dlerror());
    }

    auto export_batch = reinterpret_cast<export_fn_t>(
        dlsym(handle, "udf_runner_cpp_v2_demo_export_record_batch"));
    auto consume_batch = reinterpret_cast<consume_fn_t>(
        dlsym(handle, "udf_runner_cpp_v2_demo_consume_record_batch"));
    auto last_error = reinterpret_cast<error_fn_t>(
        dlsym(handle, "udf_runner_cpp_v2_demo_last_error"));
    if (export_batch == nullptr || consume_batch == nullptr || last_error == nullptr) {
        fail(dlerror());
    }

    const std::string library_path = find_library_path(reinterpret_cast<void*>(export_batch));
    verify_symbols(library_path);

    ArrowArray array{};
    ArrowSchema schema{};
    if (export_batch(&array, &schema) != 0) {
        fail(last_error());
    }

    const std::shared_ptr<arrow::RecordBatch> batch = import_record_batch(&array, &schema);
    assert(batch->num_rows() == 4);
    assert(batch->num_columns() == 2);
    assert(batch->schema()->field(0)->name() == "id");
    assert(batch->schema()->field(1)->name() == "name");

    auto ids = std::static_pointer_cast<arrow::Int64Array>(batch->column(0));
    auto names = std::static_pointer_cast<arrow::StringArray>(batch->column(1));
    assert(ids->Value(0) == 1);
    assert(ids->Value(3) == 4);
    assert(names->GetString(0) == "alpha");
    assert(names->GetString(3) == "delta");

    ArrowArray second_array{};
    ArrowSchema second_schema{};
    if (export_batch(&second_array, &second_schema) != 0) {
        fail(last_error());
    }

    int64_t row_count = 0;
    int64_t id_sum = 0;
    if (consume_batch(&second_array, &second_schema, &row_count, &id_sum) != 0) {
        fail(last_error());
    }
    assert(row_count == 4);
    assert(id_sum == 10);

    return 0;
}
