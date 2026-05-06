# NexusLang Error Handling & Debugging Experience Assessment

**Date:** November 20, 2025
**Status:** Current State Analysis
**Purpose:** Honest assessment of NLPL's error handling capabilities and debugging experience for attracting non-programmer users

---

## Executive Summary

**Current Rating: 7.5-8/10 for Non-Programmers**

NLPL's error handling is **significantly better** than traditional programming languages and is **80% of the way** to the "grammar mistake" ease-of-use goal. The technical foundation is solid, with clear visual indicators, smart typo detection, and helpful suggestions. However, some technical jargon remains and additional user-friendly features are needed to reach the 95%+ target.

### The Core Question

**Will debugging NexusLang be pure hell?** **No, definitely not.**

**Will it be as straightforward as fixing typos/grammar?** **Almost, but not quite yet.**

---

## 1. Error Handling Architecture

### Multi-Layered Detection System

NLPL implements comprehensive error detection across all compilation/interpretation phases:

#### **1.1 Lexical Errors (Lexer)**
- **Location:** `src/nlpl/parser/lexer.py:743-755`
- **Detects:**
 - Invalid characters
 - Unterminated strings
 - Indentation errors (Python-style blocks)
- **Context:** Stores source line, line number, and column for each token

#### **1.2 Syntax Errors (Parser)**
- **Location:** `src/nlpl/parser/parser.py:36-66`
- **Detects:**
 - Missing keywords/tokens
 - Unexpected tokens
 - Malformed statements
- **Features:** Provides expected vs. got information, context-specific suggestions

#### **1.3 Type Errors (Type Checker)**
- **Location:** `src/nlpl/typesystem/typechecker.py:27-465`
- **Detects:**
 - Type mismatches in assignments
 - Incompatible types in operations
 - Wrong number/types of function arguments
 - Invalid return types

#### **1.4 Runtime Errors (Interpreter)**
- **Location:** `src/nlpl/interpreter/interpreter.py` (multiple locations)
- **Detects:**
 - Division by zero
 - Index out of bounds
 - Key not found in dictionaries
 - Undefined variables/functions
 - Module import errors

### Error Class Hierarchy

**Base:** `NxlError` (`src/nlpl/errors.py:10-39`)
- Stores: message, line number, column, source line, error type
- Formats with visual caret pointer (^) showing exact location

**Specialized Classes:**

1. **`NxlSyntaxError`** (lines 42-63)
 - Includes expected vs. got tokens
 - Provides contextual suggestions
 - Example: " Suggestion: Make sure to close all blocks with 'end'"

2. **`NxlRuntimeError`** (lines 66-92)
 - Shows stack trace with numbered frames
 - Displays local variables at error point (truncated to 50 chars)

3. **`NxlNameError`** (lines 95-120)
 - **Most user-friendly feature:** "Did you mean?" suggestions
 - Uses fuzzy string matching (difflib) with 0.6 similarity cutoff
 - Shows top 3 similar names
 - Example: "conter" suggests "counter"

4. **`NxlTypeError`** (lines 123-140)
 - Shows expected vs. got types clearly
 - Example: "Expected type: Integer / Got type: String"

---

## 2. Current Strengths

### 2.1 Visual Formatting Excellence

**Example Error Message:**
```
Name Error: Name 'conter' is not defined
 at line 5, column 11

 5 | print text conter
 ^

 Did you mean: 'counter'?
```

**Features:**
- Line numbers with source code context
- Caret pointer (^) showing exact error location
- Emoji indicators () for suggestions
- Bullet points (•) for multiple suggestions
- Proper indentation and spacing
- Shows 2 lines before and after error for context

### 2.2 Smart Typo Detection

**Implementation:** `src/nlpl/errors.py:143-153`
- Uses `difflib.get_close_matches()` for fuzzy matching
- 60% similarity threshold (0.6 cutoff)
- Works for variable names, function names, class names
- Shows top 3 most likely corrections

**Real-world Example:**
- User types: `conter`
- System suggests: `counter`
- Works like spell-check in word processors

### 2.3 Context-Aware Suggestions

**Implementation:** `src/nlpl/errors.py:200-225`

Current suggestions include:
- **missing_end:** "Make sure to close all blocks (if, while, for, function, class) with 'end'"
- **undefined_variable:** "Declare variables with 'set <name> to <value>' before using them"
- **undefined_function:** "Define functions with 'function <name>' or import them from a module"
- **type_mismatch:** "Check that you're using compatible types in operations"
- **division_by_zero:** "Check that the divisor is not zero"
- **index_out_of_range:** "Ensure the index is within the list/array bounds"

### 2.4 Natural Language Error Messages

Uses plain English instead of programmer jargon:
- "Name 'conter' is not defined" (not "NameError in global scope")
- "Did you mean: 'counter'?" (conversational)
- Shows what's wrong and where, not technical implementation details

### 2.5 Stack Traces with Context

For runtime errors:
- Displays numbered call stack frames
- Shows local variables at error point
- Values truncated to prevent overwhelming output
- Human-readable format

---

## 3. Current Weaknesses

### 3.1 Technical Jargon Remains

**Problem:** Some error messages still use programmer terminology

**Examples:**
- Says "iterable" instead of "list you can loop through"
- Says "boolean" instead of "true/false value"
- "Type mismatch" could be "You're mixing things that don't work together"

**Impact:** Non-programmers may not understand what "iterable" or "boolean" mean

**Location:** `src/nlpl/errors.py:211-223` (suggestion messages)

### 3.2 No Documentation Links

**Problem:** Errors don't provide "Learn More" links

**Missing:**
- Links to documentation explaining the concept
- Examples showing the correct way to do something
- Related tutorials or guides

**Example of what's needed:**
```
 Did you mean: 'counter'?
 Learn more about variables: https://nlpl.dev/docs/variables
```

### 3.3 No "Common Fixes" Examples

**Problem:** Errors tell you what's wrong but not how to fix it

**Current:**
```
Name Error: Name 'conter' is not defined
 Did you mean: 'counter'?
```

**Should be:**
```
Name Error: Name 'conter' is not defined
 Did you mean: 'counter'?

Common fix:
 Wrong: print text conter
 Right: print text counter
```

### 3.4 Stops at First Runtime Error

**Problem:** Interpreter halts on first runtime error, doesn't collect multiple errors

**Impact:**
- User fixes one error, runs again, finds another error
- Frustrating cycle of fix-run-error-repeat
- Unlike text editors that show all spelling mistakes at once

**Desired behavior:**
```
Found 3 issues:
 1. Line 5: Name 'conter' not defined Did you mean 'counter'?
 2. Line 7: Missing 'end' to close if block
 3. Line 12: Can't add text to number
```

### 3.5 No Beginner Mode

**Problem:** No verbose explanations for newcomers

**Current:** Technical but friendly
**Needed:** Educational explanations

**Example of beginner mode:**
```
Name Error (What this means):
You tried to use 'conter', but I don't know what that is yet.

Think of it like this:
You asked me to grab a box labeled 'conter', but I can't find
any box with that label in the room.

What to do:
Create the variable first: set conter to 0
Or did you mean: 'counter'?

 Variables are like labeled boxes that hold information.
 You need to create them before you can use them.

 Learn more: https://nlpl.dev/docs/variables
```

---

## 4. User Experience Comparison

### Traditional Programming (C++, Java): 3/10 for Beginners
```
error: 'conter' was not declared in this scope
 5 | std::cout << conter;
 | ^~~~~~
```
- Cryptic messages
- Technical jargon
- No suggestions
- Scary for non-programmers

### Python: 6/10 for Beginners
```
NameError: name 'conter' is not defined
 File "test.py", line 5, in <module>
```
- Clearer than C++
- Still technical
- No suggestions or help
- Just tells you what's wrong

### NexusLang Today: 7.5-8/10 for Beginners
```
Name Error: Name 'conter' is not defined
 at line 5, column 11

 5 | print text conter
 ^

 Did you mean: 'counter'?
```
- Clear, visual
- Shows exact location
- Helpful suggestions
- Natural language (mostly)

### Target Goal: 9-10/10
```
Name Error: I don't recognize 'conter'
 at line 5, column 11

 5 | print text conter
 ^

 Did you mean: 'counter'?

Common fix:
 Wrong: print text conter
 Right: print text counter

Why this happened:
 You need to create a variable before using it.
 Set it up first: set counter to 0

 Learn more about variables: https://nlpl.dev/docs/variables
```

---

## 5. Who Will Succeed vs. Struggle

### Who Will Succeed Today

**People comfortable with:**
- Basic logic (if/then thinking)
- Following patterns
- Reading error messages carefully
- Spreadsheet formulas
- Trial and error learning

**Example users:**
- Writers with logical thinking skills
- Game designers who understand rules
- Spreadsheet power users
- People who don't fear error messages
- Those willing to learn incrementally

### Who Might Struggle

**People expecting:**
- Zero learning curve
- No error messages ever
- Mind-reading capabilities
- Instant success without reading feedback

**People who:**
- Panic at any error message
- Won't read explanations
- Expect it to "just work" magically
- Have no patience for iteration

### Sweet Spot Target Audience

**Best fit for:**
- Creative professionals who understand systems
- Hobby programmers frustrated by syntax
- Domain experts (scientists, designers, educators) wanting to automate
- People who can write, "If the user clicks the button, show a message"
- Those who understand cause and effect

---

## 6. Critical Improvements Needed

### Priority 1: Replace Technical Jargon (HIGH PRIORITY)

**File:** `src/nlpl/errors.py:211-223`

**Current problems:**
```python
"type_mismatch": "Check that you're using compatible types in operations"
"undefined_variable": "Declare variables with 'set <name> to <value>' before using them"
```

**Should be:**
```python
"type_mismatch": "You're trying to mix things that don't work together (like adding a number to text). Make sure both sides match."
"undefined_variable": "You're using a name I don't recognize yet. Create it first with: set <name> to <value>"
"missing_end": "Your if/while/for/function block is missing its closing 'end'. Every block needs an 'end' to mark where it stops."
"division_by_zero": "You're trying to divide by zero, which doesn't work (even calculators can't do this!). Make sure the bottom number isn't zero."
```

**Additional jargon to fix:**
- "iterable" "list or collection you can loop through"
- "boolean" "true or false value"
- "index out of range" "you're asking for item #10 but the list only has 5 items"
- "compatible types" "matching types (both numbers, or both text)"

### Priority 2: Add "Common Fixes" Examples (HIGH PRIORITY)

**File:** `src/nlpl/errors.py` (new function)

**Implementation suggestion:**
```python
def get_fix_example(error_type: str, context: dict) -> Optional[str]:
 """Get example fix for common errors."""
 examples = {
 "undefined_variable": {
 "wrong": "print text {name}",
 "right": "set {name} to \"some value\"\nprint text {name}"
 },
 "missing_end": {
 "wrong": "if condition is true\n do something",
 "right": "if condition is true\n do something\nend"
 },
 "type_mismatch": {
 "wrong": "set result to 5 + \"hello\"",
 "right": "set result to 5 + 10 # Both numbers\n# OR\nset result to \"hello\" + \" world\" # Both text"
 }
 }
 return examples.get(error_type)
```

**Update error formatting to include:**
```python
if fix_example:
 base += f"\n\n Common fix:"
 base += f"\n Wrong: {fix_example['wrong']}"
 base += f"\n Right: {fix_example['right']}"
```

### Priority 3: Create Beginner Mode (MEDIUM PRIORITY)

**Implementation:** Add CLI flag `--beginner-mode` or `--explain`

**Features:**
1. Longer, educational explanations
2. Real-world analogies ("Variables are like labeled boxes")
3. "What this means" section
4. "Why this happened" section
5. "How to fix it" step-by-step

**Example implementation:**
```python
class NxlError(Exception):
 def __init__(self, ..., beginner_mode: bool = False):
 self.beginner_mode = beginner_mode
 ...

 def _format_error(self) -> str:
 base = self._format_basic_error()

 if self.beginner_mode:
 base += self._add_beginner_explanation()
 base += self._add_analogy()
 base += self._add_fix_steps()

 return base
```

### Priority 4: Add Documentation Links (MEDIUM PRIORITY)

**Files to modify:**
- `src/nlpl/errors.py` - add URL field to error classes
- Create documentation URL map

**Implementation:**
```python
ERROR_DOCS = {
 "undefined_variable": "https://nlpl.dev/docs/variables",
 "type_mismatch": "https://nlpl.dev/docs/types",
 "missing_end": "https://nlpl.dev/docs/blocks",
 "undefined_function": "https://nlpl.dev/docs/functions",
}

def _format_error(self) -> str:
 ...
 if self.error_type in ERROR_DOCS:
 base += f"\n\n Learn more: {ERROR_DOCS[self.error_type]}"
```

### Priority 5: Collect Multiple Errors (LOW PRIORITY)

**Current behavior:** Interpreter stops on first runtime error

**Desired behavior:** Collect all errors and display at end

**Implementation approach:**
1. Add error collection mode to interpreter
2. Continue execution where possible after errors
3. Display summary of all errors at end

**Challenges:**
- Some errors prevent continued execution (e.g., undefined function)
- Need to determine which errors are fatal
- More complex to implement

**Recommendation:** Start with syntax/type errors (parser/type checker already collect multiple errors), add runtime error collection later

---

## 7. Implementation Roadmap

### Phase 1: Quick Wins (1-2 weeks)
1. **Replace technical jargon** in existing error messages
 - Update `src/nlpl/errors.py:211-223`
 - Review all error messages in lexer, parser, type checker, interpreter
 - Create glossary of natural language replacements

2. **Add common fix examples**
 - Implement `get_fix_example()` function
 - Add 10-15 most common error examples
 - Integrate into error formatting

### Phase 2: Enhanced UX (2-3 weeks)
3. **Create beginner mode**
 - Add CLI flag support
 - Write educational explanations for top 20 errors
 - Create analogy library
 - Add step-by-step fix instructions

4. **Add documentation links**
 - Create error documentation pages
 - Map error types to URLs
 - Integrate links into error messages

### Phase 3: Advanced Features (3-4 weeks)
5. **Implement multiple error collection**
 - Design error collection system
 - Determine fatal vs. non-fatal errors
 - Implement error summary display
 - Add error filtering options

### Phase 4: Polish & Testing (1-2 weeks)
6. **User testing with non-programmers**
 - Create test scenarios
 - Record user reactions
 - Iterate based on feedback
 - Measure comprehension and fix success rate

---

## 8. Test Coverage Analysis

NLPL has comprehensive error testing:

**Test Files:**
- `tests/test_errors.py` - General error handling
- `tests/test_comprehensive_errors.py` - Detailed error reporting tests
- `test_programs/test_*.nlpl` - Various error scenarios

**Tested Categories:**
- Lexer errors (invalid characters, unterminated strings)
- Parser errors (missing tokens, invalid syntax)
- Type errors (type mismatches, wrong argument types)
- Runtime errors (division by zero, index errors, undefined names)
- Module errors (missing imports, undefined attributes)

**Gaps in testing:**
- Need user comprehension tests (do non-programmers understand the messages?)
- Need fix success rate tests (can users fix errors based on messages?)
- Need A/B testing (technical vs. beginner-friendly messages)

---

## 9. Marketing Positioning

### Current Honest Positioning

**DO say:**
- "Clearer error messages than traditional programming"
- "Errors point to exactly what's wrong and suggest fixes"
- "Like having a helpful tutor looking over your shoulder"
- "Debugging is easier than most programming languages"
- "Smart suggestions help you fix typos and mistakes"

**DON'T say (yet):**
- "Never debug again"
- "Errors are impossible"
- "As easy as spell-check" (not quite there yet)
- "No learning curve"
- "Perfect for complete beginners" (needs qualifiers)

### Target Messaging

**For Creative Professionals:**
> "NLPL gives you clear, helpful feedback when something's not quite right - like a copyeditor for your code. Errors show you exactly where the issue is and suggest fixes, so you spend less time stuck and more time creating."

**For Domain Experts:**
> "When NexusLang encounters an issue, it explains what went wrong in plain language and points to the exact location. Smart suggestions help you quickly fix typos and common mistakes, similar to how spell-check works in documents."

**For Hobby Programmers:**
> "Frustrated by cryptic error messages in other languages? NexusLang shows you exactly what's wrong, where it's wrong, and suggests fixes - all in natural language you can actually understand."

### Comparison Chart for Marketing

| Feature | Traditional Languages | Python | NexusLang |
|---------|----------------------|---------|------|
| Visual error location | | | |
| Natural language messages | | | |
| "Did you mean?" suggestions | | | |
| Shows expected vs. got | | | |
| Context-aware hints | | | |
| Beginner-friendly | | | (in progress) |

Legend: Yes, Partial, No

---

## 10. Success Metrics

### Quantitative Metrics

**To measure error handling effectiveness:**

1. **Error Fix Rate**
 - Metric: % of errors fixed on first attempt after reading message
 - Target: >70% for common errors (undefined variable, typos, missing end)
 - Current: Unknown (needs testing)

2. **Time to Fix**
 - Metric: Average time from error to successful fix
 - Target: <2 minutes for common errors
 - Current: Unknown (needs testing)

3. **Errors per Session**
 - Metric: Average number of errors encountered per coding session
 - Target: Decreasing over time as user learns
 - Track: First session vs. 10th session

4. **Abandonment Rate**
 - Metric: % of users who quit after hitting errors
 - Target: <15% in first session
 - Current: Unknown (needs user testing)

### Qualitative Metrics

**User feedback questions:**

1. "How well did you understand what went wrong?" (1-5 scale)
 - Target: 4+ average

2. "How helpful was the error message in fixing the issue?" (1-5 scale)
 - Target: 4+ average

3. "How frustrated did you feel?" (1-5 scale, lower is better)
 - Target: <2 average

4. "Would you recommend NexusLang to a non-programmer friend?" (Yes/No)
 - Target: >70% yes

---

## 11. Competitive Analysis

### How NexusLang Compares to Natural Language Attempts

**Inform (1960s-1970s):**
- Error handling: Minimal, technical
- User friendliness: Poor
- NexusLang advantage: Significantly better visual feedback and suggestions

**COBOL:**
- Error handling: Verbose but technical
- User friendliness: Moderate for era, dated now
- NexusLang advantage: Modern UX, better suggestions

**AppleScript:**
- Error handling: Better than most, still cryptic
- User friendliness: Moderate
- NexusLang advantage: Smarter suggestions, better visual formatting

**Modern comparisons:**

**Scratch (visual programming):**
- Error handling: Prevents many errors through UI
- User friendliness: Excellent (can't make syntax errors)
- NexusLang advantage: More powerful, text-based flexibility
- NexusLang disadvantage: Scratch prevents errors; NexusLang detects and explains

**English (natural language):**
- Error handling: Context-dependent, ambiguity allowed
- User friendliness: Native speakers = excellent
- NexusLang challenge: Programming requires precision that natural language doesn't
- This is the fundamental tension: precision vs. naturalness

---

## 12. Fundamental Challenges

### The Precision vs. Naturalness Paradox

**Challenge:** Programming requires precision; natural language is inherently ambiguous.

**Example:**
```
Natural language: "If the user is logged in, show their profile"
- What if login status is unknown?
- What if profile doesn't exist?
- What if network is down?

Programming requires: Explicit handling of all edge cases
```

**NLPL's approach:**
- Allow natural phrasing for common cases
- Require explicit handling for edge cases
- Provide clear errors when assumptions fail

**Error handling implication:**
- Can't eliminate all errors (precision requirements)
- Can make errors maximally helpful
- Can guide users to complete specifications

### The Learning Curve Reality

**Truth:** There will always be a learning curve for programming concepts.

**Concepts that require learning (regardless of syntax):**
- Variables and state
- Conditional logic
- Loops and iteration
- Functions and abstraction
- Data structures
- Types and type compatibility
- Scope and lifetime
- Error handling itself

**What NexusLang can do:**
- Make error messages educational
- Teach concepts through feedback
- Provide examples and analogies
- Guide users toward correct understanding

**What NexusLang cannot do:**
- Make programming concepts disappear
- Eliminate the need to think logically
- Read the user's mind
- Automatically fix all mistakes

---

## 13. Recommendations

### For Development Team

1. **Prioritize jargon removal** - Quick win with high impact
2. **Add fix examples** - Users need to see correct patterns
3. **Implement beginner mode** - Differentiator vs. competitors
4. **User test with non-programmers** - Critical for validation
5. **Create error documentation** - Link from error messages

### For Marketing Team

1. **Be honest about current state** - Avoid overpromising
2. **Target realistic user segments** - Not complete beginners (yet)
3. **Emphasize improvement over traditional languages** - Not perfection
4. **Collect user testimonials** - Real experiences trump claims
5. **Show before/after comparisons** - C++ error vs. NexusLang error

### For Documentation Team

1. **Create "Understanding Errors" guide** - Explain error types
2. **Build error message reference** - Searchable error catalog
3. **Write error-driven tutorials** - Learn by fixing
4. **Create video walkthroughs** - Show debugging process
5. **Maintain FAQ of common errors** - User-contributed

---

## 14. Conclusion

### Current State Summary

**NLPL's error handling is genuinely good.** It represents a significant improvement over traditional programming languages and shows thoughtful design focused on user experience.

**Strengths:**
- Visual, clear error location indicators
- Smart typo detection with suggestions
- Natural language (mostly)
- Comprehensive error coverage
- Context-aware hints

**Remaining gaps:**
- Some technical jargon
- No documentation links
- No beginner mode
- Stops on first runtime error
- Missing fix examples

### Path Forward

**To reach 95%+ ease-of-use goal:**
1. Implement Priority 1-2 improvements (high priority items)
2. Test with real non-programmer users
3. Iterate based on feedback
4. Add Priority 3-4 improvements (medium priority items)
5. Create comprehensive error documentation

**Timeline estimate:**
- Phase 1 (Quick wins): 1-2 weeks
- Phase 2 (Enhanced UX): 2-3 weeks
- Phase 3 (Advanced features): 3-4 weeks
- Phase 4 (Polish & testing): 1-2 weeks
- **Total: 7-11 weeks to reach target**

### Final Verdict

**Should NexusLang attract users now?**

**Yes, with appropriate messaging:**
- Target: People comfortable with logical thinking
- Promise: Easier than traditional coding, still has learning curve
- Positioning: "Like writing with autocorrect for logic"
- Honesty: Not "zero errors possible" but "errors are helpful"

**The debugging experience will not be "pure hell."**

It's already quite good and with the recommended improvements, it can become excellent - genuinely competitive with visual programming tools while maintaining the power and flexibility of text-based code.

**The key:** Set realistic expectations, target the right audience, and continue improving based on real user feedback.

---

## Appendix: Error Message Examples

### Example 1: Undefined Variable (Current)
```
Name Error: Name 'conter' is not defined
 at line 5, column 11

 5 | print text conter
 ^

 Did you mean: 'counter'?
```

### Example 1: Undefined Variable (Proposed Beginner Mode)
```
Name Error: I don't recognize the name 'conter'
 at line 5, column 11

 5 | print text conter
 ^

 Did you mean: 'counter'?

What happened:
 You're trying to use 'conter', but I haven't seen it before.
 Think of it like asking me to grab a box labeled 'conter' -
 I can't find any box with that label.

How to fix:
 1. If you meant 'counter', fix the typo
 2. If 'conter' is new, create it first:
 set conter to 0

Common fix:
 Wrong: print text conter
 Right: set conter to 0
 print text conter

 Learn more about variables: https://nlpl.dev/docs/variables
```

### Example 2: Type Mismatch (Current)
```
Type Error: Cannot add String and Integer
 at line 10, column 15

 10 | set result to "hello" + 5
 ^

 Expected type: String
 Got type: Integer
```

### Example 2: Type Mismatch (Proposed Beginner Mode)
```
Type Error: Can't mix text and numbers
 at line 10, column 15

 10 | set result to "hello" + 5
 ^

What happened:
 You're trying to add "hello" (text) and 5 (a number).
 This is like trying to add apples and oranges - they don't combine.

How to fix:
 Make both sides match:
 - Both text: "hello" + "5" "hello5"
 - Both numbers: 10 + 5 15

Common fix:
 Wrong: set result to "hello" + 5
 Right: set result to "hello" + " world" # Both text
 OR: set result to 10 + 5 # Both numbers

 Learn more about types: https://nlpl.dev/docs/types
```

### Example 3: Missing End (Current)
```
Syntax Error: Expected 'end' to close block
 at line 8, column 1

 8 | print text "done"
 ^

 Suggestion: Make sure to close all blocks with 'end'
```

### Example 3: Missing End (Proposed Beginner Mode)
```
Syntax Error: Missing 'end' to close your block
 at line 8, column 1

 3 | if score is greater than 10
 4 | print text "You win!"
 5 | add 1 to wins
 6 |
 7 | # Missing 'end' here!
 8 | print text "done"
 ^

What happened:
 You started an 'if' block on line 3, but forgot to close it.
 Every block (if, while, for, function) needs an 'end' to mark
 where it stops.

How to fix:
 Add 'end' after the last line that belongs in the if block:

 if score is greater than 10
 print text "You win!"
 add 1 to wins
 end Add this!
 print text "done"

 Tip: Every 'if', 'while', 'for', 'function', or 'class'
 needs a matching 'end'. Count them to make sure they match!

 Learn more about blocks: https://nlpl.dev/docs/blocks
```

---

**Document Version:** 1.0
**Last Updated:** November 20, 2025
**Next Review:** After Phase 1 implementation
