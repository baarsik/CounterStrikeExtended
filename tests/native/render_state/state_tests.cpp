#include "legacy_commands.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <string>

using namespace cse::render;

void Check(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "FAIL %s\n", message);
        std::exit(EXIT_FAILURE);
    }
}

Command Make(Op op, std::initializer_list<double> values = {}, std::uint32_t argument = 0) {
    Command command;
    command.op = op;
    command.argument = argument;
    std::size_t index = 0;
    for (double value : values) {
        command.values[index++] = value;
    }
    return command;
}

void Send(Recorder& recorder, const Command& command) {
    Check(recorder.Submit(command) == Status::Ok, "valid command accepted");
}

void Triangle(Recorder& recorder) {
    Send(recorder, Make(Op::Begin, {}, static_cast<std::uint32_t>(Primitive::Triangles)));
    Send(recorder, Make(Op::Vertex, {0, 0, 0}));
    Send(recorder, Make(Op::Vertex, {1, 0, 0}));
    Send(recorder, Make(Op::Vertex, {0, 1, 0}));
    Send(recorder, Make(Op::End));
}

void Matrices() {
    Recorder recorder;
    Send(recorder, Make(Op::Translate, {2, 3, 4}));
    Send(recorder, Make(Op::Scale, {5, 6, 7}));
    const auto point = Transform(recorder.State().Matrix(MatrixMode::ModelView), {{1, 1, 1, 1}});
    Check(point == std::array<float, 4>{{7, 9, 11, 1}}, "column-major postmultiply exact known result");
    Send(recorder, Make(Op::Push));
    Send(recorder, Make(Op::Identity));
    Send(recorder, Make(Op::Pop));
    Check(Transform(recorder.State().Matrix(MatrixMode::ModelView), {{1, 1, 1, 1}}) == point, "stack restore");
    Send(recorder, Make(Op::ActiveTexture, {}, 2));
    Send(recorder, Make(Op::MatrixMode, {}, static_cast<std::uint32_t>(MatrixMode::Texture)));
    Send(recorder, Make(Op::Scale, {2, 4, 1}));
    Check(recorder.State().Matrix(MatrixMode::Texture, 2).values[0] == 2, "unit2 texture matrix");
    Check(recorder.State().Matrix(MatrixMode::Texture, 0).values[0] == 1, "unit0 independent");
}

void Attributes() {
    Recorder recorder;
    Send(recorder, Make(Op::Color, {0.25, 0.5, 0.75, 1}));
    Send(recorder, Make(Op::TexCoord, {3, 4}, 2));
    Triangle(recorder);
    const auto& vertex = recorder.Draws()[0].vertices[1];
    Check(vertex.color == std::array<float, 4>{{0.25f, 0.5f, 0.75f, 1}}, "current color latched");
    Check(vertex.texcoords[2] == std::array<float, 4>{{3, 4, 0, 1}}, "explicit unit2 coordinates latched");
    Send(recorder, Make(Op::Color, {1, 0, 0, 1}));
    Check(vertex.color[0] == 0.25f, "later current state cannot mutate prior draw");
}

void Lists() {
    Recorder recorder;
    constexpr std::uint32_t list = 7;
    Check(recorder.BeginList(list, false) == Status::Ok, "begin compile-only list");
    Triangle(recorder);
    Check(recorder.EndList() == Status::Ok, "commit list");
    Check(recorder.Draws().empty(), "compile-only does not execute");
    Send(recorder, Make(Op::Translate, {5, 0, 0}));
    Send(recorder, Make(Op::Color, {0, 1, 0, 1}));
    Send(recorder, Make(Op::CallList, {}, list));
    Check(recorder.Draws().size() == 1 && recorder.Draws()[0].modelView.values[12] == 5,
        "list replay uses later matrix");
    Check(recorder.Draws()[0].vertices[0].color[1] == 1, "list replay inherits later current color");
    Check(recorder.BeginList(list, true) == Status::Ok, "replace compile-and-execute");
    Triangle(recorder);
    Check(recorder.EndList() == Status::Ok && recorder.Draws().size() == 2, "one execution while compiling");
    Check(recorder.DeleteList(list) == Status::Ok, "delete owned list");
    Send(recorder, Make(Op::CallList, {}, list));
    Check(recorder.Draws().size() == 2, "undefined list ignored, old draws remain owned");
}

void Ownership() {
    Recorder copy;
    {
        Recorder source;
        Command command = Make(Op::DrawArrays, {}, static_cast<std::uint32_t>(Primitive::Triangles));
        command.vertices.resize(3);
        command.vertices[0].position[0] = 19;
        Check(source.BeginList(1, false) == Status::Ok, "array list compile");
        Send(source, command);
        Check(source.EndList() == Status::Ok, "array list committed");
        command.vertices[0].position[0] = -9;
        command.vertices.clear();
        copy = source;
    }
    Send(copy, Make(Op::TexCoord, {9, 11}, 2));
    Send(copy, Make(Op::CallList, {}, 1));
    Check(copy.Draws()[0].vertices[0].position[0] == 19, "deep-owned list/client array survives caller/context death");
    Check(copy.Draws()[0].vertices[0].texcoords[2][0] == 9,
        "unit0 client array retains other units current coordinates at replay");
    const auto saved = copy.Draws()[0];
    Check(copy.DeleteList(1) == Status::Ok, "delete after submitted draw");
    copy.ClearDraws();
    Check(saved.vertices[0].position[0] == 19, "independently copied draw survives deletion/clear");
}

void Bounds() {
    Recorder recorder;
    Check(recorder.Submit(Make(Op::Pop)) == Status::StackUnderflow, "underflow checked");
    Check(recorder.Submit(Make(Op::ActiveTexture, {}, kTextureUnits)) == Status::InvalidValue, "texture unit checked");
    const auto original = recorder.State().Matrix(MatrixMode::ModelView).values;
    Check(recorder.Submit(Make(Op::Frustum, {-1, 1, -1, 1, 0, 10})) == Status::InvalidValue,
        "invalid near plane rejected");
    Check(recorder.State().Matrix(MatrixMode::ModelView).values == original, "failed matrix operation atomic");
    Check(recorder.Submit(Make(Op::Frustum, {-1.0e-100, 1.0e-100, -1, 1, 1, 10})) == Status::InvalidValue,
        "unrepresentable projection coefficient rejected before narrowing");
    Check(recorder.State().Matrix(MatrixMode::ModelView).values == original, "projection coefficient failure atomic");
    for (std::size_t index = 1; index < kModelViewStackDepth; ++index) {
        Send(recorder, Make(Op::Push));
    }
    Check(recorder.Submit(Make(Op::Push)) == Status::StackOverflow, "fixed stack capacity checked");
    Check(recorder.BeginList(1, false) == Status::Ok, "recursive list compile");
    Send(recorder, Make(Op::CallList, {}, 1));
    Check(recorder.EndList() == Status::Ok, "recursive list commit");
    Check(recorder.Submit(Make(Op::CallList, {}, 1)) == Status::LimitExceeded, "recursive replay bounded");
    Send(recorder, Make(Op::Begin, {}, static_cast<std::uint32_t>(Primitive::Triangles)));
    Check(recorder.Submit(Make(Op::Translate, {1, 2, 3})) == Status::InvalidOperation, "matrix forbidden in begin/end");
    Check(recorder.EndList() == Status::InvalidOperation, "end list without compile rejected");
    Send(recorder, Make(Op::End));
    CaptureLimits limits;
    limits.verticesPerDraw = 3;
    limits.frameVertices = 3;
    limits.drawPackets = 1;
    limits.storedVertices = 3;
    limits.listCommands = 5;
    limits.lists = 1;
    Recorder bounded(limits);
    Triangle(bounded);
    Send(bounded, Make(Op::Begin, {}, static_cast<std::uint32_t>(Primitive::Triangles)));
    Send(bounded, Make(Op::Vertex, {0, 0, 0}));
    Check(bounded.Submit(Make(Op::End)) == Status::LimitExceeded, "frame limit does not drop pending draw");
    bounded.ClearDraws();
    Send(bounded, Make(Op::End));
    Check(bounded.Draws().size() == 1, "end retried after explicit frame clear");
    Check(bounded.BeginList(1, false) == Status::Ok, "bounded list compile");
    Triangle(bounded);
    Check(bounded.EndList() == Status::Ok, "aggregate list storage committed");
    Check(bounded.BeginList(2, false) == Status::LimitExceeded, "list count bounded");
    Check(bounded.DeleteList(1) == Status::Ok && bounded.BeginList(2, false) == Status::Ok,
        "deletion releases aggregate list capacity");
    Send(bounded, Make(Op::Begin, {}, static_cast<std::uint32_t>(Primitive::Triangles)));
    Check(bounded.EndList() == Status::InvalidOperation, "unfinished list commit rejected");
    Send(bounded, Make(Op::End));
    Check(bounded.EndList() == Status::Ok, "balanced list commits after failed attempt");
    Recorder invalidList;
    Check(invalidList.BeginList(1, false) == Status::Ok, "validation list compile");
    Check(invalidList.Submit(Make(Op::ActiveTexture, {}, kTextureUnits)) == Status::InvalidValue,
        "invalid unit rejected during compile-only capture");
    Check(invalidList.EndList() == Status::Ok, "failed command not stored in list");
    Check(invalidList.Submit(Make(Op::Translate, {1.0e300, 0, 0})) == Status::InvalidValue,
        "unrepresentable command scalar rejected before float narrowing");
}

void Topology() {
    DrawPacket draw;
    draw.primitive = Primitive::TriangleStrip;
    draw.shade = Shade::Flat;
    draw.vertices.resize(4);
    for (std::size_t index = 0; index < draw.vertices.size(); ++index) {
        draw.vertices[index].position[0] = static_cast<float>(index);
        draw.vertices[index].color[0] = static_cast<float>(index) / 4;
    }
    std::vector<Vertex> output;
    Primitive result{};
    Check(ExpandTopology(draw, output, result) == Status::Ok && output.size() == 6, "strip expanded");
    Check(output[3].position[0] == 2 && output[4].position[0] == 1 && output[5].position[0] == 3,
        "odd strip winding preserved");
    Check(output[3].color[0] == 0.75f && output[4].color[0] == 0.75f, "provoking color retained");
    draw.primitive = Primitive::Quads;
    Check(ExpandTopology(draw, output, result) == Status::UnsupportedTopology, "unqualified quad lowering explicit");
}

void Vgui() {
    Recorder recorder;
    Send(recorder, Make(Op::Translate, {17, 23, 0}));
    const auto model = recorder.State().Matrix(MatrixMode::ModelView).values;
    Send(recorder, Make(Op::Push));
    Send(recorder, Make(Op::MatrixMode, {}, static_cast<std::uint32_t>(MatrixMode::Projection)));
    Send(recorder, Make(Op::Identity));
    Send(recorder, Make(Op::Ortho, {0, 640, 480, 0, -1, 1}));
    const auto projection = recorder.State().Matrix(MatrixMode::Projection).values;
    Send(recorder, Make(Op::MatrixMode, {}, static_cast<std::uint32_t>(MatrixMode::ModelView)));
    Send(recorder, Make(Op::Push));
    Send(recorder, Make(Op::Identity));
    Send(recorder, Make(Op::Translate, {5, 7, 0}));
    // Exact inherited EngineSurface::popMakeCurrent matrix sequence.
    Send(recorder, Make(Op::MatrixMode, {}, static_cast<std::uint32_t>(MatrixMode::Projection)));
    Send(recorder, Make(Op::MatrixMode, {}, static_cast<std::uint32_t>(MatrixMode::ModelView)));
    Send(recorder, Make(Op::Pop));
    Send(recorder, Make(Op::Pop));
    Check(recorder.State().Matrix(MatrixMode::ModelView).values == model, "inherited modelview restored");
    Check(recorder.State().Matrix(MatrixMode::Projection).values == projection, "inherited projection stays rewritten");
}

void GeneratedLists() {
    CaptureLimits limits;
    constexpr std::size_t kListQuota = 3;
    limits.lists = kListQuota;
    Recorder recorder(limits);
    std::uint32_t first = 0;
    Check(recorder.GenerateLists(2, first) == Status::Ok && first == 1, "contiguous list names reserved");
    constexpr std::uint32_t kOutputSentinel = 99;
    first = kOutputSentinel;
    Check(recorder.GenerateLists(2, first) == Status::LimitExceeded && first == kOutputSentinel,
        "list name quota checked without changing output");
    Check(recorder.BeginList(3, false) == Status::Ok && recorder.EndList() == Status::Ok,
        "failed range reservation did not consume namespace quota");
    Check(recorder.DeleteList(2) == Status::Ok, "reserved list deletion");
    Check(recorder.GenerateLists(1, first) == Status::Ok && first == 2, "deleted list name available for reservation");
    Check(recorder.DeleteList(1) == Status::Ok && recorder.DeleteList(2) == Status::Ok &&
        recorder.DeleteList(3) == Status::Ok, "all reserved and defined lists released");
    Send(recorder, Make(Op::Begin, {}, static_cast<std::uint32_t>(Primitive::Triangles)));
    first = kOutputSentinel;
    Check(recorder.GenerateLists(1, first) == Status::InvalidOperation && first == kOutputSentinel,
        "list names cannot be generated inside a primitive");
    Send(recorder, Make(Op::End));
    Check(recorder.GenerateLists(kListQuota, first) == Status::Ok && first == 1,
        "namespace recovers after rejected primitive-time generation");
}

int main(int argc, char** argv) {
    Check(argc == 2, "one case required");
    const std::string name = argv[1];
    if (name == "matrices") {
        Matrices();
    } else if (name == "attributes") {
        Attributes();
    } else if (name == "lists") {
        Lists();
    } else if (name == "ownership") {
        Ownership();
    } else if (name == "bounds") {
        Bounds();
    } else if (name == "topology") {
        Topology();
    } else if (name == "generated-lists") {
        GeneratedLists();
    } else if (name == "vgui") {
        Vgui();
    } else {
        Check(false, "unknown case");
    }
    std::printf("PASS %s\n", name.c_str());
}
