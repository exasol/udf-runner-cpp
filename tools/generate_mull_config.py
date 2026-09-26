import argparse
import json
from pathlib import Path, PurePosixPath
import re
import subprocess


SOURCE_SUFFIXES = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx"}
EXCLUDED_PATH_PARTS = {"external", "bazel-out", "third_party"}


def _artifact_path(path_fragments: dict[int, dict], fragment_id: int) -> str:
    parts = []
    while fragment_id:
        fragment = path_fragments[fragment_id]
        parts.append(fragment["label"])
        fragment_id = fragment.get("parentId", 0)
    return "/".join(reversed(parts))


def _source_paths(query_result: dict) -> tuple[str, ...]:
    path_fragments = {
        fragment["id"]: fragment for fragment in query_result.get("pathFragments", [])
    }
    artifacts = {
        artifact["id"]: _artifact_path(path_fragments, artifact["pathFragmentId"])
        for artifact in query_result.get("artifacts", [])
    }
    dep_sets = {
        dep_set["id"]: dep_set for dep_set in query_result.get("depSetOfFiles", [])
    }

    def artifact_ids(dep_set_id: int, seen: set[int]) -> set[int]:
        if dep_set_id in seen:
            return set()
        seen.add(dep_set_id)
        dep_set = dep_sets[dep_set_id]
        result = set(dep_set.get("directArtifactIds", []))
        for transitive_id in dep_set.get("transitiveDepSetIds", []):
            result.update(artifact_ids(transitive_id, seen))
        return result

    input_ids = set()
    for action in query_result.get("actions", []):
        for dep_set_id in action.get("inputDepSetIds", []):
            input_ids.update(artifact_ids(dep_set_id, set()))

    paths = set()
    for artifact_id in input_ids:
        path = artifacts[artifact_id]
        path_parts = set(PurePosixPath(path).parts)
        if path_parts & EXCLUDED_PATH_PARTS:
            continue
        if re.search(r"(?:_test|_benchmark)\.(?:c|cc|cpp|cxx|h|hh|hpp|hxx)$", path):
            continue
        if Path(path).suffix in SOURCE_SUFFIXES:
            paths.add(path)
    return tuple(sorted(paths))


def _query_compile_inputs(
    bazel: str, output_user_root: Path, v2_root: Path, targets: tuple[str, ...]
) -> tuple[str, ...]:
    target_set = " ".join(targets)
    query = f'mnemonic("CppCompile", deps(set({target_set})))'
    command = [
        bazel,
        f"--output_user_root={output_user_root}",
        "aquery",
        "--output=jsonproto",
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
        raise RuntimeError(result.stderr.strip() or "Bazel aquery failed")
    return _source_paths(json.loads(result.stdout))


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--bazel", default="bazel")
    parser.add_argument("--output-user-root", type=Path, required=True)
    parser.add_argument("--v2-root", type=Path, required=True)
    parser.add_argument("--template", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--target", action="append", required=True)
    args = parser.parse_args()

    paths = _query_compile_inputs(
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
