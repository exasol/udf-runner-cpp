#include <functional>
#include <iostream>
#include "exa_vm_factory.h"

#ifdef ENABLE_STREAMING_VM
#include "streaming_container/streamingcontainer.h"
#endif

#ifdef ENABLE_BENCHMARK_VM
#include "benchmark_container/benchmark_container.h"
#endif

std::function<SWIGVMContainers::SWIGVM*()> create_vm(const std::string& argv_lang, bool use_ctpg_options_parser) {
    if(argv_lang.compare("lang=streaming") == 0) {
        #ifdef ENABLE_STREAMING_VM
            return []() { return new SWIGVMContainers::StreamingVM(false); };
        #else
            throw SWIGVMContainers::SWIGVM::exception("this exaudfclient has been compilied without Streaming support");
        #endif
    }
    else if(argv_lang.compare("lang=benchmark") == 0) {
        #ifdef ENABLE_BENCHMARK_VM
            return []() { return new SWIGVMContainers::BenchmarkVM(false); };
        #else
            throw SWIGVMContainers::SWIGVM::exception("this exaudfclient has been compilied without Benchmark support");
        #endif
    }
    else {
        throw SWIGVMContainers::SWIGVM::exception("unsupported language specified in argv");
    }   
}
