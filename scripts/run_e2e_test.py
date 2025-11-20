#!/usr/bin/env python3
"""Helper to run full pipeline tests for M8-lang programs."""

from __future__ import annotations

import argparse
import difflib
import shutil
import subprocess
import sys
from pathlib import Path


def run_checked(cmd, *, cwd=None, input_text=None):
    result = subprocess.run(
        cmd,
        cwd=cwd,
        input=input_text,
        text=True,
        capture_output=True,
        check=False,
    )
    if result.returncode != 0:
        error_lines = [
            f"Command failed: {' '.join(cmd)}",
            f"stdout:\n{result.stdout}",
            f"stderr:\n{result.stderr}",
        ]
        raise RuntimeError("\n".join(error_lines))
    return result


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description="Execute end-to-end compiler test")
    parser.add_argument("--compiler", required=True, help="Path to M8-lang compiler executable")
    parser.add_argument("--source", required=True, help="Source .m8 program")
    parser.add_argument("--work-dir", required=True, help="Directory for intermediate artifacts")
    parser.add_argument("--test-name", required=True, help="Unique test identifier for artifacts")
    parser.add_argument("--expected", required=True, help="File containing expected stdout")
    parser.add_argument("--input", help="Optional file providing stdin for the program")
    parser.add_argument("--llc", help="Path to the llc executable")
    parser.add_argument("--clang", help="Path to the clang executable")
    args = parser.parse_args(argv)

    compiler = Path(args.compiler)
    source = Path(args.source)
    work_dir = Path(args.work_dir)
    expected_path = Path(args.expected)
    input_path = Path(args.input) if args.input else None

    work_dir.mkdir(parents=True, exist_ok=True)

    ll_path = work_dir / f"{args.test_name}.ll"
    obj_path = work_dir / f"{args.test_name}.o"
    exe_path = work_dir / f"{args.test_name}.exe"

    llc = Path(args.llc) if args.llc else shutil.which("llc")
    clang = Path(args.clang) if args.clang else shutil.which("clang")
    if not llc:
        raise RuntimeError("Required tool 'llc' not found on PATH")
    if not clang:
        raise RuntimeError("Required tool 'clang' not found on PATH")

    run_checked([str(compiler), str(source), str(ll_path)])
    run_checked([str(llc), "-filetype=obj", str(ll_path), "-o", str(obj_path)])
    run_checked([str(clang), str(obj_path), "-o", str(exe_path)])

    input_text = input_path.read_text() if input_path else None
    exec_result = run_checked([str(exe_path)], input_text=input_text)
    actual_stdout = exec_result.stdout

    expected_stdout = expected_path.read_text()
    if actual_stdout != expected_stdout:
        diff = "".join(
            difflib.unified_diff(
                expected_stdout.splitlines(keepends=True),
                actual_stdout.splitlines(keepends=True),
                fromfile="expected",
                tofile="actual",
            )
        )
        raise RuntimeError(f"Stdout mismatch for {args.test_name}:\n{diff}")

    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main(sys.argv[1:]))
    except RuntimeError as exc:
        print(str(exc), file=sys.stderr)
        raise SystemExit(1)
