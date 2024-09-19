#!/usr/bin/env python3

import argparse
import os

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('-v', '--verbose', action='store_true', help='Enable verbose output')
    parser.add_argument('modified_files_list', type=argparse.FileType('r'), help='Input file containing list of modified files')
    parser.add_argument('idf_build_apps_args', type=argparse.FileType('w'), help='Output file containing idf-build-apps arguments')
    args = parser.parse_args()

    modified_files = args.modified_files_list.read().splitlines()
    idf_build_apps_args = []
    idf_build_apps_args += [
        '--modified-files',
        ';'.join(modified_files)
        ]
    
    if args.verbose:
        print('Modified files:')
        for file in sorted(modified_files):
            print(f'  - {file}')

    modified_components = set()
    excluded_dirs = ['.github', 'test_app']
    for file in modified_files:
        toplevel = file.split('/')[0]
        if toplevel in excluded_dirs:
            continue
        if not os.path.isdir(toplevel):
            continue
        modified_components.add(toplevel)
    
    idf_build_apps_args += [
        '--modified-components',
        ';'.join(modified_components)
        ]
    
    args.idf_build_apps_args.write(' '.join(idf_build_apps_args))

    if args.verbose:
        print('Modified components:')
        for component in sorted(modified_components):
            print(f'  - {component}')


if __name__ == '__main__':
    main()
