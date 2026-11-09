#!/usr/bin/env python3
"""Bind unselected constant AppCold metadata to immutable post-link image identity. Never dlopen."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys

import engine_bootstrap_evidence as common
import vgui1_app_cold_evidence as evidence


def _fresh_receipt_snapshot(directory):
    """Return parsed facts together with the exact bytes checked by this validation."""
    receipt_bytes = (directory / 'receipt.json').read_bytes()
    receipt = json.loads(receipt_bytes.decode('utf-8'))
    common.require(receipt['status'] == 'accepted_component_only' and receipt['metadata_ready'] is True
                   and receipt['fixture_only'] is False and receipt['cold_load_ready'] is False
                   and receipt['publisher_ready'] is False, 'Not fresh actual unselected metadata evidence')
    common.require(receipt['effective_environment'] == {key: os.environ[key] for key in common.ENVIRONMENT_INPUTS
        if key in os.environ}, 'Compiler environment drift')
    digest = receipt['input_digest']
    for marker in ('accepted.stamp', 'metadata.stamp'):
        common.require((directory / marker).read_text().strip() == digest, 'Missing/revoked metadata stamp')
    common.require(common.file_digest(directory / 'metadata.h') == receipt['metadata_header_sha256']
                   and common.file_digest(directory / 'evidence.h') == receipt['generated_header_sha256'],
                   'Generated header drift')
    for key in ('inputs', 'compilers', 'dependency_hashes'):
        common.verify_inputs(receipt[key])
    provenance = {key: receipt[key] for key in ('format', 'contexts', 'compilers', 'inputs',
        'dependency_hashes', 'effective_environment', 'compiles', 'fixture_only', 'preprocessed_hashes')}
    common.require(common.digest_bytes(common.stable(provenance)) == digest, 'Receipt provenance digest differs')
    observations_bytes = (directory / 'observations.json').read_bytes()
    observations = json.loads(observations_bytes.decode('utf-8'))
    common.require(len(observations) == 3 and len({row['signature'] for row in observations}) == 1
                   and observations[0]['signature'] == receipt['signature'], 'Compiler observations differ')
    expected_header = evidence.metadata_header(observations[0], digest,
        Path(__file__).resolve().parents[1] / 'GoldSrc-SDK/public/cse_vgui1_app_cold_metadata.h')
    common.require((directory / 'metadata.h').read_text() == expected_header, 'Generated metadata disagrees with facts')
    common.require((directory / 'receipt.json').read_bytes() == receipt_bytes
                   and (directory / 'observations.json').read_bytes() == observations_bytes,
                   'Receipt or observations changed during validation')
    return receipt, observations[0], receipt_bytes, observations_bytes


def fresh_receipt(directory):
    receipt, reference, _, _ = _fresh_receipt_snapshot(directory)
    return receipt, reference


def produce(options):
    output = options.output.resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    output.unlink(missing_ok=True)  # A failed refresh can never leave old acceptance.
    snapshot = _fresh_receipt_snapshot(options.evidence.resolve())
    receipt, reference, receipt_bytes, observations_bytes = snapshot
    receipt_digest = common.digest_bytes(receipt_bytes)
    observations_digest = common.digest_bytes(observations_bytes)
    image = options.image.resolve(strict=True)
    inspector = options.inspector.resolve(strict=True)
    before = common.file_digest(image)
    inspector_digest = common.file_digest(inspector)
    result = subprocess.run([str(inspector), str(image)], capture_output=True, timeout=30, check=True)
    facts = json.loads(result.stdout)
    common.require(common.file_digest(image) == before == facts['sha256'], 'Image changed or reader hash disagrees')
    common.require(common.file_digest(inspector) == inspector_digest, 'Reader image drift')
    common.require(facts['provenance'] == receipt['input_digest'], 'Image belongs to another compiler receipt')
    common.require(facts['kind'] == {'offer': 1, 'requirement': 2}[options.kind], 'Image role differs')
    prefix = reference['typed']['prefix']
    expected_catalog = [288, 1, 10, prefix[6], prefix[9] * 8, prefix[7], 3, 0]
    role, role_family, profile_family, app_family = reference['metadata_facts']
    profile = common.fnv(prefix[6:11] + prefix[21:23] + [prefix[26]])
    expected_tables = [[role_family, 0, role], [profile_family, 0, profile], [app_family, 0, reference['signature']]]
    common.require(facts['catalog'] == expected_catalog and facts['tables'] == expected_tables,
                   'Image metadata differs from actual typed compiler facts')
    with image.open('rb') as stream:
        stream.seek(facts['metadata_offset'])
        record = stream.read(352)
    common.require(hashlib.sha256(record).hexdigest() == facts['record_sha256'], 'Record binding differs')
    common.require(_fresh_receipt_snapshot(options.evidence.resolve()) == snapshot,
                   'Receipt or observations changed before publication')
    common.require(common.file_digest(image) == before, 'Image drift before publication')
    manifest = {'format': 1, 'status': 'pinned_unselected_metadata', 'metadata_ready': True,
                'cold_load_ready': False, 'native_service_ready': False, 'canonical_path': str(image),
                'image': facts, 'evidence_directory': str(options.evidence.resolve()),
                'receipt_sha256': receipt_digest, 'observations_sha256': observations_digest,
                'reader': {'path': str(inspector), 'sha256': inspector_digest},
                'trust_boundary': 'immutable staged input, not code signing or a loader namespace guarantee'}
    common.atomic(output, json.dumps(manifest, indent=2) + '\n')
    print('PINNED unselected post-link metadata; cold_load_ready=false')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('image', 'evidence', 'inspector', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--kind', choices=('offer', 'requirement'), required=True)
    options = parser.parse_args()
    try:
        produce(options)
    except Exception as error:
        options.output.unlink(missing_ok=True)
        print('REJECTED:', error, file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
