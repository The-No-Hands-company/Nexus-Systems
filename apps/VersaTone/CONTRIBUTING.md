# Contributing to Project Template

Thank you for your interest in contributing to this project! We welcome contributions from everyone and are grateful for every pull request, issue report, or suggestion.

## Table of Contents

- [Code of Conduct](#code-of-conduct)
- [Getting Started](#getting-started)
- [Development Setup](#development-setup)
- [Making Contributions](#making-contributions)
- [Coding Standards](#coding-standards)
- [Testing Guidelines](#testing-guidelines)
- [Commit Message Guidelines](#commit-message-guidelines)
- [Pull Request Process](#pull-request-process)
- [Issue Guidelines](#issue-guidelines)
- [Community](#community)

## Code of Conduct

This project adheres to a [Code of Conduct](CODE_OF_CONDUCT.md). By participating, you are expected to uphold this code. Please report unacceptable behavior to [conduct@yourproject.com](mailto:conduct@yourproject.com).

## Getting Started

### Prerequisites

Before you begin, ensure you have:

- **C++23** compatible compiler (GCC 13+, Clang 16+, MSVC 2022+)
- **CMake 3.16+**
- **Git** for version control
- **Python 3.7+** for pre-commit hooks
- **pre-commit** tool: `pip install pre-commit`

### Fork and Clone

1. Fork the repository on GitHub
2. Clone your fork locally:
   ```bash
   git clone https://github.com/yourusername/project_template.git
   cd project_template
   git remote add upstream https://github.com/originalowner/project_template.git
   ```

### Development Setup

1. **Install pre-commit hooks**:
   ```bash
   pre-commit install
   ```

2. **Build the project**:
   ```bash
   mkdir build && cd build
   cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug -DENABLE_TESTING=ON ..
   ninja
   ```

3. **Run tests** to ensure everything works:
   ```bash
   ctest --verbose
   ```

## Making Contributions

### Types of Contributions

We welcome several types of contributions:

- 🐛 **Bug fixes** - Fix issues in the codebase
- ✨ **New features** - Add new functionality
- 📚 **Documentation** - Improve or add documentation
- 🎨 **Code quality** - Refactoring, style improvements
- 🧪 **Tests** - Add or improve test coverage
- 🔧 **Infrastructure** - Build system, CI/CD improvements

### Before You Start

1. **Check existing issues** - Look for existing issues related to your contribution
2. **Create an issue** - For significant changes, create an issue to discuss the approach
3. **Get feedback** - For large features, get feedback on the design before implementing

## Coding Standards

### C++ Guidelines

We follow modern C++ best practices:

#### Code Style
- **C++23** standard compliance
- **100-character line limit**
- **2-space indentation** (no tabs)
- **PascalCase** for classes and structs
- **camelCase** for functions and variables
- **snake_case** for file names
- **m_** prefix for member variables

#### Example:
```cpp
class ExampleClass {
public:
  void doSomething(int parameter);
  int getSomething() const { return m_value; }

private:
  int m_value = 0;
  std::string m_name;
};
```

#### Best Practices
- Use **const correctness** throughout
- Prefer **smart pointers** over raw pointers
- Use **RAII** for resource management
- Write **self-documenting code** with clear names
- Add **comments for complex logic** only

### Formatting

We use **clang-format** for automatic code formatting:

```bash
# Format all source files
find . -name "*.cpp" -o -name "*.h" | xargs clang-format -i

# Check formatting
git diff --name-only HEAD~1 | grep -E '\.(cpp|h)$' | xargs clang-format --dry-run -Werror
```

### Static Analysis

We use **clang-tidy** for static analysis:

```bash
# Run clang-tidy manually
pre-commit run --hook-stage manual clang-tidy

# Run on specific files
clang-tidy src/file.cpp -p build/
```

## Testing Guidelines

### Test Requirements

- **All new features** must have tests
- **All bug fixes** should have regression tests
- **Maintain or improve** test coverage
- **Tests should be fast** and deterministic

### Test Structure

```cpp
#include <gtest/gtest.h>
#include "your_module.h"

class YourModuleTest : public ::testing::Test {
protected:
  void SetUp() override {
    // Setup code
  }
  
  void TearDown() override {
    // Cleanup code
  }
  
  YourModule module;
};

TEST_F(YourModuleTest, ShouldDoSomethingCorrectly) {
  // Arrange
  auto input = createTestInput();
  
  // Act
  auto result = module.doSomething(input);
  
  // Assert
  EXPECT_EQ(expected_value, result);
  ASSERT_TRUE(result.isValid());
}
```

### Running Tests

```bash
# Run all tests
cd build && ctest --verbose

# Run specific test
ctest -R TestName --verbose

# Run with coverage (Linux)
cmake -DENABLE_COVERAGE=ON ..
make && make coverage
```

## Commit Message Guidelines

We use **Conventional Commits** for clear commit history:

### Format
```
type(scope): description

[optional body]

[optional footer]
```

### Types
- **feat**: New feature
- **fix**: Bug fix
- **docs**: Documentation changes
- **style**: Code style changes (formatting, etc.)
- **refactor**: Code refactoring
- **perf**: Performance improvements
- **test**: Test changes
- **build**: Build system changes
- **ci**: CI configuration changes
- **chore**: Maintenance tasks

### Examples
```bash
feat(ui): add dark mode toggle
fix(parser): handle empty input correctly
docs(readme): update installation instructions
refactor(core): simplify error handling logic
test(utils): add tests for string utilities
```

### Guidelines
- Use **present tense** ("add" not "added")
- Use **imperative mood** ("move cursor to..." not "moves cursor to...")
- **Capitalize** the first letter
- **No period** at the end
- **72 character limit** for the first line
- **Include issue number** if applicable: `fixes #123`

## Pull Request Process

### Before Submitting

1. **Rebase** your branch on the latest main:
   ```bash
   git fetch upstream
   git rebase upstream/main
   ```

2. **Run pre-commit hooks**:
   ```bash
   pre-commit run --all-files
   ```

3. **Run tests**:
   ```bash
   cd build && ctest --verbose
   ```

4. **Update documentation** if needed

### PR Guidelines

- **Clear title** following conventional commit format
- **Detailed description** explaining the changes
- **Link related issues** using "fixes #123" or "relates to #123"
- **Include screenshots** for UI changes
- **Keep PRs focused** - one feature/fix per PR
- **Write descriptive commit messages**

### PR Template

Our PR template includes:
- Description and motivation
- Type of change
- Testing performed
- Checklist for code quality
- Documentation updates

### Review Process

1. **Automated checks** must pass (CI, formatting, tests)
2. **Code review** by at least one maintainer
3. **Address feedback** promptly and professionally
4. **Squash commits** if requested before merge

## Issue Guidelines

### Bug Reports

When reporting bugs, include:
- **Clear description** of the issue
- **Steps to reproduce** the problem
- **Expected vs actual** behavior
- **Environment information** (OS, compiler, version)
- **Code samples** or error messages
- **Screenshots** if applicable

### Feature Requests

When requesting features:
- **Explain the use case** and motivation
- **Describe the proposed solution**
- **Consider alternatives** you've evaluated
- **Estimate complexity** and impact
- **Provide mockups** for UI features

### Questions

For questions:
- **Search existing issues** first
- **Check documentation** and examples
- **Provide context** about what you're trying to achieve
- **Include relevant code** samples

## Development Workflow

### Typical Workflow

1. **Create issue** (for significant changes)
2. **Create feature branch**: `git checkout -b feature/amazing-feature`
3. **Make changes** following guidelines
4. **Write tests** for new functionality
5. **Run pre-commit**: `pre-commit run --all-files`
6. **Commit changes**: `git commit -m "feat: add amazing feature"`
7. **Push branch**: `git push origin feature/amazing-feature`
8. **Create pull request** using the template
9. **Address review feedback**
10. **Merge after approval**

### Branch Naming

- **feature/**: New features (`feature/user-authentication`)
- **fix/**: Bug fixes (`fix/memory-leak-in-parser`)
- **docs/**: Documentation (`docs/update-api-reference`)
- **refactor/**: Code refactoring (`refactor/simplify-config-system`)
- **test/**: Test improvements (`test/add-integration-tests`)

## Community

### Getting Help

- **GitHub Discussions** for general questions
- **GitHub Issues** for bug reports and feature requests
- **Discord/Slack** (if applicable) for real-time chat

### Code Reviews

- Be **respectful and constructive**
- Focus on **code, not the person**
- **Explain reasoning** behind suggestions
- **Acknowledge good work** and improvements
- **Be responsive** to feedback

### Recognition

We recognize contributors in:
- **Release notes** for significant contributions
- **Contributors list** in README
- **Special mentions** for exceptional help

## Release Process

### Versioning

We use [Semantic Versioning](https://semver.org/):
- **MAJOR**: Breaking changes
- **MINOR**: New features (backwards compatible)
- **PATCH**: Bug fixes (backwards compatible)

### Release Workflow

1. Update version numbers
2. Update CHANGELOG.md
3. Create release PR
4. Tag release after merge
5. Automated build and publish

## Getting Started Checklist

- [ ] Fork and clone the repository
- [ ] Install development dependencies
- [ ] Set up pre-commit hooks
- [ ] Build the project successfully
- [ ] Run tests to ensure everything works
- [ ] Read through coding standards
- [ ] Look at existing issues for good first contributions

## Thank You!

Your contributions make this project better for everyone. Whether it's code, documentation, bug reports, or feature suggestions, we appreciate your involvement in the community.

---

**Questions?** Feel free to reach out by creating an issue or starting a discussion. We're here to help!

**Happy coding!** 🚀
