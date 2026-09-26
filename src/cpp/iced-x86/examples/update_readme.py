#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (C) 2018-present iced project and contributors

"""Copies the example source files (examples/*.cpp) to the code blocks in ../README.md.

README.md contains marker comments like this:

    <!-- example: disassemble.cpp -->
    ```cpp
    ...
    ```
    <!-- example-end -->

Everything between the two markers is replaced with the example file's code (without the
license header). Run this script after editing an example:

    python3 src/cpp/iced-x86/examples/update_readme.py

Use `--check` to only verify that README.md is up to date (exit code 1 if it's not). CTest runs
it (test `examples_readme`) if Python 3 is found.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

EXAMPLES_DIR = Path(__file__).resolve().parent
README_PATH = EXAMPLES_DIR.parent / "README.md"
LICENSE_HEADER = (
    "// SPDX-License-Identifier: MIT\n"
    "// Copyright (C) 2018-present iced project and contributors\n"
)
BLOCK_RE = re.compile(r"(<!-- example: (?P<name>[\w.]+) -->\n)(?P<body>.*?)(<!-- example-end -->)", re.DOTALL)


def get_example_code(name: str) -> str:
    path = EXAMPLES_DIR / name
    if not path.is_file():
        raise SystemExit(f"README.md references a missing example: {path}")
    code = path.read_text(encoding="utf-8").replace("\r\n", "\n")
    if code.startswith(LICENSE_HEADER):
        code = code[len(LICENSE_HEADER):]
    code = code.strip("\n") + "\n"
    if "```" in code:
        raise SystemExit(f"{path} contains ``` which can't be used in a Markdown code block")
    return code


def update(readme: str) -> tuple[str, set[str]]:
    used: set[str] = set()

    def replace(m: re.Match) -> str:
        name = m.group("name")
        used.add(name)
        return f"{m.group(1)}```cpp\n{get_example_code(name)}```\n{m.group(4)}"

    return BLOCK_RE.sub(replace, readme), used


def main() -> int:
    check_only = "--check" in sys.argv[1:]
    readme = README_PATH.read_text(encoding="utf-8").replace("\r\n", "\n")
    new_readme, used = update(readme)

    all_examples = {p.name for p in EXAMPLES_DIR.glob("*.cpp")}
    missing = sorted(all_examples - used)
    if missing:
        print(f"Examples not in README.md (add a '<!-- example: NAME -->' block): {', '.join(missing)}", file=sys.stderr)
        return 1

    if new_readme == readme:
        return 0
    if check_only:
        print(f"{README_PATH} is out of date, run: python3 {Path(__file__).resolve()}", file=sys.stderr)
        return 1
    README_PATH.write_text(new_readme, encoding="utf-8", newline="\n")
    print(f"Updated {README_PATH}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
