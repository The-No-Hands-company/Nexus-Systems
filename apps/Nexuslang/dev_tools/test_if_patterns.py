"""Test multi-statement if patterns and nested structures."""
import subprocess
import sys
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PYTHON = sys.executable


def run(code):
    r = subprocess.run(
        [PYTHON, '-c', f'import sys; sys.path.insert(0,"src"); from nexuslang.main import run_program; run_program({repr(code)})'],
        capture_output=True, text=True, timeout=15, cwd=ROOT
    )
    return r.returncode, r.stdout, r.stderr


def test(name, code, expected_output=None):
    rc, out, err = run(code)
    combined = out + err
    if rc != 0:
        print(f'  [FAIL] {name}')
        print(f'         {combined[:500].strip()}')
        return False
    elif expected_output is not None and expected_output not in combined:
        print(f'  [FAIL] {name} — output mismatch: got {combined.strip()!r}, expected {expected_output!r}')
        return False
    else:
        actual = combined.strip() if expected_output is None else expected_output
        print(f'  [OK]   {name}')
        return True


cases = [
    # Basic multi-statement if + end
    ('multi-stmt if + end', 'if 1 is equal to 1\n    set x to 10\n    set x to x plus 5\nend\nprint text x', '15'),

    # if/else with multi-stmt body
    ('multi-stmt if/else',
     'set x to 5\nif x is equal to 0\n    set y to 100\n    set y to y plus 1\nelse\n    set y to 200\n    set y to y plus 1\nend\nprint text y', '201'),

    # else if chain
    ('else-if chain multi-stmt',
     'set x to 2\nif x is equal to 1\n    set y to 10\n    set y to y plus 1\nelse if x is equal to 2\n    set y to 20\n    set y to y plus 2\nelse\n    set y to 30\n    set y to y plus 3\nend\nprint text y', '22'),

    # nested while inside if body — THE BIG NESTING BUG
    ('nested while inside if',
     'set x to 0\nif x is equal to 0\n    set i to 0\n    while i is less than 3\n        set i to i plus 1\n    end\n    set x to i\nend\nprint text x', '3'),

    # nested if inside if
    ('nested if inside if',
     'set x to 0\nif x is equal to 0\n    if x is equal to 0\n        set x to 1\n    end\n    set x to x plus 1\nend\nprint text x', '2'),

    # nested while inside else body
    ('nested while inside else',
     'set x to 1\nif x is equal to 0\n    set i to 0\n    while i is less than 3\n        set i to i plus 1\n    end\nelse\n    set i to 0\n    while i is less than 5\n        set i to i plus 1\n    end\n    set x to i\nend\nprint text x', '5'),

    # keyword method .has in condition
    ('keyword .has in condition',
     'set d to {}\ncall d.set("sword", 1)\nif d.has("sword")\n    set found to 1\nelse\n    set found to 0\nend\nprint text found', '1'),

    # multi-stmt body with .set calls (keyword method in body)
    ('keyword .set in multi-stmt body',
     'set d to {}\ncall d.set("a", 1)\nif d.has("a")\n    call d.set("a", 99)\n    call d.set("b", 88)\nend\nprint text d.get("a")', '99'),

    # for each inside if
    ('for each inside if',
     'set items to [1, 2, 3]\nset total to 0\nif 1 is equal to 1\n    for each item in items\n        set total to total plus item\n    end\n    set total to total plus 0\nend\nprint text total', '6'),

    # stmts AFTER nested end (the critical post-nested-end statement bug)
    ('stmt after nested end inside if',
     'set x to 0\nif 1 is equal to 1\n    while x is less than 2\n        set x to x plus 1\n    end\n    set x to x plus 10\nend\nprint text x', '12'),

    # inline no-indent style
    ('inline no-indent multi-stmt',
     'if 1 is equal to 1 set x to 5 set x to x plus 1 end\nprint text x', '6'),
]

passed = 0
failed = 0
for c in cases:
    if len(c) == 3:
        result = test(c[0], c[1], c[2])
    else:
        result = test(c[0], c[1])
    if result:
        passed += 1
    else:
        failed += 1

print(f'\n{passed}/{passed+failed} passed')
