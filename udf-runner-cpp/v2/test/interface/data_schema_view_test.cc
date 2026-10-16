#include <exasol/udf/v2/interface/message_view/data_schema_view.hpp>

#include <gtest/gtest.h>

#include "../common/arrow_test_utils.hpp"

namespace exasol::udf::v2
{

TEST(DataSchemaViewTest, TransfersSchemaAndReleasesItOnce)
{
    int releases = 0;
    ArrowSchema schema{};
    schema.private_data = &releases;
    schema.release      = test::count_schema_release;
    {
        DataSchemaView view(DataSchema{ArrowSchemaPtr(&schema), true, true});
        EXPECT_TRUE(view.has_group_id());
        EXPECT_TRUE(view.has_row_id());
        auto transferred = std::move(view).take_schema();
        EXPECT_EQ(transferred.get(), &schema);
    }
    EXPECT_EQ(releases, 1);
}

TEST(DataSchemaViewTest, SupportsMoveOnlyArrowOwner)
{
    ArrowSchema first{};
    ArrowSchema second{};
    ArrowSchemaPtr owner(&first);
    ArrowSchemaPtr replacement(&second);
    owner = std::move(replacement);
    EXPECT_EQ(owner.get(), &second);
}

} // namespace exasol::udf::v2
