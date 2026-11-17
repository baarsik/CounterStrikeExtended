"""Compile the production file loader, mixer and VOX paths in the fixture."""
from pathlib import Path
import sys


def function(text, signature):
    assert text.count(signature) == 1, signature
    start = text.index(signature)
    end = text.index("{", start) + 1
    depth = 1
    while depth:
        depth += (text[end] == "{") - (text[end] == "}")
        end += 1
    return text[start:end]


root = Path(sys.argv[1])
limited = function((root / "common.cpp").read_text(), "byte* COM_LoadFileLimit(")
mix = (root / "snd_mix.cpp").read_text()
signatures = (
    "void SND_InitScaletable(",
    'extern "C" void Wavstream_Close(',
    'extern "C" void S_FreeChannel(',
    'extern "C" char* VOX_GetDirectory(',
    'extern "C" void VOX_ParseString(',
    'extern "C" int VOX_ParseWordParams(',
    'extern "C" int VOX_IFindEmptySentence(',
    'extern "C" sfxcache_t* VOX_LoadSound(',
    "static sfxcache_t *Wavstream_GetNextChunk(",
    "qboolean S_CheckWavEnd(",
    "void SND_PaintChannelFrom8(",
    "static void SND_PaintChannelFrom8Offs(",
    "static void SND_PaintChannelFrom16Offs(",
    "void S_MixChannelsToPaintbuffer(",
)
Path(sys.argv[2]).write_text("\n\n".join([limited, *(function(mix, sig) for sig in signatures)]) + "\n")
