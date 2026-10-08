#include "common/simulator.hpp"

#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>

int main(int argc, char** argv)
{
    try
    {
        if (argc < 2 || argc > 4)
        {
            throw std::invalid_argument("usage: client SOCKET_PATH [SCENARIO.json] [TIMEOUT_SECONDS]");
        }
        using namespace exasol::udf::simulator;
        const auto steps = argc >= 3 ? load_steps(argv[2]) : default_steps(Role::Client);
        run(Role::Client, argv[1], steps, argc == 4 ? std::stoi(argv[3]) : 10);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
