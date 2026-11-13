"""Replay retained HW/SW/launcher owning policies without mutating a qualified cache.

Use the reviewed music-focus tuple as the input link closure; retain its corrected
real-main objects. Recompile only changed native owners and regenerate the launcher
cohort. No game, window, provider probe, VM, staging, or dependency build is run.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shlex
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--repo', type=Path, required=True)
parser.add_argument('--baseline', type=Path, required=True)
parser.add_argument('--provider', type=Path, required=True)
parser.add_argument('--packet', type=Path, required=True)
parser.add_argument('--profile', choices=['release', 'debug'], required=True)
parser.add_argument('--predecessor', type=Path, help='Latest admitted packet with owning-<profile>/receipt.json')
parser.add_argument('--owners', nargs='+', help='Engine source suffixes to rebuild instead of the original owner list')
args = parser.parse_args()
repo = args.repo.resolve()
packet = args.packet.resolve()
packet.mkdir(parents=True, exist_ok=False)
base_receipt = args.baseline / 'owning' / args.profile / 'receipt.json'
baseline = json.loads(base_receipt.read_text())
cache = Path(baseline['baseBuild'])
config = args.profile.capitalize()
base_source = cache.parent / 'source'
entries = json.loads((cache / 'compile_commands.json').read_text())
sha = lambda path: hashlib.sha256(Path(path).read_bytes()).hexdigest()
commands = []
pins = {str(base_receipt): sha(base_receipt), str(cache / 'compile_commands.json'): sha(cache / 'compile_commands.json'),
        str(cache / 'build.ninja'): sha(cache / 'build.ninja')}
replacements = {}
new_objects = {}
compiled_dependencies = {}

def run(name, argv, cwd=cache):
    result = subprocess.run([str(x) for x in argv], cwd=cwd, capture_output=True, text=True, timeout=240)
    (packet / (name + '.log')).write_text(result.stdout + result.stderr)
    commands.append(dict(name=name, argv=[str(x) for x in argv], cwd=str(cwd), exit=result.returncode))
    (packet / 'commands.json').write_text(json.dumps(commands, indent=2) + '\n')
    print(name, result.returncode, flush=True)
    if result.returncode:
        raise RuntimeError(name)
    return result.stdout

changed = ['native_mouse_window.cpp', 'native_mouse_cocoa.cpp', 'sys_sdlwind.cpp', 'sys_dll2.cpp',
           'native_runtime_bootstrap.cpp', 'VGUI_EngineSurfaceWrap.cpp', 'vgui2/BaseUISurface.cpp',
           'snd_dma.cpp', 'webm_video.cpp', 'cdll_int.cpp']
# Resolve every actual linked engine/launcher object to its retained owning recipe.
# The admitted music source is the baseline: do not reinterpret earlier accepted
# header changes against the older underlying cache snapshot.
admitted_source = args.baseline.resolve() / 'source'
if args.predecessor:
    predecessor_receipt = args.predecessor.resolve() / ('owning-' + args.profile) / 'receipt.json'
    previous = json.loads(predecessor_receipt.read_text())
    assert previous['profile'] == args.profile and previous['embeddedCohortVerified']
    pins[str(predecessor_receipt)] = sha(predecessor_receipt)
    # Verify the admitted packet before consuming its exact links, objects and provider.
    for path, expected in previous['pinnedInputs'].items():
        assert sha(path) == expected, 'Predecessor pin changed: ' + path
        pins[path] = expected
    for output in previous['outputs'].values():
        assert sha(output['path']) == output['sha256'], 'Predecessor image changed: ' + output['path']
        pins[output['path']] = output['sha256']
    admitted_source = args.predecessor.resolve() / 'source'
    recipe_compiles = [row for row in baseline['commands'] if row['step'].startswith('compile-')]
    baseline['commands'] = recipe_compiles + [dict(step=row['name'], argv=row['argv'])
                                               for row in previous['commands']]
    baseline['reusedCurrentObjects'] = list(previous['retainedCorrectedMainObjects'])
    for obj, details in previous['newObjects'].items():
        role, = [role for role in ['hw_engine', 'swds', 'launcher']
                 if Path(obj).name.startswith(role + '-')]
        baseline['reusedCurrentObjects'].append(dict(role=role, object=obj, objectSha256=details['sha256'],
            source=details['source'], sourceSha256=details['sourceSha256']))
    changed = args.owners if args.owners is not None else changed
elif args.owners is not None:
    changed = args.owners
entry_by_object = {str(Path(entry['output']).resolve()): entry for entry in entries}
object_origins = {name: name for name in entry_by_object}

def original_object(role, source):
    matches = [entry for entry in entries if Path(entry['file']).is_relative_to(base_source)
               and str(Path(source)).endswith('/' + str(Path(entry['file']).relative_to(base_source)))
               and '/targets/' + role + '/' in entry['output']]
    entry, = matches
    return str(Path(entry['output']).resolve())

for item in baseline['reusedCurrentObjects']:
    assert sha(item['object']) == item['objectSha256']
    assert sha(item['source']) == item['sourceSha256']
    if args.predecessor:
        origin = original_object(item['role'], item['source'])
        relative = Path(entry_by_object[origin]['file']).relative_to(base_source)
        assert sha(admitted_source / relative) == item['sourceSha256'], 'Admitted source/object mismatch: ' + str(relative)
    object_origins[str(Path(item['object']).resolve())] = original_object(item['role'], item['source'])
for row in baseline['commands']:
    if row['step'].startswith('compile-'):
        role = next(role for role in ['hw_engine', 'swds', 'launcher']
                    if row['step'].startswith('compile-' + role + '-'))
        argv = row['argv']
        object_origins[str(Path(argv[argv.index('-o') + 1]).resolve())] = original_object(
            role, argv[argv.index('-c') + 1])

selected_objects = {}
for target in ['hw.dylib', 'swds.dylib', 'hl']:
    row, = [row for row in baseline['commands'] if row['step'] == 'link-' + target]
    selected_objects[target] = {}
    for name in row['argv']:
        if not name.endswith('.o'):
            continue
        path = Path(name) if Path(name).is_absolute() else cache / name
        actual = str(path.resolve())
        assert actual in object_origins, 'No owning recipe for linked object: ' + actual
        selected_objects[target][actual] = object_origins[actual]

selected_originals = set(origin for objects in selected_objects.values() for origin in objects.values())
dependency_output = run('selected-object-dependencies', ['ninja', '-t', 'deps'] +
    [str(Path(name).relative_to(cache)) for name in sorted(selected_originals)])
dependency_map = {}
current_object = None
for line in dependency_output.splitlines():
    if ': #deps ' in line:
        name = line.split(': #deps ', 1)[0]
        current_object = str((cache / name).resolve())
        dependency_map[current_object] = set()
    elif line.startswith('    '):
        assert current_object is not None
        path = Path(line.strip())
        dependency_map[current_object].add((path if path.is_absolute() else cache / path).resolve())
assert set(dependency_map) == selected_originals, 'Incomplete retained dependency graph'
assert all(dependency_map.values()), 'Empty retained owning dependency list'

changed_headers = {}
checked_headers = set()
header_consumers = {}
for origin, dependencies in dependency_map.items():
    for path in dependencies:
        if not path.is_relative_to(base_source) or path.suffix not in ['.h', '.hpp', '.hh', '.inc']:
            continue
        relative = str(path.relative_to(base_source))
        if relative not in checked_headers:
            checked_headers.add(relative)
            before = admitted_source / relative
            after = repo / relative
            assert before.is_file() and after.is_file(), 'Missing admitted/current header: ' + relative
            before_sha = sha(before)
            after_sha = sha(after)
            pins[str(before)] = before_sha
            pins[str(after)] = after_sha
            if before_sha != after_sha:
                changed_headers[relative] = dict(beforeSha256=before_sha, afterSha256=after_sha)
        if relative in changed_headers:
            header_consumers.setdefault(relative, set()).add(origin)

planned_objects = set()
for entry in entries:
    role = next((role for role in ['hw_engine', 'swds', 'launcher']
                 if '/targets/' + role + '/' in entry['output']), None)
    suffixes = ['MacRuntimeSession.cpp'] if role == 'launcher' else changed
    if role and any(entry['file'].endswith('/' + suffix) for suffix in suffixes):
        planned_objects.add(str(Path(entry['output']).resolve()))
changed_sources = {}
for origin, dependencies in dependency_map.items():
    if not args.predecessor:
        continue
    for path in dependencies:
        if not path.is_relative_to(base_source) or path.suffix not in ['.cpp', '.c', '.cc', '.mm', '.m']:
            continue
        relative = str(path.relative_to(base_source))
        before = admitted_source / relative
        after = repo / relative
        assert before.is_file() and after.is_file(), 'Missing admitted/current source: ' + relative
        pins[str(before)] = sha(before)
        pins[str(after)] = sha(after)
        if sha(before) != sha(after):
            assert origin in planned_objects, 'Changed source omitted from owning rebuild: ' + relative
            changed_sources[relative] = dict(beforeSha256=sha(before), afterSha256=sha(after))
missing = sorted(set().union(*header_consumers.values()) - planned_objects) if header_consumers else []
closure = dict(admittedSource=str(admitted_source), selectedObjects=selected_objects,
    selectedObjectCount=len(selected_originals), inspectedHeaderCount=len(checked_headers),
    changedSources=changed_sources, changedHeaders=changed_headers, consumers={key: sorted(value) for key, value in header_consumers.items()},
    plannedObjects=sorted(planned_objects), omittedConsumers=missing, verifiedLinks={})
(packet / 'header-closure.json').write_text(json.dumps(closure, indent=2) + '\n')
assert not missing, 'Changed-header consumers omitted from owning rebuild: ' + ', '.join(missing)

# Verify corrected current-source bootstrap anchors before preserving their objects.
for source, expected in {
    'src/engine/VGui1NativeRuntime.cpp': '4cc77d72972d3607e7ccfbfb9b61961894b0f65b05e21701d15f8f56c806a2c5',
    'src/launcher/native/VGui1HostImageOwner.cpp': 'e6216d79d22ae49c4b02c92313eeb86a1ab2d4057fd1116924433691e053175a',
}.items():
    assert sha(repo / source) == expected
    pins[str(repo / source)] = expected

def compile_one(role, suffix, cohort=False):
    matches = [e for e in entries if e['file'].endswith('/' + suffix) and '/targets/' + role + '/' in e['output']]
    if not matches:
        return
    entry, = matches
    path = repo / Path(entry['file']).relative_to(base_source)
    obj = packet / (role + '-' + suffix.replace('/', '_') + '.o')
    dep = obj.with_suffix('.d')
    argv = [arg.replace(str(base_source), str(repo)) for arg in shlex.split(entry['command'])]
    argv[argv.index('-c') + 1] = str(path)
    argv[argv.index('-o') + 1] = str(obj)
    if cohort:
        argv = [('-I' + str(packet / 'cohort')) if arg == '-I' + str(cache / 'native-cohort' / config) else arg
                for arg in argv]
    argv += ['-MMD', '-MF', str(dep)]
    run('compile-' + role + '-' + suffix.replace('/', '_'), argv)
    replacements[str(Path(entry['output']))] = str(obj)
    # Baseline music receipts replace some cache objects with reviewed newer inputs.
    for old in baseline['reusedCurrentObjects']:
        if old['role'] == role and old['source'].endswith('/' + suffix):
            replacements[old['object']] = str(obj)
    new_objects[str(obj)] = dict(sha256=sha(obj), source=str(path), sourceSha256=sha(path), dependencyFile=str(dep))
    dependencies = shlex.split(dep.read_text().replace('\\\n', ' '))[1:]
    compiled_dependencies[str(Path(entry['output']).resolve())] = set()
    for name in dependencies:
        item = Path(name)
        item = item if item.is_absolute() else cache / item
        compiled_dependencies[str(Path(entry['output']).resolve())].add(item.resolve())
        if item.exists():
            pins[str(item)] = sha(item)

outputs = {}
retained_objects = []
for role in ['hw_engine', 'swds']:
    for suffix in changed:
        compile_one(role, suffix)

provider = args.provider.resolve() / 'libSDL3.0.dylib'
assert provider.exists(), provider
pins[str(provider)] = sha(provider)

def link(target):
    row, = [r for r in baseline['commands'] if r['step'] == 'link-' + target]
    argv = list(row['argv'])
    for i, arg in enumerate(argv):
        if i and argv[i - 1] in ['-o', '-install_name']:
            continue
        path = Path(arg) if Path(arg).is_absolute() else cache / arg
        if str(path) in replacements:
            argv[i] = replacements[str(path)]
        elif arg.endswith('libSDL3.0.dylib'):
            argv[i] = str(provider)
        elif path.is_file() and arg.endswith(('.o', '.a', '.dylib')):
            pins[str(path)] = sha(path)
            if path.suffix == '.o':
                retained_objects.append(path)
    # Enforce the dependency closure on the actual final link arguments too,
    # including reviewed external objects substituted by the admitted baseline.
    linked = {str((Path(arg) if Path(arg).is_absolute() else cache / arg).resolve())
              for arg in argv if arg.endswith('.o')}
    verified_consumers = []
    for relative, consumers in header_consumers.items():
        for actual, origin in selected_objects[target].items():
            if origin not in consumers:
                continue
            assert actual in replacements, 'Linked changed-header consumer was not replaced: ' + actual
            assert replacements[actual] in linked, 'Recompiled consumer absent from link: ' + actual
            assert origin in compiled_dependencies, 'Consumer has no fresh compiler dependency evidence: ' + origin
            assert (repo / relative).resolve() in compiled_dependencies[origin], \
                'Recompiled consumer did not select current header: ' + origin + ' -> ' + relative
            assert (base_source / relative).resolve() not in compiled_dependencies[origin], \
                'Recompiled consumer still selected cached header: ' + origin + ' -> ' + relative
            verified_consumers.append(dict(header=relative, retainedObject=actual, newObject=replacements[actual]))
    closure['verifiedLinks'][target] = verified_consumers
    (packet / 'header-closure.json').write_text(json.dumps(closure, indent=2) + '\n')
    output = packet / target
    argv[argv.index('-o') + 1] = str(output)
    run('link-' + target, argv)
    # Explicit ad-hoc signatures make candidate identities immutable after this step.
    run('sign-' + target, ['codesign', '--force', '--sign', '-', str(output)])
    run('verify-' + target, ['codesign', '--verify', '--strict', str(output)])
    run('identity-' + target, ['codesign', '-dv', '--verbose=4', str(output)])
    run('dependencies-' + target, ['otool', '-L', str(output)])
    outputs[target] = dict(path=str(output), sha256=sha(output))

link('hw.dylib')
link('swds.dylib')
vgui = Path(next(arg.split('=', 1)[1] for r in baseline['commands'] if r['step'] == 'generate-cohort'
                 for arg in r['argv'] if arg.startswith('-DCSE_VGUI=')))
pins[str(vgui)] = sha(vgui)
run('generate-cohort', ['cmake', '-DCSE_ENGINE=' + str(packet / 'hw.dylib'),
    '-DCSE_SOFTWARE_ENGINE=' + str(packet / 'swds.dylib'), '-DCSE_VGUI=' + str(vgui),
    '-DCSE_HEADER=' + str(packet / 'cohort/cse_native_cohort_images.h'), '-P', str(repo / 'cmake/WriteNativeCohort.cmake')])
compile_one('launcher', 'MacRuntimeSession.cpp', True)
# Original final-v3 launcher session was recompiled outside the cache too.
prior_session = next(r for r in baseline['commands'] if r['step'] == 'compile-launcher-MacRuntimeSession.cpp')
replacements[prior_session['argv'][prior_session['argv'].index('-o') + 1]] = next(
    path for path in new_objects if path.endswith('launcher-MacRuntimeSession.cpp.o'))
link('hl')
launcher = (packet / 'hl').read_bytes()
assert all(outputs[name]['sha256'].encode() in launcher for name in ['hw.dylib', 'swds.dylib'])
assert sha(vgui).encode() in launcher
# Retained cache dependencies establish exact unchanged input provenance.
cache_objects = [str(path.relative_to(cache)) for path in retained_objects if path.is_relative_to(cache)]
deps = run('retained-dependencies', ['ninja', '-t', 'deps'] + cache_objects)
for line in deps.splitlines():
    if line.startswith('    '):
        path = Path(line.strip())
        path = path if path.is_absolute() else cache / path
        if path.is_file():
            pins[str(path)] = sha(path)
for reused in baseline['reusedCurrentObjects']:
    if reused['object'] not in replacements:
        assert sha(reused['object']) == reused['objectSha256']
        assert sha(reused['source']) == reused['sourceSha256']
        pins[reused['source']] = reused['sourceSha256']
assert all(sha(path) == expected for path, expected in pins.items())
(packet / 'receipt.json').write_text(json.dumps(dict(profile=args.profile, commands=commands, newObjects=new_objects,
    pinnedInputs=pins, outputs=outputs, providerSha256=sha(provider), retainedCorrectedMainObjects=[
        item for item in baseline['reusedCurrentObjects'] if item['object'] not in replacements],
    baseBuild=str(cache), recipeReceipt=str(base_receipt),
    predecessorReceipt=str(predecessor_receipt) if args.predecessor else None,
    rebuiltOwners=changed, headerClosure=closure,
    cohortHeader=(packet / 'cohort/cse_native_cohort_images.h').read_text(), embeddedCohortVerified=True), indent=2) + '\n')
