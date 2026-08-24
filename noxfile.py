import argparse
import nox
from packaging.version import InvalidVersion, Version
from pathlib import Path
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
    tag_version_cmp = _check_tag(tag)  #  returns 0 if tag is most recent
    if not is_tag_on_main or tag_version_cmp != 0:
        session.error("Release tag is not on origin/main or is not the latest.")


#  invoke this session before creating and pushing the tag
@nox.session(name="prepare-release", python=False)
def prepare_release(session: nox.Session):
    """Prepare changelog files for the supplied release tag."""
    parser = argparse.ArgumentParser(
        usage=f"nox -s {session.name} -- --version <version>",
    )
    parser.add_argument("--version", required=True, help="version for the release that gets prepared.")
    version = parser.parse_args(session.posargs).version

    version_cmp = _check_tag(version)  #  returns 1 if version is highest
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
