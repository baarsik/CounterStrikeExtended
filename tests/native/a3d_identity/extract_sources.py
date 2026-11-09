"""Build a component TU from exact production bodies, without a Windows COM/device shim.

Only declarations/types are adapted in fixture_support.h. Extracted source is never
rewritten; #line and a hash receipt identify each compiled production body.
"""
import argparse
import hashlib
import json
from pathlib import Path


def function(text, signature):
    start = text.index(signature)
    opening = text.index("{", start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (text[end] == "{") - (text[end] == "}")
        end += 1
    return start, text[start:end]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    blocks = []
    receipt = []

    def add(name, start, body):
        path = args.source / name
        line = path.read_text(encoding="cp1252").count("\n", 0, start) + 1
        blocks.append(f'#line {line} "{path.as_posix()}"\n{body}\n')
        receipt.append({"file": str(path), "line": line,
                        "body_sha256": hashlib.sha256(body.encode()).hexdigest()})

    def take(name, signature):
        text = (args.source / name).read_text(encoding="cp1252")
        add(name, *function(text, signature))

    def region(name, first, last):
        text = (args.source / name).read_text(encoding="cp1252")
        start = text.index(first)
        add(name, start, text[start:text.index(last, start)])

    for name, prefixes in {
        "ia3dapi.h": ["A3D_LINES", "A3D_TRIANGLES", "A3D_QUADS", "A3D_MATERIAL", "A3D_VERTEX_MASK",
                      "A3D_SUBFACE", "A3D_SUB_LINES", "A3D_SUB_TRIANGLES", "A3D_SUB_QUADS",
                      "A3D_INVALID_INPUTMODE", "A3D_EPSILON"],
        "A3dGeom.h": ["A3D_PRIMITIVE_COMPAT1009"],
        "A3dGeom.cpp": ["A3D_FACE_SKIP_RENDER", "HALF_PI"],
        "source_geometry.cpp": ["A3D_INTERSECTION_EDGE_LIMIT"],
        "A3dSource.h": ["MAX_A3D_SOURCE_REFLECTIONS", "A3D_SPEED_OF_SOUND"],
        "spatial_audio.cpp": ["A3D_DISTANCE_EQ_RANGE_SCALE"],
    }.items():
        text = (args.source / name).read_text(encoding="cp1252")
        for prefix in prefixes:
            import re
            match = re.search(r"^#define " + prefix + r"\s+[^\n]+", text, re.MULTILINE)
            add(name, match.start(), match.group())
    region("A3dGeom.h", "typedef struct a3d_primitive_s", "typedef struct a3d_edge_s")
    region("A3dSource.h", "typedef struct a3d_source_channel_s", "class IA3dSource202")
    if (args.source / "native_geometry_identity.h").exists():
        blocks.append('#include "native_geometry_identity.h"\n')
    text = (args.source / "ChunkPage.h").read_text(encoding="cp1252")
    start = text.index("class CA3dCommandBuffer")
    add("ChunkPage.h", start, text[start:])
    blocks.append('#include "fixture_support.h"\n')
    declarations = "\n".join(blocks)
    region("source_reflections.cpp", "#define A3D_REFLECTION_CHANNEL_BUDGET", "HRESULT CA3dSource::StartPendingPlayback")
    for name, signatures in {
        "apimapper.cpp": ["CA3dCommandBuffer::CA3dCommandBuffer(", "CA3dCommandBuffer::~CA3dCommandBuffer()",
                          "int __cdecl CommandBuffer_CheckFit(", "int __cdecl CommandBuffer_Copy(",
                          "void* CA3dCommandBuffer::Next(", "HRESULT STDMETHODCALLTYPE CA3d::GetOutputGain("],
        "A3dGeom.cpp": ["HRESULT STDMETHODCALLTYPE CA3d::Begin(", "HRESULT STDMETHODCALLTYPE CA3d::End()",
                        "HRESULT STDMETHODCALLTYPE CA3d::Tag(", "void CA3d::AppendGeometryPool(",
                        "a3d_geometry_command_t* CA3dCommandBuffer::ReadCommand()",
                        "void* CA3dObjectList::Next()", "void CA3dObjectList::Append(",
                        "void CA3dObjectList::Clear()", "CA3dListNode::CA3dListNode(",
                        "void CA3dGeometryFace::Render(", "void CA3d::NextFreeChunk()",
                        "HRESULT STDMETHODCALLTYPE CA3d::Vertex3fv(", "HRESULT STDMETHODCALLTYPE CA3d::Vertex3f(",
                        "HRESULT STDMETHODCALLTYPE CA3d::Normal3fv(", "HRESULT STDMETHODCALLTYPE CA3d::Normal3f(",
                        "CA3dGeometryInstance::CA3dGeometryInstance()", "CA3dGeometryInstance::~CA3dGeometryInstance()",
                        "void CA3dGeometryInstance::Clear()", "void CA3dGeometryInstance::CopyPolygons(",
                        "CA3dList::CA3dList(", "CA3dList::~CA3dList()", "HRESULT STDMETHODCALLTYPE CA3dList::Begin()",
                        "HRESULT STDMETHODCALLTYPE CA3dList::End()", "HRESULT STDMETHODCALLTYPE CA3dList::Call()",
                        "void __cdecl Vector_ToSphericalRadians(", "double CA3d::ReflectPoint(",
                        "BOOL A3dIntersectQuad("],
        "dal_a2d.cpp": ["CA3dListNode* CA3dListNode::Append("],
        "spatial_audio.cpp": ["void CA3dSource::ComputeDistanceGain(", "void CA3dSource::ComputeDistanceEq(",
                              "void CA3dTransform::TransformPolygon(",
                              "void CA3dTransform::TransformVector( A3DVECTOR vector )"],
        "source_geometry.cpp": ["void CA3d::RenderSourceGeometry("],
        "A3dScene.cpp": ["BOOL STDMETHODCALLTYPE CA3dScene::GetNextVertex(", "HRESULT STDMETHODCALLTYPE CA3d::Clear()"],
        "A3dSource.cpp": ["HRESULT STDMETHODCALLTYPE CA3dSource::InitializeReflectionSlots("],
    }.items():
        for signature in signatures:
            take(name, signature)
    text = (args.source / "A3dGeom.cpp").read_text(encoding="cp1252")
    if "void CA3d::TagOwnedPolygon(" in text:
        # This complete helper is excluded from the actual Win32 translation unit.
        blocks.append("#if UINTPTR_MAX > UINT32_MAX")
        take("A3dGeom.cpp", "void CA3d::TagOwnedPolygon(")
        blocks.append("#endif")
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / "production_bodies.inc").write_text("\n".join(blocks))
    (args.output / "fixture_declarations.inc").write_text(declarations)
    (args.output / "extracted-bodies.json").write_text(json.dumps(receipt, indent=2) + "\n")


if __name__ == "__main__":
    main()
