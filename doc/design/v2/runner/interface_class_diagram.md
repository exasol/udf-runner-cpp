# v2 Interface and Implementation Diagrams

This document separates the worker-facing interface from the internal implementation and serialization
relationships.

## Abstract worker-facing interface

The class diagram describes only the public abstract interface in `exasol::udf::v2::interface`. It intentionally does
not show concrete implementations, decoded frames, generated FlatBuffer types, or other serialization details.

- [Mermaid source](interface_class_diagram.mmd)
- [Rendered SVG](interface_class_diagram.svg)

The interface includes the context, call and control-stream contracts, message builders, message views, and typed child
view contracts. Relationships describe only how public interfaces create, receive, or expose other public interfaces.

## Interface, implementation, and FlatBuffer relationships

This overview shows the high-level bridge from abstract interfaces to internal implementations and the verified
generated FlatBuffer representation. It names representative components but intentionally omits members, methods,
fields, and individual factory functions.

- [Mermaid source](interface_implementation_relationships.mmd)
- [Rendered SVG](interface_implementation_relationships.svg)

The abstract interface is independent of the generated protocol objects. Internal view implementations retain the
verified frame while exposing serialization-independent typed views to callers.

Legend for the relationship overview:

- Solid arrows show high-level interface or data-flow relationships.
- Dashed arrows show implementation relationships.
- The relationship diagram is architectural and does not prescribe concrete class layout beyond the named
  representative components.
