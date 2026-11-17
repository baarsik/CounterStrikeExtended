struct NativeFixture {
    FrameRecorder frame;
    std::uint64_t serial = 0;
    std::vector<FrameSnapshot> submitted;
    bool allowEnsure = true;
    bool allowSync = true;
    int reads = 0;

    static bool Ensure(void* user) {
        auto& self = *static_cast<NativeFixture*>(user);
        if (!self.allowEnsure) {
            return false;
        }
        if (self.frame.CaptureStatus() == Status::Ok) {
            return true;
        }
        return self.frame.BeginFrame({++self.serial, 1, 64, 64}) == Status::Ok;
    }

    static bool Finish(void* user) {
        auto& self = *static_cast<NativeFixture*>(user);
        if (!self.allowSync) {
            return false;
        }
        FrameSnapshot snapshot;
        if (self.frame.Seal(snapshot) != Status::Ok) {
            return false;
        }
        self.submitted.push_back(std::move(snapshot));
        return true;
    }

    static bool Read(void* user, int, int, int width, int height, unsigned components, unsigned alignment,
        void* output) {
        auto& self = *static_cast<NativeFixture*>(user);
        if (!Finish(user)) {
            return false;
        }
        const int channels = static_cast<int>(components);
        auto* bytes = static_cast<unsigned char*>(output);
        for (int index = 0; index < width * height * channels; ++index) {
            const int row = index / (width * channels);
            const int column = index % (width * channels);
            const int packing = static_cast<int>(alignment);
            const int stride = (width * channels + packing - 1) / packing * packing;
            bytes[row * stride + column] = static_cast<unsigned char>(index + 1);
        }
        ++self.reads;
        return true;
    }

    void Install() {
        std::string error;
        LegacyNativeBridge bridge;
        bridge.user = this;
        bridge.ensureRecording = &Ensure;
        bridge.finish = &Finish;
        bridge.readPixels = &Read;
        Check(InstallNativeDispatch(&frame, bridge, error), error.c_str());
    }
};

namespace RenderModeOwners {
// Only the data read by the extracted production mode-setting bodies is needed.
struct cl_entity_t {
    struct {
        int rendermode = 0;
    } curstate;
};
struct FixtureColor {
    unsigned r;
    unsigned g;
    unsigned b;
};
struct FixtureCvar {
    float value;
};
constexpr float r_blend = 0.5f;
constexpr FixtureCvar gl_alphamin{0.25f};
constexpr FixtureCvar gl_spriteblend{1};
#include "engine_render_modes.inc"
}

void NativeTransColor(const std::string& owner) {
    ResetDriver();
    NativeFixture native;
    native.Install();
    qglTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    RenderModeOwners::cl_entity_t entity;
    entity.curstate.rendermode = RenderModeOwners::kRenderTransColor;
    if (owner == "brush") {
        RenderModeOwners::R_SetRenderMode(&entity);
    } else if (owner == "studio") {
        RenderModeOwners::studioapi_GL_SetRenderMode(entity.curstate.rendermode);
    } else {
        Check(owner == "sprite", "unknown TransColor owner");
        RenderModeOwners::SpriteRenderMode(&entity, {255, 128, 64});
    }
    Check(LegacyDispatchStatus() == Status::Ok, "TransColor mode setup poisoned native recording");
    qglBegin(GL_TRIANGLES);
    qglVertex3f(0, 0, 0);
    qglVertex3f(1, 0, 0);
    qglVertex3f(0, 1, 0);
    qglEnd();
    qglFinish();
    Check(LegacyDispatchStatus() == Status::Ok && native.submitted.size() == 1,
        "TransColor frame could not be submitted");
    const auto& snapshot = native.submitted.front();
    Check(snapshot.draws.size() == 1, "TransColor draw missing");
    const auto& draw = snapshot.draws.front();
    Check(draw.textures[0].environment == EnvironmentMode::Replace,
        "TransColor changed the incoming texture environment");
    Check(draw.raster.enabled[static_cast<std::size_t>(Capability::Blend)] &&
        draw.raster.blendSource == BlendFactor::SourceAlpha &&
        draw.raster.blendDestination == BlendFactor::InverseSourceAlpha,
        "TransColor alpha blending changed");
}

void NativeInstallRed() {
    ResetDriver();
    RemoveLegacyDispatch();
#define NULL_NATIVE_SLOT(name) qgl##name = nullptr;
    CORE_SLOTS(NULL_NATIVE_SLOT)
    T2B_CORE_SLOTS(NULL_NATIVE_SLOT)
#undef NULL_NATIVE_SLOT
    qglSelectTextureSGIS = nullptr;
    qglMTexCoord2fSGIS = nullptr;
    NativeFixture native;
    native.Install();
    Check(qglBegin == &CseRenderBegin && qglEnd == &CseRenderEnd && qglGenTextures == &CseRenderGenTextures,
        "Native dispatch requires driverless ownership");
    GLuint texture = 0;
    qglGenTextures(1, &texture);
    Check(texture != 0, "startup native texture name missing");
    qglBindTexture(GL_TEXTURE_2D, texture);
    GLint value = -1;
    qglGetIntegerv(GL_TEXTURE_BINDING_2D, &value);
    Check(value == static_cast<GLint>(texture), "startup native binding query incorrect");
    qglGetIntegerv(GL_UNPACK_ALIGNMENT, &value);
    Check(value == 4, "default unpack alignment incorrect");
    qglPixelStorei(GL_UNPACK_ROW_LENGTH, 7);
    qglGetIntegerv(GL_UNPACK_ROW_LENGTH, &value);
    Check(value == 7, "unpack row length query incorrect");
    qglGetIntegerv(GL_MAX_TEXTURE_UNITS_ARB, &value);
    Check(value == static_cast<int>(kTextureUnits), "native units metadata incorrect");
    qglGetIntegerv(GL_DEPTH_BITS, &value);
    Check(value == 32, "native default depth metadata must match D32");
    qglGetIntegerv(GL_STENCIL_BITS, &value);
    Check(value == 0, "native default stencil metadata must not imply a stencil attachment");
    Check(RouteLegacyExtension("glActiveTextureARB", nullptr) != nullptr, "driverless alias missing");
    qglSelectTextureSGIS(GL_TEXTURE0_ARB + 1);
    Check(native.frame.State().ActiveTexture() == 1, "native ARB alias enum changed");
    RefreshLegacyDispatchReferences();
    Check(LegacyDispatchStatus() == Status::Ok, "startup native operation refused");
    RemoveLegacyDispatch();
    Check(qglBegin == nullptr && qglGenTextures == nullptr && qglSelectTextureSGIS == nullptr,
        "native removal did not restore null originals");
    LegacyNativeBridge configured;
    configured.user = &native;
    configured.ensureRecording = &NativeFixture::Ensure;
    configured.finish = &NativeFixture::Finish;
    configured.readPixels = &NativeFixture::Read;
    constexpr unsigned kConfiguredTextureUnits = 2;
    constexpr unsigned kConfiguredMaximumSize = 4096;
    configured.textureUnits = kConfiguredTextureUnits;
    configured.maximumTextureSize = kConfiguredMaximumSize;
    std::string error;
    Check(InstallNativeDispatch(&native.frame, configured, error), error.c_str());
    qglGetIntegerv(GL_MAX_TEXTURE_UNITS_ARB, &value);
    Check(value == static_cast<GLint>(kConfiguredTextureUnits), "configured native units metadata ignored");
    qglGetIntegerv(GL_MAX_TEXTURE_SIZE, &value);
    Check(value == static_cast<GLint>(kConfiguredMaximumSize), "configured native size metadata ignored");
}

void NativeCommands() {
    ResetDriver();
    NativeFixture native;
    native.Install();
    qglEnable(GL_DITHER);
    qglDisable(GL_LIGHTING);
    qglEnable(0x81fb); // shared palette enabled by default in GL_Init
    Check(qglIsEnabled(0x81fb) == GL_TRUE, "shared palette enable missing");
    qglClearColor(0.25f, 0.5f, 0.75f, 1);
    qglClearDepth(0.25);
    qglColorMask(GL_TRUE, GL_FALSE, GL_TRUE, GL_FALSE);
    qglDepthMask(GL_FALSE);
    qglEnable(GL_SCISSOR_TEST);
    qglScissor(2, 3, 4, 5);
    qglClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    qglPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    qglFrontFace(GL_CW);
    std::array<GLfloat, 16> matrix{};
    matrix[0] = matrix[5] = matrix[10] = matrix[15] = 1;
    matrix[12] = 9;
    qglLoadMatrixf(matrix.data());
    qglMultMatrixf(matrix.data());
    GLfloat queried[16]{};
    qglGetFloatv(GL_MODELVIEW_MATRIX, queried);
    Check(queried[12] == 18, "native matrix load/multiply missing");
    const GLuint list = qglGenLists(2);
    Check(list != 0, "native list allocation missing");
    qglNewList(list, GL_COMPILE);
    qglBegin(GL_TRIANGLES);
    qglVertex3f(0, 0, 0);
    qglVertex3f(1, 0, 0);
    qglVertex3f(0, 1, 0);
    qglEnd();
    qglEndList();
    qglCallList(list);
    qglDeleteLists(list, 2);
    qglFinish();
    Check(LegacyDispatchStatus() == Status::Ok, "native commands refused");
    Check(events.empty(), "native operations forwarded to GL");
    Check(native.submitted.size() == 1, "finish did not submit synchronous frame");
    const auto& snapshot = native.submitted.front();
    Check(snapshot.events.size() == 2 && snapshot.events[0].kind == FrameEventKind::Clear &&
        snapshot.events[1].kind == FrameEventKind::Draw, "clear/list execution order lost");
    const auto& clear = snapshot.events[0].clear;
    Check(clear.depth == 0.25 && !clear.depthWrite && clear.scissorEnabled &&
        !clear.colorWrite[1] && clear.color[2] == 0.75f, "clear state snapshot incomplete");
    Check(!snapshot.draws[0].raster.frontCounterClockwise && !snapshot.draws[0].raster.colorWrite[3],
        "native raster state missing");
}

void NativeIndexed() {
    ResetDriver();
    NativeFixture native;
    native.Install();
    std::vector<Vertex> vertices(4);
    vertices[1].position[0] = 12;
    vertices[2].color = {{0.25f, 0.5f, 0.75f, 1}};
    vertices[3].texcoords[0] = {{0.3f, 0.7f, 0, 1}};
    vertices[3].color = {{1, 0, 0, 1}};
    const std::vector<std::uint32_t> indices{2, 1, 0, 0, 1, 2};
    qglColor4f(0, 1, 0, 1);
    Check(SubmitNativeIndexed(vertices, indices, 3, false) == Status::Ok, "native indexed submit refused");
    Check(native.frame.State().Current().texcoords[0][0] == 0.3f &&
        native.frame.State().Current().color[1] == 1, "chrome current attributes not preserved");
    Check(SubmitNativeIndexed(vertices, indices, 3, true) == Status::Ok, "native colored indexed refused");
    Check(native.frame.State().Current().color[0] == 1 && native.frame.State().Current().color[1] == 0,
        "last source indexed color missing");
    qglFinish();
    const auto& draws = native.submitted[0].draws;
    Check(draws.size() == 2 && draws[0].vertices.size() == vertices.size() &&
        draws[1].vertices.size() == vertices.size(), "native indexed geometry expanded shared vertices");
    Check(draws[0].indices == indices && draws[1].indices == indices, "native triangle indices lost");
    Check(draws.size() == 2 && draws[0].vertices[1].position[0] == 12 &&
        draws[0].vertices[0].color[1] == 1 && draws[1].vertices[2].color[0] == 0.25f,
        "indexed order or per-vertex color changed");
    Check(events.empty(), "native indexed forwarded");
}

void NativeReadback() {
    ResetDriver();
    NativeFixture native;
    native.Install();
    qglReadBuffer(GL_FRONT);
    qglDrawBuffer(GL_BACK);
    Check(LegacyNativeReadTarget() == ReadbackStage::PresentedColor &&
        LegacyNativeDrawTarget() == ReadbackStage::SceneColor, "native target selection lost");
    std::array<unsigned char, 16> pixels;
    pixels.fill(0xee);
    qglPixelStorei(GL_PACK_ALIGNMENT, 8);
    qglReadPixels(0, 0, 1, 2, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    Check(native.reads == 1 && pixels[0] == 1 && pixels[2] == 3 && pixels[8] == 4 && pixels[10] == 6 &&
        pixels[3] == 0xee && pixels[11] == 0xee, "native readback packing/orientation changed");
    // Callback seals partial rendering; the next state operation must open a fresh recorder frame.
    qglColor4f(0, 1, 0, 1);
    qglFinish();
    Check(native.submitted.size() == 2 && events.empty(), "readback did not reopen native recording");
    Check(LegacyDispatchStatus() == Status::Ok, "native readback failed");
}

void NativeFailure() {
    ResetDriver();
    NativeFixture native;
    native.Install();
    qglEnable(GL_LIGHTING); // Unsupported enabled lighting must report its actual operation.
    Check(LegacyDispatchStatus() != Status::Ok &&
        LegacyDispatchDiagnostic().find("Enable") != std::string::npos, "unsupported native context missing");
    const std::string original = LegacyDispatchDiagnostic();
    qglGetString(GL_EXTENSIONS);
    qglFinish();
    Check(LegacyDispatchDiagnostic() == original && events.empty(), "sticky native failure hidden or forwarded");
}

void NativeAliasOwnership() {
    ResetDriver();
    qglSelectTextureSGIS = &SpySelectTexture;
    qglBindTexture = &SpyBindTexture;
    NativeFixture native;
    native.Install();
    Check(RouteLegacyExtension("glBindTextureEXT", nullptr) != nullptr &&
        RouteLegacyExtension("glActiveTextureARB", nullptr) != nullptr, "native aliases require a GL resolver");
    RefreshLegacyDispatchReferences();
    qglBegin = &SpyLoggedBegin;
    RefreshLegacyDispatchReferences();
    qglBegin(GL_TRIANGLES);
    qglEnd();
    Check(events.empty(), "native refresh forwarded into logging GL");
    RemoveLegacyDispatch();
    Check(qglBindTexture == &SpyBindTexture && qglSelectTextureSGIS == &SpySelectTexture &&
        qglBegin == &SpyLoggedBegin, "native aliases lost saved originals during refresh/removal");
}

void NativeListClearRefusal() {
    ResetDriver();
    NativeFixture native;
    native.Install();
    const GLuint list = qglGenLists(1);
    qglNewList(list, GL_COMPILE);
    qglClearColor(1, 0, 0, 1);
    Check(LegacyDispatchStatus() == Status::InvalidOperation &&
        LegacyDispatchDiagnostic().find("ClearColor") != std::string::npos,
        "unrecorded clear-list state was silently accepted");
}

void NativeStartupArrays() {
    ResetT2bDriver();
    NativeFixture native;
    native.Install();
    GLuint texture = 0;
    qglGenTextures(1, &texture);
    qglBindTexture(GL_TEXTURE_2D, texture);
    qglEnable(GL_TEXTURE_2D);
    qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 16);
    std::vector<GLubyte> pixels{1, 2, 3, 4};
    qglTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA4, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    pixels.assign(pixels.size(), 0);
    std::vector<GLfloat> data{0, 0, 0, 0, 1, 0, 8, 0, 1, 1, 8, 8, 0, 1, 0, 8};
    T2bArrays(data.data());
    qglDrawArrays(GL_QUADS, 0, kT2bQuadVertices);
    const GLuint list = qglGenLists(1);
    qglNewList(list, GL_COMPILE);
    qglDrawArrays(GL_QUADS, 0, kT2bQuadVertices);
    qglEndList();
    data.clear();
    data.shrink_to_fit();
    qglColor4f(0, 1, 0, 1);
    qglCallList(list);
    qglDeleteTextures(1, &texture);
    qglFinish();
    Check(LegacyDispatchStatus() == Status::Ok && events.empty(), "native startup texture/array path failed");
    const auto& draws = native.submitted[0].draws;
    Check(draws.size() == 2 && draws[1].vertices[1].position[0] == 8 && draws[1].vertices[0].color[1] == 1,
        "native VGUI arrays not copied or replayed with current color");
    const auto image = draws[0].resources[0].texture2D;
    Check(image && image->sampler.requestedAnisotropy == 16 &&
        image->levels.at(0)->pixels == std::vector<std::uint8_t>({1, 2, 3, 4}),
        "native deleted texture draw lost upload bytes/sampler lifetime");
}

void NativeListNamespace() {
    ResetDriver();
    NativeFixture native;
    native.Install();
    constexpr GLuint kExplicitList = 7;
    qglNewList(kExplicitList, GL_COMPILE);
    std::array<float, 16> matrix{};
    matrix[0] = matrix[5] = matrix[10] = matrix[15] = 1;
    matrix[12] = 5;
    qglLoadMatrixf(matrix.data());
    qglEndList();
    const auto generated = qglGenLists(8);
    Check(generated == 8, "native generated range overlaps explicitly defined list");
    matrix.fill(0);
    qglCallList(kExplicitList);
    Check(native.frame.State().Matrix(MatrixMode::ModelView).values[12] == 5,
        "compiled native matrix retained borrowed source storage");
    qglDeleteLists(generated, 8);
    Check(qglGenLists(7) == 8, "native list range deletion did not release reservations");
    Check(LegacyDispatchStatus() == Status::Ok, "native list namespace failed");
}

void NativeCallbackFailure() {
    ResetDriver();
    NativeFixture native;
    native.Install();
    qglColor4f(1, 0, 0, 1);
    native.allowSync = false;
    qglFinish();
    Check(LegacyDispatchStatus() == Status::InvalidOperation &&
        LegacyDispatchDiagnostic().find("Finish") != std::string::npos &&
        native.frame.CaptureStatus() == Status::InvalidOperation, "sync failure did not poison recorder");
    native.allowSync = true;
    qglFinish();
    Check(native.submitted.empty() && events.empty(), "failed native frame was submitted/forwarded");
}

void NativeEnsureFailure() {
    ResetDriver();
    NativeFixture native;
    native.Install();
    native.allowEnsure = false;
    qglColor4f(1, 0, 0, 1);
    Check(LegacyDispatchStatus() == Status::InvalidOperation &&
        LegacyDispatchDiagnostic().find("Color4f") != std::string::npos,
        "startup ensure failure missing operation context");
    native.allowEnsure = true;
    qglColor4f(0, 1, 0, 1);
    Check(native.serial == 0 && events.empty(), "failed startup silently resumed recording");
}

void NativeFrontDrawRefusal() {
    ResetDriver();
    NativeFixture native;
    native.Install();
    qglDrawBuffer(GL_FRONT);
    Check(LegacyDispatchStatus() == Status::InvalidOperation &&
        LegacyDispatchDiagnostic().find("DrawBuffer") != std::string::npos,
        "unsupported native FRONT target was silently accepted");
}

void NativePaletteSnapshot() {
    ResetT2bDriver();
    NativeFixture native;
    native.Install();
    std::array<GLubyte, kPaletteBytes> palette{};
    palette[0] = 23;
    constexpr GLenum kSharedPaletteTarget = 0x81fb;
    qglColorTableEXT(kSharedPaletteTarget, GL_RGB, static_cast<GLsizei>(kPaletteEntries), GL_RGB,
        GL_UNSIGNED_BYTE, palette.data());
    T2bQuad();
    qglEnable(kSharedPaletteTarget);
    T2bQuad();
    qglDisable(kSharedPaletteTarget);
    T2bQuad();
    qglFinish();
    Check(LegacyDispatchStatus() == Status::Ok, "native palette snapshot failed");
    const auto& draws = native.submitted[0].draws;
    Check(draws.size() == 3 && !draws[0].sharedTexturePalette && draws[1].sharedTexturePalette &&
        !draws[2].sharedTexturePalette && draws[1].palette && (*draws[1].palette)[0] == 23,
        "native draws lost shared palette enable state or immutable version");
}

void NativeNoopPrimitiveRefusal() {
    ResetDriver();
    NativeFixture native;
    native.Install();
    qglBegin(GL_TRIANGLES);
    qglDisable(GL_LIGHTING);
    Check(LegacyDispatchStatus() == Status::InvalidOperation,
        "native default no-op capability ignored illegal Begin/End state");
}

void NativeZeroClear(const std::string& boundary) {
    ResetDriver();
    NativeFixture native;
    native.Install();
    if (boundary == "primitive") {
        qglBegin(GL_TRIANGLES);
    } else if (boundary == "list") {
        qglNewList(qglGenLists(1), GL_COMPILE);
    }
    qglClear(0);
    Check(LegacyDispatchStatus() == (boundary == "frame" ? Status::Ok : Status::InvalidOperation),
        "zero-mask clear must enforce primitive/list boundaries");
    if (boundary == "frame") {
        qglFinish();
        Check(native.submitted[0].events.empty(), "zero-mask clear emitted an event");
    }
}

void NativeIndexedBounds(const std::string& kind) {
    ResetDriver();
    FrameLimits limits;
    limits.capture.verticesPerDraw = kind == "quota" ? 2 : 16;
    NativeFixture native;
    native.frame = FrameRecorder(limits);
    native.Install();
    std::vector<Vertex> vertices(3);
    vertices[1].position[0] = 12;
    std::vector<std::uint32_t> indices{2, 1, 0};
    if (kind == "index") {
        indices[0] = 3;
    } else if (kind == "nontriangle") {
        indices.pop_back();
    }
    if (kind == "list") {
        const auto list = qglGenLists(1);
        qglNewList(list, GL_COMPILE);
        Check(SubmitNativeIndexed(vertices, indices, 2, true) == Status::Ok, "indexed list compile failed");
        qglEndList();
        vertices[1].position[0] = 99;
        indices[1] = 0;
        qglCallList(list);
        qglFinish();
        Check(LegacyDispatchStatus() == Status::Ok && native.submitted[0].draws.size() == 1 &&
            native.submitted[0].draws[0].vertices[1].position[0] == 12 &&
            native.submitted[0].draws[0].indices == std::vector<std::uint32_t>({2, 1, 0}),
            "indexed list lost owned geometry or replayed during compile");
    } else {
        const auto expected = kind == "quota" ? Status::LimitExceeded :
            kind == "index" ? Status::InvalidValue : Status::InvalidOperation;
        Check(SubmitNativeIndexed(vertices, indices, 2, true) == expected, "invalid indexed geometry accepted");
        FrameSnapshot snapshot;
        Check(native.frame.Seal(snapshot) != Status::Ok, "invalid indexed geometry allowed a partial frame");
    }
    Check(events.empty(), "indexed path forwarded to GL");
}
