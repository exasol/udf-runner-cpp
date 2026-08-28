#include <cassert>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

[[noreturn]] void fail(const std::string& message) {
    throw std::runtime_error(message);
}

void verify_symbols(const std::string& archive_path) {
    const std::string command = "nm -g --defined-only -- '" + archive_path + "'";
    FILE* pipe = popen(command.c_str(), "r");
    if (pipe == nullptr) {
        fail("cannot inspect protocol archive symbols");
    }

    char line[4096];
    while (std::fgets(line, sizeof(line), pipe) != nullptr) {
        const std::string symbol(line);
        const std::size_t name_start = symbol.find_last_of(' ');
        if (name_start != std::string::npos &&
            symbol.compare(name_start + 1, 16, "_ZN11flatbuffers") == 0) {
            pclose(pipe);
            fail("protocol archive exports a global flatbuffers symbol: " + symbol);
        }
    }

    if (pclose(pipe) != 0) {
        fail("cannot complete protocol archive symbol inspection");
    }
}

}  // namespace

int main(int argc, char** argv) {
    assert(argc > 1);
    bool found_archive = false;
    for (int index = 1; index < argc; ++index) {
        const std::string_view path(argv[index]);
        if (path.ends_with(".a")) {
            verify_symbols(std::string(path));
            found_archive = true;
        }
    }
    assert(found_archive);
}
