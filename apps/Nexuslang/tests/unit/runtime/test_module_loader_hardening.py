from unittest.mock import patch

from nexuslang.modules.module_loader import ModuleLoader
from nexuslang.runtime.runtime import Runtime


def test_module_loader_wraps_recoverable_parse_failures_as_import_error(tmp_path):
    module_file = tmp_path / "broken_module.nxl"
    module_file.write_text("set value to 1\n", encoding="utf-8")

    loader = ModuleLoader(Runtime(), search_paths=[str(tmp_path)])

    class _BadLexer:
        def __init__(self, _text):
            pass

        def tokenize(self):
            raise ValueError("tokenization failed")

    with patch("nexuslang.modules.module_loader.Lexer", _BadLexer):
        try:
            loader._load_module_from_file(str(module_file))
        except ImportError as exc:
            message = str(exc)
        else:
            raise AssertionError("Expected ImportError for recoverable lexer failure")

    assert "Error loading module" in message
    assert str(module_file) in message
