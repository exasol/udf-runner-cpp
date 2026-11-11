#include "../common/protocol_test_utils.hpp"
#include <exasol/udf/v2/internal/control_message_view_impl.hpp>

#include <gtest/gtest.h>

#include <type_traits>

namespace exasol::udf::v2::internal
{

static_assert(std::is_abstract_v<interface::ControlMessageView>);
static_assert(std::is_abstract_v<interface::ServerCapabilitiesView>);
static_assert(std::is_abstract_v<interface::VersionView>);

TEST(ControlMessageViewTest, FrameWithoutOptionalFieldsStillHasStreamId)
{
    const ControlMessageViewImpl view(test::make_empty_decoded_frame(0));
    EXPECT_EQ(view.stream_id(), 0);
    EXPECT_FALSE(view.server_capabilities());
    EXPECT_FALSE(view.payloads());
    EXPECT_FALSE(view.error());
    EXPECT_FALSE(view.keep_alive());
    EXPECT_FALSE(view.close_connection());
}

TEST(ControlMessageViewTest, RejectsNullDecodedFrame)
{
    EXPECT_DEATH(
        {
            const ControlMessageViewImpl view(nullptr);
            static_cast<void>(view);
        },
        "");
}

} // namespace exasol::udf::v2::internal
