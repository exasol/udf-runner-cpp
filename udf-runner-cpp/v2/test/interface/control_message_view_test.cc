#include <exasol/udf/v2/interface/message_view/control_message_view.hpp>

#include <gtest/gtest.h>

namespace exasol::udf::v2::interface
{

TEST(ControlMessageViewTest, EmptyViewReportsNoFields)
{
    const ControlMessageView view;
    EXPECT_FALSE(view.has_server_capabilities());
    EXPECT_FALSE(view.server_capabilities());
    EXPECT_FALSE(view.has_payloads());
    EXPECT_FALSE(view.payloads());
    EXPECT_FALSE(view.has_error());
    EXPECT_FALSE(view.error());
    EXPECT_FALSE(view.has_keep_alive());
    EXPECT_FALSE(view.has_close_connection());
}

} // namespace exasol::udf::v2::interface
