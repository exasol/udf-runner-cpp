#pragma once

// Compatibility umbrella for the public v2 worker-facing interface.
#include <exasol/udf/v2/interface/content_types.hpp>
#include <exasol/udf/v2/interface/context.hpp>
#include <exasol/udf/v2/interface/error_info.hpp>
#include <exasol/udf/v2/interface/message_builder/call_message_builder.hpp>
#include <exasol/udf/v2/interface/message_builder/control_message_builder.hpp>
#include <exasol/udf/v2/interface/message_view/call_message_view.hpp>
#include <exasol/udf/v2/interface/message_view/control_message_view.hpp>
#include <exasol/udf/v2/interface/message_view/data_schema_view.hpp>
#include <exasol/udf/v2/interface/message_view/record_batch_view.hpp>
#include <exasol/udf/v2/interface/result.hpp>
#include <exasol/udf/v2/interface/status.hpp>
#include <exasol/udf/v2/interface/timeout.hpp>
