#include <arrow/api.h>
#include <arrow/c/bridge.h>

#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#if defined(_WIN32)
#define UDF_RUNNER_CPP_V2_EXPORT __declspec(dllexport)
#else
#define UDF_RUNNER_CPP_V2_EXPORT __attribute__((visibility("default")))
#endif

namespace {

thread_local std::string g_last_error;

void SetLastError(const arrow::Status& status) {
    g_last_error = status.ToString();
}

arrow::Result<std::shared_ptr<arrow::RecordBatch>> MakeDemoRecordBatch() {
    arrow::Int64Builder id_builder;
    arrow::StringBuilder name_builder;

    ARROW_RETURN_NOT_OK(id_builder.Append(1));
    ARROW_RETURN_NOT_OK(id_builder.Append(2));
    ARROW_RETURN_NOT_OK(id_builder.Append(3));
    ARROW_RETURN_NOT_OK(id_builder.Append(4));

    ARROW_RETURN_NOT_OK(name_builder.Append("alpha"));
    ARROW_RETURN_NOT_OK(name_builder.Append("beta"));
    ARROW_RETURN_NOT_OK(name_builder.Append("gamma"));
    ARROW_RETURN_NOT_OK(name_builder.Append("delta"));

    std::shared_ptr<arrow::Array> ids;
    std::shared_ptr<arrow::Array> names;
    ARROW_RETURN_NOT_OK(id_builder.Finish(&ids));
    ARROW_RETURN_NOT_OK(name_builder.Finish(&names));
    const int64_t num_rows = ids->length();

    auto schema = arrow::schema({
        arrow::field("id", arrow::int64()),
        arrow::field("name", arrow::utf8()),
    });
    return arrow::RecordBatch::Make(schema, num_rows, {std::move(ids), std::move(names)});
}

arrow::Status ExportDemoRecordBatch(ArrowArray* out_array, ArrowSchema* out_schema) {
    if (out_array == nullptr || out_schema == nullptr) {
        return arrow::Status::Invalid("output ArrowArray and ArrowSchema pointers must not be null");
    }

    std::memset(out_array, 0, sizeof(*out_array));
    std::memset(out_schema, 0, sizeof(*out_schema));

    auto maybe_batch = MakeDemoRecordBatch();
    if (!maybe_batch.ok()) {
        return maybe_batch.status();
    }
    ARROW_RETURN_NOT_OK(arrow::ExportRecordBatch(*maybe_batch.ValueOrDie(), out_array, out_schema));
    return arrow::Status::OK();
}

arrow::Status ConsumeDemoRecordBatch(ArrowArray* array, ArrowSchema* schema,
                                     int64_t* out_row_count, int64_t* out_id_sum) {
    if (array == nullptr || schema == nullptr || out_row_count == nullptr ||
        out_id_sum == nullptr) {
        return arrow::Status::Invalid("input and output pointers must not be null");
    }

    ARROW_ASSIGN_OR_RAISE(auto batch, arrow::ImportRecordBatch(array, schema));
    if (batch->num_columns() != 2) {
        return arrow::Status::Invalid("expected two columns");
    }
    if (batch->schema()->field(0)->name() != "id" ||
        batch->schema()->field(1)->name() != "name") {
        return arrow::Status::Invalid("unexpected schema");
    }

    auto ids = std::static_pointer_cast<arrow::Int64Array>(batch->column(0));
    int64_t sum = 0;
    for (int64_t index = 0; index < ids->length(); ++index) {
        if (!ids->IsNull(index)) {
            sum += ids->Value(index);
        }
    }

    *out_row_count = batch->num_rows();
    *out_id_sum = sum;
    return arrow::Status::OK();
}

}  // namespace

extern "C" UDF_RUNNER_CPP_V2_EXPORT int udf_runner_cpp_v2_demo_export_record_batch(
    ArrowArray* out_array, ArrowSchema* out_schema) {
    const arrow::Status status = ExportDemoRecordBatch(out_array, out_schema);
    if (!status.ok()) {
        SetLastError(status);
        return 1;
    }
    g_last_error.clear();
    return 0;
}

extern "C" UDF_RUNNER_CPP_V2_EXPORT int udf_runner_cpp_v2_demo_consume_record_batch(
    ArrowArray* array, ArrowSchema* schema, int64_t* out_row_count,
    int64_t* out_id_sum) {
    const arrow::Status status =
        ConsumeDemoRecordBatch(array, schema, out_row_count, out_id_sum);
    if (!status.ok()) {
        SetLastError(status);
        return 1;
    }
    g_last_error.clear();
    return 0;
}

extern "C" UDF_RUNNER_CPP_V2_EXPORT const char* udf_runner_cpp_v2_demo_last_error(void) {
    return g_last_error.c_str();
}
