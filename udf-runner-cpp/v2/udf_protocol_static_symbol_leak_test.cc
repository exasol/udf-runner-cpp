#include <cassert>
#include <cstdio>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

#include "nm_runner.hpp"

namespace {

[[noreturn]] void fail(const std::string& message) {
    throw std::runtime_error(message);
}

void verify_symbols(const std::string& archive_path) {
    const std::string command = exasol::udf::v2::test::run_nm("-g", archive_path);
    std::istringstream lines(command);
    std::string symbol;
    while (std::getline(lines, symbol)) {
        const std::size_t name_start = symbol.find_last_of(' ');
        if (name_start != std::string::npos &&
            symbol.compare(name_start + 1, 16, "_ZN11flatbuffers") == 0) {
            fail("protocol archive exports a global flatbuffers symbol: " + symbol);
        }
    }
}

} // namespace

int main(int argc, char** argv) {
    try {
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
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
