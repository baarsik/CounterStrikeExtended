#!/usr/bin/env python3
"""Generate and restore runtime translations from the maintained UTF-8 catalogue."""
import argparse
from collections import Counter
import json
from pathlib import Path, PurePosixPath
import re
import sys


def catalogue_tokens(text, reject_duplicates=False):
    """Read the token block without treating braces or comments inside strings as syntax."""
    words = []
    position = 0
    escapes = {'n': '\n', 'r': '\r', 't': '\t', '"': '"', '\\': '\\'}
    while position < len(text):
        if text[position].isspace():
            position += 1
        elif text.startswith('//', position):
            end = text.find('\n', position)
            position = len(text) if end < 0 else end + 1
        elif text[position] in '{}':
            words.append(('brace', text[position]))
            position += 1
        elif text[position] == '"':
            match = re.match(r'"(?:\\.|[^"\\])*"', text[position:])
            if not match:
                raise ValueError('Unterminated quoted token')
            value = re.sub(r'\\(.)', lambda item: escapes.get(item[1], item[0]), match[0][1:-1])
            words.append(('string', value))
            position += len(match[0])
        else:
            match = re.match(r'[^\s{}"]+', text[position:])
            if not match:
                raise ValueError('Invalid token syntax')
            words.append(('bare', match[0]))
            position += len(match[0])

    index = 0

    def block(nested):
        nonlocal index
        values = {}
        seen = set()
        while index < len(words):
            kind, key = words[index]
            index += 1
            if kind == 'brace':
                if key == '}' and nested:
                    return values
                raise ValueError('Unexpected token brace')
            if index >= len(words):
                raise ValueError('Token has no value: ' + key)
            value_kind, value = words[index]
            index += 1
            if value_kind == 'brace':
                if value != '{':
                    raise ValueError('Token has no value: ' + key)
                value = block(True)
            if reject_duplicates and key.casefold() in seen:
                raise ValueError('Duplicate project token: ' + key)
            seen.add(key.casefold())
            values[key] = value
            # Valve files can attach platform conditions to a value.
            if index < len(words) and words[index][0] == 'bare' and words[index][1].startswith('['):
                index += 1
        if nested:
            raise ValueError('Unclosed token block')
        return values

    data = block(False)
    for key, value in data.items():
        if key.casefold() == 'lang' and isinstance(value, dict):
            data = value
            break
    tokens = next((value for key, value in data.items() if key.casefold() == 'tokens'), None)
    if not isinstance(tokens, dict) or any(not isinstance(value, str) for value in tokens.values()):
        raise ValueError('Expected a flat Tokens block')
    return tokens


def validate_project_catalogues(catalogues):
    if not catalogues:
        return
    english = next((tokens for language, tokens in catalogues if language == 'english'), None)
    if english is None:
        raise ValueError('Project catalogue requires an English fallback')
    argument_pattern = r'%(?:s\d+|[-+#0-9.* ]*(?:ll|l|h|z|I64)?[csdiuoxXfFeEgGaApn%])'
    for language, tokens in catalogues:
        if tokens.keys() != english.keys():
            raise ValueError('Project token coverage differs from English: ' + language)
        for key, value in tokens.items():
            if Counter(re.findall(argument_pattern, value)) != Counter(re.findall(argument_pattern, english[key])):
                raise ValueError(f'Project substitution arguments differ: {language}/{key}')


def relative_path(value):
    """Require portable runtime-relative paths, including on Windows hosts."""
    if not isinstance(value, str) or not value or '\\' in value or ':' in value:
        raise ValueError('Expected a portable relative path: ' + repr(value))
    path = PurePosixPath(value)
    if path.is_absolute() or '..' in path.parts or str(path) != value:
        raise ValueError('Expected a normalized relative path: ' + repr(value))
    return path


def runtime_files(source):
    """Validate all inputs before returning destination/bytes pairs to stage."""
    manifest = json.loads((source / 'manifest.json').read_text(encoding='utf-8'))
    if manifest.get('format_version') != 1 or not isinstance(manifest.get('files'), list):
        raise ValueError('Unsupported localization manifest format')
    outputs = []
    names = set()
    project_catalogues = []
    for entry in manifest['files']:
        path = relative_path(entry['source'])
        input_path = source / path
        if not input_path.resolve().is_relative_to(source.resolve()):
            raise ValueError('Source must remain relative to the catalogue: ' + str(path))
        text = input_path.read_text(encoding='utf-8')
        if entry.get('kind') in ('vgui', 'project'):
            try:
                tokens = catalogue_tokens(text, entry['kind'] == 'project')
            except ValueError as error:
                raise ValueError(f'{path}: {error}') from error
            if entry['kind'] == 'project':
                project_catalogues.append((entry['language'], tokens))
        encoding = entry['encoding']
        if encoding == 'utf-16-le-bom':
            data = b'\xff\xfe' + text.encode('utf-16-le')
        else:
            data = text.encode(encoding)
        destinations = [entry['destination']] + entry.get('aliases', [])
        for name in destinations:
            destination = relative_path(name)
            # Windows is case-insensitive; reject collisions on every build host.
            key = name.casefold()
            if key in names:
                raise ValueError('Duplicate runtime destination: ' + name)
            names.add(key)
            outputs.append((destination, data))
    validate_project_catalogues(project_catalogues)
    return outputs


def write_changed(path, data):
    if path.is_file() and path.read_bytes() == data:
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)


def english_fallbacks(source):
    text = (source / 'platform/resource/cse_english.txt').read_text(encoding='utf-8')
    entries = [(key, value) for key, value in catalogue_tokens(text, True).items()
               if key.startswith('CSEServerBrowser_')]
    if not entries:
        raise ValueError('English CSE catalogue contains no server-browser tokens')
    lines = ['// Generated from the maintained English CSE catalogue. Do not edit.']
    seen = set()
    for token, value in entries:
        if token.casefold() in seen:
            raise ValueError('Duplicate English CSE token: ' + token)
        seen.add(token.casefold())
        value = json.dumps(value, ensure_ascii=True)
        lines.append(f'AddFallback({json.dumps(token)}, L{value});')
    return ('\n'.join(lines) + '\n').encode('utf-8')


def language_entries(source):
    manifest = json.loads((source / 'manifest.json').read_text(encoding='utf-8'))
    languages = manifest.get('languages', {})
    native_names = manifest.get('native_language_names', {})
    if not languages or languages.keys() != native_names.keys():
        raise ValueError('Every bundled language requires a native name')
    lines = ['// Generated from the maintained language manifest. Do not edit.']
    for identifier, english_name in languages.items():
        if not re.fullmatch(r'[a-z0-9_]{1,31}', identifier):
            raise ValueError('Invalid bundled language identifier: ' + identifier)
        native_name = native_names[identifier]
        if any(not isinstance(name, str) or not name or any(ord(char) < 32 for char in name)
               for name in (english_name, native_name)):
            raise ValueError('Expected nonempty language display names: ' + identifier)
        fields = (json.dumps(identifier), 'L' + json.dumps(native_name, ensure_ascii=True),
                  json.dumps(english_name, ensure_ascii=True))
        lines.append('{' + ', '.join(fields) + '},')
    return ('\n'.join(lines) + '\n').encode('ascii')


def build(source, output, cache=None, fallback_header=None, language_header=None):
    outputs = runtime_files(source)
    fallback = english_fallbacks(source) if fallback_header is not None else None
    languages = language_entries(source) if language_header is not None else None
    for relative, data in outputs:
        if cache is not None:
            write_changed(cache / relative, data)
        write_changed(output / relative, data)
    if fallback_header is not None:
        write_changed(fallback_header, fallback)
    if language_header is not None:
        write_changed(language_header, languages)
    return len(outputs)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--cache', type=Path)
    parser.add_argument('--fallback-header', type=Path)
    parser.add_argument('--language-header', type=Path)
    arguments = parser.parse_args()
    try:
        count = build(arguments.source, arguments.output, arguments.cache, arguments.fallback_header,
                      arguments.language_header)
    except (OSError, ValueError, KeyError, TypeError, LookupError) as error:
        print('Localization generation failed: ' + str(error), file=sys.stderr)
        return 1
    print(f'Staged {count} localization resources')
    return 0


if __name__ == '__main__':
    sys.exit(main())
