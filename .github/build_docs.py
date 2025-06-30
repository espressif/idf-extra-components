#!/usr/bin/env python3
"""
Documentation build script for ESP-IDF Extra Components.

This script searches for components with documentation, builds them using mdbook,
and copies the built documentation to an output directory.
"""

import argparse
import logging
import os
import pathlib
import shutil
import subprocess
import sys
from contextlib import contextmanager
from dataclasses import dataclass
from typing import List, Optional, Union


# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s - %(levelname)s - %(message)s",
    datefmt="%Y-%m-%d %H:%M:%S"
)
logger = logging.getLogger("build_docs")


@dataclass
class BuildConfig:
    """Configuration for the documentation build process."""
    repo_root: pathlib.Path
    output_dir: pathlib.Path
    pr_number: Optional[str] = None
    fail_fast: bool = True


@contextmanager
def change_directory(path: Union[str, pathlib.Path]):
    """Context manager for temporarily changing the working directory."""
    original_dir = pathlib.Path.cwd()
    try:
        os.chdir(path)
        yield
    finally:
        os.chdir(original_dir)


def find_components_with_docs(repo_root: pathlib.Path) -> List[str]:
    """
    Search the repo for component folders that have docs/book.toml.

    Args:
        repo_root: Repository root directory

    Returns:
        List of component names with documentation
    """
    components = []

    for item in repo_root.iterdir():
        if not item.is_dir():
            continue

        docs_path = item / "docs"
        book_toml_path = docs_path / "book.toml"

        if docs_path.is_dir() and book_toml_path.is_file():
            components.append(item.name)
            logger.info(f"Found component with docs: {item.name}")

    return components


def generate_api_docs(component_docs_path: pathlib.Path) -> bool:
    """
    Generate API documentation using esp-doxybook.

    Args:
        component_docs_path: Path to component docs directory

    Returns:
        True if successful, False otherwise
    """
    api_md_path = component_docs_path / "src" / "api.md"

    # Create src directory if it doesn't exist
    (component_docs_path / "src").mkdir(exist_ok=True)

    logger.info(f"Generating API documentation in {api_md_path}")

    try:
        with change_directory(component_docs_path):
            # Note: Using relative paths since we're in the docs directory
            subprocess.run(
                ["esp-doxybook", "-i", "doxygen_output/xml", "-o", "src/api.md"],
                check=True,
                capture_output=True,
                text=True
            )
        logger.info("API documentation generated successfully")
        return True
    except subprocess.CalledProcessError as e:
        logger.warning(f"Failed to generate API documentation: {e}")
        logger.warning(f"stdout: {e.stdout if hasattr(e, 'stdout') else 'N/A'}")
        logger.warning(f"stderr: {e.stderr if hasattr(e, 'stderr') else 'N/A'}")
        logger.warning("Continuing with mdbook build...")
        return False


def build_component_docs(component_name: str, config: BuildConfig) -> bool:
    """
    Build documentation for a component using mdbook.

    Args:
        component_name: Component name
        config: Build configuration

    Returns:
        True if build was successful, False otherwise
    """
    component_docs_path = config.repo_root / component_name / "docs"

    logger.info(f"Building docs for {component_name}...")

    try:
        # Generate API documentation
        generate_api_docs(component_docs_path)

        # Build mdbook
        env = os.environ.copy()

        # Set site-url based on whether this is a PR or merge event
        if config.pr_number:
            site_url = f"/idf-extra-components/pr-preview-{config.pr_number}/{component_name}/"
        else:
            site_url = f"/idf-extra-components/latest/{component_name}/"

        logger.info(f"Setting site-url to: {site_url}")
        env["MDBOOK_OUTPUT__HTML__SITE_URL"] = site_url

        result = subprocess.run(
            ["mdbook", "build", str(component_docs_path)],
            check=True,
            env=env,
            capture_output=True,
            text=True
        )

        # Log mdbook output at debug level
        logger.debug(f"mdbook stdout: {result.stdout}")
        logger.debug(f"mdbook stderr: {result.stderr}")

        # Verify the book directory was created
        book_path = component_docs_path / "book"
        if not book_path.exists():
            logger.error(f"Book path {book_path} does not exist after build")
            return False

        return True
    except subprocess.CalledProcessError as e:
        logger.error(f"Error building docs for {component_name}: {e}")
        logger.error(f"stdout: {e.stdout if hasattr(e, 'stdout') else 'N/A'}")
        logger.error(f"stderr: {e.stderr if hasattr(e, 'stderr') else 'N/A'}")
        return False


def copy_docs_to_output(component_name: str, config: BuildConfig) -> bool:
    """
    Copy the built documentation to the docs output directory.

    Args:
        component_name: Component name
        config: Build configuration

    Returns:
        True if copy was successful, False otherwise
    """
    source_path = config.repo_root / component_name / "docs" / "book"
    dest_path = config.output_dir / component_name

    if not source_path.exists():
        logger.warning(f"Source path {source_path} does not exist, skipping copy")
        return False

    # Remove destination if it exists to avoid issues with copytree
    if dest_path.exists():
        shutil.rmtree(dest_path)

    # Create parent directory if needed
    dest_path.parent.mkdir(exist_ok=True)

    # Copy the documentation
    shutil.copytree(source_path, dest_path)
    logger.info(f"Copied docs from {source_path} to {dest_path}")
    return True


def build_all_docs(config: BuildConfig) -> bool:
    """
    Build documentation for all components.

    Args:
        config: Build configuration

    Returns:
        True if all builds were successful, False otherwise
    """
    # Find components with docs
    components = find_components_with_docs(config.repo_root)
    logger.info(f"Found {len(components)} components with documentation: {', '.join(components)}")

    if not components:
        logger.warning("No components with documentation found")
        return True

    # Clean output directory
    if config.output_dir.exists():
        shutil.rmtree(config.output_dir)
    config.output_dir.mkdir(exist_ok=True)

    # Build docs for each component
    build_success = True

    for component in components:
        if not build_component_docs(component, config):
            logger.error(f"Documentation build failed for component {component}")
            build_success = False
            if config.fail_fast:
                logger.info("Fail-fast enabled, stopping build")
                break
        else:
            # Only copy docs if build was successful
            copy_docs_to_output(component, config)

    return build_success


def parse_args() -> argparse.Namespace:
    """Parse command line arguments."""
    parser = argparse.ArgumentParser(
        description="Build component documentation",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter
    )
    parser.add_argument(
        "--pr-number",
        type=str,
        help="PR number for preview builds"
    )
    parser.add_argument(
        "--output-dir",
        type=str,
        default="docs_build_output",
        help="Directory where built documentation will be stored"
    )
    parser.add_argument(
        "--no-fail-fast",
        action="store_true",
        help="Continue building even if one component fails"
    )
    parser.add_argument(
        "--verbose", "-v",
        action="store_true",
        help="Enable verbose output"
    )
    return parser.parse_args()


def main():
    """Main entry point."""
    args = parse_args()

    # Set logging level based on verbosity
    if args.verbose:
        logger.setLevel(logging.DEBUG)

    # Create build configuration
    config = BuildConfig(
        repo_root=pathlib.Path.cwd(),
        output_dir=pathlib.Path.cwd() / args.output_dir,
        pr_number=args.pr_number,
        fail_fast=not args.no_fail_fast
    )

    logger.info("Building documentation with config:")
    logger.info(f"- Output directory: {config.output_dir}")
    logger.info(f"- PR number: {config.pr_number or 'None'}")
    logger.info(f"- Fail fast: {config.fail_fast}")

    # Build all documentation
    success = build_all_docs(config)

    if success:
        logger.info("All documentation built successfully")
        return 0
    else:
        logger.error("Documentation build failed")
        return 1


if __name__ == "__main__":
    sys.exit(main())
