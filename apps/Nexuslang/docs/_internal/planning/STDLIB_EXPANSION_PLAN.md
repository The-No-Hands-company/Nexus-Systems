# Standard Library Expansion Plan

**Date:** February 16, 2026  
**Priority:** 🔴 CRITICAL (Next phase after debugger completion)  
**Goal:** Expand NexusLang stdlib from 62 to 70+ modules with deep, production-ready implementations  
**Timeline:** 4-6 months  
**Current Status:** Foundation exists, needs deepening and expansion

---

## Executive Summary

NLPL has **62 stdlib modules** with basic implementations. The next critical phase is **deepening existing modules** and **adding missing critical functionality** to support real-world applications across all domains (business, data, scientific, web, systems).

**Key Finding:** We already have foundational modules for crypto, HTTP, databases, and async. The work is to:
1. **Deepen** existing modules (add missing features)
2. **Test** thoroughly (unit + integration tests)
3. **Document** comprehensively (API docs + examples)
4. **Validate** with real-world use cases

---

## Current State Assessment

### Existing Critical Modules (Current Status)

#### 1. **Cryptography (`crypto/`)** - ⚠️ 70% COMPLETE (177 lines)

**What We Have:**
- ✅ Hashing: MD5, SHA-1, SHA-256, SHA-512, SHA3-256, SHA3-512, BLAKE2b, BLAKE2s
- ✅ HMAC: HMAC-SHA256, HMAC-SHA512
- ✅ Base64 encoding/decoding (standard + URL-safe)
- ✅ Cryptographically secure random (bytes, hex, tokens)
- ✅ Constant-time comparison (timing attack prevention)

**What's Missing:**
- ❌ Symmetric encryption (AES-128/192/256, ChaCha20)
- ❌ Asymmetric crypto (RSA, Ed25519, ECDSA)
- ❌ Key derivation (PBKDF2, Argon2, scrypt)
- ❌ Digital signatures
- ❌ Certificate handling (X.509)
- ❌ TLS/SSL support (or integration with OpenSSL via FFI)

**Recommendation:** Add encryption and asymmetric crypto via `cryptography` library (Python) or OpenSSL FFI.

---

#### 2. **HTTP Client (`http/`)** - ⚠️ 60% COMPLETE (210 lines)

**What We Have:**
- ✅ HTTP methods: GET, POST, PUT, DELETE, PATCH, HEAD
- ✅ Request headers
- ✅ JSON request/response handling
- ✅ Timeout support
- ✅ HTTPResponse wrapper (status, headers, body, text, json, ok)
- ✅ Error handling (HTTPError, URLError)

**What's Missing:**
- ❌ HTTP server framework (Flask-like routing)
- ❌ Session management (cookies, persistent sessions)
- ❌ Connection pooling (reuse connections)
- ❌ Async HTTP client (non-blocking I/O)
- ❌ WebSocket client (beyond basic utils)
- ❌ HTTP/2 support
- ❌ Streaming responses
- ❌ File upload/download helpers
- ❌ Authentication helpers (OAuth, JWT, Basic Auth)
- ❌ Retry logic with exponential backoff

**Recommendation:** Add HTTP server framework and async client as priorities.

---

#### 3. **Database Connectors (`databases/`)** - ⚠️ 75% COMPLETE (549 lines)

**What We Have:**
- ✅ PostgreSQL connector (psycopg2) with connection pooling
- ✅ MySQL connector (mysql-connector-python) with pooling
- ✅ MongoDB connector (pymongo)
- ✅ Query execution (parameterized queries)
- ✅ Transaction support (commit, rollback)
- ✅ Result fetching (fetchone, fetchall, iterate)
- ✅ Connection management (connect, close, disconnect)

**What's Missing:**
- ❌ SQLite integration (exists in `sqlite/` but not unified)
- ❌ Generic database abstraction layer (database-agnostic API)
- ❌ ORM-like query builder (SELECT * FROM WHERE)
- ❌ Migration tools (schema versioning)
- ❌ Async database clients (asyncpg, aiomysql)
- ❌ Connection health checks and reconnection
- ❌ Query logging and profiling
- ❌ Prepared statement caching

**Recommendation:** Unify SQLite, add query builder, and async support.

---

#### 4. **Async I/O (`asyncio_utils/`)** - ❌ 5% COMPLETE (13 lines + promise.py)

**What We Have:**
- ⚠️ Promise implementation (promise.py)
- ⚠️ Basic async function registration

**What's Missing:**
- ❌ Complete async/await runtime (event loop, executor)
- ❌ Async file operations (async read, write, open, close)
- ❌ Async network operations (TCP, UDP sockets)
- ❌ Async timers and delays
- ❌ Async channels (for inter-task communication)
- ❌ Async locks and synchronization primitives
- ❌ Task spawning and cancellation
- ❌ Future/Promise composition (then, map, all, race)
- ❌ Async context managers

**Recommendation:** This requires completing the async/await runtime in the interpreter first (see Part 3.3 in MISSING_FEATURES_ROADMAP.md). Defer until runtime is ready, or implement basic async patterns with threading.

---

### Other High-Value Modules to Add

#### 5. **Compression (`compression/`)** - EXISTS (check implementation)

**Directory exists**, need to assess current capabilities:
- Need: gzip, zlib, bz2, lzma, zstd
- Need: Compress/decompress functions
- Need: File compression utilities

#### 6. **Logging (`logging_utils/`)** - EXISTS (check implementation)

**Directory exists**, need to assess:
- Need: Log levels (DEBUG, INFO, WARN, ERROR, CRITICAL)
- Need: Multiple handlers (console, file, syslog)
- Need: Log formatting and rotation
- Need: Structured logging (JSON logs)

#### 7. **Email (`email_utils/`)** - EXISTS (check implementation)

**Directory exists**, need to assess:
- Need: SMTP client (send emails)
- Need: IMAP/POP3 client (receive emails)
- Need: Email parsing (MIME, multipart)
- Need: Attachment handling

#### 8. **Template Engine (`templates/`)** - EXISTS (check implementation)

**Directory exists**, need to assess:
- Need: Jinja2-like templating
- Need: Variable substitution
- Need: Control flow (if, for, include)
- Need: Filters and functions

#### 9. **UUID (`uuid_utils/`)** - EXISTS (check implementation)

**Directory exists**, likely complete (UUID generation is straightforward)

#### 10. **Subprocess (`subprocess_utils/`)** - EXISTS (check implementation)

**Directory exists**, need to assess:
- Need: Run external commands
- Need: Pipe handling (stdin, stdout, stderr)
- Need: Process control (wait, kill, terminate)
- Need: Async subprocess support

---

## Implementation Strategy

### Phase 1: Deepen Existing Critical Modules (Months 1-2)

**Priority Order:**

1. **Crypto Module** (2 weeks)
   - Add AES encryption via `cryptography` library
   - Add RSA/Ed25519 via `cryptography` library
   - Add key derivation (PBKDF2, Argon2)
   - Add digital signatures
   - Test suite: 50+ test cases
   - Example: Encrypt/decrypt file, sign/verify data

2. **HTTP Module** (3 weeks)
   - Add HTTP server framework (routing, middleware)
   - Add session management
   - Add connection pooling
   - Add authentication helpers (JWT, OAuth)
   - Test suite: 60+ test cases
   - Example: REST API server, OAuth client

3. **Database Module** (2 weeks)
   - Unify SQLite connector
   - Add query builder (SELECT/INSERT/UPDATE/DELETE DSL)
   - Add connection health checks
   - Add query profiling
   - Test suite: 70+ test cases
   - Example: CRUD application, query builder demo

4. **Compression Module** (1 week)
   - Implement/verify gzip, zlib, bz2, lzma
   - Add file compression utilities
   - Test suite: 30+ test cases
   - Example: Compress/decompress files

**Deliverable:** 4 modules deepened, 210+ test cases, 4 example programs

---

### Phase 2: Add Missing High-Value Modules (Months 3-4)

**Priority Order:**

1. **Async I/O Foundation** (3 weeks)
   - **Option A:** Simple async patterns with threading (interim solution)
   - **Option B:** Complete async/await runtime (requires interpreter work)
   - Recommendation: Option A for now, Option B as Phase 3
   - Add async file I/O (read, write, open, close)
   - Add async TCP client
   - Add async timers
   - Test suite: 40+ test cases
   - Example: Async HTTP requests, async file processing

2. **Logging Module** (1 week)
   - Multiple log levels and handlers
   - Log formatting and rotation
   - Structured logging (JSON)
   - Test suite: 25+ test cases
   - Example: Application logging, structured logs

3. **Email Module** (1 week)
   - SMTP client (send emails)
   - Email parsing (MIME)
   - Attachment handling
   - Test suite: 20+ test cases
   - Example: Send HTML email with attachments

4. **Template Engine** (2 weeks)
   - Variable substitution
   - Control flow (if, for)
   - Filters and functions
   - Template inheritance
   - Test suite: 35+ test cases
   - Example: HTML template rendering, markdown processing

**Deliverable:** 4 new modules, 120+ test cases, 4 example programs

---

### Phase 3: Polish and Validate (Months 5-6)

**Activities:**

1. **Documentation Sprint** (2 weeks)
   - API reference for all 70+ modules
   - Usage guides with examples
   - Cookbook recipes (common patterns)
   - Performance benchmarks

2. **Real-World Validation** (3 weeks)
   - Build 3 showcase applications:
     - **Web Application:** REST API with database (HTTP + DB)
     - **Data Pipeline:** ETL with compression and crypto
     - **Business App:** Invoice generator with PDF and email
   - Identify gaps and fix

3. **Testing and Security Audit** (3 weeks)
   - Achieve 90%+ test coverage for new modules
   - Fuzz test parsers and network code
   - Security review of crypto and auth code
   - Performance profiling

4. **Community Feedback** (ongoing)
   - Release beta stdlib to early adopters
   - Gather feedback on API design
   - Iterate based on usage patterns

**Deliverable:** Production-ready stdlib, 3 showcase apps, 90%+ coverage

---

## Module Priorities Matrix

### 🔴 CRITICAL (Month 1-2)

| Module | Current | Target | Effort | Impact |
|--------|---------|--------|--------|--------|
| Crypto | 70% | 95% | 2 weeks | HIGH (security, encryption) |
| HTTP | 60% | 90% | 3 weeks | HIGH (web services) |
| Database | 75% | 95% | 2 weeks | HIGH (data persistence) |
| Compression | ? | 90% | 1 week | MEDIUM (file handling) |

### 🟡 HIGH (Month 3-4)

| Module | Current | Target | Effort | Impact |
|--------|---------|--------|--------|--------|
| Async I/O | 5% | 70% | 3 weeks | HIGH (modern apps) |
| Logging | ? | 95% | 1 week | MEDIUM (debugging) |
| Email | ? | 90% | 1 week | MEDIUM (notifications) |
| Templates | ? | 90% | 2 weeks | MEDIUM (web/reports) |

### 🟢 MEDIUM (Month 5-6, if time allows)

| Module | Current | Target | Effort | Impact |
|--------|---------|--------|--------|--------|
| UUID | ? | 100% | 2 days | LOW (simple utility) |
| Subprocess | ? | 90% | 1 week | MEDIUM (system integration) |
| PDF | ? | 80% | 2 weeks | LOW (reporting) |
| Image Utils | ? | 80% | 2 weeks | LOW (graphics) |

---

## Success Metrics

### Quantitative

- ✅ **70+ stdlib modules** (from 62)
- ✅ **8+ critical modules deepened** (crypto, HTTP, DB, compression, async, logging, email, templates)
- ✅ **400+ test cases added** (across all new/updated modules)
- ✅ **90%+ test coverage** (for stdlib)
- ✅ **3 showcase applications** (demonstrating stdlib capabilities)
- ✅ **2000+ lines of documentation** (API reference + guides)

### Qualitative

- ✅ **Production-ready modules** - No placeholders, full error handling
- ✅ **Consistent API design** - Natural language patterns, intuitive naming
- ✅ **Domain-balanced examples** - Business, data, web, scientific use cases
- ✅ **Real-world validated** - Used in actual applications
- ✅ **Community feedback positive** - Beta testers report high satisfaction

---

## Implementation Principles (CRITICAL)

### 1. **No Shortcuts, No Compromises**

- ✅ Full implementations, not stubs
- ✅ Comprehensive error handling
- ✅ Edge cases covered
- ✅ Production-quality code

### 2. **Domain Neutrality**

- ❌ Don't optimize for specific domains (web, graphics, etc.)
- ✅ Provide universal building blocks
- ✅ Balanced examples across all domains
- ✅ Generic, reusable abstractions

### 3. **Natural Language Philosophy**

- ✅ Readable function names (`http_get`, `hash_sha256`)
- ✅ Clear parameter names (`with url`, `and timeout`)
- ✅ Explicit over implicit
- ✅ English-like syntax

### 4. **Testing First**

- ✅ Write tests before code (TDD where possible)
- ✅ Unit tests for all functions
- ✅ Integration tests for workflows
- ✅ Real-world use case validation

---

## Resource Requirements

### Developer Time

- **1 developer full-time:** 6 months
- **2 developers full-time:** 3-4 months
- **3 developers full-time:** 2-3 months (parallel work)

### Dependencies

**Python Libraries (for stdlib implementations):**
- `cryptography` - Encryption and asymmetric crypto
- `psycopg2` - PostgreSQL (already used)
- `mysql-connector-python` - MySQL (already used)
- `pymongo` - MongoDB (already used)
- `aiofiles` - Async file I/O
- `aiohttp` - Async HTTP client
- `jinja2` - Template engine (or implement custom)

**System Requirements:**
- Python 3.10+ (for development)
- PostgreSQL, MySQL, MongoDB (for testing)
- OpenSSL (for crypto via FFI, optional)

---

## Risk Assessment

### HIGH RISK

- ⚠️ **Async/await runtime incomplete** - May delay async I/O module
  - **Mitigation:** Implement threading-based async patterns as interim solution

- ⚠️ **Scope creep** - Adding too many features, losing focus
  - **Mitigation:** Strict priority matrix, focus on critical modules first

### MEDIUM RISK

- ⚠️ **API design inconsistency** - Different modules use different patterns
  - **Mitigation:** Establish API design guidelines early, code review

- ⚠️ **Test coverage gaps** - Hard-to-test edge cases
  - **Mitigation:** Mandatory test requirements, coverage tracking

### LOW RISK

- ⚠️ **Performance issues** - Stdlib functions too slow
  - **Mitigation:** Benchmark critical functions, optimize hotspots

- ⚠️ **Documentation lag** - Code ahead of docs
  - **Mitigation:** Document as you code, mandatory doc review

---

## Next Immediate Actions

### Week 1: Crypto Module Expansion

**Tasks:**
1. Add AES-256 encryption/decryption (via `cryptography` library)
2. Add RSA key generation and encryption
3. Add Ed25519 signature generation/verification
4. Add PBKDF2 key derivation
5. Write 50+ unit tests
6. Write example program: Encrypt/decrypt file with password
7. Update documentation

**Deliverable:** `crypto/` module at 95% completion

### Week 2: HTTP Server Framework

**Tasks:**
1. Design HTTP server API (routes, handlers, middleware)
2. Implement basic HTTP server (socket + request parsing)
3. Add routing system (URL patterns, path parameters)
4. Add middleware support (authentication, logging, CORS)
5. Add session management (cookies, in-memory store)
6. Write 60+ unit tests
7. Write example program: REST API with CRUD operations
8. Update documentation

**Deliverable:** `http/` module at 90% completion

### Week 3-4: Database Query Builder

**Tasks:**
1. Design query builder API (fluent interface)
2. Implement SELECT/INSERT/UPDATE/DELETE builders
3. Add WHERE clause builder (conditions, operators)
4. Add JOIN support
5. Unify SQLite connector with PostgreSQL/MySQL
6. Add connection health checks
7. Write 70+ unit tests
8. Write example program: ORM-like CRUD application
9. Update documentation

**Deliverable:** `databases/` module at 95% completion

---

## Communication Plan

### Weekly Progress Reports

- What was completed
- What's in progress
- Blockers and risks
- Next week's plan

### Milestone Reviews

- End of Month 2: Critical modules deepened (crypto, HTTP, DB)
- End of Month 4: High-value modules added (async, logging, email, templates)
- End of Month 6: Production-ready stdlib, showcase apps complete

### Community Updates

- Blog posts on new modules
- Example code snippets
- Tutorial videos (optional)
- Beta tester feedback sessions

---

## Conclusion

NLPL's stdlib expansion is **critical for production readiness**. We have solid foundations (62 modules, 4 critical modules with implementations) but need to **deepen, test, and validate** before declaring victory.

**This 4-6 month effort transforms NexusLang from "feature-rich toy" to "production-ready universal language" by ensuring developers can build real-world applications without reinventing the wheel.**

**Next Step:** Begin Week 1 - Crypto Module Expansion (AES, RSA, Ed25519, PBKDF2).
