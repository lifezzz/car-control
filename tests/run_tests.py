#!/usr/bin/env python3
"""Regression tests for the four wheel car simulator.

Every test case is a pair of files inside tests/cases/:

    03_manual_release_restores_auto.in    commands fed to the program on stdin
    03_manual_release_restores_auto.out   expected content of stdout

Only stdout is compared, because the program writes every human readable
message to stderr - stdout must contain nothing but the "status" blocks, one
six line block per "status" command.  Trailing newlines are ignored so a case
file may or may not end with one.

Usage:
    python3 tests/run_tests.py [--bin path/to/car] [-v]
"""

import argparse
import difflib
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CASES_DIR = os.path.join(ROOT, "tests", "cases")

# Where the binary may live, relative to the repository root.
BINARY_CANDIDATES = (
    "car.exe",
    "car",
    os.path.join("build", "car.exe"),
    os.path.join("build", "car"),
    os.path.join("build", "Release", "car.exe"),
    os.path.join("build", "Debug", "car.exe"),
)


def locate_binary(explicit):
    if explicit:
        path = explicit if os.path.isabs(explicit) else os.path.join(os.getcwd(), explicit)
        return path if os.path.isfile(path) else None
    for candidate in BINARY_CANDIDATES:
        path = os.path.join(ROOT, candidate)
        if os.path.isfile(path):
            return path
    return None


def normalize(text):
    return text.replace("\r\n", "\n").replace("\r", "\n").rstrip("\n")


def read_text(path):
    return open(path, "rb").read().decode("utf-8", "replace")


def check_status_shape(output):
    """Every stdout line must belong to a six line status block."""
    lines = output.split("\n") if output else []
    if len(lines) % 6 != 0:
        return "stdout has %d lines, which is not a multiple of 6" % len(lines)
    for index, line in enumerate(lines):
        field = ("STATE", "CONTROL", "AUTO", "MANUAL", "TARGET", "WHEELS")[index % 6]
        if not line.startswith(field + " "):
            return "line %d should start with '%s ', got %r" % (index + 1, field, line)
    return None


def run_case(binary, case_path, verbose):
    name = os.path.splitext(os.path.basename(case_path))[0]
    expected_path = os.path.splitext(case_path)[0] + ".out"

    with open(case_path, "rb") as handle:
        stdin_data = handle.read()

    try:
        proc = subprocess.run([binary], input=stdin_data,
                              stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=20)
    except subprocess.TimeoutExpired:
        return name, "TIMEOUT", "", "", ""
    except OSError as error:
        return name, "FAIL (cannot run: %s)" % error, "", "", ""

    got = normalize(proc.stdout.decode("utf-8", "replace"))
    want = normalize(read_text(expected_path)) if os.path.isfile(expected_path) else ""

    problem = None
    if proc.returncode != 0:
        problem = "exit code %d" % proc.returncode
    elif not os.path.isfile(expected_path):
        problem = "missing expected file %s" % expected_path
    elif check_status_shape(got) is not None:
        problem = check_status_shape(got)
    elif got != want:
        problem = "output differs"

    diff = ""
    if problem == "output differs":
        diff = "\n".join(difflib.unified_diff(
            want.split("\n"), got.split("\n"),
            fromfile="expected", tofile="actual", lineterm=""))

    stderr_text = ""
    if verbose:
        messages = proc.stderr.decode("utf-8", "replace").rstrip("\n")
        stderr_text = "stderr:\n" + messages if messages else ""

    return name, ("ok" if problem is None else "FAIL (%s)" % problem), diff, stderr_text, got


def main():
    parser = argparse.ArgumentParser(description="run the car simulator regression tests")
    parser.add_argument("--bin", dest="binary", default=None,
                        help="path of the compiled program (default: autodetect)")
    parser.add_argument("-v", "--verbose", action="store_true",
                        help="show stderr (the human readable messages) of every case")
    args = parser.parse_args()

    binary = locate_binary(args.binary)
    if binary is None:
        print("error: no binary found - build it first, e.g. 'make' or 'cmake --build build'")
        return 2
    print("binary : %s" % binary)

    if not os.path.isdir(CASES_DIR):
        print("error: %s does not exist" % CASES_DIR)
        return 2

    cases = sorted(os.path.join(CASES_DIR, name)
                   for name in os.listdir(CASES_DIR) if name.endswith(".in"))
    if not cases:
        print("error: no *.in test cases found in %s" % CASES_DIR)
        return 2

    failures = 0
    for case in cases:
        name, status, diff, stderr_text, _ = run_case(binary, case, args.verbose)
        if status == "ok":
            print("[ok]   %s" % name)
        else:
            failures += 1
            print("[FAIL] %s -> %s" % (name, status))
        if diff:
            print(diff)
        if stderr_text:
            print(stderr_text)

    print("-" * 60)
    print("%d case(s), %d failure(s)" % (len(cases), failures))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
