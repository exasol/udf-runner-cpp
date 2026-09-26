#include <cassert>
#include <cstdio>
#include <sstream>
#include <stdexcept>
#include <string>

#include "nm_runner.hpp"

namespace
{

[[noreturn]] void fail(const std::string& message)
{
    throw std::runtime_error(message);
}

void verify_symbols(const std::string& library_path)
{
    const std::string command = exasol::udf::v2::test::run_nm("-D", library_path);
    std::istringstream lines(command);
    std::string symbol;
    while (std::getline(lines, symbol))
    {
        const std::size_t name_start = symbol.find_last_of(' ');
        if (name_start != std::string::npos &&
            symbol.compare(name_start + 1, 16, "_ZN11flatbuffers") == 0)
        {
            fail("protocol library exports a global flatbuffers symbol: " + symbol);
        }
    }
}

} // namespace

int main(int argc, char** argv)
{
    try
    {
        assert(argc == 2);
        verify_symbols(argv[1]);
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
