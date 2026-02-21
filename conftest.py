import pytest

@pytest.hookimpl(tryfirst=True)  # run early
def pytest_ignore_collect(collection_path, config):
    ignoring = config.getoption("ignore") or []
    for pattern in ignoring:
        if collection_path.match(pattern) or str(collection_path).startswith(pattern):
            print(f"pytest would ignore: {collection_path}")
            return True

    for glob_pattern in config.getoption("ignore_glob") or []:
        if collection_path.match(glob_pattern):
            print(f"pytest would ignore by glob: {collection_path}")
            return True

    return False
