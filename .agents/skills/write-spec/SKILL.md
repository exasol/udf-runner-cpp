---
name: write-spec
description: Write or revise design specifications in this repository, including behavior, interactions, diagrams, and focused C++ sketches.
---

# Write a design spec

Inspect the relevant design documents and implementation boundaries before writing. Use the established level of detail and terminology, but give the new spec its own structure and explain its design directly. Choose sections that help the reader understand purpose, participants, behavior, ownership, and failure handling; omit sections that do not apply.

Organize the design documentation from a high-level architecture overview to detailed component specs. Explain each design decision in one spec; when another spec needs that information, link to its authoritative section instead of repeating the explanation.

Describe the intended implementation as an existing system. Write the spec in declarative present tense: "The context owns the connection" rather than "The context will own the connection." State decisions and observable behavior, not implementation tasks. Do not include TODO lists, task lists, roadmaps, or instructions to future implementers in the spec.

Use concrete names for participants and example values whenever they clarify a relationship or order. Name components, classes, calls, messages, and batches consistently across prose, diagrams, and code. For example, explain what happens when record batch A arrives before record batch B instead of referring to "the earlier batch" without identifying it.

Include a visual when it makes behavior easier to follow. Use a sequence diagram for interactions, message order, or handoffs; a state diagram for lifecycle transitions; and a component or data-flow diagram for structure or ownership. Keep diagrams focused and label their participants and events. Use Mermaid; when adding a standalone diagram, keep its `.mmd` source and matching rendered `.svg` beside the spec.

Include simplified C++ code for the most important parts of the design. Show the key interface, ownership shape, or control flow that makes the contract concrete. Keep snippets short and illustrative rather than writing a full implementation. Use the same names, types, and behavior in the snippets, prose, and diagrams, and explain any simplification that could otherwise mislead the reader.

Read the completed spec as a standalone description: a reader can identify who does what, in which order, what each participant owns, and what happens on relevant errors or shutdown paths without reconstructing the design from unrelated documents.
