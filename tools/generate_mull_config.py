import argparse
import json
from pathlib import Path, PurePosixPath
import re
import subprocess


EXCLUDED_PATH_PARTS = {"external", "bazel-out", "third_party"}


def _source_paths(labels: list[str]) -> tuple[str, ...]:
    paths = set()
    for label in labels:
        if not label.startswith("//"):
            continue
        package, separator, target = label[2:].partition(":")
        path = "/".join(part for part in (package, target if separator else "") if part)
        path_parts = set(PurePosixPath(path).parts)
        if path_parts & EXCLUDED_PATH_PARTS:
            continue
        if Path(path).suffix not in {".c", ".cc", ".cpp", ".cxx"}:
            continue
        if re.search(r"(?:_test|_benchmark)\.(?:c|cc|cpp|cxx)$", path):
            continue
        paths.add(path)
    return tuple(sorted(paths))


def _select_mull_sources(paths: tuple[str, ...], target: str) -> tuple[str, ...]:
    """Use the implementation matching a focused test target when available."""
    target_name = target.rsplit(":", 1)[-1]
    test_component = Path(target_name).stem.removesuffix("_test")
    matching_sources = tuple(path for path in paths if Path(path).stem == test_component)
    return matching_sources or paths


def _query_target_sources(
    bazel: str, output_user_root: Path, v2_root: Path, targets: tuple[str, ...]
) -> tuple[str, ...]:
    target_set = " ".join(targets)
    query = f"labels(srcs, deps(set({target_set}), 1))"
    command = [
        bazel,
        "--batch",
        f"--output_user_root={output_user_root}",
        "query",
        "--output=label",
        query,
    ]
    result = subprocess.run(
        command,
        cwd=v2_root,
        capture_output=True,
        text=True,
        check=False,
    )
    if result.returncode != 0:
        raise RuntimeError(result.stderr.strip() or "Bazel query failed")
    paths = _source_paths(result.stdout.splitlines())
    if len(targets) == 1:
        return _select_mull_sources(paths, targets[0])
    return paths


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--bazel", default="bazel")
    parser.add_argument("--output-user-root", type=Path, required=True)
    parser.add_argument("--v2-root", type=Path, required=True)
    parser.add_argument("--template", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--target", action="append", required=True)
    args = parser.parse_args()

    paths = _query_target_sources(
        args.bazel,
        args.output_user_root,
        args.v2_root,
        tuple(args.target),
    )
    include_paths = "includePaths:\n"
    if paths:
        include_paths += "".join(
            f"  - {json.dumps(r'(^|.*/)' + re.escape(path) + r'$')}\n"
            for path in paths
        )
    else:
        # Keep Mull from falling back to scanning every file when a target has
        # no Mull-compatible production source after filtering.
        include_paths += "  - \"(?!)\"\n"
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(include_paths + "\n" + args.template.read_text())


if __name__ == "__main__":
    main()
