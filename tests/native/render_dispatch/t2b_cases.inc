// Independent source-shaped examples. No expected values are generated from dispatch or the contract ledger.
constexpr GLuint kT2bTextureName = 0xf1234567u;
constexpr GLuint kT2bListName = 0xe2345678u;
constexpr GLsizei kT2bQuadVertices = 4;
constexpr GLint kT2bArrayComponents = 2;
constexpr GLsizei kT2bArrayStride = 4 * sizeof(GLfloat);
constexpr GLenum kT2bTexture1Sgis = 0x835f;
GLuint t2bGeneratedTexture = kT2bTextureName;
GLuint t2bGeneratedList = kT2bListName;

void APIENTRY SpyBindTexture(GLenum target, GLuint texture) {
    Record("BindTexture", {double(target), double(texture)});
}

void APIENTRY SpyDeleteTextures(GLsizei count, const GLuint* textures) {
    Record("DeleteTextures", {double(count)}, textures);
}

void APIENTRY SpyGenTextures(GLsizei count, GLuint* textures) {
    Record("GenTextures", {double(count)}, textures);
    if (count > 0 && textures) {
        textures[0] = t2bGeneratedTexture;
    }
}

void APIENTRY SpyPixelStorei(GLenum name, GLint value) {
    Record("PixelStorei", {double(name), double(value)});
}

void APIENTRY SpyTexImage2D(GLenum target, GLint level, GLint internalFormat, GLsizei width, GLsizei height,
    GLint border, GLenum format, GLenum type, const GLvoid* pixels) {
    Record("TexImage2D", {double(target), double(level), double(internalFormat), double(width), double(height),
        double(border), double(format), double(type)}, pixels);
}

void APIENTRY SpyTexSubImage2D(GLenum target, GLint level, GLint x, GLint y, GLsizei width, GLsizei height,
    GLenum format, GLenum type, const GLvoid* pixels) {
    Record("TexSubImage2D", {double(target), double(level), double(x), double(y), double(width), double(height),
        double(format), double(type)}, pixels);
}

void APIENTRY SpyTexParameteri(GLenum target, GLenum name, GLint value) {
    Record("TexParameteri", {double(target), double(name), double(value)});
}

void APIENTRY SpyColorTableEXT(GLenum target, GLenum internalFormat, GLsizei width, GLenum format,
    GLenum type, const void* table) {
    Record("ColorTableEXT", {double(target), double(internalFormat), double(width), double(format), double(type)},
        table);
}

GLuint APIENTRY SpyGenLists(GLsizei range) {
    Record("GenLists", {double(range)});
    return t2bGeneratedList;
}

void APIENTRY SpyNewList(GLuint list, GLenum mode) {
    Record("NewList", {double(list), double(mode)});
}

void APIENTRY SpyEndList() {
    Record("EndList");
}

void APIENTRY SpyCallList(GLuint list) {
    Record("CallList", {double(list)});
}

void APIENTRY SpyDeleteLists(GLuint list, GLsizei range) {
    Record("DeleteLists", {double(list), double(range)});
}

void APIENTRY SpyEnableClientState(GLenum array) {
    Record("EnableClientState", {double(array)});
}

void APIENTRY SpyTexCoordPointer(GLint size, GLenum type, GLsizei stride, const GLvoid* pointer) {
    Record("TexCoordPointer", {double(size), double(type), double(stride)}, pointer);
}

void APIENTRY SpyVertexPointer(GLint size, GLenum type, GLsizei stride, const GLvoid* pointer) {
    Record("VertexPointer", {double(size), double(type), double(stride)}, pointer);
}

void APIENTRY SpyDrawArrays(GLenum mode, GLint first, GLsizei count) {
    Record("DrawArrays", {double(mode), double(first), double(count)});
}

void APIENTRY SpyScissor(GLint x, GLint y, GLsizei width, GLsizei height) {
    Record("Scissor", {double(x), double(y), double(width), double(height)});
}

void APIENTRY SpyPolygonOffset(GLfloat factor, GLfloat units) {
    Record("PolygonOffset", {factor, units});
}

void ResetT2bDriver() {
    ResetDriver();
#define T2B_RESET_SLOT(name) qgl##name = &Spy##name;
    T2B_CORE_SLOTS(T2B_RESET_SLOT)
#undef T2B_RESET_SLOT
    qglColorTableEXT = &SpyColorTableEXT;
    qglSelectTextureSGIS = &SpySelectTexture;
    t2bGeneratedTexture = kT2bTextureName;
    t2bGeneratedList = kT2bListName;
}

void T2bQuad() {
    qglBegin(GL_QUADS);
    qglVertex2f(0, 0);
    qglVertex2f(8, 0);
    qglVertex2f(8, 8);
    qglVertex2f(0, 8);
    qglEnd();
}

void T2bArrays(GLfloat* data) {
    qglEnableClientState(GL_TEXTURE_COORD_ARRAY);
    qglEnableClientState(GL_VERTEX_ARRAY);
    qglTexCoordPointer(kT2bArrayComponents, GL_FLOAT, kT2bArrayStride, data);
    qglVertexPointer(kT2bArrayComponents, GL_FLOAT, kT2bArrayStride, data + kT2bArrayComponents);
}

void APIENTRY SpyLoggedBindTexture(GLenum target, GLuint texture) {
    Record("LogBindTexture", {double(target), double(texture)});
    SpyBindTexture(target, texture);
}

void T2bSlotLifecycle() {
    ResetT2bDriver();
    Install(DispatchMode::ReferenceGL, nullptr);
#define T2B_OWNED_SLOT(name) Check(qgl##name == &CseRender##name, "t2b-slot-lifecycle: unowned " #name);
    T2B_CORE_SLOTS(T2B_OWNED_SLOT)
#undef T2B_OWNED_SLOT
    Check(qglColorTableEXT == &CseRenderColorTableEXT, "t2b-slot-lifecycle: unowned palette slot");
    Install(DispatchMode::ReferenceGL, nullptr);
    qglBindTexture = &SpyLoggedBindTexture;
    RefreshLegacyDispatchReferences();
    RefreshLegacyDispatchReferences();
    qglBindTexture(GL_TEXTURE_2D, kT2bTextureName);
    Expect({{"LogBindTexture", {GL_TEXTURE_2D, kT2bTextureName}},
        {"BindTexture", {GL_TEXTURE_2D, kT2bTextureName}}});
    RemoveLegacyDispatch();
    Check(qglBindTexture == &SpyLoggedBindTexture, "t2b-slot-lifecycle: lost logged original");
    qglBindTexture = &SpyBindTexture;
#define T2B_RESTORED_SLOT(name) Check(qgl##name == &Spy##name, "t2b-slot-lifecycle: lost original " #name);
    T2B_CORE_SLOTS(T2B_RESTORED_SLOT)
#undef T2B_RESTORED_SLOT
    Check(qglColorTableEXT == &SpyColorTableEXT, "t2b-slot-lifecycle: lost palette original");
    qglColorTableEXT = nullptr;
    qglGenLists = nullptr;
    Install(DispatchMode::ReferenceGL, nullptr);
    RefreshLegacyDispatchReferences();
    RemoveLegacyDispatch();
    Check(!qglColorTableEXT && !qglGenLists, "t2b-slot-lifecycle: advertised null procedure");
}

void T2bForwardSurface() {
    for (const auto mode : {DispatchMode::ReferenceGL, DispatchMode::ShadowCapture}) {
        ResetT2bDriver();
        FrameRecorder frame;
        Start(frame);
        Install(mode, mode == DispatchMode::ReferenceGL ? nullptr : &frame);
        std::array<GLubyte, 4> pixel{{1, 2, 3, 4}};
        std::array<GLfloat, 16> geometry{};
        GLuint generated = 0;
        qglBindTexture(GL_TEXTURE_2D, kT2bTextureName);
        qglDeleteTextures(1, &kT2bTextureName);
        qglGenTextures(1, &generated);
        qglPixelStorei(GL_UNPACK_ROW_LENGTH, 3);
        qglTexImage2D(GL_TEXTURE_2D, 2, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
        qglTexSubImage2D(GL_TEXTURE_2D, 2, 5, 7, 1, 1, GL_BGRA, GL_UNSIGNED_BYTE, pixel.data());
        constexpr GLint kExactLargeLevel = 123456789;
        qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, kExactLargeLevel);
        qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 3.25f);
        constexpr GLenum kSharedTexturePalette = 0x81fb;
        qglColorTableEXT(kSharedTexturePalette, GL_RGB, 1, GL_RGB, GL_UNSIGNED_BYTE, pixel.data());
        Check(qglGenLists(1) == kT2bListName, "t2b-forward-surface: driver list result changed");
        qglNewList(kT2bListName, GL_COMPILE);
        qglEndList();
        qglCallList(kT2bListName);
        qglDeleteLists(kT2bListName, 1);
        qglEnableClientState(GL_VERTEX_ARRAY);
        qglTexCoordPointer(2, GL_FLOAT, kT2bArrayStride, geometry.data());
        qglVertexPointer(2, GL_FLOAT, kT2bArrayStride, geometry.data() + 2);
        qglDrawArrays(GL_QUADS, 0, kT2bQuadVertices);
        qglScissor(-1, 2, 3, 4);
        qglPolygonOffset(1.25f, -2.5f);
        Check(generated == kT2bTextureName, "t2b-forward-surface: driver texture output changed");
        Expect({{"BindTexture", {GL_TEXTURE_2D, kT2bTextureName}}, {"DeleteTextures", {1}, &kT2bTextureName},
            {"GenTextures", {1}, &generated}, {"PixelStorei", {GL_UNPACK_ROW_LENGTH, 3}},
            {"TexImage2D", {GL_TEXTURE_2D, 2, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE}, pixel.data()},
            {"TexSubImage2D", {GL_TEXTURE_2D, 2, 5, 7, 1, 1, GL_BGRA, GL_UNSIGNED_BYTE}, pixel.data()},
            {"TexParameteri", {GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, kExactLargeLevel}},
            {"TexParameterf", {GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 3.25}},
            {"ColorTableEXT", {kSharedTexturePalette, GL_RGB, 1, GL_RGB, GL_UNSIGNED_BYTE}, pixel.data()},
            {"GenLists", {1}}, {"NewList", {kT2bListName, GL_COMPILE}}, {"EndList", {}},
            {"CallList", {kT2bListName}}, {"DeleteLists", {kT2bListName, 1}},
            {"EnableClientState", {GL_VERTEX_ARRAY}},
            {"TexCoordPointer", {2, GL_FLOAT, kT2bArrayStride}, geometry.data()},
            {"VertexPointer", {2, GL_FLOAT, kT2bArrayStride}, geometry.data() + 2},
            {"DrawArrays", {GL_QUADS, 0, kT2bQuadVertices}}, {"Scissor", {-1, 2, 3, 4}},
            {"PolygonOffset", {1.25, -2.5}}});
        if (mode == DispatchMode::ShadowCapture) {
            Check(LegacyDispatchStatus() == Status::InvalidOperation,
                "t2b-forward-surface: unsupported generated-name capture not sticky");
        }
    }
}

void T2bMixedTexture() {
    ResetT2bDriver();
    FrameRecorder frame;
    Start(frame);
    Install(DispatchMode::ShadowCapture, &frame);
    std::array<GLubyte, 4> pixel{{11, 22, 33, 44}};
    std::array<GLubyte, 4> patch{{55, 66, 77, 88}};
    qglSelectTextureSGIS(kT2bTexture1Sgis);
    CseRenderBindTexture(GL_TEXTURE_2D, kT2bTextureName);
    qglPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GLfloat(GL_NEAREST));
    qglTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
    CseRenderTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, patch.data());
    T2bQuad();
    Expect({{"SelectTexture", {kT2bTexture1Sgis}}, {"BindTexture", {GL_TEXTURE_2D, kT2bTextureName}},
        {"PixelStorei", {GL_UNPACK_ALIGNMENT, 1}},
        {"TexParameteri", {GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST}},
        {"TexParameterf", {GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST}},
        {"TexImage2D", {GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE}, pixel.data()},
        {"TexSubImage2D", {GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE}, patch.data()},
        {"Begin", {GL_QUADS}}, {"Vertex2f", {0, 0}}, {"Vertex2f", {8, 0}}, {"Vertex2f", {8, 8}},
        {"Vertex2f", {0, 8}}, {"End", {}}});
    Check(frame.State().textures[1].texture2D == kT2bTextureName,
        "t2b-mixed-texture: raw bind did not capture full-width name on selected unit");
    FrameSnapshot snapshot;
    Check(frame.Seal(snapshot) == Status::Ok && snapshot.draws.size() == 1,
        "t2b-mixed-texture: texture command route missing from sealed draw");
    const auto texture = snapshot.draws[0].resources[1].texture2D;
    Check(texture && texture->levels.at(0)->pixels == std::vector<std::uint8_t>({55, 66, 77, 88}),
        "t2b-mixed-texture: owned subimage or active-unit resource missing");
    Check(texture->sampler.minification == Filter::Nearest && texture->sampler.magnification == Filter::Nearest,
        "t2b-mixed-texture: integer/float sampler enum mapping missing");
}

void T2bTextureMapping() {
    ResetT2bDriver();
    FrameRecorder frame;
    Start(frame);
    Install(DispatchMode::CaptureOnly, &frame);
    qglBindTexture(GL_TEXTURE_2D, kT2bTextureName);
    struct FilterCase {
        GLenum source;
        Filter expected;
    };
    const FilterCase filters[] = {{GL_NEAREST, Filter::Nearest}, {GL_LINEAR, Filter::Linear},
        {GL_NEAREST_MIPMAP_NEAREST, Filter::NearestMipmapNearest},
        {GL_LINEAR_MIPMAP_NEAREST, Filter::LinearMipmapNearest},
        {GL_NEAREST_MIPMAP_LINEAR, Filter::NearestMipmapLinear},
        {GL_LINEAR_MIPMAP_LINEAR, Filter::LinearMipmapLinear}};
    for (const auto& filter : filters) {
        qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GLint(filter.source));
        const auto texture = frame.Textures().Bound(frame.State(), TextureTarget::Texture2D);
        Check(LegacyDispatchStatus() == Status::Ok && texture && texture->sampler.minification == filter.expected,
            "t2b-texture-mapping: explicit GL filter to CPU enum map missing");
    }
    struct WrapCase {
        GLenum source;
        Wrap expected;
    };
    const WrapCase wraps[] = {{GL_REPEAT, Wrap::Repeat}, {GL_CLAMP, Wrap::Clamp},
        {GL_CLAMP_TO_EDGE, Wrap::ClampToEdge}};
    for (const auto& wrap : wraps) {
        qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GLint(wrap.source));
        qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GLfloat(wrap.source));
        const auto texture = frame.Textures().Bound(frame.State(), TextureTarget::Texture2D);
        Check(LegacyDispatchStatus() == Status::Ok && texture && texture->sampler.wrapS == wrap.expected &&
            texture->sampler.wrapT == wrap.expected, "t2b-texture-mapping: integer/float wrap enum map missing");
    }
    // Above float's exact integer range: an integer parameter must not pass through GLfloat.
    constexpr GLint kExactLargeLevel = 123456789;
    qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, kExactLargeLevel);
    const auto texture = frame.Textures().Bound(frame.State(), TextureTarget::Texture2D);
    Check(texture && texture->sampler.maximumLevel == kExactLargeLevel,
        "t2b-texture-mapping: integer sampler precision changed");
    qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 3.25f);
    Check(frame.Textures().Bound(frame.State(), TextureTarget::Texture2D)->sampler.requestedAnisotropy == 3.25f,
        "t2b-texture-mapping: fractional anisotropy changed");
    // This invalid minification enum cannot silently become CPU enum ordinal zero.
    qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, 0);
    Check(LegacyDispatchStatus() == Status::InvalidValue, "t2b-texture-mapping: unknown filter enum accepted");
    FrameSnapshot snapshot;
    Check(frame.Seal(snapshot) == Status::InvalidValue && events.empty(),
        "t2b-texture-mapping: rejected enum sealed or CaptureOnly called driver");
}

void T2bUploadExtentContract() {
    TextureUpload upload;
    upload.width = 1;
    upload.height = 2;
    upload.format = PixelFormat::RGB;
    PixelStoreState unpack;
    constexpr std::size_t kUntouchedExtent = 91;
    std::size_t extent = kUntouchedExtent;
    Check(TextureStore::UploadByteExtent(upload, unpack, extent) == Status::Ok && extent == 7,
        "t2b-upload-extent: RGB final row must not include padding");
    upload.format = PixelFormat::BGRA;
    unpack.unpackRowLength = 3;
    Check(TextureStore::UploadByteExtent(upload, unpack, extent) == Status::Ok && extent == 16,
        "t2b-upload-extent: BGRA source row length changed");
    upload.height = 0;
    Check(TextureStore::UploadByteExtent(upload, unpack, extent) == Status::Ok && extent == 0,
        "t2b-upload-extent: empty upload has a nonzero read bound");
    upload.height = 2;
    upload.width = -1;
    extent = kUntouchedExtent;
    Check(TextureStore::UploadByteExtent(upload, unpack, extent) == Status::InvalidValue &&
        extent == kUntouchedExtent, "t2b-upload-extent: invalid dimensions changed output");
    upload.width = 1;
    unpack.unpackAlignment = 3;
    Check(TextureStore::UploadByteExtent(upload, unpack, extent) == Status::InvalidValue &&
        extent == kUntouchedExtent, "t2b-upload-extent: invalid alignment changed output");
    unpack.unpackAlignment = 4;
    unpack.unpackRowLength = 0;
    upload.width = kTextureDimension;
    upload.height = kTextureDimension;
    Check(TextureStore::UploadByteExtent(upload, unpack, extent) == Status::LimitExceeded &&
        extent == kUntouchedExtent, "t2b-upload-extent: upload quota changed failure/output");
}

void T2bPaletteOwnership() {
    ResetT2bDriver();
    FrameRecorder frame;
    Start(frame);
    Install(DispatchMode::ShadowCapture, &frame);
    std::array<GLubyte, kPaletteBytes> palette{};
    palette[0] = 9;
    palette[1] = 23;
    palette[2] = 47;
    constexpr GLenum kSharedTexturePalette = 0x81fb;
    CseRenderColorTableEXT(kSharedTexturePalette, GL_RGB, static_cast<GLsizei>(kPaletteEntries),
        GL_RGB, GL_UNSIGNED_BYTE, palette.data());
    Expect({{"ColorTableEXT", {kSharedTexturePalette, GL_RGB, double(kPaletteEntries), GL_RGB, GL_UNSIGNED_BYTE},
        palette.data()}});
    palette.fill(0);
    T2bQuad();
    FrameSnapshot snapshot;
    Check(frame.Seal(snapshot) == Status::Ok && snapshot.draws.size() == 1 && snapshot.draws[0].palette,
        "t2b-palette-ownership: source palette did not enter owned snapshot");
    Check((*snapshot.draws[0].palette)[0] == 9 && (*snapshot.draws[0].palette)[1] == 23 &&
        (*snapshot.draws[0].palette)[2] == 47, "t2b-palette-ownership: retained caller palette pointer");
}

void T2bUnpackOwned() {
    ResetT2bDriver();
    FrameRecorder frame;
    Start(frame);
    Install(DispatchMode::CaptureOnly, &frame);
    qglBindTexture(GL_TEXTURE_2D, kT2bTextureName);
    std::vector<GLubyte> rgb{1, 2, 3, 99, 4, 5, 6};
    qglPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    qglTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 2, 0, GL_RGB, GL_UNSIGNED_BYTE, rgb.data());
    rgb.assign(rgb.size(), 0);
    T2bQuad();
    std::vector<GLubyte> bgra{3, 2, 1, 4, 90, 90, 90, 90, 91, 91, 91, 91, 7, 6, 5, 8};
    qglPixelStorei(GL_UNPACK_ROW_LENGTH, 3);
    qglTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 2, GL_BGRA, GL_UNSIGNED_BYTE, bgra.data());
    CseRenderPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    bgra.clear();
    bgra.shrink_to_fit();
    T2bQuad();
    Check(frame.Textures().Unpack().unpackRowLength == 0, "t2b-unpack-owned: raw unpack reset was not captured");
    FrameSnapshot snapshot;
    Check(frame.Seal(snapshot) == Status::Ok && snapshot.draws.size() == 2,
        "t2b-unpack-owned: upload/raw-unpack routes cannot seal two owned draws");
    Check(snapshot.draws[0].resources[0].texture2D->levels.at(0)->pixels ==
        std::vector<std::uint8_t>({1, 2, 3, 255, 4, 5, 6, 255}), "t2b-unpack-owned: RGB stride/lifetime changed");
    Check(snapshot.draws[1].resources[0].texture2D->levels.at(0)->pixels ==
        std::vector<std::uint8_t>({1, 2, 3, 4, 5, 6, 7, 8}), "t2b-unpack-owned: BGRA row length/lifetime changed");
    Check(events.empty(), "t2b-unpack-owned: CaptureOnly called driver");
}

void T2bArrayCopyTime() {
    ResetT2bDriver();
    FrameRecorder frame;
    Start(frame);
    Install(DispatchMode::CaptureOnly, &frame);
    std::vector<GLfloat> data{0, 0, 0, 0, 1, 0, 8, 0, 1, 1, 8, 8, 0, 1, 0, 8};
    constexpr std::uintptr_t kMaximum32BitAddress = 0xffffffffu;
    Check(sizeof(void*) > sizeof(std::uint32_t) &&
        reinterpret_cast<std::uintptr_t>(data.data()) > kMaximum32BitAddress,
        "t2b-array-copy-time: qualification requires actual source address above UINT32_MAX");
    T2bArrays(data.data());
    data[0] = 0.25f;
    data[2] = 3;
    qglDrawArrays(GL_QUADS, 0, kT2bQuadVertices);
    CseRenderNewList(kT2bListName, GL_COMPILE);
    data[0] = 0.75f;
    data[2] = 5;
    qglDrawArrays(GL_QUADS, 0, kT2bQuadVertices);
    CseRenderEndList();
    data.assign(data.size(), -99);
    data.clear();
    data.shrink_to_fit();
    qglColor4f(0, 1, 0, 1);
    CseRenderCallList(kT2bListName);
    CseRenderDeleteLists(kT2bListName, 1);
    FrameSnapshot snapshot;
    Check(frame.Seal(snapshot) == Status::Ok && snapshot.draws.size() == 2,
        "t2b-array-copy-time: draw/list compile did not own source-profile arrays");
    Check(snapshot.draws[0].vertices[0].position[0] == 3 && snapshot.draws[0].vertices[0].texcoords[0][0] == 0.25f,
        "t2b-array-copy-time: copied at registration or retained draw pointer");
    Check(snapshot.draws[1].vertices[0].position[0] == 5 && snapshot.draws[1].vertices[0].texcoords[0][0] == 0.75f,
        "t2b-array-copy-time: list replay read dead source storage");
    Check(snapshot.draws[1].vertices[0].color == std::array<float, 4>{{0, 1, 0, 1}},
        "t2b-array-copy-time: list arrays lost replay-time current color");
    Check(events.empty(), "t2b-array-copy-time: CaptureOnly called driver");
}

void T2bListRouting() {
    for (const auto mode : {GL_COMPILE, GL_COMPILE_AND_EXECUTE}) {
        ResetT2bDriver();
        FrameRecorder frame;
        Start(frame);
        Install(DispatchMode::ShadowCapture, &frame);
        CseRenderNewList(kT2bListName, GLenum(mode));
        T2bQuad();
        CseRenderEndList();
        Check(LegacyDispatchStatus() == Status::Ok,
            "t2b-list-routing: raw NewList/EndList facade did not capture qgl geometry");
        qglColor4f(0, 1, 0, 1);
        qglTranslatef(2, 3, 0);
        CseRenderCallList(kT2bListName);
        CseRenderDeleteLists(kT2bListName, 1);
        Expect({{"NewList", {kT2bListName, double(mode)}}, {"Begin", {GL_QUADS}},
            {"Vertex2f", {0, 0}}, {"Vertex2f", {8, 0}}, {"Vertex2f", {8, 8}}, {"Vertex2f", {0, 8}},
            {"End", {}}, {"EndList", {}}, {"Color4f", {0, 1, 0, 1}}, {"Translatef", {2, 3, 0}},
            {"CallList", {kT2bListName}}, {"DeleteLists", {kT2bListName, 1}}});
        FrameSnapshot snapshot;
        const std::size_t expectedDraws = mode == GL_COMPILE ? 1 : 2;
        Check(frame.Seal(snapshot) == Status::Ok && snapshot.draws.size() == expectedDraws,
            "t2b-list-routing: COMPILE/COMPILE_AND_EXECUTE draw count changed");
        const auto& replay = snapshot.draws.back();
        Check(replay.vertices[0].color == std::array<float, 4>{{0, 1, 0, 1}} &&
            replay.modelView.values[12] == 2 && replay.modelView.values[13] == 3,
            "t2b-list-routing: replay ignored later current color/matrix");
    }
}

void T2bArrayRefusal() {
    struct Case {
        const char* label;
        GLenum capability;
        GLint size;
        GLenum type;
        GLsizei stride;
        GLint first;
        GLsizei count;
        bool nullPointer;
        bool overflowPointer;
        Status status;
    };
    // R6 permits only the engine's interleaved float2 UV/XY profile.
    const Case cases[] = {
        {"negative first", GL_VERTEX_ARRAY, 2, GL_FLOAT, kT2bArrayStride, -1, 4, false, false, Status::InvalidValue},
        {"negative count", GL_VERTEX_ARRAY, 2, GL_FLOAT, kT2bArrayStride, 0, -1, false, false, Status::InvalidValue},
        {"negative stride", GL_VERTEX_ARRAY, 2, GL_FLOAT, -1, 0, 4, false, false, Status::InvalidValue},
        {"unsupported size", GL_VERTEX_ARRAY, 3, GL_FLOAT, kT2bArrayStride, 0, 4, false, false,
            Status::InvalidOperation},
        {"unsupported type", GL_VERTEX_ARRAY, 2, GL_DOUBLE, kT2bArrayStride, 0, 4, false, false,
            Status::InvalidOperation},
        {"unsupported capability", GL_NORMAL_ARRAY, 2, GL_FLOAT, kT2bArrayStride, 0, 4, false, false,
            Status::InvalidOperation},
        {"unsupported first", GL_VERTEX_ARRAY, 2, GL_FLOAT, kT2bArrayStride, 1, 4, false, false,
            Status::InvalidOperation},
        {"unsupported zero stride", GL_VERTEX_ARRAY, 2, GL_FLOAT, 0, 0, 4, false, false, Status::InvalidOperation},
        {"null positive count", GL_VERTEX_ARRAY, 2, GL_FLOAT, kT2bArrayStride, 0, 4, true, false, Status::InvalidValue},
        {"pointer extent overflow", GL_VERTEX_ARRAY, 2, GL_FLOAT, kT2bArrayStride, 0, 4, false, true,
            Status::InvalidValue},
        {"bounded geometry quota", GL_VERTEX_ARRAY, 2, GL_FLOAT, kT2bArrayStride, 0, 4, false, false,
            Status::LimitExceeded}
    };
    for (const auto mode : {DispatchMode::ShadowCapture, DispatchMode::CaptureOnly}) {
        for (const auto& test : cases) {
            ResetT2bDriver();
            FrameLimits limits;
            if (test.status == Status::LimitExceeded) {
                limits.capture.verticesPerDraw = kT2bQuadVertices - 1;
            }
            FrameRecorder frame(limits);
            Start(frame);
            Install(mode, &frame);
            std::array<GLfloat, 16> data{};
            const void* pointer = data.data();
            if (test.nullPointer) {
                pointer = nullptr;
            } else if (test.overflowPointer) {
                constexpr std::uintptr_t kLastAlignedAddress = ~std::uintptr_t{0} - (sizeof(GLfloat) - 1);
                pointer = reinterpret_cast<const void*>(kLastAlignedAddress);
            }
            qglEnableClientState(GL_TEXTURE_COORD_ARRAY);
            qglEnableClientState(test.capability);
            qglTexCoordPointer(2, GL_FLOAT, kT2bArrayStride, pointer);
            qglVertexPointer(test.size, test.type, test.stride, pointer);
            qglDrawArrays(GL_QUADS, test.first, test.count);
            if (LegacyDispatchStatus() != test.status) {
                std::fprintf(stderr, "T2b array refusal row: %s\n", test.label);
            }
            Check(LegacyDispatchStatus() == test.status,
                "t2b-array-refusal: missing explicit typed/extent/quota refusal");
            FrameSnapshot snapshot;
            Check(frame.Seal(snapshot) == test.status, "t2b-array-refusal: refused geometry sealed");
            if (mode == DispatchMode::ShadowCapture) {
                Expect({{"EnableClientState", {GL_TEXTURE_COORD_ARRAY}},
                    {"EnableClientState", {double(test.capability)}},
                    {"TexCoordPointer", {2, GL_FLOAT, kT2bArrayStride}, pointer},
                    {"VertexPointer", {double(test.size), double(test.type), double(test.stride)}, pointer},
                    {"DrawArrays", {GL_QUADS, double(test.first), double(test.count)}}});
            } else {
                Check(events.empty(), "t2b-array-refusal: CaptureOnly invoked driver");
            }
        }
    }
    ResetT2bDriver();
    FrameRecorder frame;
    Start(frame);
    Install(DispatchMode::CaptureOnly, &frame);
    qglEnableClientState(GL_TEXTURE_COORD_ARRAY);
    qglEnableClientState(GL_VERTEX_ARRAY);
    qglTexCoordPointer(2, GL_FLOAT, kT2bArrayStride, nullptr);
    qglVertexPointer(2, GL_FLOAT, kT2bArrayStride, nullptr);
    qglDrawArrays(GL_QUADS, 0, 0);
    FrameSnapshot snapshot;
    Check(frame.Seal(snapshot) == Status::Ok && snapshot.draws.empty() && events.empty(),
        "t2b-array-refusal: zero count dereferenced/refused empty source profile");
}

void T2bUiLightmapTrace() {
    ResetT2bDriver();
    FrameRecorder frame;
    Start(frame);
    Install(DispatchMode::ShadowCapture, &frame);
    std::array<GLfloat, 16> glyph{{0, 0, 0, 0, 1, 0, 8, 0, 1, 1, 8, 8, 0, 1, 0, 8}};
    std::array<GLubyte, 4> rgba{{1, 2, 3, 4}};
    std::array<GLubyte, 4> bgra{{7, 6, 5, 8}};
    std::array<GLubyte, 3> rgb{{9, 10, 11}};
    std::array<GLubyte, 1> luminance{{12}};
    qglBindTexture(GL_TEXTURE_2D, kT2bTextureName);
    qglTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    qglPixelStorei(GL_UNPACK_ROW_LENGTH, 3);
    qglTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_BGRA, GL_UNSIGNED_BYTE, bgra.data());
    CseRenderPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    T2bArrays(glyph.data());
    qglDrawArrays(GL_QUADS, 0, kT2bQuadVertices);
    qglTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, rgb.data());
    T2bQuad();
    qglTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, 1, 1, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, luminance.data());
    qglTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_LUMINANCE, GL_UNSIGNED_BYTE, luminance.data());
    T2bQuad();
    Check(events.size() == 25, "t2b-ui-lightmap-trace: source-shaped driver event count changed");
    Check(events[4].name == "PixelStorei" && events[4].values == std::vector<double>({GL_UNPACK_ROW_LENGTH, 0}) &&
        events[9].name == "DrawArrays" && events[10].name == "TexImage2D" &&
        events[17].name == "TexImage2D" && events[18].name == "TexSubImage2D",
        "t2b-ui-lightmap-trace: raw reset/array/lightmap driver order changed");
    FrameSnapshot snapshot;
    Check(frame.Seal(snapshot) == Status::Ok && snapshot.events.size() == 3 && snapshot.draws.size() == 3,
        "t2b-ui-lightmap-trace: glyph/lightmap producer routes missing");
    Check(snapshot.draws[0].resources[0].texture2D->levels.at(0)->pixels == std::vector<std::uint8_t>({5, 6, 7, 8}) &&
        snapshot.draws[1].resources[0].texture2D->levels.at(0)->pixels == std::vector<std::uint8_t>({9, 10, 11, 255}) &&
        snapshot.draws[2].resources[0].texture2D->levels.at(0)->pixels == std::vector<std::uint8_t>({12, 12, 12, 255}),
        "t2b-ui-lightmap-trace: RGBA/BGRA/RGB/luminance map or retained version changed");
    Check(frame.Textures().Unpack().unpackRowLength == 0, "t2b-ui-lightmap-trace: source row length leaked");
}

void T2bGeneratedNames() {
    for (const auto mode : {DispatchMode::ReferenceGL, DispatchMode::ShadowCapture, DispatchMode::CaptureOnly}) {
        for (const GLuint driverName : {kT2bTextureName, GLuint{0}}) {
            ResetT2bDriver();
            FrameRecorder frame;
            Start(frame);
            Install(mode, mode == DispatchMode::ReferenceGL ? nullptr : &frame);
            t2bGeneratedTexture = driverName;
            t2bGeneratedList = driverName;
            constexpr GLuint kUntouchedOutput = 0xabcd1234u;
            GLuint texture = kUntouchedOutput;
            qglGenTextures(1, &texture);
            const GLuint list = CseRenderGenLists(1);
            if (mode == DispatchMode::CaptureOnly) {
                Check(texture == kUntouchedOutput && list == 0 && events.empty(),
                    "t2b-generated-names: CaptureOnly fabricated names or called driver");
            } else {
                Check(texture == driverName && list == driverName,
                    "t2b-generated-names: actual driver return/output changed");
                Expect({{"GenTextures", {1}, &texture}, {"GenLists", {1}}});
            }
            if (mode != DispatchMode::ReferenceGL) {
                Check(LegacyDispatchStatus() == Status::InvalidOperation,
                    "t2b-generated-names: unsupported name generation was not explicit");
                FrameSnapshot snapshot;
                Check(frame.Seal(snapshot) == Status::InvalidOperation,
                    "t2b-generated-names: unsupported namespace sealed");
            }
        }
    }
}

void T2bListImmediateState() {
    ResetT2bDriver();
    FrameRecorder frame;
    Start(frame);
    Install(DispatchMode::CaptureOnly, &frame);
    qglBindTexture(GL_TEXTURE_2D, kT2bTextureName);
    Check(frame.Textures().IsTexture(kT2bTextureName), "t2b-list-immediate-state: bound texture route missing");
    CseRenderNewList(kT2bListName, GL_COMPILE);
    qglPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    qglDeleteTextures(1, &kT2bTextureName);
    Check(frame.Textures().Unpack().unpackAlignment == 1 && !frame.Textures().IsTexture(kT2bTextureName),
        "t2b-list-immediate-state: PixelStore/DeleteTextures deferred during COMPILE");
    std::vector<GLubyte> rgb{1, 2, 3, 4, 5, 6};
    qglBindTexture(GL_TEXTURE_2D, kT2bTextureName);
    qglTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 2, 0, GL_RGB, GL_UNSIGNED_BYTE, rgb.data());
    T2bQuad();
    CseRenderEndList();
    rgb.clear();
    rgb.shrink_to_fit();
    qglPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    CseRenderCallList(kT2bListName);
    Check(frame.Textures().Unpack().unpackAlignment == 4 && frame.Textures().IsTexture(kT2bTextureName),
        "t2b-list-immediate-state: immediate list state replayed unexpectedly");
    FrameSnapshot snapshot;
    Check(frame.Seal(snapshot) == Status::Ok && snapshot.draws.size() == 1,
        "t2b-list-immediate-state: compile-time upload did not survive caller death");
    Check(snapshot.draws[0].resources[0].texture2D->levels.at(0)->pixels ==
        std::vector<std::uint8_t>({1, 2, 3, 255, 4, 5, 6, 255}),
        "t2b-list-immediate-state: upload lost compile-time unpack state");
    Check(events.empty(), "t2b-list-immediate-state: CaptureOnly called driver");
}

void T2bListRefusal() {
    const Status expected[] = {Status::InvalidValue, Status::InvalidValue, Status::InvalidOperation,
        Status::InvalidOperation, Status::InvalidOperation, Status::InvalidValue, Status::InvalidOperation,
        Status::LimitExceeded};
    for (std::size_t test = 0; test < sizeof(expected) / sizeof(expected[0]); ++test) {
        ResetT2bDriver();
        FrameLimits limits;
        limits.capture.replayCommands = 1;
        FrameRecorder frame(limits);
        Start(frame);
        Install(DispatchMode::ShadowCapture, &frame);
        switch (test) {
        case 0:
            CseRenderNewList(0, GL_COMPILE);
            break;
        case 1:
            CseRenderNewList(kT2bListName, GL_TRIANGLES);
            break;
        case 2:
            CseRenderNewList(kT2bListName, GL_COMPILE);
            CseRenderNewList(kT2bListName, GL_COMPILE);
            break;
        case 3:
            qglBegin(GL_QUADS);
            CseRenderNewList(kT2bListName, GL_COMPILE);
            break;
        case 4:
            CseRenderEndList();
            break;
        case 5:
            CseRenderDeleteLists(kT2bListName, -1);
            break;
        case 6:
            CseRenderDeleteLists(~GLuint{0}, 2);
            break;
        default:
            CseRenderNewList(kT2bListName, GL_COMPILE);
            qglColor4f(1, 0, 0, 1);
            qglColor4f(0, 1, 0, 1);
            CseRenderEndList();
            CseRenderCallList(kT2bListName);
            break;
        }
        const Status first = LegacyDispatchStatus();
        Check(first == expected[test], "t2b-list-refusal: wrong invalid/unsupported/range/replay refusal");
        CseRenderBindTexture(GL_TEXTURE_2D, kT2bTextureName);
        Check(LegacyDispatchStatus() == first, "t2b-list-refusal: first refusal changed");
        Check(!events.empty() && events.back().name == "BindTexture",
            "t2b-list-refusal: poisoned Shadow stopped forwarding");
        FrameSnapshot snapshot;
        Check(frame.Seal(snapshot) == first, "t2b-list-refusal: invalid/unsupported list sealed");
    }
}

void T2bListZeroRangeInPrimitive() {
    ResetT2bDriver();
    FrameRecorder frame;
    Start(frame);
    Install(DispatchMode::ShadowCapture, &frame);
    qglBegin(GL_QUADS);
    CseRenderDeleteLists(kT2bListName, 0);
    qglEnd();
    Expect({{"Begin", {GL_QUADS}}, {"DeleteLists", {kT2bListName, 0}}, {"End", {}}});
    FrameSnapshot snapshot;
    Check(LegacyDispatchStatus() == Status::InvalidOperation && frame.Seal(snapshot) == Status::InvalidOperation,
        "t2b-list-refusal: zero range bypassed Begin/End refusal");
}

void T2bExtensionOwnership() {
    ResetT2bDriver();
    qglColorTableEXT = nullptr;
    Install(DispatchMode::ReferenceGL, nullptr);
    const auto bind = reinterpret_cast<LegacyProcedure>(&SpyBindTexture);
    const auto palette = reinterpret_cast<LegacyProcedure>(&SpyColorTableEXT);
    Check(RouteLegacyExtension("glBindTextureEXT", bind) == reinterpret_cast<LegacyProcedure>(&CseRenderBindTexture),
        "t2b-extension-ownership: late BindTextureEXT handoff is missing");
    Check(RouteLegacyExtension("glColorTableEXT", palette) ==
        reinterpret_cast<LegacyProcedure>(&CseRenderColorTableEXT),
        "t2b-extension-ownership: late ColorTableEXT handoff is missing");
    const auto wrapper = reinterpret_cast<LegacyProcedure>(&CseRenderBindTexture);
    Check(RouteLegacyExtension("glBindTextureEXT", wrapper) == wrapper,
        "t2b-extension-ownership: wrapper-as-procedure lost original");
    RefreshLegacyDispatchReferences();
    qglBindTexture(GL_TEXTURE_2D, kT2bTextureName);
    constexpr GLenum kSharedTexturePalette = 0x81fb;
    std::array<GLubyte, 3> table{{1, 2, 3}};
    qglColorTableEXT(kSharedTexturePalette, GL_RGB, 1, GL_RGB, GL_UNSIGNED_BYTE, table.data());
    Expect({{"BindTexture", {GL_TEXTURE_2D, kT2bTextureName}},
        {"ColorTableEXT", {kSharedTexturePalette, GL_RGB, 1, GL_RGB, GL_UNSIGNED_BYTE}, table.data()}});
    Check(RouteLegacyExtension("glUnownedTextureEXT", bind) == nullptr,
        "t2b-extension-ownership: unknown extension advertised");
    Check(RouteLegacyExtension("glColorTableEXT", nullptr) == nullptr && qglColorTableEXT == nullptr,
        "t2b-extension-ownership: absent palette advertised");
    RemoveLegacyDispatch();
    Check(qglBindTexture == &SpyBindTexture && !qglColorTableEXT,
        "t2b-extension-ownership: last original/null not restored");
    Install(DispatchMode::ReferenceGL, nullptr);
    Check(qglBindTexture == &CseRenderBindTexture && !qglColorTableEXT,
        "t2b-extension-ownership: core bind fallback or absent palette changed on reinstall");
}

void T2bShadowAfterRefusal() {
    std::vector<Event> reference;
    std::array<GLubyte, 4> pixel{{3, 6, 9, 12}};
    std::array<GLfloat, 16> geometry{{0, 0, 0, 0, 1, 0, 8, 0, 1, 1, 8, 8, 0, 1, 0, 8}};
    GLuint generated = 0;
    GLint query = 0;
    for (const auto mode : {DispatchMode::ReferenceGL, DispatchMode::ShadowCapture}) {
        ResetT2bDriver();
        FrameLimits limits;
        limits.capture.textureStorageBytes = pixel.size() - 1;
        FrameRecorder frame(limits);
        Start(frame);
        Install(mode, mode == DispatchMode::ReferenceGL ? nullptr : &frame);
        qglBindTexture(GL_TEXTURE_2D, kT2bTextureName);
        qglTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
        if (mode == DispatchMode::ShadowCapture) {
            Check(LegacyDispatchStatus() == Status::LimitExceeded,
                "t2b-shadow-after-refusal: bounded valid upload did not reach storage quota");
        }
        qglTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
        CseRenderNewList(kT2bListName, GL_COMPILE);
        T2bArrays(geometry.data());
        qglDrawArrays(GL_QUADS, 0, kT2bQuadVertices);
        CseRenderEndList();
        CseRenderCallList(kT2bListName);
        qglGenTextures(1, &generated);
        Check(CseRenderGenLists(1) == kT2bListName && generated == kT2bTextureName,
            "t2b-shadow-after-refusal: driver name return/output changed");
        qglGetIntegerv(GL_MAX_TEXTURE_SIZE, &query);
        Check(query == 8, "t2b-shadow-after-refusal: driver query output changed");
        if (mode == DispatchMode::ReferenceGL) {
            reference = events;
        } else {
            Check(events.size() == reference.size(), "t2b-shadow-after-refusal: driver count differs from ReferenceGL");
            for (std::size_t index = 0; index < reference.size(); ++index) {
                Check(events[index].name == reference[index].name && events[index].values == reference[index].values &&
                    events[index].pointer == reference[index].pointer,
                    "t2b-shadow-after-refusal: driver order/arguments/pointers differ from ReferenceGL");
            }
            Check(LegacyDispatchStatus() == Status::LimitExceeded,
                "t2b-shadow-after-refusal: later unsupported generation replaced first failure");
            FrameSnapshot snapshot;
            Check(frame.Seal(snapshot) == Status::LimitExceeded, "t2b-shadow-after-refusal: poisoned frame sealed");
            RemoveLegacyDispatch();
            frame.Abort();
            Start(frame);
            Install(DispatchMode::CaptureOnly, &frame);
            qglBindTexture(GL_TEXTURE_2D, kT2bTextureName);
            T2bQuad();
            Check(frame.Seal(snapshot) == Status::Ok,
                "t2b-shadow-after-refusal: explicit Abort/new frame did not recover");
        }
    }
}

void T2bStateAndNoFrame() {
    ResetT2bDriver();
    FrameRecorder frame;
    Install(DispatchMode::ShadowCapture, &frame);
    qglBindTexture(GL_TEXTURE_2D, kT2bTextureName);
    Check(LegacyDispatchStatus() == Status::InvalidOperation,
        "t2b-state-no-frame: out-of-frame Shadow dropped initialization");
    Expect({{"BindTexture", {GL_TEXTURE_2D, kT2bTextureName}}});
    RemoveLegacyDispatch();
    frame.Abort();
    Start(frame);
    Install(DispatchMode::CaptureOnly, &frame);
    qglScissor(1, 2, 3, 4);
    qglPolygonOffset(1.25f, -2.5f);
    CseRenderEnable(GL_FOG);
    CseRenderFogi(GL_FOG_MODE, GL_LINEAR);
    CseRenderDisable(GL_FOG);
    Check(frame.State().raster.scissor == std::array<int, 4>{{1, 2, 3, 4}} &&
        frame.State().raster.polygonOffset == std::array<float, 2>{{1.25f, -2.5f}} &&
        frame.State().raster.fogMode == FogMode::Linear &&
        !frame.State().raster.enabled[static_cast<std::size_t>(Capability::Fog)],
        "t2b-state-no-frame: Scissor/PolygonOffset/raw state mapping missing");
}

bool RunT2bCase(const std::string& name) {
    if (name == "t2b-slot-lifecycle") {
        T2bSlotLifecycle();
        T2bForwardSurface();
    } else if (name == "t2b-mixed-texture") {
        T2bMixedTexture();
    } else if (name == "t2b-texture-mapping") {
        T2bTextureMapping();
        T2bPaletteOwnership();
    } else if (name == "t2b-unpack-owned") {
        T2bUploadExtentContract();
        T2bUnpackOwned();
    } else if (name == "t2b-ui-lightmap-trace") {
        T2bUiLightmapTrace();
    } else if (name == "t2b-array-copy-time") {
        T2bArrayCopyTime();
    } else if (name == "t2b-array-refusal") {
        T2bArrayRefusal();
    } else if (name == "t2b-list-routing") {
        T2bListRouting();
    } else if (name == "t2b-list-immediate-state") {
        T2bListImmediateState();
    } else if (name == "t2b-list-refusal") {
        T2bListRefusal();
        T2bListZeroRangeInPrimitive();
    } else if (name == "t2b-generated-names") {
        T2bGeneratedNames();
    } else if (name == "t2b-extension-ownership") {
        T2bExtensionOwnership();
    } else if (name == "t2b-shadow-after-refusal") {
        T2bShadowAfterRefusal();
    } else if (name == "t2b-state-no-frame") {
        T2bStateAndNoFrame();
    } else {
        return false;
    }
    return true;
}
