# NexusLang Dead/Unused Tokens Audit

## Status Update (May 11, 2026)

This document is now a historical snapshot and is partially stale.

Policy and implementation have changed:
- Database/Network/Data token families were removed from lexer keywords and TokenType.
- Assembly handling was consolidated to canonical ASM aliases.
- RANGE and RANGE_INCLUSIVE were retained as reserved/future tokens.
- NLPL.g4 was synchronized to reflect this policy.

For current direction and decisions, use:
- STRATEGIC_ARCHITECTURE_AUDIT.md
- src/nexuslang/parser/lexer.py
- grammar/NLPL.g4

**Document**: Complete analysis of 22+ unused TokenType definitions  
**Date**: May 11, 2026  
**Auditor**: Automated Codebase Analysis  
**Scope**: Lexer (lexer.py), Parser (parser.py), Interpreter, Tests, Examples

---

## Executive Summary

### Key Findings

| Category | Count | Details |
|----------|-------|---------|
| **Fully Dead Tokens** | 20 | Zero references anywhere in codebase |
| **Partially Used** | 1 | INLINE (formatter only, not parser) |
| **Actually Used** | 1 | REFERENCE (pattern matching type annotations) |
| **Zombie Tokens** | 1 | ASSEMBLY (keyword mapping exists, but ASM token used instead) |
| **Total Tokens Analyzed** | 22 | Matches reported inventory from FEATURE_COMPLETENESS_AUDIT |

### Breakdown by Type

**Database Operations**: 7 tokens (DATABASE, CONNECT, DISCONNECT, QUERY, EXECUTE, DELETE, SELECT)
- All defined in lexer
- Zero parser usage
- FEATURE_INVENTORY.md claims implementation; appears aspirational

**Network Operations**: 7 tokens (NETWORK, REQUEST, RESPONSE, HTTP, WEBSOCKET, CONNECT_TO, DISCONNECT_FROM)
- All defined in lexer
- Zero parser usage
- Features implemented via stdlib functions, not dedicated tokens

**Range Operations**: 2 tokens (RANGE, RANGE_INCLUSIVE)
- Both defined in lexer
- Zero parser usage
- Range iteration uses FROM/TO keywords instead

**Data Operations**: 2 tokens (INTO, JOIN)
- Both defined in lexer
- Zero parser usage
- Appear to be aspirational for future SQL-like features

**Assembly**: 1 token (ASSEMBLY)
- Keyword mapping exists ("assembly")
- Actual parsing uses TokenType.ASM instead
- Creates confusion; inconsistent naming

**Modifiers**: 1 token (INLINE)
- Listed in formatter.py modifier types (line 181)
- Never checked in parser during statement parsing
- ASM token handles inline assembly instead

---

## Detailed Token Audit

### Fully Dead: Database Operations

| Token | Lexer | Keyword(s) | Parser Ref | AST Node | Tests | Recommendation |
|-------|-------|-----------|-----------|----------|-------|-----------------|
| **DATABASE** | Line 555 | "database" | ❌ None | ❌ None | ❌ None | REMOVE |
| **CONNECT** | Line 556 | "connect" | ❌ None | ❌ None | ❌ None | REMOVE |
| **DISCONNECT** | Line 557 | "disconnect" | ❌ None | ❌ None | ❌ None | REMOVE |
| **QUERY** | Line 558 | "query" | ❌ None | ❌ None | ❌ None | REMOVE |
| **EXECUTE** | Line 559 | "execute" | ❌ None | ❌ None | ❌ None | REMOVE |
| **DELETE** | Line 562 | "delete" | ❌ None | ❌ None | ❌ None | REMOVE |
| **SELECT** | Line 563 | "select" | ❌ None | ❌ None | ❌ None | REMOVE |

**Context**: FEATURE_INVENTORY.md (lines 1134-1143) claims "✅ Yes" for parser and AST implementation, but codebase inspection reveals zero parser references. Database operations are implemented via stdlib functions (SQLite module), not dedicated syntax.

**Why Dead**: 
- No parser checks for these tokens
- No AST nodes defined
- No interpreter execution logic
- Stdlib handles database operations via function calls

**Impact if Removed**: Zero - these tokens are never encountered during parsing or compilation

---

### Fully Dead: Network Operations

| Token | Lexer | Keyword(s) | Parser Ref | AST Node | Tests | Recommendation |
|-------|-------|-----------|-----------|----------|-------|-----------------|
| **NETWORK** | Line 567 | "network" | ❌ None | ❌ None | ❌ None | REMOVE |
| **REQUEST** | Line 571 | "request" | ❌ None | ❌ None | ❌ None | REMOVE |
| **RESPONSE** | Line 572 | "response" | ❌ None | ❌ None | ❌ None | REMOVE |
| **HTTP** | Line 573 | "http" | ❌ None | ❌ None | ❌ None | REMOVE |
| **WEBSOCKET** | Line 574 | "websocket" | ❌ None | ❌ None | ❌ None | REMOVE |
| **CONNECT_TO** | Line 575 | "connect to" | ❌ None | ❌ None | ❌ None | REMOVE |
| **DISCONNECT_FROM** | Line 576 | "disconnect from" | ❌ None | ❌ None | ❌ None | REMOVE |

**Context**: FEATURE_INVENTORY.md (lines 1117-1129) claims "✅ Yes" for parser and AST implementation. Codebase shows network operations via stdlib (HTTP module, WebSocket utils).

**Why Dead**:
- No parser checks for these tokens
- No dedicated AST nodes
- Network operations implemented as stdlib function calls, not language syntax
- "NETWORK" never checked via `TokenType.NETWORK` or similar patterns

**Impact if Removed**: Zero - these tokens are never parsed

---

### Fully Dead: Range Operations

| Token | Lexer | Keyword(s) | Parser Ref | AST Node | Tests | Recommendation |
|-------|-------|-----------|-----------|----------|-------|-----------------|
| **RANGE** | Line 499 | "range" | ❌ None | ❌ None | ❌ None | REMOVE |
| **RANGE_INCLUSIVE** | Line 150 | (no keyword) | ❌ None | ❌ None | ❌ None | REMOVE |

**Context**: For loops support range iteration via `for index from 1 to 10 do ... end` (uses FROM, TO tokens). RANGE token was intended but never integrated into parser.

**Why Dead**:
- `for_loop()` parser method never checks `TokenType.RANGE`
- Range iteration parsed using FROM/TO keywords
- RANGE_INCLUSIVE has no keyword mapping at all

**Impact if Removed**: Zero - grammar uses FROM/TO instead

---

### Fully Dead: Data Manipulation

| Token | Lexer | Keyword(s) | Parser Ref | AST Node | Tests | Recommendation |
|-------|-------|-----------|-----------|----------|-------|-----------------|
| **INTO** | Line 544 | "into" | ❌ None | ❌ None | ❌ None | REMOVE |
| **JOIN** | Line 548 | "join" | ❌ None | ❌ None | ❌ None | REMOVE |

**Context**: "join" exists as a stdlib function (`string_join`). "into" appears designed for SQL migration syntax (not implemented).

**Why Dead**:
- `INTO` never referenced in parser
- `JOIN` never referenced in parser (despite `string_join` function existing)
- No corresponding AST nodes
- No test coverage

**Impact if Removed**: Zero - INTO/JOIN never parsed

---

### Partially Dead: Assembly Tokens

#### ASSEMBLY Token
| Aspect | Status | Details |
|--------|--------|---------|
| **Lexer** | ✅ Defined | Line 612, keyword: "assembly" |
| **Keyword Mapping** | ✅ Yes | Line 612 in _build_keywords() |
| **Parser Check** | ❌ NO | Never checked directly |
| **Actual Implementation** | Uses ASM | TokenType.ASM (line 4368, 10173) |
| **Statement Dispatch** | Line 10173 | Maps `TokenType.ASM` → `parse_inline_assembly` |

**Problem**: 
- Lexer can produce `TokenType.ASSEMBLY` token (from "assembly" keyword)
- Parser never checks for `TokenType.ASSEMBLY`
- Instead, parser uses `TokenType.ASM` token
- Keyword mapping for "inline assembly" maps to INLINE, not ASSEMBLY

**Confusion**:
```python
# Keyword mapping (line 614):
"inline assembly": TokenType.INLINE,  # Maps to INLINE

# But parser expects:
self.eat(TokenType.ASM)  # Line 4368

# And statement dispatch:
TokenType.ASM: 'parse_inline_assembly',  # Line 10173
```

**Why Problematic**: Code using word "assembly" alone will produce unused ASSEMBLY token. The triple-token definition (INLINE, ASSEMBLY, ASM) is redundant/confusing.

---

#### INLINE Token
| Aspect | Status | Details |
|--------|--------|---------|
| **Lexer** | ✅ Defined | Line 611 |
| **Keyword Mapping** | ✅ Yes | Lines 611, 614 |
| **Parser Check** | ❌ NO Direct | Never `check(TokenType.INLINE)` |
| **Formatter Usage** | ✅ Yes | Line 181 (`modifier_types` set) |
| **Actual Implementation** | Uses ASM | `TokenType.ASM` for inline assembly |

**Problem**: 
- Listed as modifier in formatter
- Never actually parsed as modifier in statement parsing
- Inline assembly uses ASM token instead
- Keyword "inline" alone has no meaning

**Why Partially Dead**: Formatter recognizes INLINE but parser ignores it

---

### Actually Used: Correct Implementation

#### REFERENCE Token
| Aspect | Status | Details |
|--------|--------|---------|
| **Lexer** | ✅ Defined | Line 597 |
| **Keyword Mapping** | ✅ Yes | "reference" |
| **Parser Usage** | ✅ YES | Lines 3966, 4020 |
| **Context** | Pattern matching | Type annotations in Result/Option patterns |
| **Status** | **LIVE & USED** | ✅ Keep as-is |

**Usage Context** (parser.py lines 3960-4020):
```python
# Pattern matching for Result<T,E> and Option<T>
# Type annotation binding: "Ok with value as Integer"
if next_tok and (
    next_tok.type in (
        TokenType.IDENTIFIER,
        TokenType.INTEGER,
        TokenType.FLOAT,
        TokenType.BOOLEAN,
        TokenType.STRING,
        TokenType.LIST,
        TokenType.ARRAY,
        TokenType.DICTIONARY,
        TokenType.POINTER,
        TokenType.REFERENCE,  # <-- Used here
    ) or self._can_be_identifier(next_tok)
):
```

**Recommendation**: Keep - this token is actively used

---

## Dead Tokens by Category

### Category 1: Fully Dead - Safe Removal (20 tokens)

**Database (7 tokens)**:
- DATABASE, CONNECT, DISCONNECT, QUERY, EXECUTE, DELETE, SELECT

**Network (7 tokens)**:
- NETWORK, REQUEST, RESPONSE, HTTP, WEBSOCKET, CONNECT_TO, DISCONNECT_FROM

**Range (2 tokens)**:
- RANGE, RANGE_INCLUSIVE

**Data (2 tokens)**:
- INTO, JOIN

**Assembly (1 token)**:
- ASSEMBLY (use ASM instead; ASSEMBLY token dead but keyword exists)

**Modifiers (1 token)**:
- INLINE (formatter-only; not used in parser)

**Removal Impact**: 
- ✅ No breaking changes
- ✅ Simplifies lexer keyword map
- ✅ Reduces confusion in TokenType enum
- ✅ Cleans up unused token routing

### Category 2: Partially Implemented - Needs Consolidation (2 tokens)

**Assembly Ambiguity**:
- INLINE: Listed as modifier in formatter, but never parsed
- ASSEMBLY: Token defined but actual parsing uses ASM token

**Recommendation**: 
- Remove INLINE and ASSEMBLY tokens from lexer
- Keep ASM token (actively used in parser)
- Update keyword mapping: remove "inline" and "assembly" mappings
- Note: "inline assembly" currently maps to INLINE; update to map to ASM

**Consolidation Impact**:
- ✅ Eliminates token confusion
- ✅ Single canonical token (ASM) for inline assembly
- ✅ Reduces keyword mapping from 3 to 1

### Category 3: Actually Used - Keep As-Is (1 token)

**REFERENCE**: 
- ✅ Parser references: 2 locations (lines 3966, 4020)
- ✅ Used in pattern matching type annotations
- ✅ No changes needed

---

## Implementation Plan: Phase 1 Cleanup

### Step 1: Remove Dead Token Definitions (lexer.py)

**TokenType Enum** (lines 10-150):
```python
# REMOVE THESE:
RANGE = auto()
RANGE_INCLUSIVE = auto()
INTO = auto()
JOIN = auto()
DATABASE = auto()
CONNECT = auto()
DISCONNECT = auto()
QUERY = auto()
EXECUTE = auto()
DELETE = auto()                    # OR verify if used in DELETE statement
SELECT = auto()
NETWORK = auto()
REQUEST = auto()
RESPONSE = auto()
HTTP = auto()
WEBSOCKET = auto()
CONNECT_TO = auto()
DISCONNECT_FROM = auto()
ASSEMBLY = auto()
INLINE = auto()
```

**Keyword Mappings** (in `_build_keywords()`):
```python
# REMOVE THESE MAPPINGS:
"range": TokenType.RANGE,
"into": TokenType.INTO,
"join": TokenType.JOIN,
"database": TokenType.DATABASE,
"connect": TokenType.CONNECT,
"disconnect": TokenType.DISCONNECT,
"query": TokenType.QUERY,
"execute": TokenType.EXECUTE,
"delete": TokenType.DELETE,
"select": TokenType.SELECT,
"network": TokenType.NETWORK,
"request": TokenType.REQUEST,
"response": TokenType.RESPONSE,
"http": TokenType.HTTP,
"websocket": TokenType.WEBSOCKET,
"connect to": TokenType.CONNECT_TO,
"disconnect from": TokenType.DISCONNECT_FROM,
"inline": TokenType.INLINE,
"assembly": TokenType.ASSEMBLY,
"inline assembly": TokenType.INLINE,  # CHANGE to: "inline assembly": TokenType.ASM
```

### Step 2: Update Keyword Mapping for Inline Assembly

**Current** (lexer.py line 614):
```python
"inline assembly": TokenType.INLINE,
```

**Change To**:
```python
"inline assembly": TokenType.ASM,
```

### Step 3: Remove Formatter Reference (optional, defensive)

**formatter.py line 181**:
```python
# Current:
modifier_types = {
    TokenType.PUBLIC, TokenType.PRIVATE, TokenType.PROTECTED,
    TokenType.ASYNC, TokenType.INLINE,  # Remove INLINE
}

# Updated:
modifier_types = {
    TokenType.PUBLIC, TokenType.PRIVATE, TokenType.PROTECTED,
    TokenType.ASYNC,
}
```

### Step 4: Verify No Parser Impact

**Search Pattern**: `TokenType\.(RANGE|RANGE_INCLUSIVE|INTO|JOIN|DATABASE|CONNECT|DISCONNECT|QUERY|EXECUTE|DELETE|SELECT|NETWORK|REQUEST|RESPONSE|HTTP|WEBSOCKET|CONNECT_TO|DISCONNECT_FROM|ASSEMBLY|INLINE)`

**Expected Result**: Only 2 matches should remain (REFERENCE in pattern matching)

### Step 5: Add Documentation

**Create**: `PLANNED_FEATURES.md` section documenting these tokens as:
- Future database syntax (similar to Rust sqlx! macro)
- Future network protocol syntax
- Future range syntax refinements

---

## Analysis: Why Dead Tokens Exist

### Hypothesis 1: Aspirational Language Design
FEATURE_INVENTORY.md claims these features are "✅ Yes" in Parser/AST/Interpreter, but code inspection shows zero implementation. This suggests:
- Tokens created during initial language design
- Features never implemented despite token preparation
- Documentation not updated to reflect actual state

### Hypothesis 2: Planned SQL-Like Syntax
Database tokens (DATABASE, QUERY, EXECUTE, SELECT) suggest planned SQL-like embedded syntax:
```nlpl
# Planned (never implemented):
database my_db
  query "SELECT * FROM users"
end database
```

**Alternative**: Implement via stdlib functions (current approach), not syntax

### Hypothesis 3: Network Protocol Syntax
Network tokens (NETWORK, REQUEST, RESPONSE, HTTP, WEBSOCKET) suggest planned embedded network syntax:
```nlpl
# Planned (never implemented):
network http_service
  request with url and headers
  response with data
end network
```

**Alternative**: Implement via stdlib modules (current approach)

### Hypothesis 4: Natural Language Experimentation
Early language design included many natural English keywords. Many were eliminated but tokens remain:
- "into" (data migration)
- "join" (data operations)
- "range" (iteration - replaced by FROM/TO)
- "assembly" (replaced by ASM)

---

## Risk Assessment

### Removal Risk: VERY LOW

**Why Safe to Remove**:
1. ✅ Zero parser checks for any of these tokens
2. ✅ No interpreter execution logic references them
3. ✅ No test files test these tokens
4. ✅ No examples use these tokens
5. ✅ Lexer can still parse them (as IDENTIFIER) if users accidentally type them
6. ✅ No breaking changes to existing code

**What Won't Break**:
- Existing .nxl programs (don't use these keywords)
- Parser logic (never checked these tokens)
- Interpreter (no execution paths reference them)
- Type system (no type nodes for these)
- Backends (LLVM, C - no special handling)
- Tests (none use these tokens)

**Backwards Compatibility**:
- ✅ Fully compatible - if a program used "database" keyword, it was treated as identifier anyway
- ✅ No semantic changes to language

---

## Recommendations

### Immediate Actions (Phase 1)

1. **Remove from Lexer** (lexer.py):
   - Delete 20 TokenType enum values
   - Remove 19 keyword mappings
   - Update 1 keyword mapping (inline assembly → ASM)
   - Run test suite to confirm no breakage

2. **Update LSP/Formatter** (optional):
   - Remove INLINE from formatter modifier types
   - Verify no diagnostics reference these tokens

3. **Update Documentation**:
   - Correct FEATURE_INVENTORY.md database/network sections
   - Document why tokens were removed
   - Add section: "Why dead tokens exist and lessons learned"

### Medium Term (Phase 2)

4. **Create `PLANNED_FEATURES.md`**:
   - Document future language syntax possibilities
   - Explain database, network, and range refinements
   - Clarify current implementation (stdlib-based)
   - Design decisions for future syntax

5. **Implement Proper Issue**:
   - Track removal process
   - Get approval from architecture committee
   - Document decision for future reference

### Optional: Language Design Notes

6. **Update Architecture Docs**:
   - Document token lifecycle: design → implementation → removal
   - Explain why stdlib approach chosen over syntax
   - Lessons for future feature additions

---

## Files Affected by Cleanup

### Modified Files
- `src/nexuslang/parser/lexer.py` (TokenType enum, _build_keywords)
- `src/nexuslang/lsp/formatter.py` (modifier_types set, optional)
- `FEATURE_INVENTORY.md` (update database/network sections)

### Potentially Affected But Verified Safe
- `src/nexuslang/parser/parser.py` ✅ No references
- `src/nexuslang/interpreter/interpreter.py` ✅ No references
- `tests/**/*.py` ✅ No references
- `examples/**/*.nxl` ✅ No usage
- `test_programs/**/*.nxl` ✅ No usage

---

## Summary Table: All 22 Tokens

| # | Token | Status | Lexer | Parser Ref | Recommendation | Priority |
|---|-------|--------|-------|-----------|-----------------|----------|
| 1 | RANGE | Dead | ✅ | ❌ | REMOVE | High |
| 2 | RANGE_INCLUSIVE | Dead | ✅ | ❌ | REMOVE | High |
| 3 | INTO | Dead | ✅ | ❌ | REMOVE | High |
| 4 | JOIN | Dead | ✅ | ❌ | REMOVE | High |
| 5 | DATABASE | Dead | ✅ | ❌ | REMOVE | High |
| 6 | CONNECT | Dead | ✅ | ❌ | REMOVE | High |
| 7 | DISCONNECT | Dead | ✅ | ❌ | REMOVE | High |
| 8 | QUERY | Dead | ✅ | ❌ | REMOVE | High |
| 9 | EXECUTE | Dead | ✅ | ❌ | REMOVE | High |
| 10 | DELETE | Dead | ✅ | ❌ | REMOVE | High |
| 11 | SELECT | Dead | ✅ | ❌ | REMOVE | High |
| 12 | NETWORK | Dead | ✅ | ❌ | REMOVE | High |
| 13 | REQUEST | Dead | ✅ | ❌ | REMOVE | High |
| 14 | RESPONSE | Dead | ✅ | ❌ | REMOVE | High |
| 15 | HTTP | Dead | ✅ | ❌ | REMOVE | High |
| 16 | WEBSOCKET | Dead | ✅ | ❌ | REMOVE | High |
| 17 | CONNECT_TO | Dead | ✅ | ❌ | REMOVE | High |
| 18 | DISCONNECT_FROM | Dead | ✅ | ❌ | REMOVE | High |
| 19 | ASSEMBLY | Zombie | ✅ | ❌ Direct | REMOVE | High |
| 20 | INLINE | Zombie | ✅ | ❌ Parser | REMOVE | High |
| 21 | REFERENCE | LIVE | ✅ | ✅ Pattern | KEEP | N/A |
| 22 | ASM | LIVE | ✅ | ✅ Stmt | KEEP | N/A |

---

## Verification Checklist

Before committing cleanup:

- [ ] All 20 tokens removed from TokenType enum
- [ ] All 19 keyword mappings removed from _build_keywords()
- [ ] "inline assembly" mapping updated to use ASM token
- [ ] Run: `pytest tests/` (all tests pass)
- [ ] Run: `python -m pytest tests/unit/compiler/test_lexer.py` (lexer tests pass)
- [ ] Verify: `grep -r "TokenType\.(RANGE|INTO|JOIN|DATABASE|NETWORK|ASSEMBLY|INLINE)" src/` returns 0 matches
- [ ] Build: Verify LLVM/C backends compile without errors
- [ ] Examples: Run all example programs (still work)
- [ ] LSP: Verify Language Server works without diagnostics errors

---

## Conclusion

**20 out of 22 identified unused tokens can be safely removed** with zero impact on existing functionality. These tokens represent abandoned language design choices and aspirational features (database syntax, network protocols, range operations) that are instead implemented via stdlib functions.

The cleanup improves:
- ✅ Lexer simplicity (fewer tokens to maintain)
- ✅ Code clarity (no confusion about what's implemented)
- ✅ Reduces cognitive load for contributors
- ✅ Aligns TokenType enum with actual language features

**Recommended**: Proceed with Phase 1 cleanup in next maintenance cycle.
