"""Exercise the archive contract in a caller with a different project layout."""

import importlib.util
import json
from pathlib import Path
import tarfile
import tempfile
import unittest

TOOLS = Path(__file__).resolve().parents[1] / 'actions/ci-tools'


def load(name):
    spec = importlib.util.spec_from_file_location(name, TOOLS / f'{name}.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


PACKAGE = load('package_binaries')
BUILD = load('run_build')


class ArtifactTests(unittest.TestCase):
    def test_archive_round_trip_preserves_layout_and_linux_executable(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            # A root-level app and an arbitrary nested app, neither under examples/test_apps.
            binary = root / 'custom-output' / 'program.elf'
            binary.parent.mkdir()
            binary.write_bytes(b'linux executable')
            binary.chmod(0o755)
            ignored = root / 'custom-output' / 'huge-object.o'
            ignored.write_bytes(b'not a runtime artifact')
            esp_bin = root / 'projects/demo/out/bootloader/bootloader.bin'
            esp_bin.parent.mkdir(parents=True)
            esp_bin.write_bytes(b'bootloader')
            metadata = root / 'build_info_1.json'
            records = [
                dict(build_status='build success', app_dir='.', work_dir='.', build_dir='custom-output'),
                dict(build_status='build success', app_dir='projects/demo', work_dir='projects/demo', build_dir='out'),
                dict(build_status='skipped', app_dir='missing', work_dir='missing', build_dir='out'),
            ]
            metadata.write_text('\n'.join(json.dumps(r) for r in records) + '\n')
            archive_path = root / 'binaries.tar'
            PACKAGE.package(metadata, archive_path, root)
            with tarfile.open(archive_path) as archive:
                self.assertEqual(set(archive.getnames()), {
                    'custom-output/program.elf', 'projects/demo/out/bootloader/bootloader.bin',
                })
                self.assertEqual(archive.getmember('custom-output/program.elf').mode, 0o755)

    def test_missing_binary_in_one_successful_app_fails_the_whole_shard(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            good = root / 'good/build'
            good.mkdir(parents=True)
            (good / 'app.elf').write_bytes(b'valid binary')
            metadata = root / 'build_info.json'
            metadata.write_text('\n'.join(json.dumps(dict(
                build_status='build success', work_dir=app, build_dir='build',
            )) for app in ('good', 'missing')) + '\n\n')
            archive = root / 'binaries.tar'
            with self.assertRaisesRegex(ValueError, 'no runtime binaries'):
                PACKAGE.package(metadata, archive, root)
            self.assertFalse(archive.exists())

    def test_archive_rejects_paths_outside_workspace(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            metadata = root / 'build_info.json'
            metadata.write_text(json.dumps(dict(build_status='build success', work_dir='..', build_dir='outside')))
            with self.assertRaises(ValueError):
                PACKAGE.package(metadata, root / 'binaries.tar', root)

    def test_skipped_shard_keeps_metadata_but_does_not_create_binary_archive(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            metadata = root / 'build_info_esp32_1.json'
            metadata.write_text(json.dumps(dict(build_status='skipped')) + '\n')
            archive = root / 'binaries.tar'
            self.assertFalse(PACKAGE.package(metadata, archive, root))
            self.assertTrue(metadata.is_file())
            self.assertFalse(archive.exists())

    def test_changed_paths_are_arguments_not_shell_commands(self):
        files = ['component/file with spaces.c', 'component/$(touch unwanted).c']
        command = BUILD.build_command('esp32', 2, 3, 'build_info.json', files, ['component'])
        self.assertEqual(command[command.index('--modified-files') + 1], ';'.join(files))
        self.assertEqual(command[command.index('--modified-components') + 1], 'component')
        self.assertEqual(command[:4], ['idf-build-apps', 'build', '--target', 'esp32'])

    def test_build_uses_sdk_supported_defaults_for_hardware_and_host(self):
        for target in ['linux', 'esp32', 'esp32c5', 'all']:
            with self.subTest(target=target):
                command = BUILD.build_command(target, 1, 1, 'build_info.json', [], [])
                self.assertNotIn('--enable-preview-targets', command)

    def test_input_lists_reject_non_strings_and_ambiguous_separators(self):
        for value in ['null', '{}', '[1]', '["path;other"]']:
            with self.subTest(value=value), self.assertRaises(ValueError):
                BUILD.string_list(value)
        self.assertEqual(BUILD.string_list('[]'), [])


if __name__ == '__main__':
    unittest.main()
