"""Compile the exact retained native LoadWaveFile body with labeled buffer test doubles."""
import pathlib
import sys
source = pathlib.Path(sys.argv[1]).read_text()
start = source.index('HRESULT STDMETHODCALLTYPE CA3dSource::LoadWaveFile(')
end = source.index('\nHRESULT STDMETHODCALLTYPE CA3dSource::AllocateWaveData', start)
pathlib.Path(sys.argv[2]).write_text(source[start:end])
