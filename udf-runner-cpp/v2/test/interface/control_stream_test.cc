#include <exasol/udf/v2/interface/control_stream.hpp>

#include <type_traits>

namespace exasol::udf::v2::interface
{

static_assert(std::is_abstract_v<ControlStream>);

} // namespace exasol::udf::v2::interface
