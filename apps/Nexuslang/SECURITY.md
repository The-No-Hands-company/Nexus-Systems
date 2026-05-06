# Security Policy

## Supported Versions

| Version | Supported          |
| ------- | ------------------ |
| 0.x     | Yes (current dev)  |

## Reporting a Vulnerability

If you discover a security vulnerability in NexusLang, please report it responsibly.

**Do NOT open a public issue for security vulnerabilities.**

Instead, please email: **erichakansson84@gmail.com**

Include the following in your report:

- Description of the vulnerability
- Steps to reproduce
- Potential impact
- Suggested fix (if any)

### Response Timeline

- **Acknowledgement**: Within 48 hours
- **Initial assessment**: Within 7 days
- **Fix or mitigation**: Within 30 days for critical issues

### Disclosure Policy

- We will coordinate disclosure timing with the reporter
- We will credit reporters in the security advisory (unless they prefer anonymity)
- We aim to release a fix before or simultaneously with public disclosure

## Security Considerations for NexusLang Programs

NLPL is a general-purpose programming language. When writing NexusLang programs that
handle untrusted input, follow these security practices:

### Input Validation
- Validate all external input at system boundaries
- Use the type system to enforce data constraints

### Memory Safety
- NLPL's borrow checker prevents use-after-move and dangling references
- Smart pointers (Rc, Arc) manage reference-counted memory automatically
- The runtime enforces borrow rules at execution time

### FFI Safety
- Foreign function calls (`load library`, `extern function`) bypass NLPL's safety guarantees
- Validate all data crossing the FFI boundary
- Prefer NLPL-native implementations where possible

### Cryptography
- Use the `crypto` stdlib module for cryptographic operations
- Do not implement custom cryptographic algorithms in application code

## Scope

This policy covers:
- The NexusLang interpreter and compiler (`src/nlpl/`)
- The standard library (`src/nlpl/stdlib/`)
- The build system and CI infrastructure
- Official documentation and examples

Out of scope:
- Third-party libraries or tools not maintained by the NexusLang project
- Vulnerabilities in user-written NexusLang programs
