#include "mutation_smoke.hpp"

#include <cassert>

int main()
{
    assert(exasol::udf::v2::mutation_smoke::transform(-7) == 7);
    assert(exasol::udf::v2::mutation_smoke::transform(0) == 1);
    assert(exasol::udf::v2::mutation_smoke::transform(7) == 8);
}
