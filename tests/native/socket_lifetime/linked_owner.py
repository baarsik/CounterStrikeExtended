"""Build the actual shared socket producer for existing linked component fixtures, serially."""
import hashlib
import json
from pathlib import Path
import subprocess


def arguments(root, output, optimization):
    output = Path(output)
    import importlib.util
    spec = importlib.util.spec_from_file_location('network_extractor', root / 'tests/native/network_platform/extract_functions.py')
    extractor = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(extractor)
    from types import SimpleNamespace
    extractor.run(SimpleNamespace(source=root / 'GoldSrc-SDK/engine/net_ws.cpp', output=output / 'network'))
    library = output / 'component_socket.dylib'
    command = ['clang++', '-std=c++17', optimization, '-Wall', '-Wextra', '-Werror', '-dynamiclib',
               str(root / 'GoldSrc-SDK/engine/native_socket_lifetime.cpp'),
               str(root / 'tests/native/socket_lifetime/empty_provider.cpp'),
               str(root / 'tests/native/socket_lifetime/network_environment.cpp'), '-I' + str(output / 'network'), '-o', str(library)]
    result = subprocess.run(command, capture_output=True)
    log = output / 'socket-provider-compile.log'
    log.write_bytes(result.stdout + result.stderr)
    (output / 'socket-provider-receipt.json').write_text(json.dumps(dict(command=command, exit=result.returncode,
        log_sha256=hashlib.sha256(log.read_bytes()).hexdigest()), indent=2) + '\n')
    if result.returncode:
        raise RuntimeError(log.read_text())
    return ['-I' + str(root / 'tests/native/socket_lifetime'),
            str(root / 'GoldSrc-SDK/launcher/native/SocketImageOwner.cpp'), str(library), '-Wl,-export_dynamic']
