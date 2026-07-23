#include "test_container.h"

#include <cstring>
#include <string>

using namespace SWIGVMContainers;

namespace {

constexpr const char* kEmitMetadata = "emit_metadata";
constexpr const char* kForwardInput = "forward_input";

void requireStringColumn(
    SWIGMetadata& metadata,
    unsigned int column,
    bool input,
    const char* strategy) {
    const SWIGVM_datatype_e type = input
        ? metadata.inputColumnType(column)
        : metadata.outputColumnType(column);
    if (type != STRING) {
        throw SWIGVM::exception(
            (std::string(strategy) + " requires VARCHAR "
                + (input ? "input" : "output") + " columns").c_str());
    }
}

}  // namespace

TestVM::TestVM(bool checkOnly)
    : meta(), input(), output(&input) {
    if (meta.inputType() != MULTIPLE || meta.outputType() != MULTIPLE) {
        throw SWIGVM::exception("TEST language container only supports SET-EMITS UDFs");
    }
}

void TestVM::shutdown() {}

bool TestVM::run() {
    const std::string strategy(meta.scriptCode());
    if (strategy == kEmitMetadata) {
        emitMetadata();
    } else if (strategy == kForwardInput) {
        forwardInput();
    } else {
        throw SWIGVM::exception(
            ("unsupported test strategy: " + strategy).c_str());
    }
    return true;
}

void TestVM::emitMetadata() {
    if (meta.outputColumnCount() != 2) {
        throw SWIGVM::exception("emit_metadata requires exactly two VARCHAR output columns");
    }
    requireStringColumn(meta, 0, false, kEmitMetadata);
    requireStringColumn(meta, 1, false, kEmitMetadata);

    const char* script_user = meta.scopeUser();
    const char* script_code = meta.scriptCode();
    output.setString(0, script_user, std::strlen(script_user));
    output.setString(1, script_code, std::strlen(script_code));
    output.next();
    output.flush();
}

void TestVM::forwardInput() {
    if (meta.inputColumnCount() != 1 || meta.outputColumnCount() != 1) {
        throw SWIGVM::exception("forward_input requires exactly one VARCHAR input and output column");
    }
    requireStringColumn(meta, 0, true, kForwardInput);
    requireStringColumn(meta, 0, false, kForwardInput);

    while (!input.eot()) {
        size_t length = 0;
        const char* value = input.getString(0, &length);
        if (input.wasNull()) {
            output.setNull(0);
        } else {
            output.setString(0, value, length);
        }
        output.next();
        input.next();
    }
    output.flush();
}

const char* TestVM::singleCall(
    single_call_function_id_e fn,
    const ExecutionGraph::ScriptDTO& args) {
    throw SWIGVM::exception("singleCall is not supported for the TEST language container");
}
