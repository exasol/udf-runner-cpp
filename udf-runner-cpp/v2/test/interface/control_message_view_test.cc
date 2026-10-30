#include <exasol/udf/v2/internal/control_message_view_impl.hpp>

#include <gtest/gtest.h>

#include <type_traits>

namespace exasol::udf::v2::internal
{

static_assert(std::is_abstract_v<interface::ControlMessageView>);

TEST(ControlMessageViewTest, EmptyViewReportsNoFields)
{
    const ControlMessageViewImpl view(nullptr);
    EXPECT_FALSE(view.has_server_capabilities());
    EXPECT_FALSE(view.server_capabilities());
    EXPECT_FALSE(view.has_payloads());
    EXPECT_FALSE(view.payloads());
    EXPECT_FALSE(view.has_error());
    EXPECT_FALSE(view.error());
    EXPECT_FALSE(view.has_keep_alive());
    EXPECT_FALSE(view.has_close_connection());
}

} // namespace exasol::udf::v2::internal
