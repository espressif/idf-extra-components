#!/usr/bin/env python3

import argparse
import copy
import json
import typing


def process(in_file: typing.TextIO, out_file: typing.TextIO, include_prefix_list: typing.List[str]) -> None:
    in_json = json.load(in_file)
    if len(in_json['runs']) != 1:
        raise NotImplementedError('Only 1 run is supported')
    in_results = in_json['runs'][0]['results']
    out_results = []
    for result in in_results:
        locations = result['locations']
        if len(locations) != 1:
            raise NotImplementedError('Only 1 location is supported')
        artifact_location = locations[0]['physicalLocation']['artifactLocation']
        uri = artifact_location['uri']
        new_uri = None
        for include_prefix in include_prefix_list:
            if uri.startswith(include_prefix):
                new_uri = uri.replace(include_prefix, '')
                break
        if not new_uri:
            continue
        new_result = copy.deepcopy(result)
        new_result['locations'][0]['physicalLocation']['artifactLocation']['uri'] = new_uri
        out_results.append(new_result)

    out_json = copy.deepcopy(in_json)
    out_json['runs'][0]['results'] = out_results
    json.dump(out_json, out_file, indent=True)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('-o', '--output', type=argparse.FileType('w'), help='Output filtered SARIF file')
    parser.add_argument('--include-prefix', required=True, action='append',
                        help='File prefix for source code to include in analysis')
    parser.add_argument('input_file', type=argparse.FileType('r'), help='Input SARIF file')
    args = parser.parse_args()
    process(args.input_file, args.output, args.include_prefix)


if __name__ == '__main__':
    main()
