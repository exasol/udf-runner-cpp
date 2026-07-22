#ifndef TEST_CONTAINER_H
#define TEST_CONTAINER_H

#include "exaudflib/swig/swig_meta_data.h"
#include "exaudflib/swig/swig_result_handler.h"
#include "exaudflib/swig/swig_table_iterator.h"
#include "exaudflib/vm/swig_vm.h"

class TestVM : public SWIGVMContainers::SWIGVM {
public:
    explicit TestVM(bool checkOnly);
    void shutdown() override;
    bool run() override;
    const char* singleCall(
        SWIGVMContainers::single_call_function_id_e fn,
        const ExecutionGraph::ScriptDTO& args) override;
    bool useZmqSocketLocks() override { return true; }

private:
    SWIGVMContainers::SWIGMetadata meta;
    SWIGVMContainers::SWIGTableIterator input;
    SWIGVMContainers::SWIGResultHandler output;

    void emitMetadata();
    void forwardInput();
};

#endif
