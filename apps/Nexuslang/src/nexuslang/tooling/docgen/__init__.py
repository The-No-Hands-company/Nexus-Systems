"""NLPL Documentation Generator.

The ``docgen`` package provides three capabilities:

1. **Extraction** — tokenise NexusLang source files and pull out ``##`` doc comment
   blocks attached to named definitions (:mod:`.extractor`).

2. **HTML generation** — produce a self-contained, searchable HTML documentation
   site from extracted :class:`~.extractor.DocItem` objects (:mod:`.html_writer`).

3. **Doc tests** — execute ``@example`` code blocks embedded in doc comments and
   report pass/fail (:mod:`.doc_tester`).

Typical usage
-------------
::

    from nexuslang.tooling.docgen import DocGenerator
    from nexuslang.tooling.config import ProjectConfig

    cfg = ProjectConfig.load("nexuslang.toml")
    gen = DocGenerator(cfg)

    # Generate HTML docs into docs/
    count = gen.generate()
    print(f"Documented {count} items.")

    # Run doc tests
    result = gen.test()
    result.print_summary()

Low-level API
-------------
::

    from nexuslang.tooling.docgen.extractor import extract_from_directory
    from nexuslang.tooling.docgen.html_writer import generate_html
    from nexuslang.tooling.docgen.doc_tester import run_doc_tests

    items_by_file = extract_from_directory("src/")
    generate_html(items_by_file, "docs/", pkg_name="MyLib", pkg_version="1.0")
    result = run_doc_tests([i for lst in items_by_file.values() for i in lst])
    result.print_summary()
"""

from .extractor import DocItem, ParamDoc, extract_from_directory, extract_from_file, extract_from_source
from .doc_tester import DocTestResult, DocTestFailure, run_doc_tests
from .html_writer import generate_html

__all__ = [
    "DocGenerator",
    "DocItem",
    "ParamDoc",
    "DocTestResult",
    "DocTestFailure",
    "extract_from_directory",
    "extract_from_file",
    "extract_from_source",
    "run_doc_tests",
    "generate_html",
]


class DocGenerator:
    """High-level documentation generation facade.

    Accepts a :class:`~nlpl.tooling.config.ProjectConfig` and provides
    :meth:`generate` / :meth:`test` methods that mirror the ``nlpl doc``
    CLI command.

    Args:
        config: Project configuration object.  The following attributes are
                read if present: ``name``, ``version``, ``src_dir``.
    """

    def __init__(self, config) -> None:
        self._config = config

    @property
    def _src_dir(self) -> str:
        return getattr(self._config, "src_dir", None) or "src"

    @property
    def _pkg_name(self) -> str:
        return getattr(self._config, "name", None) or "NLPL Project"

    @property
    def _pkg_version(self) -> str:
        return getattr(self._config, "version", None) or "0.0.0"

    def generate(self, output_dir: str = "docs") -> int:
        """Extract docs from the project source and write HTML to *output_dir*.

        Returns:
            Total number of documented items written.
        """
        items_by_file = extract_from_directory(self._src_dir)
        generate_html(
            items_by_file,
            output_dir=output_dir,
            pkg_name=self._pkg_name,
            pkg_version=self._pkg_version,
        )
        return sum(len(v) for v in items_by_file.values())

    def test(self, verbose: bool = False) -> "DocTestResult":
        """Run all ``@example`` doc tests in the project.

        Returns:
            A :class:`DocTestResult` with pass/fail statistics.
        """
        items_by_file = extract_from_directory(self._src_dir)
        all_items = [item for lst in items_by_file.values() for item in lst]
        return run_doc_tests(all_items, verbose=verbose)
