#include <exasol/udf/v2/interface/call.hpp>

#include <type_traits>

namespace exasol::udf::v2::interface
{

static_assert(std::is_abstract_v<Call>);

} // namespace exasol::udf::v2::interface
