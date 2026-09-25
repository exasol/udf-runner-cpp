import argparse
import json
import nox
import os
from packaging.version import InvalidVersion, Version
from pathlib import Path
import re
import shutil
import subprocess

from exasol.slc_ci_setup.nox.tasks import *

ROOT = Path(__file__).parent


# default actions to be run if nothing is explicitly specified with the -s option
nox.options.sessions = []


def _parse_release_tag(tag: str) -> Version:
    try:
        version = Version(tag)
    except InvalidVersion as error:
        raise ValueError(f"Invalid release tag '{tag}'.") from error

    # Tag shall not contain alpha-numeric suffixes, e.g.: "1.0.0-alpha".
    if version.is_prerelease or version.is_devrelease:
        raise ValueError("Release tag must be a final release version.")
    return version


def _is_tag_on_main_history(tag: str) -> bool:
    main_ref = "origin/main"
    ref_exists = subprocess.run(
        ["git", "rev-parse", "--verify", "--quiet", main_ref],
        capture_output=True,
        text=True,
        check=False,
    )
    if ref_exists.returncode != 0:
        main_ref = "main"

    result = subprocess.run(
        ["git", "merge-base", "--is-ancestor", tag, main_ref],
        capture_output=True,
        text=True,
        check=False,
    )
    if result.returncode not in (0, 1):
        stderr = result.stderr.strip()
        raise RuntimeError(
            f"Failed to verify whether tag '{tag}' is on '{main_ref}': {stderr or 'unknown error'}"
        )

    return result.returncode == 0


def _check_tag(tag: str) -> int:
    requested_version = _parse_release_tag(tag)
    fetch_tags = subprocess.run(
        ["git", "fetch", "--tags"],
        capture_output=True,
        text=True,
        check=False,
    )
    if fetch_tags.returncode != 0:
        stderr = fetch_tags.stderr.strip()
        raise RuntimeError(f"Failed to fetch git tags: {stderr or 'unknown error'}")

    tags = subprocess.run(
        ["git", "tag", "--list"],
        capture_output=True,
        text=True,
        check=False,
    )
    if tags.returncode != 0:
        stderr = tags.stderr.strip()
        raise RuntimeError(f"Failed to list git tags: {stderr or 'unknown error'}")

    released_versions = []
    for release_tag in tags.stdout.splitlines():
        try:
            released_version = _parse_release_tag(release_tag)
        except ValueError:
            continue
        released_versions.append(released_version)

    if not released_versions:  # if no tag exists, then requested tag is latest
        return 1

    latest_version = max(released_versions)
    if requested_version < latest_version:
        return -1
    if requested_version > latest_version:
        return 1
    return 0


# invoke this session after creating and pushing the tag
@nox.session(name="validate-release", python=False)
def validate_release(session: nox.Session):
    """Validate supplied release tag is on origin/main and is the latest release."""
    parser = argparse.ArgumentParser(
        usage=f"nox -s {session.name} -- --tag <tag>",
    )
    parser.add_argument("--tag", required=True, help="tag for the release that gets validated.")
    tag = parser.parse_args(session.posargs).tag

    is_tag_on_main = _is_tag_on_main_history(tag)
    tag_version_cmp = _check_tag(tag)  # returns 0 if tag is most recent
    if not is_tag_on_main or tag_version_cmp != 0:
        session.error("Release tag is not on origin/main or is not the latest.")


# invoke this session before creating and pushing the tag
@nox.session(name="prepare-release", python=False)
def prepare_release(session: nox.Session):
    """Prepare changelog files for the supplied release tag."""
    parser = argparse.ArgumentParser(
        usage=f"nox -s {session.name} -- --version <version>",
    )
    parser.add_argument("--version", required=True, help="version for the release that gets prepared.")
    version = parser.parse_args(session.posargs).version

    version_cmp = _check_tag(version)  # returns 1 if version is highest
    if version_cmp != 1:
        session.error("Release version is not the latest.")

    changes_dir = ROOT / "doc" / "changes"
    unreleased_file = changes_dir / "unreleased.md"
    changes_file = changes_dir / f"changes_{version}.md"
    changelog_file = changes_dir / "changelog.md"

    if not unreleased_file.is_file():
        session.error(f"Unreleased changelog file does not exist: {unreleased_file}")
    if changes_file.exists():
        session.error(f"Release changelog file already exists: {changes_file}")

    changelog = changelog_file.read_text()
    changelog_heading = "# Changes\n"
    if not changelog.startswith(changelog_heading):
        session.error(f"Unexpected changelog heading in: {changelog_file}")

    release_entry = f"* [{version}](changes_{version}.md)\n"
    changelog_file.write_text(
        f"{changelog_heading}\n{release_entry}{changelog[len(changelog_heading):].lstrip()}"
    )
    unreleased_file.rename(changes_file)


def get_oft_jar(session: nox.Session) -> Path:
    oft_version = "4.1.0"
    oft_jar = Path.home() / ".m2" / "repository" / "org" / "itsallcode" / "openfasttrace" / "openfasttrace" / oft_version / f"openfasttrace-{oft_version}.jar"
    if not oft_jar.exists():
        print(f"Downloading OpenFastTrace {oft_version}")
        session.run("mvn", "--batch-mode", "org.apache.maven.plugins:maven-dependency-plugin:3.3.0:get", f"-Dartifact=org.itsallcode.openfasttrace:openfasttrace:{oft_version}")
    return oft_jar

def run_oft_for_udf_client(session: nox.Session, *args) -> None:
    oft_jar = get_oft_jar(session)
    udf_client_base_dir = ROOT / "udf-runner-cpp" / "v1"
    udf_client_src_dir = udf_client_base_dir / "base"

    with session.chdir(ROOT):
        session.run(
            "java",
            "-jar",
            oft_jar,
            "trace",
            "-a",
            "feat,req,dsn",
            f"{udf_client_base_dir}/docs",
            f"{udf_client_src_dir}",
            "-t",
            "V2,_",
            *args
        )


@nox.session(name="mull-targets", python=False)
def list_mull_targets_session(session: nox.Session):
    """Expose Mull target discovery as a Nox session for CI."""
    list_mull_targets(session)




def list_mull_targets(session: nox.Session):
    """List Mull targets and optionally write a GitHub Actions matrix."""
    parser = argparse.ArgumentParser(usage=f"nox -s {session.name} -- [options]")
    parser.add_argument(
        "--github-output-var",
        help="write the matrix JSON to this variable in GITHUB_OUTPUT",
    )
    args = parser.parse_args(session.posargs)

    matrix = json.dumps({"target": list(_get_mull_targets(session))}, separators=(",", ":"))
    if args.github_output_var:
        github_output = os.environ.get("GITHUB_OUTPUT")
        if not github_output:
            session.error("GITHUB_OUTPUT is required with --github-output-var")
        with open(github_output, "a") as output:
            output.write(f"{args.github_output_var}={matrix}\n")
    else:
        print(matrix)


def _get_mull_targets(session: nox.Session) -> tuple[str, ...]:
    """Discover Bazel cc_test targets not explicitly excluded from Mull."""
    v2_root = ROOT / "udf-runner-cpp" / "v2"
    bazel = os.environ.get("BAZEL", "bazel")
    bazel_startup_args = []
    if output_user_root := os.environ.get("MULL_BAZEL_OUTPUT_ROOT"):
        bazel_startup_args.append(f"--output_user_root={output_user_root}")
    with session.chdir(v2_root):
        labels = session.run(
            bazel,
            *bazel_startup_args,
            "query",
            'kind("cc_test rule", //...) except attr("tags", "no-mull", //...)',
            "--output=label",
            silent=True,
            external=True,
        )

    targets = tuple(
        sorted(
            label.rsplit(":", maxsplit=1)[1]
            for label in labels.splitlines()
            if label.startswith("//:") and ":" in label
        )
    )
    if not targets:
        session.error("No Bazel cc_test targets available for Mull were found")
    return targets


@nox.session(name="mull", python=False)
def run_mull(session: nox.Session):
    """Run Mull mutation testing for the functional v2 C++ tests."""
    parser = argparse.ArgumentParser(usage=f"nox -s {session.name} -- [options]")
    parser.add_argument("--target")
    args = parser.parse_args(session.posargs)

    llvm_version = os.environ.get("MULL_LLVM_VERSION", "20")
    bazel = os.environ.get("BAZEL", "bazel")
    compiler = os.environ.get("MULL_CXX", f"clang++-{llvm_version}")
    c_compiler = os.environ.get("MULL_CC", compiler.replace("clang++", "clang", 1))
    runner = os.environ.get("MULL_RUNNER", f"mull-runner-{llvm_version}")
    frontend = os.environ.get(
        "MULL_IR_FRONTEND", f"/usr/lib/mull-ir-frontend-{llvm_version}"
    )

    required_tools = [bazel, c_compiler, compiler, runner]
    missing_tools = [tool for tool in required_tools if shutil.which(tool) is None]
    if missing_tools:
        session.error(
            "Mull requires these executable(s) on PATH: "
            + ", ".join(missing_tools)
            + ". Install the matching LLVM/Mull toolchain or override MULL_CXX "
            "and MULL_RUNNER."
        )
    if not Path(frontend).exists():
        session.error(
            f"Mull IR frontend does not exist: {frontend}. "
            "Override MULL_IR_FRONTEND with the version-matched plugin path."
        )

    v2_root = ROOT / "udf-runner-cpp" / "v2"
    report_dir = ROOT / ".build_output" / "mull"
    report_dir.mkdir(parents=True, exist_ok=True)
    bazel_output_root = Path(
        os.environ.get("MULL_BAZEL_OUTPUT_ROOT", ROOT / ".build_output" / "bazel-mull")
    )

    target_names = (args.target,) if args.target else _get_mull_targets(session)
    targets = [f"//:{target}" for target in target_names]
    bazel_startup_args = [f"--output_user_root={bazel_output_root}"]
    generated_config = report_dir / "mull.yml"
    session.run(
        "python",
        str(ROOT / "tools" / "generate_mull_config.py"),
        "--bazel",
        bazel,
        "--output-user-root",
        str(bazel_output_root),
        "--v2-root",
        str(v2_root),
        "--template",
        str(ROOT / "mull.yml"),
        "--output",
        str(generated_config),
        *sum((["--target", target] for target in targets), []),
    )
    bazel_args = [
        "build",
        "--compilation_mode=dbg",
        "--copt=-O0",
        "--copt=-g",
        "--copt=-grecord-command-line",
        f"--copt=-fpass-plugin={frontend}",
        f"--action_env=MULL_CONFIG={generated_config}",
        "--per_file_copt=.*\\.c$@-std=gnu11",
        f"--repo_env=CC={c_compiler}",
        f"--repo_env=CXX={compiler}",
        "--verbose_failures",
        *targets,
    ]
    if build_jobs := os.environ.get("MULL_BAZEL_BUILD_JOBS"):
        bazel_args.insert(1, f"--jobs={build_jobs}")

    run_env = os.environ.copy()
    run_env["MULL_CONFIG"] = str(generated_config)

    with session.chdir(v2_root):
        session.run(bazel, *bazel_startup_args, *bazel_args, env=run_env)
        bazel_bin = Path(
            session.run(
                bazel,
                *bazel_startup_args,
                "info",
                "bazel-bin",
                "--compilation_mode=dbg",
                silent=True,
                external=True,
            ).strip()
        )
        library_paths = sorted(bazel_bin.glob("_solib_*"))
        library_paths.extend(
            path
            for path in (
                Path("/lib64"),
                Path("/lib/x86_64-linux-gnu"),
                Path("/usr/lib/x86_64-linux-gnu"),
            )
            if path.is_dir()
        )
        ld_search_args = [
            argument
            for path in library_paths
            for argument in ("--ld-search-path", str(path))
        ]
        for target in targets:
            target_name = target.rsplit(":", maxsplit=1)[1]
            executable = Path("bazel-bin") / target_name
            if not executable.exists():
                session.error(f"Bazel did not produce expected test binary: {executable}")
            mull_output = session.run(
                runner,
                "--mutation-score-threshold",
                "80",
                *ld_search_args,
                "--ide-reporter-show-killed",
                "--reporters",
                "IDE",
                "--reporters",
                "Elements",
                "--report-dir",
                str(report_dir),
                "--report-name",
                target_name,
                executable,
                env=run_env,
                silent=True,
            )
            report_output = ""
            report_file = report_dir / f"{target_name}.txt"
            if report_file.exists():
                report_output = report_file.read_text()
            print(mull_output, end="")
            mutation_counts = re.findall(
                r"(?:Killed|Survived) mutants \((\d+)/(\d+)\)",
                f"{mull_output or ''}\n{report_output}",
            )
            if not mutation_counts or max(int(total) for _, total in mutation_counts) == 0:
                session.error(
                    f"Mull target '{target_name}' produced no mutants; "
                    "check the instrumentation configuration"
                )

@nox.session(name="run-oft", python=False)
def run_oft_udf_client_plaintext(session: nox.Session):
    """
    Downloads (if needed) OFT and executes it for the udf client for tag "V2,_" printing the output to stdout.
    """
    run_oft_for_udf_client(session)


@nox.session(name="run-oft-html", python=False)
def run_oft_udf_client_html(session: nox.Session):
    """
    Downloads (if needed) OFT and executes it for the udf client for tag "V2,_" creating a html page as output.
    """
    html_file = session.posargs[0] if session.posargs else "report.html"
    run_oft_for_udf_client(session, "-o", "html", "-f", html_file)


def _get_v2_fuzz_targets(session: nox.Session) -> tuple[str, ...]:
    """Discover v2 fuzz targets from the Bazel package using a rule pattern."""
    v2_dir = ROOT / "udf-runner-cpp" / "v2"
    with session.chdir(v2_dir):
        labels = session.run(
            "bazel",
            "query",
            'filter("_fuzz_test$", //...)',
            "--output=label",
            silent=True,
            external=True,
        )

    suffix = "_fuzz_test"
    targets = {
        label.removeprefix("//:").removesuffix(suffix)
        for label in labels.splitlines()
        if label.startswith("//:") and label.endswith(suffix)
    }
    if not targets:
        session.error("No v2 cc_fuzz_test targets were found")
    return tuple(sorted(targets))


@nox.session(name="v2-fuzzing-targets", python=False)
def list_v2_fuzzing_targets(session: nox.Session):
    """Discover v2 Bazel fuzz targets and optionally write a GitHub matrix."""
    parser = argparse.ArgumentParser(
        usage=f"nox -s {session.name} -- [options]",
    )
    parser.add_argument(
        "--github-output-var",
        help="write the matrix JSON to this variable in GITHUB_OUTPUT",
    )
    args = parser.parse_args(session.posargs)

    matrix = json.dumps(
        {"target": list(_get_v2_fuzz_targets(session))},
        separators=(",", ":"),
    )
    if args.github_output_var:
        github_output = os.environ.get("GITHUB_OUTPUT")
        if not github_output:
            session.error("GITHUB_OUTPUT is required with --github-output-var")
        with open(github_output, "a") as output:
            output.write(f"{args.github_output_var}={matrix}\n")
    else:
        print(matrix)


@nox.session(name="v2-fuzzing", python=False)
def run_v2_fuzzing(session: nox.Session):
    """Run v2 Bazel fuzzers with configurable sanitizer and output settings."""
    parser = argparse.ArgumentParser(
        usage=f"nox -s {session.name} -- [options]",
    )
    parser.add_argument("--timeout-secs", type=int, default=300)
    parser.add_argument(
        "--output-root",
        type=Path,
        default=Path(os.environ.get("RUNNER_TEMP", "/tmp")) / "fuzzing",
    )
    parser.add_argument("--bazel-config", default="asan-ubsan-libfuzzer")
    parser.add_argument("--target", help="run only this discovered fuzz target")
    args = parser.parse_args(session.posargs)

    if args.timeout_secs < 0:
        session.error("--timeout-secs must be non-negative")

    output_root = args.output_root.expanduser().resolve()

    targets = _get_v2_fuzz_targets(session)
    if args.target:
        if args.target not in targets:
            session.error(f"Unknown v2 fuzz target: {args.target}")
        targets = (args.target,)
    v2_dir = ROOT / "udf-runner-cpp" / "v2"
    output_root.mkdir(parents=True, exist_ok=True)

    with session.chdir(v2_dir):
        for target in targets:
            target_output_root = output_root / target
            target_output_root.mkdir(parents=True, exist_ok=True)
            session.run(
                "bazel",
                "run",
                f"--config={args.bazel_config}",
                f"//:{target}_fuzz_test_run",
                "--",
                f"--fuzzing_output_root={target_output_root}",
                f"--timeout_secs={args.timeout_secs}",
                external=True,
            )
