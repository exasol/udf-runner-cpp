#include <cassert>
#include <cstdint>
#include <memory>

#include <arrow/array/builder_primitive.h>

int main() {
    arrow::Int64Builder builder;
    assert(builder.Append(int64_t{42}).ok());

    std::shared_ptr<arrow::Array> array;
    assert(builder.Finish(&array).ok());
    assert(array->length() == 1);
}
