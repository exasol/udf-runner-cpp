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
            throw std::invalid_argument("usage: udf_runner SOCKET_PATH [SCENARIO.json|--echo] [TIMEOUT_SECONDS]");
        }
        using namespace exasol::udf::simulator;
        if (argc >= 3 && std::string(argv[2]) == "--echo")
        {
            run_echo(argv[1], argc == 4 ? std::stoi(argv[3]) : 10);
            return 0;
        }
        const auto steps = argc >= 3 ? load_steps(argv[2]) : default_steps(Role::Runner);
        run(Role::Runner, argv[1], steps, argc == 4 ? std::stoi(argv[3]) : 10);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
