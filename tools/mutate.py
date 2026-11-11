#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 Niko Sagiadinos
"""Source-level mutation testing, see Architecture.md ("Mutation tests").

Changes one spot of a source file at a time (operator, constant, condition, statement), rebuilds, runs the tests of
the module and records whether a test noticed. A mutant the tests do not notice ("survived") marks a gap in the tests.
Mutants that do not compile are skipped. Every file is restored after its mutant, also on Ctrl+C.

Run it on a separate checkout (git worktree) with its own build directory, never on the working tree you edit:

    git worktree add ../djw-mutation HEAD
    cmake --preset linux-debug -S ../djw-mutation -B ../djw-mutation/build/mutation
    cmake --build ../djw-mutation/build/mutation
    tools/mutate.py --root ../djw-mutation --build ../djw-mutation/build/mutation src/core/engine

The operators only mutate code formatted by clang-format (binary operators with a blank on both sides), so template
brackets, shifts and arrows are left alone. Comments, string literals, preprocessor lines and static_assert are
skipped.
"""

from __future__ import annotations

import argparse
import dataclasses
import json
import re
import subprocess
import sys
import time
from pathlib import Path

# Binary operators: replacement for each, only matched with a blank on both sides
BINARY = {
    "<": ["<=", ">"],
    "<=": ["<", ">"],
    ">": [">=", "<"],
    ">=": [">", "<"],
    "==": ["!="],
    "!=": ["=="],
    "&&": ["||"],
    "||": ["&&"],
    "+": ["-"],
    "-": ["+"],
    "*": ["/"],
    "/": ["*"],
    "%": ["*"],
}
COMPOUND = {"+=": "-=", "-=": "+=", "*=": "/=", "/=": "*="}
BINARY_RE = re.compile(r"(?<=\s)(<=|>=|==|!=|&&|\|\||<|>|\+|-|\*|/|%)(?=\s)")
COMPOUND_RE = re.compile(r"(?<=\s)(\+=|-=|\*=|/=)(?=\s)")
BOOL_RE = re.compile(r"\b(true|false)\b")
NOT_RE = re.compile(r"(?<![\w)\]!])!(?!=)")
# Integer and floating literals not part of an identifier; digit separators allowed
NUMBER_RE = re.compile(r"(?<![\w.'])(\d[\d']*(?:\.\d+)?(?:[eE][+-]?\d+)?)([fFuUlL]*)(?![\w.'])")
INCREMENT_RE = re.compile(r"(\+\+|--)")
# A statement worth deleting: a call, an assignment or an increment, not a declaration or a return
STATEMENT_RE = re.compile(
    r"^\s*(?!return\b|co_return\b|break\b|continue\b|case\b|default\b|using\b|namespace\b|static_assert\b)"
    r"[\w:.\->\[\]*()]+(\s*(=|\+=|-=|\*=|/=|\|=|&=)\s|\(|\+\+|--).*;\s*$"
)
DECLARATION_HINT_RE = re.compile(r"^\s*(const\s+|constexpr\s+|static\s+|auto\s|std::\w+(<.*>)?\s+\w+\s*[={(])")

RAW_STRING_RE = re.compile(r'R"([^(\s]*)\(')
PURE_VIRTUAL_RE = re.compile(r"=\s*0;\s*$")
SKIP_LINE_RE = re.compile(r"^\s*(#|//|\*|/\*|static_assert|assert\(|SDL_Log|template\s*<)")


@dataclasses.dataclass
class Mutant:
    file: str
    line: int
    kind: str
    original: str
    mutated: str
    status: str = ""
    seconds: float = 0.0


def strip_strings_and_comments(line: str) -> str:
    """The line with string/char literals and a trailing // comment blanked out, same length"""
    out = []
    i = 0
    quote = None
    while i < len(line):
        c = line[i]
        if quote:
            if c == "\\":
                out.append("  ")
                i += 2
                continue
            out.append(" " if c != quote else c)
            if c == quote:
                quote = None
        elif c in "\"'" and not (c == "'" and i > 0 and line[i - 1].isalnum()):
            quote = c
            out.append(c)
        elif line.startswith("//", i):
            out.append(" " * (len(line) - i))
            break
        else:
            out.append(c)
        i += 1
    return "".join(out)[: len(line)].ljust(len(line))


def mutants_of_line(text: str) -> list[tuple[str, str]]:
    """(kind, mutated line) for every mutation of one line"""
    if SKIP_LINE_RE.match(text) or not text.strip():
        return []
    code = strip_strings_and_comments(text)
    result: list[tuple[str, str]] = []

    def replace(span: tuple[int, int], new: str) -> str:
        return text[: span[0]] + new + text[span[1] :]

    for m in BINARY_RE.finditer(code):
        op = m.group(1)
        # "a * b" at the start of a declaration is a pointer, "- x" after "(" or "=" is unary: both have no blank
        for new in BINARY[op]:
            result.append((f"operator {op} -> {new}", replace(m.span(1), new)))
    for m in COMPOUND_RE.finditer(code):
        result.append((f"operator {m.group(1)} -> {COMPOUND[m.group(1)]}", replace(m.span(1), COMPOUND[m.group(1)])))
    for m in BOOL_RE.finditer(code):
        new = "false" if m.group(1) == "true" else "true"
        result.append((f"constant {m.group(1)} -> {new}", replace(m.span(1), new)))
    for m in NOT_RE.finditer(code):
        result.append(("negation removed", replace(m.span(), "")))
    for m in INCREMENT_RE.finditer(code):
        new = "--" if m.group(1) == "++" else "++"
        result.append((f"operator {m.group(1)} -> {new}", replace(m.span(1), new)))
    for m in NUMBER_RE.finditer(code):
        literal = m.group(1)
        if "." in literal or "e" in literal.lower():
            new = f"({literal} * 1.1)"
        else:
            value = int(literal.replace("'", ""), 10) if not literal.startswith("0") or literal == "0" else None
            if value is None:
                continue
            new = str(value + 1)
        result.append((f"constant {literal} -> {new}", replace(m.span(1), new)))
    if STATEMENT_RE.match(code) and not DECLARATION_HINT_RE.match(code) and code.rstrip().endswith(";"):
        indent = text[: len(text) - len(text.lstrip())]
        result.append(("statement removed", indent + "; // mutant: statement removed\n"))
    if re.match(r"^\s*(if|while)\s*\(", code) and code.rstrip().endswith(")"):
        start = code.index("(")
        result.append(("condition negated", text[:start] + "(!" + text[start:].rstrip("\n").rstrip() + ")\n"))
    return result


def mutants_of_file(root: Path, path: Path) -> list[Mutant]:
    lines = path.read_text(encoding="utf-8").splitlines(keepends=True)
    result = []
    in_block_comment = False
    raw_end = None  # the closing delimiter while inside a raw string literal (SQL, for example)
    for number, text in enumerate(lines, start=1):
        stripped = text.strip()
        if raw_end is not None:
            if raw_end in text:
                raw_end = None
            continue
        raw = RAW_STRING_RE.search(text)
        if raw and (")" + raw.group(1) + '"') not in text[raw.end() :]:
            raw_end = ")" + raw.group(1) + '"'
            continue
        if PURE_VIRTUAL_RE.search(text):
            continue
        if in_block_comment:
            if "*/" in stripped:
                in_block_comment = False
            continue
        if stripped.startswith("/*") and "*/" not in stripped:
            in_block_comment = True
            continue
        if "mutation: skip" in text:
            continue
        for kind, mutated in mutants_of_line(text):
            if not mutated.endswith("\n"):
                mutated += "\n"
            if mutated != text:
                result.append(Mutant(str(path.relative_to(root)), number, kind, text.rstrip("\n"), mutated.rstrip("\n")))
    return result


def squeeze(text: str) -> str:
    """text without blanks, so a line found again after clang-format aligned it differently"""
    return "".join(text.split())


def run(command: list[str], cwd: Path, timeout: float) -> tuple[int | None, str]:
    try:
        done = subprocess.run(command, cwd=cwd, capture_output=True, text=True, timeout=timeout)
        return done.returncode, done.stdout + done.stderr
    except subprocess.TimeoutExpired:
        return None, ""


def tests_for(path: str) -> str:
    """ctest regex of the tests for a source file: its module's tests plus the integration tests"""
    parts = Path(path).parts
    if len(parts) >= 3 and parts[0] == "src" and parts[1] == "core":
        return f"^(core\\.{parts[2]}|integration)$"
    if len(parts) >= 3 and parts[0] == "src" and parts[1] == "player-ui":
        return r"^player_ui\."
    return "."


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("paths", nargs="+", help="source files or folders, relative to --root")
    parser.add_argument("--root", type=Path, default=Path.cwd(), help="checkout to mutate (default: current folder)")
    parser.add_argument("--build", type=Path, required=True, help="configured build folder of that checkout")
    parser.add_argument("--report", type=Path, default=Path("mutation-report.jsonl"), help="one JSON line per mutant")
    parser.add_argument("--shard", default="0/1", help="i/n: only every n-th mutant starting at i, for parallel runs")
    parser.add_argument("--timeout-factor", type=float, default=4.0, help="test timeout as a multiple of the baseline")
    parser.add_argument("--list", action="store_true", help="only list the mutants")
    parser.add_argument(
        "--retry", type=Path, action="append", default=[],
        help="only the mutants that survived in this report (repeatable); matched by text, so they are found again "
        "after lines moved")
    args = parser.parse_args()

    root = args.root.resolve()
    build = args.build.resolve()
    files: list[Path] = []
    for item in args.paths:
        p = root / item
        files += sorted(p.rglob("*.cpp")) + sorted(p.rglob("*.hpp")) if p.is_dir() else [p]

    mutants = [m for f in files for m in mutants_of_file(root, f)]
    if args.retry:
        survivors = set()
        for report_file in args.retry:
            for line in report_file.read_text(encoding="utf-8").splitlines():
                r = json.loads(line)
                if r["status"] == "survived":
                    survivors.add((r["file"], squeeze(r["original"]), r["kind"], squeeze(r["mutated"])))
        mutants = [m for m in mutants if (m.file, squeeze(m.original), m.kind, squeeze(m.mutated)) in survivors]
    index, count = (int(x) for x in args.shard.split("/"))
    mutants = mutants[index::count]
    if args.list:
        for m in mutants:
            print(f"{m.file}:{m.line}: {m.kind}: {m.mutated.strip()}")
        print(f"{len(mutants)} mutants", file=sys.stderr)
        return 0

    # Baseline: the tests must pass unchanged, and their time sets the timeout
    baselines: dict[str, float] = {}
    code, output = run(["cmake", "--build", str(build)], root, 3600)
    if code != 0:
        print(output[-4000:], file=sys.stderr)
        print("baseline build failed", file=sys.stderr)
        return 2
    for regex in sorted({tests_for(m.file) for m in mutants}):
        start = time.monotonic()
        code, output = run(["ctest", "--test-dir", str(build), "-R", regex, "--stop-on-failure"], root, 3600)
        if code != 0:
            print(output[-4000:], file=sys.stderr)
            print(f"baseline tests {regex} fail", file=sys.stderr)
            return 2
        baselines[regex] = time.monotonic() - start

    done = set()
    if args.report.exists():  # resume: mutants already in the report are not run again
        for line in args.report.read_text().splitlines():
            r = json.loads(line)
            done.add((r["file"], r["line"], r["kind"], r["mutated"]))

    summary: dict[str, int] = {}
    with args.report.open("a", encoding="utf-8") as report:
        for number, m in enumerate(mutants, start=1):
            if (m.file, m.line, m.kind, m.mutated) in done:
                continue
            path = root / m.file
            original = path.read_text(encoding="utf-8")
            lines = original.splitlines(keepends=True)
            lines[m.line - 1] = m.mutated + "\n"
            start = time.monotonic()
            try:
                path.write_text("".join(lines), encoding="utf-8")
                code, _ = run(["cmake", "--build", str(build)], root, 1800)
                if code != 0:
                    m.status = "compile_error"
                else:
                    regex = tests_for(m.file)
                    limit = max(30.0, baselines[regex] * args.timeout_factor)
                    code, _ = run(
                        ["ctest", "--test-dir", str(build), "-R", regex, "--stop-on-failure", "--timeout",
                         str(int(limit))], root, limit * 2)
                    m.status = "survived" if code == 0 else ("timeout" if code is None else "killed")
            finally:
                path.write_text(original, encoding="utf-8")
            m.seconds = round(time.monotonic() - start, 1)
            summary[m.status] = summary.get(m.status, 0) + 1
            report.write(json.dumps(dataclasses.asdict(m)) + "\n")
            report.flush()
            print(f"[{number}/{len(mutants)}] {m.status:13} {m.file}:{m.line} {m.kind}", flush=True)

    # Rebuild the unchanged sources, so the build folder matches the checkout again
    run(["cmake", "--build", str(build)], root, 3600)
    print(json.dumps(summary), flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
