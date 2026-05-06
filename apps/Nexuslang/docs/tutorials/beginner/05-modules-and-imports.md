# Tutorial 5: Modules and Imports

**Time:** ~30 minutes  
**Prerequisites:** [Variables, Functions, and Control Flow](02-variables-functions-control-flow.md)

---

## Part 1 — What Is a Module?

A **module** is a `.nlpl` file that exposes functions, classes, or variables
for other programs to use.  Modules let you split large programs into
manageable files and share code across projects.

---

## Part 2 — Importing a Built-in Module

NLPL ships with a standard library.  Import a module with `import`:

```nexuslang
import math

set pi      to math.pi
set root2   to math.sqrt with 2.0
set rounded to math.round with 3.7

print text "pi = "      plus convert pi      to string
print text "sqrt(2) = " plus convert root2   to string
print text "round(3.7) = " plus convert rounded to string
```

### Importing Specific Names

Use `import … from` to bring names directly into scope:

```nexuslang
import sqrt and round and pi from math

print text convert (sqrt with 9.0) to string   # 3.0
```

---

## Part 3 — Standard Library Overview

| Module | Purpose |
|--------|---------|
| `math` | Arithmetic, trigonometry, logarithms, constants |
| `string` | String search, split, trim, replace, format |
| `io` | File read/write, directory operations |
| `collections` | Stack, queue, set, ordered dict |
| `network` | HTTP requests, socket operations |
| `system` | Environment variables, process control, time |

---

## Part 4 — Writing Your Own Module

Create a file `greetings.nlpl`:

```nexuslang
# greetings.nlpl
# A small module for greeting messages.

function hello with name as String returns String
    return "Hello, " plus name plus "!"
end

function farewell with name as String returns String
    return "Goodbye, " plus name plus ". See you soon."
end
```

Then use it from `main.nlpl` in the same directory:

```nexuslang
# main.nlpl
import greetings

print text greetings.hello with "Alice"
print text greetings.farewell with "Bob"
```

---

## Part 5 — Modules in Subdirectories

Organise related modules into folders.  Given this layout:

```
project/
  main.nlpl
  utils/
    math_helpers.nlpl
    text_helpers.nlpl
```

`math_helpers.nlpl`:

```nexuslang
function clamp with value as Float and low as Float and high as Float returns Float
    if value is less than low
        return low
    if value is greater than high
        return high
    return value
end
```

`main.nlpl`:

```nexuslang
import utils.math_helpers

set clamped to utils.math_helpers.clamp with 150.0 and 0.0 and 100.0
print text convert clamped to string    # 100.0
```

---

## Part 6 — Module-Level Variables and Constants

You can export simple values from a module:

```nexuslang
# constants.nlpl
set MAX_RETRIES to 3
set DEFAULT_TIMEOUT to 30.0
set APP_NAME to "MyApp"
```

```nexuslang
import constants

print text constants.APP_NAME
print text convert constants.MAX_RETRIES to string
```

---

## Part 7 — Circular Imports

NLPL detects circular module dependencies and raises an error.  If
`module_a` imports `module_b` and `module_b` imports `module_a`, one of
them needs to be restructured.

The fix is usually to extract the shared code into a third module that
both can import without creating a cycle.

---

## Summary

| Concept | Syntax |
|---------|--------|
| Import entire module | `import module_name` |
| Import specific names | `import name1 and name2 from module` |
| Use module member | `module_name.function_name with args` |
| Nested module | `import folder.module_name` |

**You have completed the Beginner Track!**

**Next steps:**
- [Intermediate Track: Generics and the Type System](../intermediate/01-generics-and-type-system.md)
- [Cookbook: Common Tasks](../../cookbook/common-tasks.md)
