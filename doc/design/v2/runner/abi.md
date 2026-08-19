# Runner v2 ABI Design

## C ABI principles

Related diagram:

- [abi_boundary.svg](abi_boundary.svg)

The C ABI is the stable boundary for callers written in C or other languages. It must not expose C++ classes,
templates, exceptions, standard-library containers, RTTI-dependent types, or Arrow C++ types.

The ABI uses:

- opaque forward-declared handles for runner, acceptor, socket, worker factory, context manager, worker, and context;
- callback/vtable structs for injected behavior and implementation dependencies;
- explicit status values for success, invalid arguments, closed peer, cancelled operation, resource exhaustion, and
  implementation failure;
- an error inspection function returning a thread-local, null-terminated diagnostic string;
- explicit destroy/release operations for every owning handle;
- fixed-width integer types and `size_t` only where buffer length is paired with a pointer;
- `ArrowArray` and `ArrowSchema` for record-batch exchange, following the Arrow C Data Interface release callbacks.

Illustrative shape:

```c
typedef struct udf_v2_context udf_v2_context;
typedef struct udf_v2_context_manager udf_v2_context_manager;

typedef struct {
    int (*create_context)(void* user_data, int socket_fd,
                          udf_v2_context** out_context);
    void (*destroy)(void* user_data);
    void* user_data;
} udf_v2_context_manager_vtable;

typedef struct {
    int (*create_worker)(void* user_data, int socket_fd,
                         const udf_v2_context_manager_vtable* manager,
                         void** out_worker);
    int (*run_worker)(void* user_data, void* worker);
    void (*destroy_worker)(void* user_data, void* worker);
    void (*destroy)(void* user_data);
    void* user_data;
} udf_v2_worker_factory_vtable;
```

The exact exported names remain subject to the implementation review, but every vtable must document callback
ordering, reentrancy, thread affinity, nullability, and ownership.

## Ownership and descriptor handoff

The C ABI must make descriptor ownership explicit. A successful context-creation callback transfers the descriptor to
the returned context. A failed callback retains responsibility for closing or otherwise reclaiming the descriptor;
the runner must not close it a second time.

Opaque handles are owned by the creator until an explicitly documented transfer. Borrowed handles are valid only for
the duration of the callback that supplies them. Destroy functions must tolerate the documented null/empty state and
must not invoke user callbacks after their parent owner has been destroyed.

## Status and error handling

C callbacks return a stable integer status. They must not throw across the ABI. The runner catches implementation
exceptions and converts them to a failure status with diagnostic text.

The error API is associated with the calling thread and remains valid until the next ABI call on that thread or until
the owning object is destroyed. Callers must copy the text if they need it longer. Successful calls clear the previous
error for that thread.

The ABI must distinguish at least:

- success;
- invalid argument or incompatible vtable;
- operation cancelled or runner shutting down;
- peer closed the connection;
- queue/resource limit reached;
- protocol validation failure;
- internal implementation failure.

## Callback/vtable rules

- Vtables are versioned and include a size/version field so compatible extensions can be detected.
- Required callbacks are validated before the runner starts.
- Optional callbacks have documented defaults and are never called when absent.
- `user_data` belongs to the vtable provider and remains alive until all callbacks and destruction operations finish.
- Callbacks may execute on acceptor or worker-pool threads; the ABI documents this rather than promising a single
  caller thread.
- Reentrant calls are forbidden unless explicitly marked reentrant.
- A callback must not retain borrowed pointers or handles after returning.

## Arrow C Data Interface

Arrow record batches and schemas cross the ABI only through `struct ArrowArray` and `struct ArrowSchema`.

- Producers initialize output structures and provide valid release callbacks.
- Consumers either import the structures or release them according to the Arrow C Data Interface contract.
- A successful transfer moves release ownership to the consumer; a failed transfer leaves the producer responsible.
- No Arrow C++ symbol, `std::shared_ptr`, or Arrow C++ exception crosses the ABI.
- The C++ implementation may use Arrow C++ bridge helpers internally, but the exported symbol surface remains C-only.

## Header-only C++ facade

The C++ facade is implemented entirely in headers and forwards to the C ABI. It provides:

- RAII wrappers for owning opaque handles;
- non-owning views for borrowed handles and Arrow C data;
- typed status/error conversion to C++ exceptions or an equivalent typed error result;
- adapters from C++ worker factories and callables to C callback/vtable structures;
- compile-time checks for supported callback signatures;
- explicit move-only ownership for descriptors, contexts, and workers.

The facade must not require callers to link against private C++ implementation symbols. Its ABI compatibility is the C
ABI's compatibility; changing the facade's inline implementation must not change the C handle layout or vtable rules.

## Compatibility and versioning

The ABI version is negotiated independently from the protocol version. New vtable fields are appended, guarded by the
declared size, and given safe defaults. Existing fields cannot change meaning or ownership. Protocol capability and
message-version negotiation remains the responsibility of `Context` and the v2 protocol layer.
