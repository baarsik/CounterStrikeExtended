#!/usr/bin/env python3
"""Independent compiler/build evidence controls; no runtime module execution."""
import importlib.util
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]
TOOL = ROOT / 'tools/engine_bootstrap_evidence.py'
SCHEMA = ROOT / 'tools/engine_bootstrap_evidence_schema.json'
FOUNDATION = ROOT / 'GoldSrc-SDK/public/cse_engine_bootstrap_abi.h'


def exact_declaration(path, name):
    text = path.read_text(encoding='latin-1')
    start = text.index('class ' + name + ' : public IBaseInterface')
    stop = text.index('\n};', start) + 3
    return text[start:stop]


def fixture(directory):
    directory.mkdir(parents=True, exist_ok=True)
    sdk = ROOT / 'GoldSrc-SDK'
    edge = directory / 'edge.h'
    edge.write_text('#pragma once\n#define PIPELINE_EXTRA_SLOT\n')
    engine = sdk / 'engine/sys_dll2.cpp'
    definitions = '\n'.join(exact_declaration(engine, name)
                            for name in ['IEngineAPI', 'IDedicatedServerAPI', 'IDedicatedExports'])
    definitions = definitions.replace('public:\n\tvirtual bool Init', 'public:\n    PIPELINE_EXTRA_SLOT\n\tvirtual bool Init')
    common = '#include "' + (sdk / 'public/FileSystem.h').as_posix() + '"\n#include "edge.h"\n'
    (directory / 'engine.cpp').write_text(common + definitions + '\n')
    (directory / 'launcher.cpp').write_text(common + '#include "'
        + (sdk / 'launcher/public/engine_launcher_api.h').as_posix() + '"\n')
    dedicated = exact_declaration(sdk / 'dedicated/sys_ded.cpp', 'IDedicatedServerAPI')
    (directory / 'dedicated.cpp').write_text(common + dedicated + '\n#include "'
        + (sdk / 'public/idedicatedexports.h').as_posix() + '"\n')
    compiler = shutil.which('c++')
    contexts = []
    for role in ['engine', 'launcher', 'dedicated']:
        source = directory / (role + '.cpp')
        contexts.append({'name': role, 'role': role, 'source': str(source),
                         'directory': str(directory),
                         'arguments': [compiler, '-std=c++14', '-DOSX', '-DPOSIX', '-Wall', '-Wextra', '-Werror',
                                       '-I' + str(sdk / 'public'), '-c', str(source), '-o', str(directory / (role + '.o'))],
                         'filesystem_abi_header': str(sdk / 'public/cse_filesystem_abi.h')})
    manifest = directory / 'contexts.json'
    manifest.write_text(json.dumps({'format': 1, 'contexts': contexts, 'constructor_requirements': {'state': 'fixture-only', 'edges': []}}, indent=2))
    return manifest


class PipelineTests(unittest.TestCase):
    def setUp(self):
        retained = os.environ.get('CSE_EVIDENCE_TEST_ARTIFACTS')
        self.temporary = None
        if retained:
            self.root = Path(retained) / self._testMethodName
            self.root.mkdir(parents=True, exist_ok=False)
        else:
            self.temporary = tempfile.TemporaryDirectory(prefix='cse-bootstrap-evidence-')
            self.root = Path(self.temporary.name)
        self.manifest = fixture(self.root / 'input')
        self.output = self.root / 'evidence'
        self.schema = SCHEMA

    def tearDown(self):
        if self.temporary:
            self.temporary.cleanup()

    def generate(self, fixture_only=True):
        command = ['python3', str(TOOL), 'generate', '--schema', str(self.schema), '--contexts', str(self.manifest),
                   '--foundation', str(FOUNDATION), '--output', str(self.output)]
        if fixture_only:
            command.append('--fixture-only')
        result = subprocess.run(command, capture_output=True, text=True, timeout=120)
        if not self.temporary:
            runs = self.root / 'runs'
            runs.mkdir(exist_ok=True)
            number = len(list(runs.glob('*.json')))
            (runs / (str(number) + '.json')).write_text(json.dumps({'arguments': command, 'exit': result.returncode,
                'stdout': result.stdout, 'stderr': result.stderr}, indent=2))
        return result

    def test_corrupt_previous_receipt_cannot_leave_warm_acceptance(self):
        self.assertEqual(self.generate().returncode, 0)
        self.assertTrue((self.output / 'accepted.stamp').exists())
        self.assertTrue((self.output / 'evidence.h').exists())
        (self.output / 'receipt.json').write_text('{ malformed previous receipt')
        result = self.generate()
        self.assertNotEqual(result.returncode, 0)
        self.assertFalse((self.output / 'accepted.stamp').exists())
        self.assertFalse((self.output / 'evidence.h').exists())

    def test_warm_later_preprocess_cannot_accept_an_earlier_changed_header(self):
        earlier = self.manifest.parent / 'engine-only.h'
        earlier.write_text('// engine-only transitive dependency\n')
        engine = self.manifest.parent / 'engine.cpp'
        engine.write_text('#include "engine-only.h"\n' + engine.read_text())
        compiler = shutil.which('c++')
        wrapper = self.root / 'later-context-compiler'
        armed = self.root / 'arm-later-preprocess-mutation'
        wrapper.write_text('#!/usr/bin/env python3\nimport os,sys\nfrom pathlib import Path\n'
            + 'armed = Path(' + repr(str(armed)) + ')\n'
            + 'if "-E" in sys.argv and armed.exists() and any("/contexts/launcher/probe.cpp" in arg for arg in sys.argv):\n'
            + '    with Path(' + repr(str(earlier)) + ').open("a") as stream:\n'
            + '        stream.write("// changed after earlier context preprocessing\\n")\n'
            + '    armed.unlink()\n'
            + 'os.execv(' + repr(compiler) + ', [' + repr(compiler) + '] + sys.argv[1:])\n')
        wrapper.chmod(0o755)
        data = json.loads(self.manifest.read_text())
        for context in data['contexts']:
            if context['role'] == 'launcher':
                context['arguments'][0] = str(wrapper)
        self.manifest.write_text(json.dumps(data))
        self.assertEqual(self.generate().returncode, 0)
        armed.touch()
        result = self.generate()
        self.assertFalse(armed.exists(), 'The controlled later-context mutation did not run')
        self.assertNotEqual(result.returncode, 0)
        self.assertFalse((self.output / 'accepted.stamp').exists())
        self.assertFalse((self.output / 'evidence.h').exists())

    def test_positive_and_cached_rebuild(self):
        result = self.generate()
        self.assertEqual(result.returncode, 0, result.stderr)
        before = (self.output / 'evidence.h').read_bytes()
        result = self.generate()
        self.assertEqual(result.returncode, 0, result.stderr)
        receipt = json.loads((self.output / 'receipt.json').read_text())
        self.assertTrue(receipt['cache_hit'])
        self.assertEqual(before, (self.output / 'evidence.h').read_bytes())
        self.assertFalse(receipt['publisher_ready'])

    def test_transitive_header_drift_rebuilds(self):
        self.assertEqual(self.generate().returncode, 0)
        before = json.loads((self.output / 'receipt.json').read_text())
        edge = self.manifest.parent / 'edge.h'
        edge.write_text(edge.read_text() + '// transitive non-ABI input changed\n')
        result = self.generate()
        self.assertEqual(result.returncode, 0, result.stderr)
        after = json.loads((self.output / 'receipt.json').read_text())
        self.assertFalse(after['cache_hit'])
        self.assertNotEqual(before['input_digest'], after['input_digest'])
        self.assertEqual(before['physical_signature'], after['physical_signature'])

    def test_inserted_virtual_removes_stale_acceptance(self):
        self.assertEqual(self.generate().returncode, 0)
        (self.manifest.parent / 'edge.h').write_text(
            '#pragma once\n#define PIPELINE_EXTRA_SLOT virtual void UnexpectedPhysicalSlot() = 0;\n')
        result = self.generate()
        self.assertNotEqual(result.returncode, 0)
        self.assertFalse((self.output / 'accepted.stamp').exists())
        self.assertFalse((self.output / 'evidence.h').exists())

    def test_production_mode_requires_real_constructor_requirements(self):
        result = self.generate(fixture_only=False)
        self.assertNotEqual(result.returncode, 0)
        self.assertFalse((self.output / 'accepted.stamp').exists())

    def test_response_file_and_effective_options_are_freshness_inputs(self):
        response = self.manifest.parent / 'options.rsp'
        response.write_text('-DPIPELINE_UNUSED_OPTION=1')
        data = json.loads(self.manifest.read_text())
        for context in data['contexts']:
            context['arguments'].insert(1, '@' + str(response))
        self.manifest.write_text(json.dumps(data))
        self.assertEqual(self.generate().returncode, 0)
        before = json.loads((self.output / 'receipt.json').read_text())
        response.write_text('-DPIPELINE_UNUSED_OPTION=2')
        result = self.generate()
        self.assertEqual(result.returncode, 0, result.stderr)
        after = json.loads((self.output / 'receipt.json').read_text())
        self.assertFalse(after['cache_hit'])
        self.assertNotEqual(before['input_digest'], after['input_digest'])
        self.assertEqual(before['physical_signature'], after['physical_signature'])
        self.assertIn(str(response.resolve()), after['inputs'])

    def test_reordered_actual_methods_and_missing_requirements_reject(self):
        self.assertEqual(self.generate().returncode, 0)
        source = self.manifest.parent / 'engine.cpp'
        text = source.read_text()
        original = '\tvirtual int Shutdown() = 0;\n\tvirtual bool RunFrame() = 0;'
        self.assertIn(original, text)
        source.write_text(text.replace(original,
            '\tvirtual bool RunFrame() = 0;\n\tvirtual int Shutdown() = 0;'))
        self.assertNotEqual(self.generate().returncode, 0)
        self.assertFalse((self.output / 'accepted.stamp').exists())
        data = json.loads(self.manifest.read_text())
        del data['constructor_requirements']
        self.manifest.write_text(json.dumps(data))
        self.assertNotEqual(self.generate().returncode, 0)

    def test_reordered_source_cannot_be_excused_by_editing_only_the_json_schema(self):
        self.assertEqual(self.generate().returncode, 0)
        source = self.manifest.parent / 'engine.cpp'
        source.write_text(source.read_text().replace(
            '\tvirtual int Shutdown() = 0;\n\tvirtual bool RunFrame() = 0;',
            '\tvirtual bool RunFrame() = 0;\n\tvirtual int Shutdown() = 0;'))
        data = json.loads(SCHEMA.read_text())
        data['interfaces']['IDedicatedServerAPI']['methods'][1:3] = ['RunFrame', 'Shutdown']
        for mapping in data['header_method_mapping']:
            if mapping['interface'] == 'IDedicatedServerAPI':
                mapping['ordinal'] = data['interfaces']['IDedicatedServerAPI']['methods'].index(mapping['method'])
        self.schema = self.root / 'modified-schema.json'
        self.schema.write_text(json.dumps(data))
        result = self.generate()
        self.assertNotEqual(result.returncode, 0)
        self.assertFalse((self.output / 'accepted.stamp').exists())

    def test_unknown_report_and_changed_base_view_fail_closed(self):
        spec = importlib.util.spec_from_file_location('bootstrap_evidence_tool', TOOL)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        schema = json.loads(SCHEMA.read_text())
        with self.assertRaises(module.EvidenceError):
            module.parse_layout('unknown compiler output', False, ['IEngineAPI'], schema)
        self.assertEqual(self.generate().returncode, 0)
        source = self.manifest.parent / 'engine.cpp'
        text = source.read_text().replace('class IEngineAPI : public IBaseInterface',
            'class OtherBase { public: virtual ~OtherBase() {} };\n'
            'class IEngineAPI : public OtherBase, public IBaseInterface')
        source.write_text(text)
        self.assertNotEqual(self.generate().returncode, 0)
        self.assertFalse((self.output / 'accepted.stamp').exists())

    def test_relocation_changes_freshness_not_semantic_compatibility(self):
        self.assertEqual(self.generate().returncode, 0)
        first = json.loads((self.output / 'receipt.json').read_text())
        self.manifest = fixture(self.root / 'relocated')
        self.output = self.root / 'relocated-evidence'
        self.assertEqual(self.generate().returncode, 0)
        second = json.loads((self.output / 'receipt.json').read_text())
        self.assertNotEqual(first['input_digest'], second['input_digest'])
        self.assertEqual(first['physical_signature'], second['physical_signature'])

    def test_consumer_owned_cold_requirements_and_image_identity(self):
        family = str(int('4f4d0200', 16))
        images = []
        for name in ['engine', 'vgui', 'launcher']:
            path = self.root / (name + '.owned-fixture-bytes')
            path.write_bytes(('owned identity fixture ' + name).encode())
            images.append({'name': name, 'path': str(path), 'sha256': hashlib.sha256(path.read_bytes()).hexdigest(),
                           'provided': {family: 1234} if name == 'vgui' else {},
                           'required': {'vgui': {family: 1234}} if name in ['engine', 'launcher'] else {}})
        data = json.loads(self.manifest.read_text())
        data['constructor_requirements'] = {'state': 'modeled', 'images': images, 'edges': [
            {'consumer': 'engine', 'provider': 'vgui', 'family': family},
            {'consumer': 'launcher', 'provider': 'vgui', 'family': family}]}
        self.manifest.write_text(json.dumps(data))
        result = self.generate()
        self.assertEqual(result.returncode, 0, result.stderr)
        model = json.loads((self.output / 'metadata.json').read_text())
        self.assertFalse(model['cold_load_ready'])
        self.assertFalse(model['constructor_identity_model']['actual_constructor_safety'])
        # Launcher's expectation still matches. A different ENGINE requirement must reject.
        images[0]['required']['vgui'][family] = 5678
        self.manifest.write_text(json.dumps(data))
        self.assertNotEqual(self.generate().returncode, 0)
        self.assertFalse((self.output / 'accepted.stamp').exists())
        images[0]['required']['vgui'][family] = 1234
        self.manifest.write_text(json.dumps(data))
        Path(images[1]['path']).write_bytes(b'different staged provider bytes')
        self.assertNotEqual(self.generate().returncode, 0)

    def test_actual_cmake_database_options_are_replayed(self):
        project = self.manifest.parent
        text = '''cmake_minimum_required(VERSION 3.25)
project(ActualContextFixture LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
'''
        for role in ['engine', 'launcher', 'dedicated']:
            text += 'add_library(ctx_' + role + ' OBJECT ' + role + '.cpp)\n'
            text += 'target_compile_definitions(ctx_' + role + ' PRIVATE OSX POSIX ACTUAL_CONTEXT_OPTION=1)\n'
            text += 'target_include_directories(ctx_' + role + ' PRIVATE "' + (ROOT / 'GoldSrc-SDK/public').as_posix() + '")\n'
        (project / 'CMakeLists.txt').write_text(text)
        build = self.root / 'actual-context-build'
        result = subprocess.run(['cmake', '-S', str(project), '-B', str(build), '-G', 'Ninja'],
                                capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        data = json.loads(self.manifest.read_text())
        data['compile_database'] = str(build / 'compile_commands.json')
        for context in data['contexts']:
            del context['arguments']
            context['target'] = 'ctx_' + context['role']
        self.manifest.write_text(json.dumps(data))
        result = self.generate()
        self.assertEqual(result.returncode, 0, result.stderr)
        receipt = json.loads((self.output / 'receipt.json').read_text())
        self.assertTrue(all('ACTUAL_CONTEXT_OPTION=1' in ' '.join(row['arguments']) for row in receipt['contexts']))

    def test_during_compile_header_change_invalidates_the_result(self):
        compiler = shutil.which('c++')
        wrapper = self.root / 'compiler-wrapper'
        edge = self.manifest.parent / 'edge.h'
        wrapper.write_text('#!/usr/bin/env python3\nimport os,sys\nfrom pathlib import Path\n'
                           + 'if "-S" in sys.argv:\n    with Path(' + repr(str(edge))
                           + ').open("a") as stream:\n        stream.write("// changed during actual compile\\n")\n'
                           + 'os.execv(' + repr(compiler) + ', [' + repr(compiler) + '] + sys.argv[1:])\n')
        wrapper.chmod(0o755)
        data = json.loads(self.manifest.read_text())
        for context in data['contexts']:
            context['arguments'][0] = str(wrapper)
        self.manifest.write_text(json.dumps(data))
        result = self.generate()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('changed during compiler replay', result.stderr)
        self.assertFalse((self.output / 'accepted.stamp').exists())

    def test_new_higher_priority_header_cannot_reuse_a_cached_old_closure(self):
        high = self.root / 'high priority headers'
        low = self.root / 'fallback headers'
        high.mkdir()
        low.mkdir()
        (low / 'selection.h').write_text('// initially selected fallback header\n')
        data = json.loads(self.manifest.read_text())
        for context in data['contexts']:
            context['arguments'][1:1] = ['-I' + str(high), '-I' + str(low)]
        self.manifest.write_text(json.dumps(data))
        source = self.manifest.parent / 'engine.cpp'
        source.write_text(source.read_text().replace('#include "edge.h"',
            '#include "edge.h"\n#include <selection.h>'))
        self.assertEqual(self.generate().returncode, 0)
        (high / 'selection.h').write_text('#undef PIPELINE_EXTRA_SLOT\n'
            '#define PIPELINE_EXTRA_SLOT virtual void NewShadowHeaderSlot() = 0;\n')
        result = self.generate()
        self.assertNotEqual(result.returncode, 0)
        self.assertFalse((self.output / 'accepted.stamp').exists())

    def test_compiler_identity_change_rechecks_without_becoming_an_abi_restriction(self):
        compiler = shutil.which('c++')
        wrapper = self.root / 'compiler-wrapper'
        text = '#!/usr/bin/env python3\nimport os,sys\nos.execv(' + repr(compiler) + ', [' + repr(compiler)
        text += '] + sys.argv[1:])\n'
        wrapper.write_text(text)
        wrapper.chmod(0o755)
        data = json.loads(self.manifest.read_text())
        for context in data['contexts']:
            context['arguments'][0] = str(wrapper)
        self.manifest.write_text(json.dumps(data))
        self.assertEqual(self.generate().returncode, 0)
        before = json.loads((self.output / 'receipt.json').read_text())
        wrapper.write_text(text + '# same compiler ABI, new driver identity\n')
        self.assertEqual(self.generate().returncode, 0)
        after = json.loads((self.output / 'receipt.json').read_text())
        self.assertFalse(after['cache_hit'])
        self.assertNotEqual(before['input_digest'], after['input_digest'])
        self.assertEqual(before['physical_signature'], after['physical_signature'])

    def test_cmake_publisher_has_a_hard_generated_evidence_dependency(self):
        helper = ROOT / 'cmake/EngineBootstrapEvidence.cmake'
        self.assertTrue(helper.is_file(), 'The required owning-build helper is absent')
        source = self.root / 'project'
        source.mkdir()
        publisher = source / 'publisher.cpp'
        publisher.write_text('int OnlyCompiledPublisherFixture() {\n    return 0;\n}\n')
        project = f'''cmake_minimum_required(VERSION 3.25)
project(BootstrapEvidenceFixture LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 14)
add_library(publisher OBJECT publisher.cpp)
include("{helper.as_posix()}")
cse_add_engine_bootstrap_evidence(TARGET publisher CONTEXTS "{self.manifest.as_posix()}"
  FOUNDATION "{FOUNDATION.as_posix()}" SCHEMA "{SCHEMA.as_posix()}"
  OUTPUT_DIRECTORY "{self.output.as_posix()}" FIXTURE_ONLY)
'''
        (source / 'CMakeLists.txt').write_text(project)
        build = self.root / 'build'
        configure = subprocess.run(['cmake', '-S', str(source), '-B', str(build), '-G', 'Ninja'],
                                   capture_output=True, text=True)
        self.assertEqual(configure.returncode, 0, configure.stderr)
        command = ['cmake', '--build', str(build)]
        result = subprocess.run(command, capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertTrue((self.output / 'accepted.stamp').exists())
        objects = list(build.rglob('publisher.cpp.o')) + list(build.rglob('publisher.cpp.obj'))
        self.assertEqual(len(objects), 1)
        before = objects[0].stat().st_mtime_ns
        result = subprocess.run(command, capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(before, objects[0].stat().st_mtime_ns, 'A no-op check rewrote publisher inputs')
        self.assertTrue(json.loads((self.output / 'receipt.json').read_text())['cache_hit'])
        (self.manifest.parent / 'edge.h').write_text(
            '#pragma once\n#define PIPELINE_EXTRA_SLOT virtual void UnexpectedPhysicalSlot() = 0;\n')
        result = subprocess.run(command, capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertFalse((self.output / 'accepted.stamp').exists())


if __name__ == '__main__':
    unittest.main()
