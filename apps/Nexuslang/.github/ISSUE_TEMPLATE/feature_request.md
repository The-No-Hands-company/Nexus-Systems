---
name: Feature request
about: Propose a new language feature, stdlib addition, or tooling improvement
title: "[Feature] "
labels: enhancement
assignees: ''
---

## Summary

One-paragraph description of the proposed feature.

## Motivation

Why is this needed? What problem does it solve? Which programming domains benefit?
Link to any relevant discussions, papers, or prior art.

## Proposed syntax / API

Show how the feature would look to a user of NexusLang:

```nlpl
# Example of the proposed syntax or API
```

## Semantics

Describe the runtime / compile-time behavior precisely:
- What does the interpreter / compiler do?
- How does this interact with the type system?
- Edge cases and error conditions?

## Implementation sketch (optional)

If you have ideas about which modules need to change (`parser/lexer.py`, `parser/ast.py`,
`interpreter/interpreter.py`, `typesystem/`, `stdlib/`, etc.) describe them here.

## Alternatives considered

What other designs were considered and why were they rejected?

## Checklist

- [ ] The proposed syntax is consistent with existing NexusLang style
- [ ] I have searched existing issues for duplicates
- [ ] This is not domain-specific — NexusLang is a universal language
