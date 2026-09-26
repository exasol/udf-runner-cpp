#include "mutation_smoke.hpp"

namespace exasol::udf::v2::mutation_smoke
{

int transform(int value)
{
    if (value < 0)
    {
        return -value;
    }
    return value + 1;
}

} // namespace exasol::udf::v2::mutation_smoke
