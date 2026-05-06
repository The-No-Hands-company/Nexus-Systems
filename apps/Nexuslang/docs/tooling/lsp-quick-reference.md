# LSP Development Quick Reference

**3-Month Mission:** Build production-ready LSP + 3 showcase projects  
**Current Week:** Week 1 (Feb 15-21) - Cross-File Navigation  
**Daily Target:** 6-7 hours focused work

---

## Quick Commands

### Run LSP Server
```bash
# Start LSP server (stdio mode)
python3 -m nexuslang.lsp

# Test LSP with sample project
cd test_programs/
code .  # Opens VS Code with LSP
```

### Test LSP Features
```bash
# Run LSP test suite
pytest tests/test_lsp*.py -v

# Run specific test
pytest tests/test_workspace_index.py::test_extract_function_symbols -v

# Run LSP server tests
python3 dev_tools/test_lsp_server.py
```

### Profile Performance
```bash
# Profile LSP server
python3 -m cProfile -o lsp_profile.stats -m nexuslang.lsp

# View profile results
python3 -c "import pstats; p = pstats.Stats('lsp_profile.stats'); p.sort_stats('cumulative'); p.print_stats(20)"
```

### Check LSP Logs
```bash
# Tail LSP server logs
tail -f /tmp/nlpl-lsp.log

# View full logs
less /tmp/nlpl-lsp.log
```

---

## File Locations

### LSP Implementation
- **Server**: `src/nexuslang/lsp/server.py`
- **Completions**: `src/nexuslang/lsp/completions.py`
- **Definitions**: `src/nexuslang/lsp/definitions.py`
- **Hover**: `src/nexuslang/lsp/hover.py`
- **Diagnostics**: `src/nexuslang/lsp/diagnostics.py`
- **Symbols**: `src/nexuslang/lsp/symbols.py`
- **References**: `src/nexuslang/lsp/references.py`
- **Rename**: `src/nexuslang/lsp/rename.py`
- **Code Actions**: `src/nexuslang/lsp/code_actions.py`
- **Signature Help**: `src/nexuslang/lsp/signature_help.py`
- **Formatter**: `src/nexuslang/lsp/formatter.py`
- **Semantic Tokens**: `src/nexuslang/lsp/semantic_tokens.py`

### Core Supporting Files
- `src/nexuslang/lsp/workspace_index.py` - Workspace symbol indexing
- `tests/tooling/test_workspace_index.py` - Tests for indexing
- `docs/9_status_reports/LSP_WEEK_01_REPORT.md` - Week report

### Documentation
- **Roadmap**: `docs/8_planning/LSP_COMPLETION_ROADMAP.md`
- **Week 1 Kickoff**: `docs/9_status_reports/LSP_WEEK_01_KICKOFF.md`
- **LSP README**: `src/nexuslang/lsp/README.md`

### Test Projects (For Manual Testing)
- **Examples**: `examples/` (existing NexusLang examples)
- **Test Programs**: `test_programs/` (unit tests)
- **Multi-file test**: Create in `test_programs/lsp_workspace_test/`

---

## Key Data Structures

### Symbol Information
```python
@dataclass
class SymbolInfo:
    name: str           # Symbol name (e.g., "greet")
    kind: str          # 'function', 'class', 'variable', 'struct', 'method'
    file_uri: str      # File path (e.g., "file:///path/to/file.nxl")
    line: int          # 0-indexed line number
    column: int        # 0-indexed column number
    scope: Optional[str]       # Module or class name
    signature: Optional[str]   # For functions: "with name as String returns String"
```

### LSP Position & Range
```python
@dataclass
class Position:
    line: int          # 0-indexed
    character: int     # 0-indexed

@dataclass
class Range:
    start: Position
    end: Position

@dataclass
class Location:
    uri: str          # File URI
    range: Range
```

---

## Common LSP Methods (Week 1 Focus)

### textDocument/definition
**Request:** Go to symbol definition  
**Handler:** `definition_provider.provide_definition()`  
**Return:** `Location | Location[] | null`

### textDocument/documentSymbol
**Request:** Get document outline  
**Handler:** `symbol_provider.provide_document_symbols()`  
**Return:** `DocumentSymbol[]` (hierarchical tree)

### callHierarchy/incomingCalls
**Request:** Find callers of a function  
**Handler:** `call_hierarchy_provider.incoming_calls()`  
**Return:** `CallHierarchyIncomingCall[]`

### callHierarchy/outgoingCalls
**Request:** Find callees from a function  
**Handler:** `call_hierarchy_provider.outgoing_calls()`  
**Return:** `CallHierarchyOutgoingCall[]`

---

## AST Nodes to Handle (Week 1)

```python
# From src/nexuslang/parser/ast.py

class FunctionDefinition:
    name: str
    parameters: List[Parameter]
    return_type: Optional[Type]
    body: List[Statement]
    line_number: int

class ClassDefinition:
    name: str
    base_class: Optional[str]
    methods: List[FunctionDefinition]
    attributes: List[VariableDeclaration]
    line_number: int

class StructDefinition:
    name: str
    fields: List[Field]
    line_number: int

class VariableDeclaration:
    name: str
    value_type: Optional[Type]
    value: Expression
    line_number: int

class ImportStatement:
    module_path: str
    imported_names: Optional[List[str]]
    line_number: int
```

---

## Testing Checklist

### Unit Tests
- [ ] `test_scan_workspace()` - Find all .nlpl files
- [ ] `test_index_file()` - Extract symbols from single file
- [ ] `test_get_symbol()` - Lookup symbol by name
- [ ] `test_cross_file_definition()` - Navigate to imported symbol
- [ ] `test_document_outline()` - Generate symbol tree
- [ ] `test_incoming_calls()` - Find callers
- [ ] `test_outgoing_calls()` - Find callees

### Integration Tests
- [ ] Multi-file project (5+ files with imports)
- [ ] Large workspace (100+ files) - performance check
- [ ] Circular imports - graceful handling
- [ ] Invalid files - no crash
- [ ] Incremental updates - only changed file re-indexed

### Manual Tests in VS Code
- [ ] Open multi-file NexusLang project
- [ ] Ctrl+Click on imported function → jumps to definition
- [ ] Open outline view → shows file structure
- [ ] Right-click function → "Find All References"
- [ ] Right-click function → "Go to Callers/Callees"

---

## Performance Targets (Week 1)

| Operation | Target | Measurement |
|-----------|--------|-------------|
| Index 100 files | <1 second | Time `scan_workspace()` |
| Index 1000 files | <10 seconds | Stress test |
| Go-to-definition | <50ms | LSP response time |
| Document outline | <100ms | LSP response time |
| Memory usage (100 files) | <100MB | Check process RSS |
| Memory usage (1000 files) | <500MB | Stress test |

---

## Debugging Tips

### LSP Not Responding
1. Check `/tmp/nlpl-lsp.log` for errors
2. Verify LSP server process is running: `ps aux | grep nexuslang.lsp`
3. Restart VS Code and LSP server
4. Test with minimal NexusLang file (hello world)

### Symbol Not Found
1. Verify file is indexed: Check `workspace_index.files` dictionary
2. Verify symbol extracted: Check `workspace_index.symbols` dictionary
3. Print debug info: Add `logger.debug(f"Symbols: {self.symbols}")` to indexer
4. Test symbol extraction in isolation (unit test)

### Slow Performance
1. Profile with cProfile: `python3 -m cProfile -o profile.stats ...`
2. Find bottleneck: Sort by cumulative time
3. Optimize hot path: Use caching, faster data structures
4. Parallelize if needed: Use multiprocessing for file parsing

### Crashes on Large Workspaces
1. Add error handling: Wrap file parsing in try/except
2. Skip problematic files: Log error and continue
3. Limit memory: Use generators instead of loading all files
4. Test incrementally: Start with 10 files, then 50, then 100

---

## Daily Workflow

### Morning (Start of Day)
1. Review yesterday's progress
2. Check Week 1 kickoff document for today's tasks
3. Update checklist with completed items
4. Plan today's focus (pick 1-2 main tasks)

### During Work
1. Work in focused 90-minute blocks
2. Take 10-minute breaks between blocks
3. Commit code frequently (every major feature)
4. Write tests alongside implementation

### End of Day
1. Commit and push work
2. Update daily progress in kickoff document
3. Note any blockers or challenges
4. Plan tomorrow's first task (easy warm-up)

---

## Commit Message Format

```
[LSP] Brief description of change

- Detail 1
- Detail 2
- Detail 3

Closes #123 (if fixing an issue)
```

Examples:
```
[LSP] Add workspace symbol indexing

- Implement WorkspaceIndex class
- Add scan_workspace() to find all .nlpl files
- Add index_file() to extract symbols from AST
- Add tests for symbol extraction

[LSP] Enable cross-file go-to-definition

- Enhance definitions.py to check WorkspaceIndex
- Implement import resolution
- Add test with multi-file project
- Performance: <50ms for 100-file workspace
```

---

## Motivation & Mindset

**Remember Why:**
- LSP completion unlocks developer adoption
- Real-world showcase projects prove NexusLang viability
- 3 months = production-ready v1.0 foundation
- This work directly enables Phase 2 (Package Manager)

**When Stuck:**
1. Take a break (walk, coffee, context switch)
2. Ask for help (GitHub, Discord, search LSP docs)
3. Simplify problem (break into smaller pieces)
4. Test minimal case (reduce complexity)
5. Move to different task (come back with fresh eyes)

**Celebrate Wins:**
- ✅ First cross-file navigation working → Screenshot & celebrate!
- ✅ 100-file workspace indexed in <1s → Benchmark & share!
- ✅ Week 1 complete → Write status report, take day off!

---

## Key Contacts & Resources

**LSP Specification:**
- https://microsoft.github.io/language-server-protocol/

**LSP Examples (Rust Analyzer, TypeScript Server):**
- https://github.com/rust-lang/rust-analyzer
- https://github.com/microsoft/TypeScript/tree/main/src/server

**Python LSP Libraries:**
- pygls (Generic LSP server framework): https://github.com/openlawlibrary/pygls
- python-lsp-server (Python LSP reference): https://github.com/python-lsp/python-lsp-server

**Testing:**
- pytest documentation: https://docs.pytest.org/
- pytest-lsp (LSP testing): https://github.com/swyddfa/lsp-devtools

**Performance:**
- Python profiling: https://docs.python.org/3/library/profile.html
- Memory profiling: https://pypi.org/project/memory-profiler/

---

## Quick Wins (If Ahead of Schedule)

If you finish Week 1 tasks early:

1. **Add LSP progress notifications**
   - Report indexing progress to VS Code status bar
   - "Indexing workspace: 45/100 files"

2. **Persistent symbol cache**
   - Save symbol table to disk (pickle)
   - Load on startup (faster second launch)

3. **Parallel file parsing**
   - Use multiprocessing.Pool to parse files in parallel
   - 4x faster indexing on 4-core machine

4. **Fuzzy symbol search**
   - Allow typos in workspace symbol search
   - "gret" finds "greet" function

5. **Symbol icon provider**
   - Return icon for each symbol kind (function, class, variable)
   - Makes VS Code outline prettier

---

**Good luck with Week 1! You're building something amazing. 🚀**

**Questions? Check:**
- LSP Roadmap: `docs/8_planning/LSP_COMPLETION_ROADMAP.md`
- Week 1 Kickoff: `docs/9_status_reports/LSP_WEEK_01_KICKOFF.md`
- LSP README: `src/nexuslang/lsp/README.md`
