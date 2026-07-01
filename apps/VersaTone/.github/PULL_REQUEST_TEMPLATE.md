---
name: "🚀 Pull Request"
about: "Submit a contribution to the project"
title: ""
labels: []
assignees: []

---

## Pull Request

### Description
**What does this PR do?**
Please provide a clear and concise description of what this pull request accomplishes.

### Related Issues
**Related Issues**
- Fixes #(issue number)
- Relates to #(issue number)
- Closes #(issue number)

### Type of Change
**What type of change does this PR introduce?**
- [ ] Bug fix (non-breaking change which fixes an issue)
- [ ] New feature (non-breaking change which adds functionality)
- [ ] Breaking change (fix or feature that would cause existing functionality to not work as expected)
- [ ] Performance improvement
- [ ] Code refactoring (no functional changes)
- [ ] Documentation update
- [ ] Build/CI changes
- [ ] Test improvements
- [ ] Other: [please describe]

## Changes Made

### Summary of Changes
**What specific changes were made?**
- Change 1: Description...
- Change 2: Description...
- Change 3: Description...

### Files Modified
**Key files that were changed:**
- `src/file1.cpp`: Description of changes
- `include/file1.h`: Description of changes
- `docs/documentation.md`: Description of changes

### API Changes (if applicable)
**Breaking Changes:**
List any breaking changes and migration instructions:
```cpp
// Old API
oldFunction(param);

// New API
newFunction(param, additionalParam);
```

## Testing

### Test Coverage
**How has this been tested?**
- [ ] Unit tests added/updated
- [ ] Integration tests added/updated
- [ ] Manual testing performed
- [ ] Existing tests still pass

### Test Details
**Specific testing performed:**
- Test case 1: Description and result...
- Test case 2: Description and result...
- Performance testing: [if applicable]

### Test Environment
**Testing environment:**
- OS: [e.g. Windows 11, Ubuntu 22.04, macOS 14.0]
- Compiler: [e.g. MSVC 2022, GCC 12, Clang 15]
- Build type: [e.g. Debug, Release]

## Quality Assurance

### Code Quality
**Code quality checklist:**
- [ ] Code follows project style guidelines
- [ ] Code has been formatted with clang-format
- [ ] Static analysis (clang-tidy) passes
- [ ] No compiler warnings introduced
- [ ] Memory leaks checked (if applicable)

### Documentation
**Documentation updates:**
- [ ] Code comments added/updated
- [ ] API documentation updated
- [ ] User documentation updated
- [ ] CHANGELOG.md updated (if applicable)

## Performance Impact

### Performance Considerations
**Performance impact:**
- [ ] No performance impact
- [ ] Performance improvement (please quantify)
- [ ] Minor performance impact (acceptable)
- [ ] Significant performance impact (needs discussion)

**Benchmarks (if applicable):**
```
Before: [benchmark results]
After:  [benchmark results]
```

## Compatibility

### Backwards Compatibility
**Backwards compatibility:**
- [ ] Fully backwards compatible
- [ ] Backwards compatible with deprecation warnings
- [ ] Breaking change (major version bump required)

### Platform Compatibility
**Platform testing:**
- [ ] Windows
- [ ] Linux
- [ ] macOS
- [ ] Other: [specify]

## Review Guidelines

### For Reviewers
**Areas that need special attention:**
- Focus on: [specific areas where reviewer attention is needed]
- Security considerations: [any security implications]
- Performance implications: [any performance considerations]

### Deployment Notes
**Special deployment considerations:**
- [ ] No special deployment needed
- [ ] Database migration required
- [ ] Configuration changes required
- [ ] Dependencies updated

## Additional Context

### Screenshots (if applicable)
**Visual changes:**
[Add screenshots here if the PR includes UI changes]

### Additional Notes
**Additional information:**
Any additional information that reviewers should know about this PR.

---

## Checklist
**Before submitting this PR, please make sure:**
- [ ] I have performed a self-review of my own code
- [ ] I have commented my code, particularly in hard-to-understand areas
- [ ] I have made corresponding changes to the documentation
- [ ] My changes generate no new warnings
- [ ] I have added tests that prove my fix is effective or that my feature works
- [ ] New and existing unit tests pass locally with my changes
- [ ] I have checked my code with static analysis tools
- [ ] I have rebased my branch on the latest main/develop branch
- [ ] I have signed the Contributor License Agreement (if required)

**Commit Message:**
- [ ] My commit messages follow the conventional commit format
- [ ] Each commit represents a logical unit of change
- [ ] Commit messages are descriptive and explain the "why" not just the "what"
