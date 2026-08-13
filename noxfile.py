import argparse
from pathlib import Path

from exasol.slc_ci_setup.nox.tasks import *

ROOT = Path(__file__).parent


# default actions to be run if nothing is explicitly specified with the -s option
nox.options.sessions = []


@nox.session(name="prepare-release", python=False)
def prepare_release(session: nox.Session):
    """Prepare changelog files for the supplied release tag."""
    parser = argparse.ArgumentParser(
        usage=f"nox -s {session.name} -- --tag <tag>",
    )
    parser.add_argument("--tag", required=True, help="Release tag")
    tag = parser.parse_args(session.posargs).tag

    if Path(tag).name != tag:
        session.error("Release tag must not contain a path separator.")

    changes_dir = ROOT / "doc" / "changes"
    unreleased_file = changes_dir / "unreleased.md"
    changes_file = changes_dir / f"changes_{tag}.md"
    changelog_file = changes_dir / "changelog.md"

    if not unreleased_file.is_file():
        session.error(f"Unreleased changelog file does not exist: {unreleased_file}")
    if changes_file.exists():
        session.error(f"Release changelog file already exists: {changes_file}")

    changelog = changelog_file.read_text()
    changelog_heading = "# Changes\n"
    if not changelog.startswith(changelog_heading):
        session.error(f"Unexpected changelog heading in: {changelog_file}")

    release_entry = f"* [{tag}](changes_{tag}.md)\n"
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
