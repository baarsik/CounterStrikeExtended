#!/usr/bin/env python3
"""Install the supplied SVD test assets, without installing its legacy plugin."""

import argparse
from hashlib import sha256
from pathlib import Path, PurePosixPath
import stat
from tempfile import NamedTemporaryFile
import zipfile


MODEL_NAMES = ('p_svdex', 'shell_svdex', 'v_svdex', 'w_svdex')
SOUND_NAMES = ('svdex-1', 'svdex-launcher', 'svdex_clipin', 'svdex_clipon', 'svdex_clipout', 'svdex_draw',
               'svdex_exp', 'svdex_foley1', 'svdex_foley2', 'svdex_foley3', 'svdex_foley4')
SPRITE_NAMES = ('640hud36', '640hud41', 'scope_vip_grenade')
ASSETS = (
    *(f'models/nimroK/weapons/{name}.mdl' for name in MODEL_NAMES),
    *(f'sound/weapons/{name}.wav' for name in SOUND_NAMES),
    *(f'sprites/nimroK/{name}.spr' for name in SPRITE_NAMES),
    'sprites/nimroK/weapon_svdex.txt',
)
DEFINITION = 'weapon_svdex.cfg'
DEFINITION_DESTINATION = f'addons/amxmodx/data/csextended/weapons/{DEFINITION}'
HUD_DESCRIPTOR_DESTINATION = 'sprites/nimroK/weapon_svdex_cse.txt'
KILLFEED_DESTINATION = 'sprites/nimroK/svdex-killfeed.webp'
MAX_ASSET_BYTES = 2 * 1024 * 1024
# Descriptors shipped before the optional WebP row was added.
LEGACY_HUD_SHA256 = {
    'b41cb79209f9e278e7762fb5185c62219f44e9cbc28b626c7f0e4647eecb5d8b',
    'fcecdc0c0c363e837529d3c3299d6cc402c0004b7ba43ddb28335bbecf9f1e0b',
}


def read_assets(archive: Path) -> dict[str, bytes]:
    with zipfile.ZipFile(archive) as source:
        files = {}
        for asset in ASSETS:
            matches = [info for info in source.infolist() if info.filename == f'weapon_svdex/{asset}']
            if len(matches) != 1:
                raise ValueError(f'Expected one archive member for {asset}')
            info = matches[0]
            if not 0 < info.file_size <= MAX_ASSET_BYTES or stat.S_ISLNK(info.external_attr >> 16):
                raise ValueError(f'Invalid archive member {asset}')
            data = source.read(info)
            suffix = PurePosixPath(asset).suffix
            if suffix == '.mdl' and not data.startswith(b'IDST'):
                raise ValueError(f'Invalid studio model {asset}')
            if suffix == '.spr' and not data.startswith(b'IDSP'):
                raise ValueError(f'Invalid sprite {asset}')
            if suffix == '.wav' and (not data.startswith(b'RIFF') or data[8:12] != b'WAVE'):
                raise ValueError(f'Invalid wave sound {asset}')
            files[asset] = data
    files[DEFINITION_DESTINATION] = Path(__file__).with_name(DEFINITION).read_bytes()
    files[HUD_DESCRIPTOR_DESTINATION] = Path(__file__).with_name('weapon_svdex_hud.txt').read_bytes()
    return files


def install(archive: Path, game: Path, killfeed: Path | None = None) -> None:
    game = game.resolve(strict=True)
    if not game.is_dir():
        raise ValueError(f'Expected a game directory at {game}')
    files = read_assets(archive)
    if killfeed is not None:
        with killfeed.open('rb') as image:
            data = image.read(MAX_ASSET_BYTES + 1)
        if len(data) > MAX_ASSET_BYTES or not data.startswith(b'RIFF') or data[8:12] != b'WEBP':
            raise ValueError(f'Invalid WebP image {killfeed}')
        files[KILLFEED_DESTINATION] = data
    upgrade_hud = False
    for relative, data in files.items():
        target = game / relative
        for component in (target, *target.relative_to(game).parents):
            candidate = component if component.is_absolute() else game / component
            if candidate.is_symlink():
                raise ValueError(f'Refusing a symlink in {target}')
            if candidate != target and candidate.exists() and not candidate.is_dir():
                raise ValueError(f'Expected a directory at {candidate}')
        if target.exists():
            existing = target.read_bytes() if target.is_file() else None
            if existing == data:
                continue
            if relative == HUD_DESCRIPTOR_DESTINATION and existing is not None:
                upgrade_hud = sha256(existing).hexdigest() in LEGACY_HUD_SHA256
                if upgrade_hud:
                    continue
            raise ValueError(f'Refusing to overwrite {target}')
    for relative, data in files.items():
        target = game / relative
        if target.exists():
            continue
        target.parent.mkdir(parents=True, exist_ok=True)
        with target.open('xb') as output:
            output.write(data)
    if upgrade_hud:
        target = game / HUD_DESCRIPTOR_DESTINATION
        with NamedTemporaryFile(dir=target.parent, delete=False) as output:
            staged = Path(output.name)
            try:
                output.write(files[HUD_DESCRIPTOR_DESTINATION])
                output.close()
                staged.chmod(stat.S_IMODE(target.stat().st_mode))
                staged.replace(target)
            finally:
                staged.unlink(missing_ok=True)
    print(f'Installed {len(ASSETS)} SVD test assets, the CSE definition, and the HUD descriptor into {game}')
    if killfeed is not None:
        print(f'Installed {KILLFEED_DESTINATION}')


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('archive', type=Path)
    parser.add_argument('game', type=Path, help='Existing cstrike game directory')
    parser.add_argument('--killfeed-webp', type=Path, help='Optional SVD killfeed image')
    args = parser.parse_args()
    try:
        install(args.archive, args.game, args.killfeed_webp)
    except (OSError, ValueError, zipfile.BadZipFile) as error:
        parser.exit(1, f'{error}\n')


if __name__ == '__main__':
    main()
