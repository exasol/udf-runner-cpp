#include <exasol/udf/v2/interface/message_view/record_batch_view.hpp>

#include <gtest/gtest.h>

#include "../common/arrow_test_utils.hpp"

namespace exasol::udf::v2
{

TEST(RecordBatchViewTest, TransfersArrayAndReleasesItOnce)
{
    int releases = 0;
    ArrowArray array{};
    array.private_data = &releases;
    array.release      = test::count_array_release;
    {
        RecordBatchView view(RecordBatch{ArrowArrayPtr(&array), true});
        EXPECT_TRUE(view.is_end_of_group());
        auto transferred = std::move(view).take_array();
        EXPECT_EQ(transferred.get(), &array);
    }
    EXPECT_EQ(releases, 1);
}

TEST(RecordBatchViewTest, SupportsMoveOnlyArrowOwner)
{
    ArrowArray first{};
    ArrowArray second{};
    ArrowArrayPtr owner(&first);
    ArrowArrayPtr replacement(&second);
    owner = std::move(replacement);
    EXPECT_EQ(owner.get(), &second);
}

} // namespace exasol::udf::v2
