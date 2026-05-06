"""Token-based documentation extractor for NexusLang source files.

Extracts documentation from ``##`` doc comments that immediately precede
named definitions (functions, classes, structs, enums, traits, interfaces).

Doc comment syntax
------------------
Doc comments use ``##`` (double-hash).  A single space after ``##`` is
stripped for readability.  Tags are prefixed with ``@``:

  ``@param <name> <description>``    — describe a parameter
  ``@returns <description>``         — describe the return value
  ``@example``                       — begin a multi-line NexusLang code example
  ``@end``                           — end a multi-line example block
  ``@see <reference>``               — cross-reference link
  ``@deprecated [message]``          — mark as deprecated

Example::

    ## Compute the average of a list of numbers.
    ##
    ## @param numbers  the list of numbers to average
    ## @param default  returned when the list is empty (default 0.0)
    ## @returns the arithmetic mean of the list, or default when empty
    ## @example
    ## set nums to [1.0, 2.0, 3.0]
    ## print text average with numbers: nums
    ## @end
    ## @see sum_values
    function average with numbers as List of Float and default as Float default to 0.0 returns Float
"""

import re
from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, List, Optional, Tuple


# ---------------------------------------------------------------------------
# Data-transfer objects
# ---------------------------------------------------------------------------

@dataclass
class ParamDoc:
    """Documentation for a single parameter."""
    name: str
    description: str


@dataclass
class DocItem:
    """Documentation for a single named definition extracted from a source file.

    Consumers should treat ``examples`` as NexusLang source snippets that can be
    executed by :mod:`nlpl.tooling.docgen.doc_tester`.
    """

    name: str
    kind: str                           # "function" | "class" | "struct" | ...
    description: str                    # Main prose description
    params: List[ParamDoc] = field(default_factory=list)
    returns: Optional[str] = None       # @returns description
    examples: List[str] = field(default_factory=list)  # @example blocks
    see_also: List[str] = field(default_factory=list)  # @see references
    deprecated: Optional[str] = None   # @deprecated message (empty str = deprecated, no msg)
    raw_doc: str = ""                   # Verbatim joined doc lines
    source_file: str = ""
    line: int = 0
    # Type parameters, for generic definitions (e.g. "<T, U>")
    type_params: str = ""

    @property
    def is_deprecated(self) -> bool:
        return self.deprecated is not None

    def to_dict(self) -> Dict:
        return {
            "name": self.name,
            "kind": self.kind,
            "description": self.description,
            "params": [{"name": p.name, "description": p.description} for p in self.params],
            "returns": self.returns,
            "examples": self.examples,
            "see_also": self.see_also,
            "deprecated": self.deprecated,
            "source_file": self.source_file,
            "line": self.line,
        }


# ---------------------------------------------------------------------------
# Tag parsing
# ---------------------------------------------------------------------------

_PARAM_RE = re.compile(r"@param\s+(\S+)\s+(.*)", re.DOTALL)
_RETURNS_RE = re.compile(r"@returns?\s+(.*)", re.DOTALL)
_SEE_RE = re.compile(r"@see\s+(.*)")
_DEPRECATED_RE = re.compile(r"@deprecated\s*(.*)")


def _parse_doc_tags(raw_lines: List[str]) -> Tuple[str, List[ParamDoc], Optional[str], List[str], List[str], Optional[str]]:
    """Parse tag annotations from a list of raw doc-comment lines.

    Returns:
        (description, params, returns, examples, see_also, deprecated)
    """
    description_lines: List[str] = []
    params: List[ParamDoc] = []
    returns: Optional[str] = None
    see_also: List[str] = []
    deprecated: Optional[str] = None

    examples: List[str] = []
    current_example_lines: List[str] = []
    in_example = False

    for line in raw_lines:
        stripped = line.strip()

        # --- example block ---
        if stripped == "@example" or stripped.startswith("@example "):
            in_example = True
            # Inline single-line example after @example
            inline = stripped[len("@example"):].strip()
            if inline:
                current_example_lines.append(inline)
            continue
        if stripped == "@end" and in_example:
            examples.append("\n".join(current_example_lines))
            current_example_lines = []
            in_example = False
            continue
        if in_example:
            current_example_lines.append(line)
            continue

        # --- @param ---
        m = _PARAM_RE.match(stripped)
        if m:
            params.append(ParamDoc(name=m.group(1), description=m.group(2).strip()))
            continue

        # --- @returns ---
        m = _RETURNS_RE.match(stripped)
        if m:
            returns = m.group(1).strip()
            continue

        # --- @see ---
        m = _SEE_RE.match(stripped)
        if m:
            see_also.append(m.group(1).strip())
            continue

        # --- @deprecated ---
        m = _DEPRECATED_RE.match(stripped)
        if m:
            deprecated = m.group(1).strip()
            continue

        # --- regular description text ---
        description_lines.append(line)

    # Close any unterminated example block
    if in_example and current_example_lines:
        examples.append("\n".join(current_example_lines))

    description = "\n".join(description_lines).strip()
    return description, params, returns, examples, see_also, deprecated


# ---------------------------------------------------------------------------
# Token-based extractor
# ---------------------------------------------------------------------------

# Mapping from TokenType name (string) to the DocItem kind string
_KIND_MAP = {
    "FUNCTION": "function",
    "CLASS":    "class",
    "STRUCT":   "struct",
    "ENUM":     "enum",
    "TRAIT":    "trait",
    "INTERFACE": "interface",
}

# TokenType names that announce a new **non**-definition (clear pending docs)
_CLEAR_KINDS = {
    "IF", "WHILE", "FOR", "FOR_EACH", "REPEAT", "SET", "PRINT",
    "RETURN", "RAISE", "TRY", "IMPORT", "MODULE", "EXPORT",
}


def extract_from_source(source: str, source_file: str = "") -> List[DocItem]:
    """Extract all documented definitions from NexusLang *source* text.

    Args:
        source:       Raw NexusLang source code.
        source_file:  Path used for metadata in the returned DocItems.

    Returns:
        List of :class:`DocItem` objects in source order.
    """
    from nexuslang.parser.lexer import Lexer, TokenType

    try:
        tokens = Lexer(source).tokenize()
    except Exception:
        return []

    items: List[DocItem] = []
    pending_doc_lines: List[str] = []

    i = 0
    while i < len(tokens):
        tok = tokens[i]
        tok_name = tok.type.name

        # Accumulate doc comment lines
        if tok_name == "DOC_COMMENT":
            # Use the literal (text without ##) when available
            text = tok.literal if tok.literal is not None else tok.lexeme.lstrip("#").lstrip()
            pending_doc_lines.append(text)
            i += 1
            continue

        # Skip syntactic whitespace tokens
        if tok_name in ("NEWLINE", "INDENT", "DEDENT", "COMMENT"):
            i += 1
            continue

        # Recognised definition keyword
        if tok_name in _KIND_MAP:
            kind = _KIND_MAP[tok_name]
            def_line = tok.line

            # Find the name: the next IDENTIFIER (skipping whitespace tokens)
            name = ""
            j = i + 1
            while j < len(tokens):
                t = tokens[j]
                if t.type.name in ("NEWLINE", "INDENT", "DEDENT"):
                    j += 1
                    continue
                if t.type.name == "IDENTIFIER":
                    name = t.lexeme.strip()
                    break
                # Some keywords can be names (e.g. "function update ..."), skip them
                if t.type.name not in _KIND_MAP and t.type.name not in (
                    "WITH", "RETURNS", "EOF"
                ):
                    name = t.lexeme.strip()
                    break
                break

            if name and pending_doc_lines:
                raw_doc = "\n".join(pending_doc_lines)
                desc, params, returns, examples, see_also, deprecated = _parse_doc_tags(pending_doc_lines)
                items.append(DocItem(
                    name=name,
                    kind=kind,
                    description=desc,
                    params=params,
                    returns=returns,
                    examples=examples,
                    see_also=see_also,
                    deprecated=deprecated,
                    raw_doc=raw_doc,
                    source_file=source_file,
                    line=def_line,
                ))

            pending_doc_lines = []
            i += 1
            continue

        # Any other meaningful token — clear pending docs
        if tok_name not in ("WITH", "RETURNS", "ASYNC", "EXPORT", "PUBLIC", "PRIVATE", "PROTECTED"):
            pending_doc_lines = []

        i += 1

    return items


def extract_from_file(path: str) -> List[DocItem]:
    """Read *path* and extract documentation items.

    Returns an empty list if the file cannot be read or parsed.
    """
    try:
        source = Path(path).read_text(encoding="utf-8")
    except OSError:
        return []
    return extract_from_source(source, source_file=path)


def extract_from_directory(src_dir: str) -> Dict[str, List[DocItem]]:
    """Recursively extract docs from all ``*.nlpl`` files under *src_dir*.

    Returns:
        Mapping of relative-path string to list of :class:`DocItem` objects.
    """
    results: Dict[str, List[DocItem]] = {}
    root = Path(src_dir)
    for nxl_file in sorted(root.rglob("*.nxl")):
        rel = str(nxl_file.relative_to(root))
        items = extract_from_file(str(nxl_file))
        if items:
            results[rel] = items
    return results
