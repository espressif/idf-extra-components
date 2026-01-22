#!/usr/bin/env python3

import argparse
import copy
import json
import typing as t


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('-o', '--output', type=argparse.FileType('w'), help='Output filtered SARIF file')
    parser.add_argument('--include-prefix', required=True, action='append',
                        help='File prefix for source code to include in analysis')
    parser.add_argument('--exclude-text-contains', action='append', default=[],
                        help='Exclude results whose message.text contains this substring (may be repeated)')
    parser.add_argument('input_file', type=argparse.FileType('r'), help='Input SARIF file')
    args = parser.parse_args()
    process(args.input_file, args.output, args.include_prefix, args.exclude_text_contains)


def process(in_file: t.TextIO, out_file: t.TextIO, include_prefix_list: t.List[str], exclude_text_contains_list: t.List[str]) -> None:
    in_json = json.load(in_file)
    if len(in_json['runs']) != 1:
        raise NotImplementedError('Only 1 run is supported')
    in_results = in_json['runs'][0]['results']
    out_results = []
    for result in in_results:
        transformed = transform_result(result, include_prefix_list, exclude_text_contains_list)
        if transformed is not None:
            out_results.append(transformed)

    out_json = copy.deepcopy(in_json)
    out_json['runs'][0]['results'] = out_results
    json.dump(out_json, out_file, indent=True)


def normalize_uri_optional(uri: t.Optional[str], include_prefix_list: t.List[str], strict: bool) -> t.Optional[str]:
    if uri is None:
        return None
    for include_prefix in include_prefix_list:
        if uri.startswith(include_prefix):
            return uri.replace(include_prefix, '')
    return None if strict else uri


def message_contains_any(text: str, substrings: t.List[str]) -> bool:
    return any(substr in text for substr in substrings)


def dedupe_related_locations(related_locations: t.Any, include_prefix_list: t.List[str]) -> t.List[t.Dict[str, t.Any]]:
    if not isinstance(related_locations, list) or not related_locations:
        return []
    seen_keys: t.Set[t.Tuple[t.Any, ...]] = set()
    deduped: t.List[t.Dict[str, t.Any]] = []
    for rel in related_locations:
        if not isinstance(rel, dict):
            continue
        rel_msg_text = rel['message']['text']
        rel_uri = rel['physicalLocation']['artifactLocation']['uri']
        rel_uri_norm = normalize_uri_optional(rel_uri, include_prefix_list, strict=False)
        rel['physicalLocation']['artifactLocation']['uri'] = rel_uri_norm
        key = (rel_msg_text,
               rel_uri_norm,
               rel['physicalLocation']['region']['startLine'],
               rel['physicalLocation']['region']['startColumn'])
        if key in seen_keys:
            continue
        seen_keys.add(key)
        deduped.append(rel)
    return deduped


def transform_result(result: t.Dict[str, t.Any], include_prefix_list: t.List[str], exclude_text_contains_list: t.List[str]) -> t.Optional[t.Dict[str, t.Any]]:
    locations = result['locations']
    if len(locations) != 1:
        raise NotImplementedError('Only 1 location is supported')
    artifact_location = locations[0]['physicalLocation']['artifactLocation']
    uri = artifact_location['uri']
    normalized_uri = normalize_uri_optional(uri, include_prefix_list, strict=True)
    if not normalized_uri:
        return None
    message_text = result['message']['text']
    if message_contains_any(message_text, exclude_text_contains_list):
        return None
    new_result = copy.deepcopy(result)
    new_result['locations'][0]['physicalLocation']['artifactLocation']['uri'] = normalized_uri
    deduped_related = dedupe_related_locations(new_result.get('relatedLocations'), include_prefix_list)
    if deduped_related:
        new_result['relatedLocations'] = deduped_related
    elif 'relatedLocations' in new_result:
        # Ensure we have a list per schema even if empty
        new_result['relatedLocations'] = []
    return new_result


if __name__ == '__main__':
    main()
