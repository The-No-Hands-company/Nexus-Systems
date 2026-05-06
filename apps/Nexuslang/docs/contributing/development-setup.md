# NexusLang Development Setup Guide

## Quick Start

### 1. Set Up Virtual Environment

```bash
# Create virtual environment
python -m venv venv

# Activate it (Linux/Mac)
source venv/bin/activate

# Activate it (Windows)
venv\Scripts\activate
```

### 2. Install NexusLang in Development Mode

```bash
# Install the package in editable mode
pip install -e .

# Install development dependencies
pip install pytest pytest-cov black isort flake8
```

### 3. Verify Installation

```bash
# Test the interpreter
python -m nexuslang.main examples/01_basic_concepts.nlpl

# Or use the shorter command (if installed)
nlpl examples/01_basic_concepts.nlpl

# Run tests
pytest tests/ -v

# Check code quality
black src/ tests/
isort src/ tests/
flake8 src/ tests/
```

## Running NexusLang Programs

### Using the Python Module

```bash
# Standard execution
python -m nexuslang.main path/to/program.nlpl

# With debug output (shows tokens and AST)
python -m nexuslang.main path/to/program.nlpl --debug

# Without type checking
python -m nexuslang.main path/to/program.nlpl --no-type-check
```

### Quick Test

```bash
# Create a test file
echo 'set greeting to "Hello, NLPL!"
print text greeting' > test.nlpl

# Run it
python -m nexuslang.main test.nlpl
```

## Running Tests

### All Tests

```bash
pytest tests/ -v
```

### Specific Test File

```bash
pytest tests/test_parser.py -v
pytest tests/test_lexer.py -v
pytest tests/test_interpreter.py -v
```

### With Coverage

```bash
pytest tests/ --cov=src/nexuslang --cov-report=html
# Open htmlcov/index.html in a browser
```

### Specific Test

```bash
pytest tests/test_parser.py::TestParser::test_variable_declaration_simple -v
```

## Code Quality

### Format Code

```bash
# Format with black
black src/ tests/

# Sort imports
isort src/ tests/

# Run both
black src/ tests/ && isort src/ tests/
```

### Lint Code

```bash
# Check with flake8
flake8 src/ tests/

# Check specific file
flake8 src/nexuslang/parser/parser.py
```

## Development Workflow

### 1. Make Changes

Edit files in `src/nexuslang/`

### 2. Test Your Changes

```bash
# Run relevant tests
pytest tests/test_parser.py -v

# Or create a test NexusLang program
echo 'set x to 42
print text x' > test_programs/my_test.nlpl

python -m nexuslang.main test_programs/my_test.nlpl
```

### 3. Format and Lint

```bash
black src/ tests/
isort src/ tests/
flake8 src/ tests/
```

### 4. Run Full Test Suite

```bash
pytest tests/ -v
```

## Project Structure

```
NexusLang/
 src/nexuslang/ # Main source code
 parser/ # Lexer, parser, AST
 interpreter/ # Interpreter implementation
 runtime/ # Runtime environment
 typesystem/ # Type checking and inference
 stdlib/ # Standard library modules
 modules/ # Module loading system
 compiler/ # Compiler backends (C, C++, etc.)
 tests/ # Unit and integration tests
 examples/ # Example NexusLang programs
 test_programs/ # Test programs for validation
 docs/ # Documentation
 dev_tools/ # Development utilities
```

## Common Issues

### ImportError: No module named 'nlpl'

**Solution:** Make sure you've installed the package in editable mode:
```bash
pip install -e .
```

### Tests fail with import errors

**Solution:** Activate the virtual environment first:
```bash
source venv/bin/activate # Linux/Mac
venv\Scripts\activate # Windows
```

### "Permission denied" when running nlpl

**Solution:** Use the module form instead:
```bash
python -m nexuslang.main path/to/file.nlpl
```

## Deactivating Virtual Environment

```bash
deactivate
```

## Updating Dependencies

```bash
# Upgrade pip
pip install --upgrade pip

# Install/update all dependencies
pip install -e .
pip install pytest pytest-cov black isort flake8
```

## Tips

1. **Always activate the venv** before working on NexusLang
2. **Run tests frequently** to catch issues early
3. **Format code** before committing changes
4. **Check examples/** for correct NexusLang syntax
5. **Use --debug flag** when troubleshooting parser issues

## Next Steps

- Read `docs/language_specification.md` for language syntax
- Check `examples/` for working code samples
- Review `ROADMAP.md` for development priorities
- See `dev_tools/README.md` for advanced debugging tools
