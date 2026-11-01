#include <exasol/udf/v2/interface/message_view/data_schema_view.hpp>
#include <exasol/udf/v2/internal/data_schema_view_impl.hpp>

#include <gtest/gtest.h>

#include "../common/arrow_test_utils.hpp"

#include <utility>

namespace exasol::udf::v2::internal
{

using interface::ArrowSchemaPtr;

TEST(DataSchemaViewTest, TransfersSchemaAndReleasesItOnce)
{
    int releases         = 0;
    auto* schema         = new ArrowSchema{};
    schema->private_data = &releases;
    schema->release      = test::count_schema_release;
    {
        DataSchemaViewImpl view(ArrowSchemaPtr(schema), true, true);
        EXPECT_TRUE(view.has_group_id());
        EXPECT_TRUE(view.has_row_id());
        auto transferred = std::move(view).take_schema();
        EXPECT_EQ(transferred.get(), schema);
    }
    EXPECT_EQ(releases, 1);
}

TEST(DataSchemaViewTest, SupportsMoveOnlyArrowOwner)
{
    auto* first  = new ArrowSchema{};
    auto* second = new ArrowSchema{};
    ArrowSchemaPtr owner(first);
    ArrowSchemaPtr replacement(second);
    owner = std::move(replacement);
    EXPECT_EQ(owner.get(), second);
}

TEST(DataSchemaViewTest, ReleasesOwnedSchemaWhenDestroyed)
{
    int releases         = 0;
    auto* schema         = new ArrowSchema{};
    schema->private_data = &releases;
    schema->release      = test::count_schema_release;
    {
        ArrowSchemaPtr owner(schema);
        ArrowSchemaPtr moved(std::move(owner));
        EXPECT_EQ(moved.get(), schema);
    }
    EXPECT_EQ(releases, 1);
}

TEST(DataSchemaViewTest, SupportsEmptyOwner)
{
    ArrowSchemaPtr empty_schema;
    EXPECT_EQ(empty_schema.get(), nullptr);
}

} // namespace exasol::udf::v2::internal
