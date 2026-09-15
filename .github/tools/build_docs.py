#!/usr/bin/env python3
"""Build and collect the mdBook documentation for all components.

The script discovers every ``<component>/docs/book.toml`` in the repository,
validates the required tools before starting, builds all components and copies
the generated HTML into the output directory.
"""

import argparse
import logging
import os
import shlex
import shutil
import subprocess
import sys
import tomllib
from dataclasses import dataclass
from pathlib import Path

logger = logging.getLogger("build_docs")


class BuildError(RuntimeError):
    """Raised when an external documentation build step fails."""


@dataclass(frozen=True)
class BuildConfig:
    repo_root: Path
    output_dir: Path
    version: str = "latest"
    fail_fast: bool = False


def read_text(path: Path) -> str:
    try:
        return path.read_text(encoding="utf-8")
    except OSError as error:
        raise BuildError(f"Unable to read {path}: {error}") from error


def load_book_config(path: Path) -> dict:
    try:
        return tomllib.loads(read_text(path))
    except tomllib.TOMLDecodeError as error:
        raise BuildError(f"Invalid TOML in {path}: {error}") from error


def component_books(repo_root: Path) -> list[Path]:
    """Return component directories containing a docs/book.toml file."""
    components = sorted(
        book_toml.parent.parent for book_toml in repo_root.glob("**/docs/book.toml")
    )
    for component in components:
        logger.info("Found documentation: %s", component.relative_to(repo_root))
    return components


def preprocessor_commands(book_toml: Path) -> list[str]:
    """Extract executable names from all mdBook preprocessor commands."""
    preprocessors = load_book_config(book_toml).get("preprocessor", {})
    if not isinstance(preprocessors, dict):
        raise BuildError(f"Invalid [preprocessor] section in {book_toml}")

    commands: list[str] = []
    for name, settings in preprocessors.items():
        if not isinstance(settings, dict):
            raise BuildError(f"Invalid preprocessor configuration: {name}")
        # mdBook defaults [preprocessor.foo] to the `mdbook-foo` executable.
        command = settings.get("command", f"mdbook-{name}")
        if not isinstance(command, str):
            raise BuildError(f"Invalid command for preprocessor.{name}")
        parts = shlex.split(command)
        if parts:
            commands.append(parts[0])
    return commands


def required_tools(components: list[Path]) -> set[str]:
    tools = {"mdbook"}
    for component in components:
        book_toml = component / "docs" / "book.toml"
        tools.update(preprocessor_commands(book_toml))
        if (component / "docs" / "Doxyfile").exists():
            tools.update({"doxygen", "esp-doxybook"})
    return tools


def check_required_tools(components: list[Path]) -> None:
    missing = sorted(
        tool for tool in required_tools(components) if shutil.which(tool) is None
    )
    if missing:
        raise BuildError(
            "Missing required tools: "
            + ", ".join(missing)
            + ". See README.md for installation instructions."
        )


def run(command: list[str], *, cwd: Path, env: dict[str, str] | None = None) -> None:
    """Run a command and turn failures into a consistently reported error."""
    logger.info("$ %s", shlex.join(command))
    try:
        result = subprocess.run(
            command,
            cwd=cwd,
            env=env,
            check=False,
            text=True,
            capture_output=True,
        )
    except OSError as error:
        raise BuildError(f"Unable to run {command[0]}: {error}") from error

    if result.stdout:
        logger.debug(result.stdout.rstrip())
    if result.stderr:
        logger.debug(result.stderr.rstrip())
    if result.returncode:
        output = (result.stdout + result.stderr).strip()
        details = f"\n{output}" if output else ""
        raise BuildError(
            f"Command failed with exit code {result.returncode}: "
            f"{shlex.join(command)}{details}"
        )


def doxy_xml_dir(docs_dir: Path) -> Path | None:
    """Resolve Doxygen's XML output directory from its configuration."""
    doxyfile = docs_dir / "Doxyfile"
    if not doxyfile.exists():
        return None

    settings: dict[str, str] = {}
    for raw_line in read_text(doxyfile).splitlines():
        line = raw_line.split("#", 1)[0].strip()
        if "=" not in line:
            continue
        key, value = (part.strip() for part in line.split("=", 1))
        if len(value) >= 2 and value[0] == value[-1] and value[0] in "\"'":
            value = value[1:-1]
        settings[key] = value

    if settings.get("GENERATE_XML", "YES").upper() == "NO":
        raise BuildError(f"Doxygen XML generation is disabled in {doxyfile}")

    output_dir = docs_dir / settings.get("OUTPUT_DIRECTORY", "")
    return (output_dir / (settings.get("XML_OUTPUT") or "xml")).resolve()


def generate_api_docs(docs_dir: Path) -> None:
    doxyfile = docs_dir / "Doxyfile"
    if not doxyfile.exists():
        return

    run(["doxygen", doxyfile.name], cwd=docs_dir)
    xml_dir = doxy_xml_dir(docs_dir)
    if xml_dir is None or not xml_dir.exists():
        raise BuildError(f"Doxygen XML output was not created: {xml_dir}")

    api_file = docs_dir / "src" / "api.md"
    api_file.parent.mkdir(exist_ok=True)
    run(
        ["esp-doxybook", "-i", str(xml_dir), "-o", str(api_file)],
        cwd=docs_dir,
    )


def uses_mermaid(book_toml: Path) -> bool:
    return any(
        Path(command).name == "mdbook-mermaid"
        for command in preprocessor_commands(book_toml)
    )


def book_output_dir(docs_dir: Path) -> Path:
    config = load_book_config(docs_dir / "book.toml")
    build = config.get("build", {})
    if not isinstance(build, dict):
        raise BuildError(f"Invalid [build] section in {docs_dir / 'book.toml'}")
    build_dir = build.get("build-dir", "book")
    if not isinstance(build_dir, str) or not build_dir:
        raise BuildError(f"Invalid build.build-dir in {docs_dir / 'book.toml'}")
    return docs_dir / build_dir


def build_component(component: Path, config: BuildConfig) -> Path:
    docs_dir = component / "docs"
    relative = component.relative_to(config.repo_root)
    book_toml = docs_dir / "book.toml"
    logger.info("Building %s", relative)

    generate_api_docs(docs_dir)
    if uses_mermaid(book_toml):
        run(["mdbook-mermaid", "install", str(docs_dir)], cwd=docs_dir)

    environment = os.environ.copy()
    environment["MDBOOK_OUTPUT__HTML__SITE_URL"] = (
        f"/{repo_name()}/{config.version}/{relative.as_posix()}/"
    )
    run(["mdbook", "build", str(docs_dir)], cwd=config.repo_root, env=environment)

    output = book_output_dir(docs_dir)
    if not output.is_dir():
        raise BuildError(f"mdBook output was not created: {output}")
    return output


def repo_name() -> str:
    return os.environ.get("GITHUB_REPOSITORY", "espressif/idf-extra-components").rsplit(
        "/", 1
    )[-1]


def copy_docs(source: Path, component: Path, config: BuildConfig) -> None:
    destination = config.output_dir / component.relative_to(config.repo_root)
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copytree(source, destination, dirs_exist_ok=True)
    logger.info("Collected %s", destination.relative_to(config.output_dir))


def build_all(config: BuildConfig) -> bool:
    components = component_books(config.repo_root)
    if not components:
        logger.warning("No component documentation found")
        return True

    check_required_tools(components)
    shutil.rmtree(config.output_dir, ignore_errors=True)
    config.output_dir.mkdir(parents=True)

    failures: list[str] = []
    for component in components:
        try:
            output = build_component(component, config)
            copy_docs(output, component, config)
        except (BuildError, OSError) as error:
            name = component.relative_to(config.repo_root).as_posix()
            failures.append(name)
            logger.error("Failed to build %s: %s", name, error)
            if config.fail_fast:
                break

    if failures:
        logger.error("Documentation failures: %s", ", ".join(failures))
        return False
    return True


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.ArgumentDefaultsHelpFormatter
    )
    parser.add_argument("--version", default="latest", help="Published URL version")
    parser.add_argument(
        "--output-dir", default="docs_build_output", help="Collected documentation"
    )
    parser.add_argument(
        "--fail-fast", action="store_true", help="Stop after the first component fails"
    )
    parser.add_argument(
        "--verbose", "-v", action="store_true", help="Enable debug logs"
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    logging.basicConfig(
        level=logging.DEBUG if args.verbose else logging.INFO,
        format="%(asctime)s %(levelname)s %(message)s",
        datefmt="%H:%M:%S",
    )
    root = Path.cwd()
    output_dir = Path(args.output_dir)
    config = BuildConfig(
        repo_root=root,
        output_dir=output_dir if output_dir.is_absolute() else root / output_dir,
        version=args.version.strip("/"),
        fail_fast=args.fail_fast,
    )
    try:
        return 0 if build_all(config) else 1
    except (BuildError, OSError) as error:
        logger.error("Documentation build aborted: %s", error)
        return 1


if __name__ == "__main__":
    sys.exit(main())
