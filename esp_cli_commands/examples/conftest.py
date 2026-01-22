def pytest_ignore_collect(collection_path, config):
    skip_dirs = {'utils', 'managed_components'}
    return any(part in skip_dirs for part in collection_path.parts)