#!/usr/bin/env python
# This script performs various consistency checks on the repository.
import argparse
import logging
import glob
import os
from pathlib import Path

import yaml


LOG = logging.getLogger("consistency_check")
failures = 0


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", default=".", help="Root directory of the repository")
    args = parser.parse_args()

    logging.basicConfig(level=logging.INFO)

    check_build_manifests_added_to_config(args)
    check_components_added_to_upload_job(args)
    check_components_added_to_issue_template(args)

    if failures:
        LOG.error(f"Found {failures} issues")
        raise SystemExit(1)


#### Checks ####

def check_build_manifests_added_to_config(args):
    LOG.info("Checking that all .build-test-rules.yml files are added to .idf_build_apps.toml")

    build_manifests_from_repo = set(glob.glob(f"{args.root}/**/.build-test-rules.yml", recursive=True))
    # exclude the ones under 'managed_components'
    build_manifests_from_repo = set([Path(f) for f in build_manifests_from_repo if "managed_components" not in f])

    idf_build_apps_toml = load_toml(os.path.join(args.root, ".idf_build_apps.toml"))
    build_manifests_from_config = set([Path(f) for f in idf_build_apps_toml.get("manifest_file", [])])

    missing = build_manifests_from_repo - build_manifests_from_config
    if missing:
        LOG.error(f"Missing build manifests in .idf_build_apps.toml: {missing}") 
        add_failure()


def check_components_added_to_upload_job(args):
    LOG.info("Checking that all components are added to the upload job")

    components_from_repo = set([Path(f).name for f in get_component_dirs(args)])

    upload_job = load_yaml(os.path.join(args.root, ".github/workflows/upload_component.yml"))
    upload_job_steps = upload_job.get("jobs", {}).get("upload_components", {}).get("steps", [])
    upload_job_step = next((step for step in upload_job_steps if step.get("name") == "Upload components to component service"), None)
    components_from_upload_job = set([name.strip() for name in upload_job_step.get("with", {}).get("directories", "").split(";")])

    missing = components_from_repo - components_from_upload_job
    if missing:
        LOG.error(f"Missing components in upload job: {missing}")
        add_failure()


def check_components_added_to_issue_template(args):
    LOG.info("Checking that all components are added to the issue template")

    issue_template = load_yaml(os.path.join(args.root, ".github/ISSUE_TEMPLATE/bug-report.yml"))
    issue_template_component = next((element for element in issue_template.get("body", []) if element.get("id") == "component"), None)
    components_from_issue_template = set(issue_template_component.get("attributes", {}).get("options", []))

    components_from_repo = set([Path(component).name for component in get_component_dirs(args)])
    missing = components_from_repo - components_from_issue_template
    if missing:
        LOG.error(f"Missing components in issue template: {missing}")
        add_failure()
    extra = components_from_issue_template - components_from_repo - set(["Other"])
    if extra:
        LOG.error(f"Extra components in issue template: {extra}")
        add_failure()


#### Utility functions ####

def load_toml(filepath) -> dict:
    try:
        import tomllib  # type: ignore # python 3.11

        try:
            with open(str(filepath), 'rb') as fr:
                return tomllib.load(fr)
        except Exception as e:
            raise ValueError(f"Failed to load {filepath}: {e}")
    except ImportError:
        import toml

        try:
            return toml.load(str(filepath))
        except Exception as e:
            raise ValueError(f"Failed to load {filepath}: {e}")
        

def load_yaml(filepath) -> dict:
    with open(filepath, "r") as f:
        return yaml.safe_load(f)

   
def get_component_dirs(args):
    """
    Returns a list of component paths in this repository.
    """
    components_from_repo = set(glob.glob(f"{args.root}/*/idf_component.yml"))
    components_from_repo = [Path(f).parent.name for f in components_from_repo]
    return components_from_repo

def add_failure():
    global failures
    failures += 1


if __name__ == "__main__":
    main()
