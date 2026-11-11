"""Extract real owner bodies, with unique exact anchors; no archive algorithm lives here."""
import argparse
import hashlib
import json
from pathlib import Path

OLD_SHA256 = '0fc377268cb29bbcb53425abee63722bf6287f7263f61ab78b4083cc275c4449'
ANCHORS = {
    'FileCopy': ('void FileCopy( FileHandle_t pOutput, FileHandle_t pInput, int fileSize )\n{', '\nTYPEDESCRIPTION gGameHeaderDescription[]'),
    'FileSize': ('int FileSize( FileHandle_t pFile )\n{', '\nvoid Host_ClearGameState()\n{' ),
    'DirectoryCopy': ('void DirectoryCopy( const char* pPath, FileHandle_t pFile )\n{', '\nvoid DirectoryExtract( FileHandle_t pFile, int fileCount )\n{' ),
    'DirectoryExtract': ('void DirectoryExtract( FileHandle_t pFile, int fileCount )\n{', '\nvoid LandmarkOrigin( SAVERESTOREDATA* pSaveData, vec_t* output, const char* pLandmarkName )\n{' ),
    'Host_Load': ('int __cdecl Host_Load( const char* pName )\n{', '\nvoid FileCopy( FileHandle_t pOutput, FileHandle_t pInput, int fileSize )\n{' ),
    'SaveGameSlot': ('int SaveGameSlot( const char* pSaveName, const char* pSaveComment )\n{', '\nint SaveGame( const char* pszSlot, const char* pszComment )\n{' ),
}

def extract(source, names):
    pieces = []
    for name in names:
        start, end = ANCHORS[name]
        if source.count(start) != 1 or source.count(end) != 1:
            raise ValueError(f'{name}: missing/duplicate source anchors')
        begin = source.index(start)
        finish = source.index(end)
        if finish <= begin:
            raise ValueError(f'{name}: reversed anchors')
        pieces.append(source[begin:finish])
    return '\n'.join(pieces)

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('source', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--old', action='store_true')
    args = parser.parse_args()
    data = args.source.read_bytes()
    digest = hashlib.sha256(data).hexdigest()
    if args.old and digest != OLD_SHA256:
        raise ValueError('old source SHA256 mismatch')
    if not args.old:
        ANCHORS['DirectoryCopy'] = ('bool DirectoryCopy(const char *path, FileHandle_t file, std::int32_t expectedFileCount) {', '\nbool DirectoryExtract(')
        ANCHORS['DirectoryExtract'] = ('bool DirectoryExtract(FileHandle_t file, int fileCount) {', ANCHORS['DirectoryExtract'][1])
        if 'enum class HostLoadFailurePolicy' in data.decode():
            ANCHORS['LoadFailurePolicy'] = ('namespace {\nenum class HostLoadFailurePolicy', '\nint Host_ValidSave();')
            ANCHORS['Host_Load'] = ('namespace {\nint Host_LoadWithFailurePolicy(', ANCHORS['Host_Load'][1])
    names = ['FileCopy', 'FileSize', 'DirectoryCopy', 'DirectoryExtract']
    if not args.old:
        if 'LoadFailurePolicy' in ANCHORS:
            names += ['LoadFailurePolicy']
        names += ['Host_Load', 'SaveGameSlot']
    args.output.write_text(extract(data.decode(), names))
    args.output.with_suffix('.json').write_text(json.dumps({'source': str(args.source), 'sha256': digest, 'anchors': names}, indent=2))
