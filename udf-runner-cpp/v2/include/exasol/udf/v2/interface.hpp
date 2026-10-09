#pragma once

// Compatibility umbrella for the public v2 worker-facing interface.
#include <exasol/udf/v2/context.hpp>
#include <exasol/udf/v2/message_builder/call_message_builder.hpp>
#include <exasol/udf/v2/message_builder/control_message_builder.hpp>
#include <exasol/udf/v2/message_view/call_message_view.hpp>
#include <exasol/udf/v2/message_view/control_message_view.hpp>
#include <exasol/udf/v2/message_view/data_schema_view.hpp>
#include <exasol/udf/v2/message_view/record_batch_view.hpp>
#include <exasol/udf/v2/types.hpp>
