#!/usr/bin/env python3
"""Actual App CPP/header controls. Controlled header selections are explicitly fixtures."""
from pathlib import Path
import json
import hashlib
import re
import os
import shlex
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]
TOOL = ROOT / 'tools/vgui1_app_cold_evidence.py'
SCHEMA = ROOT / 'tools/vgui1_app_cold_evidence_schema.json'
FOUNDATION = ROOT / 'src/public/cse_vgui1_app_cold_abi.h'
DEFAULT_DB = Path('/Users/mmalakhouski/.codex/artifacts/cse-native64-20261108/macos-runtime-policy-release/compile_commands.json')


def fixture(directory, actual_pair=False):
    database = Path(os.environ.get('CSE_VGUI1_COLD_DATABASE', str(DEFAULT_DB)))
    if not database.is_file():
        raise unittest.SkipTest('Pass CSE_VGUI1_COLD_DATABASE with actual owning Apple compile database')
    rows = json.loads(database.read_text())
    private = ROOT / 'src/utils/vgui/lib-src/vgui/include'
    public = ROOT / 'src/utils/vgui/include'
    directory.mkdir(parents=True)
    engine_headers = directory / 'engine-headers'
    provider_headers = directory / 'provider-headers'
    # Positive local control uses the real provider declaration in both actual
    # CPPs. It is not an admission of the current native public/private pair.
    shutil.copytree(public if actual_pair else private, engine_headers)
    shutil.copytree(private, provider_headers)
    contexts = []
    for name, role, target, suffix, headers in [
            ('engine', 'consumer', 'hw_engine', '/engine/vgui_intwrap.cpp', engine_headers),
            ('provider', 'provider', 'vgui', '/vgui/App.cpp', provider_headers)]:
        selected = [r for r in rows if r['file'].endswith(suffix)
                    and '/CMakeFiles/' + target + '.dir/' in r['output']]
        assert len(selected) == 1
        row = selected[0]
        args = shlex.split(row['command'])
        args.insert(1, '-I' + str(headers))
        contexts.append({'name': name, 'role': role, 'directory': row['directory'],
                         'source': row['file'], 'arguments': args,
                         'fixture_header_selection': str(headers), 'actual_recipe': row})
    manifest = directory / 'contexts.json'
    manifest.write_text(json.dumps({'format': 1, 'contexts': contexts}, indent=2))
    return manifest


class PipelineTests(unittest.TestCase):
    def setUp(self):
        retained = os.environ.get('CSE_VGUI1_COLD_TEST_ARTIFACTS')
        self.temporary = None
        if retained:
            self.root = (Path(retained) / self._testMethodName).resolve()
            self.root.mkdir(parents=True)
        else:
            self.temporary = tempfile.TemporaryDirectory(prefix='cse-app-cold-')
            self.root = Path(self.temporary.name).resolve()
        self.manifest = fixture(self.root / 'input')
        self.output = self.root / 'evidence'
        self.schema = SCHEMA

    def tearDown(self):
        if self.temporary:
            self.temporary.cleanup()

    def generate(self):
        self.assertTrue(TOOL.is_file(), 'AppCold compiler evidence tool is missing')
        command = [sys.executable, str(TOOL), 'generate', '--schema', str(self.schema),
                   '--contexts', str(self.manifest), '--foundation', str(FOUNDATION),
                   '--output', str(self.output), '--fixture-only']
        snapshots = self.root / 'source-snapshot'
        snapshots.mkdir(exist_ok=True)
        for source in (TOOL, SCHEMA, FOUNDATION, ROOT / 'tools/engine_bootstrap_evidence.py'):
            content = source.read_bytes()
            destination = snapshots / (hashlib.sha256(content).hexdigest() + '-' + source.name)
            destination.write_bytes(content)
        result = subprocess.run(command, capture_output=True, text=True, timeout=180)
        runs = self.root / 'runs'
        runs.mkdir(exist_ok=True)
        (runs / (str(len(list(runs.glob('*.json')))) + '.json')).write_text(json.dumps({
            'arguments': command, 'exit': result.returncode, 'stdout': result.stdout, 'stderr': result.stderr}, indent=2))
        return result

    def test_real_declaration_control_matches_without_execution(self):
        result = self.generate()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        receipt = json.loads((self.output / 'receipt.json').read_text())
        self.assertFalse(receipt['publisher_ready'])
        self.assertFalse(receipt['cold_load_ready'])
        self.assertTrue((self.output / 'accepted.stamp').is_file())

    def test_current_copied_public_private_pair_matches(self):
        self.manifest = fixture(self.root / 'actual-input', actual_pair=True)
        result = self.generate()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertTrue(json.loads((self.output / 'receipt.json').read_text())['fixture_only'])

    def test_historical_four_registry_return_differences_reject(self):
        self.manifest = fixture(self.root / 'actual-input', actual_pair=True)
        header = self.manifest.parent / 'engine-headers/VGUI_App.h'
        text, count = re.subn(r'virtual\s+bool\s+(setRegistryString|getRegistryString|setRegistryInteger|getRegistryInteger)',
                              r'virtual int \1', header.read_text(encoding='cp1252'))
        self.assertEqual(count, 4)
        header.write_text(text, encoding='cp1252')
        result = self.generate()
        self.assertNotEqual(result.returncode, 0)
        failure = json.loads((self.output / 'failure.json').read_text())
        self.assertEqual(failure['typed_method_differences'], [32, 33, 34, 35])
        self.assertFalse((self.output / 'accepted.stamp').exists())
        self.assertFalse((self.output / 'evidence.h').exists())

    def test_same_size_private_field_reorder_rejects(self):
        header = self.manifest.parent / 'provider-headers/VGUI_App.h'
        text = header.read_text()
        first = '\tPanel*\t\t\t\t_keyFocus;'
        second = '\tPanel*\t\t\t\t_oldMouseFocus;'
        self.assertIn(first, text)
        self.assertIn(second, text)
        header.write_text(text.replace(first, 'TEMP_FIELD').replace(second, first).replace('TEMP_FIELD', second))
        result = self.generate()
        self.assertNotEqual(result.returncode, 0)
        self.assertFalse((self.output / 'accepted.stamp').exists())

    def test_same_size_private_bool_to_byte_rejects(self):
        header = self.manifest.parent / 'provider-headers/VGUI_App.h'
        text = header.read_text()
        self.assertIn('bool\t\t\t\t_running;', text)
        header.write_text(text.replace('bool\t\t\t\t_running;', 'unsigned char\t\t_running;'))
        result = self.generate()
        self.assertNotEqual(result.returncode, 0)
        self.assertFalse((self.output / 'accepted.stamp').exists())


    def test_static_borrowed_instance_type_is_observed(self):
        result = self.generate()
        self.assertEqual(result.returncode, 0, result.stderr)
        observations = json.loads((self.output / 'observations.json').read_text())
        for row in observations:
            self.assertEqual(row['typed']['static_fields'][0]['id'], 301)
            self.assertEqual(row['typed']['static_fields'][0]['bytes'], row['typed']['prefix'][9])
            self.assertEqual(row['typed']['static_methods'][0]['name'], 'getInstance')

    def test_reordered_equal_type_methods_cannot_keep_signature_after_schema_update(self):
        self.assertEqual(self.generate().returncode, 0)
        previous = json.loads((self.output / 'receipt.json').read_text())['signature']
        for owner in ('engine', 'provider'):
            header = self.manifest.parent / (owner + '-headers/VGUI_App.h')
            text = header.read_text()
            first = '\tvirtual void start();'
            second = '\tvirtual void stop();'
            self.assertIn(first + '\n' + second, text)
            header.write_text(text.replace(first + '\n' + second, second + '\n' + first))
        schema = json.loads(SCHEMA.read_text())
        schema['methods'][0:2] = schema['methods'][1::-1]
        for index, row in enumerate(schema['methods'], 1):
            row['id'] = index
        self.schema = self.root / 'reviewed-new-order.json'
        self.schema.write_text(json.dumps(schema))
        result = self.generate()
        self.assertEqual(result.returncode, 0, result.stderr)
        current = json.loads((self.output / 'receipt.json').read_text())['signature']
        self.assertNotEqual(previous, current, 'Actual method identity/order cannot disappear behind schema renumbering')

    def test_reordered_equal_type_fields_cannot_keep_signature_after_schema_update(self):
        self.assertEqual(self.generate().returncode, 0)
        previous = json.loads((self.output / 'receipt.json').read_text())['signature']
        for owner in ('engine', 'provider'):
            header = self.manifest.parent / (owner + '-headers/VGUI_App.h')
            text = header.read_text()
            first = '\tPanel*\t\t\t\t_keyFocus;'
            second = '\tPanel*\t\t\t\t_oldMouseFocus;'
            header.write_text(text.replace(first, 'TEMP_FIELD').replace(second, first).replace('TEMP_FIELD', second))
        schema = json.loads(SCHEMA.read_text())
        schema['fields'][3:5] = schema['fields'][4:2:-1]
        for index, row in enumerate(schema['fields'][:25], 1):
            row['id'] = index
        self.schema = self.root / 'reviewed-new-fields.json'
        self.schema.write_text(json.dumps(schema))
        result = self.generate()
        self.assertEqual(result.returncode, 0, result.stderr)
        current = json.loads((self.output / 'receipt.json').read_text())['signature']
        self.assertNotEqual(previous, current, 'Actual field identity/order cannot disappear behind schema renumbering')


    def test_actual_rtti_profile_is_not_omitted(self):
        data = json.loads(self.manifest.read_text())
        for row in data['contexts']:
            row['arguments'] = ['-fno-rtti' if value == '-frtti' else value for value in row['arguments']]
        self.manifest.write_text(json.dumps(data))
        result = self.generate()
        self.assertNotEqual(result.returncode, 0, 'An unqualified no-RTTI profile cannot inherit the same App contract')
        self.assertFalse((self.output / 'accepted.stamp').exists())


    def test_generated_catalog_preserves_envelope_and_refuses_unguarded_use(self):
        self.assertEqual(self.generate().returncode, 0)
        source = self.root / 'catalog.cpp'
        source.write_text('#include "VGUI_App.h"\n#include "' + str(FOUNDATION) + '"\n'
            + '#include "' + str(self.output / 'evidence.h') + '"\n'
            + 'static_assert(cse::vgui1_app_cold_evidence::Catalog.tableCount == 3, "bounded catalog");\n'
            + 'static_assert(cse::abi::RuntimeVersion == 10 && sizeof(cse::abi::Descriptor) == 288, "envelope");\n'
            + 'static_assert(cse::abi::CheckRequired(cse::vgui1_app_cold_evidence::Catalog, '
            + '&cse::vgui1_app_cold_evidence::Catalog) == cse::abi::Compatibility::Compatible, "typed physical catalog");\n')
        args = [shutil.which('c++'), '-std=c++14', '-I' + str(self.manifest.parent / 'provider-headers'),
                '-c', str(source), '-o', str(self.root / 'catalog.o')]
        denied = subprocess.run(args, capture_output=True, text=True)
        self.assertNotEqual(denied.returncode, 0)
        self.assertIn('component-only', denied.stderr)
        accepted = subprocess.run(args + ['-DCSE_VGUI1_APP_COLD_COMPONENT_EVIDENCE'], capture_output=True, text=True)
        self.assertEqual(accepted.returncode, 0, accepted.stdout + accepted.stderr)


    def compiler_wrapper(self, action=''):
        wrapper = self.root / 'controlled-compiler'
        wrapper.write_text('#!/usr/bin/env python3\nimport subprocess,sys\nfrom pathlib import Path\n'
            + action + '\nresult = subprocess.run([' + repr(shutil.which('c++')) + '] + sys.argv[1:], capture_output=True)\n'
            + 'sys.stdout.buffer.write(result.stdout)\nsys.stderr.buffer.write(result.stderr)\nsys.exit(result.returncode)\n')
        wrapper.chmod(0o755)
        data = json.loads(self.manifest.read_text())
        for row in data['contexts']:
            row['arguments'][0] = str(wrapper)
        self.manifest.write_text(json.dumps(data))
        return wrapper

    def assert_revoked(self, result, reason=None):
        self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertFalse((self.output / 'accepted.stamp').exists())
        self.assertFalse((self.output / 'evidence.h').exists())
        if reason:
            failure = json.loads((self.output / 'failure.json').read_text())
            self.assertIn(reason, failure['error'])

    def test_virtual_reorder_revokes_warm_acceptance(self):
        self.assertEqual(self.generate().returncode, 0)
        header = self.manifest.parent / 'provider-headers/VGUI_App.h'
        text = header.read_text()
        header.write_text(text.replace('\tvirtual void start();\n\tvirtual void stop();',
            '\tvirtual void stop();\n\tvirtual void start();'))
        self.assert_revoked(self.generate(), 'physical method order drift')

    def test_overload_reorder_is_distinct_from_equal_names(self):
        self.assertEqual(self.generate().returncode, 0)
        header = self.manifest.parent / 'provider-headers/VGUI_App.h'
        text = header.read_text()
        first = '\tvirtual void setMouseArena( int x0, int y0, int x1, int y1, bool enabled );'
        second = '\tvirtual void setMouseArena( Panel* panel );'
        self.assertIn(first + '\n' + second, text)
        header.write_text(text.replace(first + '\n' + second, second + '\n' + first))
        self.assert_revoked(self.generate(), 'physical method order drift')

    def test_inserted_virtual_is_not_hidden_by_type_only_checks(self):
        self.assertEqual(self.generate().returncode, 0)
        header = self.manifest.parent / 'provider-headers/VGUI_App.h'
        text = header.read_text()
        header.write_text(text.replace('\tvirtual void stop();', '\tvirtual void stop();\n\tvirtual void extraColdSlot();'))
        self.assert_revoked(self.generate())

    def test_dar_same_size_count_capacity_reorder_rejects(self):
        self.assertEqual(self.generate().returncode, 0)
        header = self.manifest.parent / 'provider-headers/VGUI_Dar.h'
        text = header.read_text()
        first = '\tint\t\t\t_count;'
        second = '\tint\t\t\t_capacity;'
        self.assertIn(first + '\n' + second, text)
        header.write_text(text.replace(first + '\n' + second, second + '\n' + first))
        self.assert_revoked(self.generate(), 'field order/count drift')

    def test_unknown_compiler_layout_report_revokes_warm_acceptance(self):
        marker = self.root / 'corrupt-layout-report'
        wrapper = self.compiler_wrapper()
        self.assertEqual(self.generate().returncode, 0)
        code = wrapper.read_text().replace('sys.stdout.buffer.write(result.stdout)',
            'result.stdout = result.stdout.replace(b"*** Dumping AST Record Layout", b"UNKNOWN LAYOUT FORMAT")\n'
            + 'sys.stdout.buffer.write(result.stdout)')
        code = code.replace('sys.stderr.buffer.write(result.stderr)',
            'result.stderr = result.stderr.replace(b"*** Dumping AST Record Layout", b"UNKNOWN LAYOUT FORMAT")\n'
            + 'sys.stderr.buffer.write(result.stderr)')
        wrapper.write_text(code)
        self.assert_revoked(self.generate(), 'Missing/unknown actual class layouts')

    def test_transitive_header_drift_changes_freshness_not_compatibility(self):
        self.assertEqual(self.generate().returncode, 0)
        before = json.loads((self.output / 'receipt.json').read_text())
        header = self.manifest.parent / 'provider-headers/VGUI_KeyCode.h'
        header.write_bytes(header.read_bytes() + b'\n// harmless transitive header drift\n')
        self.assertEqual(self.generate().returncode, 0)
        after = json.loads((self.output / 'receipt.json').read_text())
        self.assertEqual(before['signature'], after['signature'])
        self.assertNotEqual(before['input_digest'], after['input_digest'])
        self.assertNotEqual(before['dependency_hashes'][str(header)], after['dependency_hashes'][str(header)])

    def test_response_file_and_effective_option_drift_are_fresh(self):
        response = self.root / 'effective-options.rsp'
        response.write_text('-DCSE_UNUSED_COLD_OPTION=1')
        data = json.loads(self.manifest.read_text())
        for row in data['contexts']:
            row['arguments'].insert(1, '@' + str(response))
        self.manifest.write_text(json.dumps(data))
        self.assertEqual(self.generate().returncode, 0)
        before = json.loads((self.output / 'receipt.json').read_text())
        response.write_text('-DCSE_UNUSED_COLD_OPTION=2')
        self.assertEqual(self.generate().returncode, 0)
        after = json.loads((self.output / 'receipt.json').read_text())
        self.assertEqual(before['signature'], after['signature'])
        self.assertNotEqual(before['input_digest'], after['input_digest'])
        self.assertIn(str(response), after['inputs'])
        self.assertTrue(all('-DCSE_UNUSED_COLD_OPTION=2' in row['arguments'] for row in after['contexts']))

    def test_compiler_identity_drift_forces_fresh_observation(self):
        wrapper = self.compiler_wrapper()
        self.assertEqual(self.generate().returncode, 0)
        before = json.loads((self.output / 'receipt.json').read_text())
        wrapper.write_text(wrapper.read_text() + '\n# compiler transport identity changed\n')
        self.assertEqual(self.generate().returncode, 0)
        after = json.loads((self.output / 'receipt.json').read_text())
        self.assertNotEqual(before['compilers'][str(wrapper)], after['compilers'][str(wrapper)])
        self.assertNotEqual(before['input_digest'], after['input_digest'])
        self.assertEqual(before['signature'], after['signature'])
        self.assertEqual(len(after['compiles']), 2)

    def test_later_preprocess_mutation_of_earlier_unique_header_revokes_acceptance(self):
        armed = self.root / 'arm-earlier-header-mutation'
        header = self.manifest.parent / 'engine-headers/VGUI_App.h'
        self.compiler_wrapper('armed=Path(' + repr(str(armed)) + ')\n'
            + 'if "-E" in sys.argv and armed.exists() and any("/contexts/provider/probe.cpp" in value for value in sys.argv):\n'
            + '    path=Path(' + repr(str(header)) + ')\n'
            + '    path.write_bytes(path.read_bytes()+b"\\n// changed during later preprocessing\\n")\n'
            + '    armed.unlink()\n')
        self.assertEqual(self.generate().returncode, 0)
        armed.touch()
        result = self.generate()
        self.assertFalse(armed.exists(), 'The later-context mutation must actually execute')
        self.assert_revoked(result, 'changed during')

    def test_corrupt_prior_receipt_is_never_an_acceptance_fallback(self):
        self.assertEqual(self.generate().returncode, 0)
        (self.output / 'receipt.json').write_text('{ corrupt stale receipt')
        self.assertEqual(self.generate().returncode, 0)
        current = json.loads((self.output / 'receipt.json').read_text())
        self.assertEqual(current['status'], 'accepted_component_only')
        self.assertEqual(len(current['compiles']), 2)
        (self.output / 'receipt.json').write_text('{ corrupt stale receipt again')
        data = json.loads(self.manifest.read_text())
        data['contexts'][0]['arguments'][0] = str(self.root / 'missing-compiler')
        self.manifest.write_text(json.dumps(data))
        self.assert_revoked(self.generate(), 'Compiler executable missing')

    def test_relocation_changes_provenance_not_semantic_compatibility(self):
        self.assertEqual(self.generate().returncode, 0)
        before = json.loads((self.output / 'receipt.json').read_text())
        self.manifest = fixture(self.root / 'relocated path with spaces')
        self.output = self.root / 'relocated-evidence'
        self.assertEqual(self.generate().returncode, 0)
        after = json.loads((self.output / 'receipt.json').read_text())
        self.assertNotEqual(before['input_digest'], after['input_digest'])
        self.assertEqual(before['signature'], after['signature'])


    def test_observer_format_and_provenance_are_not_compatibility_facts(self):
        self.assertEqual(self.generate().returncode, 0)
        sys.path.insert(0, str(ROOT / 'tools'))
        import vgui1_app_cold_evidence as evidence
        actual = json.loads((self.output / 'observations.json').read_text())[0]
        changed = json.loads(json.dumps(actual))
        changed['context'] = 'relocated-observer'
        changed['preprocessed_sha256'] = 'different freshness input'
        changed['typed']['prefix'][0] += 1
        changed['typed']['prefix'][1] += 1
        self.assertEqual(evidence.semantic_words(actual), evidence.semantic_words(changed))


if __name__ == '__main__':
    unittest.main()
