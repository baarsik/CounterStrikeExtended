#!/usr/bin/env python3
"""Emit UTF-8 metric cases from current catalogues, retained layouts and actual schemes."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import sys

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'tools'))
from build_localization import catalogue_tokens


def keyvalues(path):
    """Read these plain resource trees without interpreting backslashes in Windows paths."""
    words = re.findall(r'//[^\n]*|"(?:\\.|[^"\\])*"|[{}]|[^\s{}"]+', path.read_text(encoding='utf-8-sig'))
    words = [word for word in words if not word.startswith('//')]
    index = 0

    def value(word):
        return word[1:-1].replace('\\"', '"') if word.startswith('"') else word

    def block():
        nonlocal index
        result = {}
        while index < len(words):
            word = words[index]
            index += 1
            if word == '}':
                return result
            key = value(word).casefold()
            word = words[index]
            index += 1
            result[key] = block() if word == '{' else value(word)
        return result

    data = block()
    return next(iter(data.values()))


def find_control(tree, name):
    for key, value in tree.items():
        if isinstance(value, dict):
            if key == name.casefold() or value.get('fieldname', '').casefold() == name.casefold():
                return value
            found = find_control(value, name)
            if found is not None:
                return found
    return None


def selected_fonts(scheme, alias, language):
    fonts = scheme['fonts']
    # Label::GetContentSize falls back to Default when a requested alias resolves to zero.
    definitions = fonts.get(alias.casefold(), fonts['default'])
    result = []
    for definition in definitions.values():
        if not isinstance(definition, dict):
            continue
        low, high = map(int, definition.get('yres', '0 0').split())
        if (low or high) and not low <= 480 <= high:
            continue
        tall = int(definition['tall'])
        # Scheme.cpp::GetAdjustedFontHeightForCurrentLanguage.
        if language == 'russian' and tall > 12:
            tall = max(12, int(tall * 0.9))
        result.append(dict(definition, tall=str(tall)))
    if not result:
        raise ValueError('No 480p font for ' + alias)
    return result


def escape(text):
    return str(text).replace('\\', '\\\\').replace('\t', '\\t').replace('\n', '\\n').replace('\r', '\\r')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game-root', type=Path, required=True, help='Installed Half-Life directory with schemes/icons')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    controls_path = ROOT / 'tests/localization_resources/fit_controls.json'
    controls = json.loads(controls_path.read_text())['controls']
    scheme_paths = {
        'tracker': args.game_root / 'platform/resource/TrackerScheme.res',
        'client': args.game_root / 'cstrike/resource/ClientScheme.res',
    }
    schemes = {name: keyvalues(path) for name, path in scheme_paths.items()}
    icon_path = args.game_root / 'platform/resource/icon_emptybox.tga'
    icon_width, icon_height = struct.unpack_from('<HH', icon_path.read_bytes(), 12)
    header = ['type', 'language', 'source', 'control', 'token', 'aliases', 'faces', 'talls', 'weights',
              'italic', 'underline', 'strikeout', 'antialias', 'dropshadow', 'width', 'height',
              'inset_x', 'inset_y', 'indicator_width', 'indicator_height', 'wrap', 'text']
    emitted = []
    inputs = {str(path): hashlib.sha256(path.read_bytes()).hexdigest() for path in scheme_paths.values()}
    inputs[str(controls_path.relative_to(ROOT))] = hashlib.sha256(controls_path.read_bytes()).hexdigest()
    counts = {}
    for language in ('russian', 'polish'):
        tokens = {}
        alphabet = set()
        for path in sorted((ROOT / 'src/deploy/localization').rglob('*_' + language + '.txt')):
            values = catalogue_tokens(path.read_text(encoding='utf-8'))
            tokens.update({key.casefold(): value for key, value in values.items()})
            alphabet.update(character for value in values.values() for character in value if character > ' ')
            inputs[str(path.relative_to(ROOT))] = hashlib.sha256(path.read_bytes()).hexdigest()
        counts[language] = 0
        for control in controls:
            settings = {key.casefold(): value for key, value in control['settings'].items()}
            # Owning admin resources may have changed after a captured stock constraint.
            owned = ROOT / 'src/deploy' / control['resource']
            if owned.is_file():
                actual = find_control(keyvalues(owned), control['control'])
                if actual is None:
                    raise ValueError('Missing owning control: ' + str(owned) + '/' + control['control'])
                settings.update(actual)
                inputs[str(owned.relative_to(ROOT))] = hashlib.sha256(owned.read_bytes()).hexdigest()
            token = settings['labeltext']
            text = tokens[token.lstrip('#').casefold()]
            is_client = control['category'].startswith('cstrike')
            scheme_name = 'client' if is_client else 'tracker'
            alias = settings.get('font', 'Default')
            fonts = selected_fonts(schemes[scheme_name], alias, language)
            kind = control['kind']
            button = 'Button' in kind
            indicator_width = 0
            indicator_height = 0
            if kind in ('CheckButton', 'CCvarToggleCheckButton'):
                indicator_width = icon_width
                indicator_height = icon_height
            elif kind == 'RadioButton':
                indicator_width = 20
                indicator_height = 13
            row = dict(type='control', language=language, source=control['resource'], control=control['control'],
                       token=token, aliases=scheme_name + '/' + alias, faces='|'.join(f['name'] for f in fonts),
                       talls='|'.join(f['tall'] for f in fonts), weights='|'.join(f.get('weight', '0') for f in fonts),
                       width=settings['wide'], height=settings['tall'], inset_x=6 if button else 0, inset_y=0,
                       indicator_width=indicator_width, indicator_height=indicator_height,
                       wrap=settings.get('wrap', '0'), text=text)
            for flag in ('italic', 'underline', 'strikeout', 'antialias', 'dropshadow'):
                row[flag] = '|'.join(f.get(flag, '0') for f in fonts)
            emitted.append(row)
            counts[language] += 1
        for scheme_name, scheme in schemes.items():
            for alias in ('Default', 'DefaultSmall', 'DefaultVerySmall'):
                fonts = selected_fonts(scheme, alias, language)
                row = dict(emitted[-1], type='coverage', source='catalogue-alphabet', control=alias, token='',
                           aliases=scheme_name + '/' + alias, faces='|'.join(f['name'] for f in fonts),
                           talls='|'.join(f['tall'] for f in fonts), weights='|'.join(f.get('weight', '0') for f in fonts),
                           width=0, height=0, inset_x=0, inset_y=0, indicator_width=0, indicator_height=0,
                           wrap=0, text=''.join(sorted(alphabet)))
                for flag in ('italic', 'underline', 'strikeout', 'antialias', 'dropshadow'):
                    row[flag] = '|'.join(f.get(flag, '0') for f in fonts)
                emitted.append(row)
        emitted.append(dict(emitted[-1], type='negative', source='negative-control', control='OverlongLabel',
                            width=24, height=24, text='This deliberately oversized label must overflow.'))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text('\t'.join(header) + '\n' + ''.join(
        '\t'.join(escape(row[column]) for column in header) + '\n' for row in emitted), encoding='utf-8')
    args.output.with_suffix('.provenance.json').write_text(json.dumps({
        'screen': [640, 480], 'control_counts': counts, 'inputs_sha256': inputs,
        'scope': 'Win32 GDI fonts and retained 480p static constraints; supplementary to actual VGUI control tests.',
    }, indent=2) + '\n')
    print('Emitted', counts, 'plus six alphabet checks and one oversized negative control per language')


if __name__ == '__main__':
    main()
