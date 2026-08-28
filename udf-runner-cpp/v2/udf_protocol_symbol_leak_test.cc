#include <cassert>
#include <cstdio>
#include <stdexcept>
#include <string>

namespace {

[[noreturn]] void fail(const std::string& message) {
    throw std::runtime_error(message);
}

void verify_symbols(const std::string& library_path) {
    const std::string command = "nm -D --defined-only -- '" + library_path + "'";
    FILE* pipe = popen(command.c_str(), "r");
    if (pipe == nullptr) {
        fail("cannot inspect protocol library symbols");
    }

    char line[4096];
    while (std::fgets(line, sizeof(line), pipe) != nullptr) {
        const std::string symbol(line);
        const std::size_t name_start = symbol.find_last_of(' ');
        if (name_start != std::string::npos &&
            symbol.compare(name_start + 1, 16, "_ZN11flatbuffers") == 0) {
            pclose(pipe);
            fail("protocol library exports a global flatbuffers symbol: " + symbol);
        }
    }

    if (pclose(pipe) != 0) {
        fail("cannot complete protocol library symbol inspection");
    }
}

}  // namespace

int main(int argc, char** argv) {
    assert(argc == 2);
    verify_symbols(argv[1]);
}
