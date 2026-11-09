// Included inside the actual oracle's private namespace; shares its real GL dispatch/context.
void CheckImage(GL& gl, const Recorder& cpu, TextureTarget target, int level, const char* label) {
    const auto image = cpu.Textures().Bound(cpu.State(), target)->levels.at(level);
    Check(image->defined && image->internal != InternalFormat::ColorIndex8, "selected defined nonindexed image");
    std::vector<std::uint8_t> bytes(image->pixels.size());
    auto expected = image->pixels;
    // GL2.1 table6.1 query mapping differs from the sampling expansion captured by the store.
    for (std::size_t index = 0; index < expected.size(); index += 4) {
        if (image->internal == InternalFormat::Alpha8) {
            expected[index] = expected[index + 1] = expected[index + 2] = 0;
        } else if (image->internal == InternalFormat::Luminance8 || image->internal == InternalFormat::Intensity8) {
            expected[index + 1] = expected[index + 2] = 0;
            expected[index + 3] = 255;
        }
    }
    gl.PixelStorei(GL_PACK_ALIGNMENT, 1);
    gl.GetTexImage(textureTargets[static_cast<std::size_t>(target)], level, GL_RGBA, GL_UNSIGNED_BYTE, bytes.data());
    std::printf("texture_readback=%s bytes=%zu\n", label, bytes.size());
    if (bytes != expected) {
        for (std::size_t index = 0; index < bytes.size(); ++index) {
            if (bytes[index] != expected[index]) {
                std::printf("texture_byte_delta=%s index=%zu GL=%u CPU=%u\n", label, index,
                    static_cast<unsigned>(bytes[index]), static_cast<unsigned>(expected[index]));
            }
        }
    }
    Check(bytes == expected, "actual GL selected 8-bit query mapping equals captured logical components");
    Check(gl.GetError() == GL_NO_ERROR, "texture readback has no GL errors");
}

Command TextureParameterCommand(TextureParameter parameter, double value) {
    auto command = Make(Op::TextureParameter, {value});
    command.second = static_cast<std::uint32_t>(parameter);
    return command;
}

Command EnvironmentCommand(EnvironmentParameter parameter, std::uint32_t value) {
    auto command = Make(Op::TextureEnvironment, {}, static_cast<std::uint32_t>(parameter));
    command.second = value;
    return command;
}

void TextureUploadOracle(GL& gl) {
    Recorder cpu;
    std::array<GLuint, 2> generated{};
    gl.GenTextures(static_cast<GLsizei>(generated.size()), generated.data());
    std::vector<std::uint32_t> reserved;
    Check(cpu.GenerateTextures(generated.size(), reserved) == Status::Ok, "CPU reserve names");
    for (std::size_t index = 0; index < generated.size(); ++index) {
        Check(generated[index] != 0 && gl.IsTexture(generated[index]) == GL_FALSE &&
            !cpu.Textures().IsTexture(reserved[index]), "generated GL/CPU names are initially reserved");
    }
    gl.DeleteTextures(static_cast<GLsizei>(generated.size()), generated.data());
    auto removeReserved = Make(Op::DeleteTextures);
    removeReserved.names = reserved;
    Check(cpu.Submit(removeReserved) == Status::Ok, "release CPU reserved names");
    auto bind = Make(Op::BindTexture);
    bind.second = 101;
    Submit(gl, cpu, bind);
    auto image = Make(Op::TextureImage);
    image.upload.width = 1;
    image.upload.height = 2;
    image.upload.format = PixelFormat::RGB;
    image.upload.bytes = {17, 34, 51, 99, 68, 85, 102};
    Submit(gl, cpu, image);
    CheckImage(gl, cpu, TextureTarget::Texture2D, 0, "RGB-padding4");
    Submit(gl, cpu, Make(Op::PixelStore, {3}, static_cast<std::uint32_t>(PixelStoreParameter::UnpackRowLength)));
    image.upload.format = PixelFormat::BGRA;
    image.upload.bytes = {51, 34, 17, 255, 90, 90, 90, 90, 91, 91, 91, 91, 102, 85, 68, 255};
    Submit(gl, cpu, image);
    CheckImage(gl, cpu, TextureTarget::Texture2D, 0, "BGRA-rowlength3");
    image.op = Op::TextureSubImage;
    image.upload.height = 1;
    image.upload.y = 1;
    image.upload.bytes = {153, 136, 119, 255};
    Submit(gl, cpu, image);
    CheckImage(gl, cpu, TextureTarget::Texture2D, 0, "BGRA-subimage");
    Submit(gl, cpu, Make(Op::PixelStore, {0}, static_cast<std::uint32_t>(PixelStoreParameter::UnpackRowLength)));
    Submit(gl, cpu, Make(Op::PixelStore, {1}, static_cast<std::uint32_t>(PixelStoreParameter::UnpackAlignment)));
    image.op = Op::TextureImage;
    image.upload.y = 0;
    image.upload.height = 2;
    image.upload.format = PixelFormat::Luminance;
    image.upload.internal = InternalFormat::Luminance8;
    image.upload.bytes = {17, 68};
    Submit(gl, cpu, image);
    CheckImage(gl, cpu, TextureTarget::Texture2D, 0, "Luminance8");
    image.upload.internal = InternalFormat::Intensity8;
    Submit(gl, cpu, image);
    CheckImage(gl, cpu, TextureTarget::Texture2D, 0, "Intensity8");
    image.upload.internal = InternalFormat::Alpha8;
    image.upload.format = PixelFormat::Alpha;
    Submit(gl, cpu, image);
    CheckImage(gl, cpu, TextureTarget::Texture2D, 0, "Alpha8");

    // GL_COMPILE owns the original client bytes and unpack state. Later state is irrelevant on replay.
    constexpr GLuint list = 133;
    Check(cpu.BeginList(list, false) == Status::Ok, "CPU upload list begin");
    gl.NewList(list, GL_COMPILE);
    image.upload.internal = InternalFormat::RGBA8;
    image.upload.format = PixelFormat::RGBA;
    image.upload.bytes = {17, 34, 51, 255, 68, 85, 102, 255};
    Submit(gl, cpu, image);
    Check(cpu.EndList() == Status::Ok, "CPU upload list end");
    gl.EndList();
    image.upload.bytes.assign(8, 0);
    Submit(gl, cpu, Make(Op::PixelStore, {7}, static_cast<std::uint32_t>(PixelStoreParameter::UnpackRowLength)));
    Submit(gl, cpu, Make(Op::CallList, {}, list));
    CheckImage(gl, cpu, TextureTarget::Texture2D, 0, "list-captured-unpack-and-data");
    gl.DeleteLists(list, 1);
    Check(cpu.DeleteList(list) == Status::Ok, "delete CPU upload list");
    Submit(gl, cpu, Make(Op::PixelStore, {0}, static_cast<std::uint32_t>(PixelStoreParameter::UnpackRowLength)));
    Submit(gl, cpu, Make(Op::PixelStore, {4}, static_cast<std::uint32_t>(PixelStoreParameter::UnpackAlignment)));

    Submit(gl, cpu, TextureParameterCommand(TextureParameter::MinFilter, static_cast<double>(Filter::Linear)));
    Submit(gl, cpu, TextureParameterCommand(TextureParameter::WrapS, static_cast<double>(Wrap::ClampToEdge)));
    Submit(gl, cpu, TextureParameterCommand(TextureParameter::MaximumLevel, 0));
    GLint value = 0;
    gl.GetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, &value);
    Check(value == static_cast<GLint>(GL_LINEAR), "minification query");
    gl.GetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, &value);
    Check(value == static_cast<GLint>(GL_CLAMP_TO_EDGE), "wrap query");
    gl.GetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, &value);
    Check(value == cpu.Textures().Bound(cpu.State(), TextureTarget::Texture2D)->sampler.maximumLevel, "maxlevel query");
    Submit(gl, cpu, EnvironmentCommand(EnvironmentParameter::Mode,
        static_cast<std::uint32_t>(EnvironmentMode::Replace)));
    const auto prior = cpu.State().textures[0].environment;
    gl.TexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_ALPHA);
    auto invalid = EnvironmentCommand(EnvironmentParameter::Mode, 99);
    Check(cpu.Submit(invalid) == Status::InvalidValue && cpu.State().textures[0].environment == prior,
        "inherited invalid GL_ALPHA environment leaves CPU mode unchanged");
    Check(gl.GetError() == GL_INVALID_ENUM, "selected SDK GL_ALPHA environment is invalid on actual GL");
    gl.GetTexEnviv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, &value);
    Check(value == static_cast<GLint>(GL_REPLACE), "inherited invalid environment leaves real GL mode unchanged");

    // DeleteTextures executes immediately inside a compiled list and is absent on replay.
    constexpr GLuint deletionList = 134;
    Check(cpu.BeginList(deletionList, false) == Status::Ok, "CPU deletion list begin");
    gl.NewList(deletionList, GL_COMPILE);
    auto remove = Make(Op::DeleteTextures);
    remove.names = {101};
    Submit(gl, cpu, remove);
    const bool cpuDeleted = !cpu.Textures().IsTexture(101);
    const auto glTexture = gl.IsTexture(101);
    const auto deleteError = gl.GetError();
    std::printf("delete_in_compiled_list CPU_deleted=%d GL_is_texture=%u GL_error=%u\n", cpuDeleted,
        static_cast<unsigned int>(glTexture), static_cast<unsigned int>(deleteError));
    Check(cpuDeleted && glTexture == GL_FALSE && deleteError == GL_NO_ERROR, "deletion immediate in list compile");
    Check(cpu.EndList() == Status::Ok, "CPU deletion list end");
    gl.EndList();
    Submit(gl, cpu, bind);
    Submit(gl, cpu, Make(Op::CallList, {}, deletionList));
    Check(cpu.Textures().IsTexture(101) && gl.IsTexture(101) == GL_TRUE, "deletion not replayed");
    gl.DeleteLists(deletionList, 1);
    Check(cpu.DeleteList(deletionList) == Status::Ok, "CPU deletion list cleanup");
    Submit(gl, cpu, remove);
    bind.argument = static_cast<std::uint32_t>(TextureTarget::Rectangle);
    bind.second = 102;
    Submit(gl, cpu, bind);
    image.argument = bind.argument;
    image.upload.bytes = {17, 34, 51, 255, 68, 85, 102, 255};
    Submit(gl, cpu, image);
    CheckImage(gl, cpu, TextureTarget::Rectangle, 0, "rectangle-RGBA8");
    gl.GetTexParameteriv(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MIN_FILTER, &value);
    Check(value == static_cast<GLint>(GL_LINEAR), "rectangle default nonmip filter");
    gl.GetTexParameteriv(GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_S, &value);
    Check(value == static_cast<GLint>(GL_CLAMP_TO_EDGE), "rectangle default clamp");
    remove.names = {102};
    Submit(gl, cpu, remove);
    Check(gl.GetError() == GL_NO_ERROR, "upload/list/sampler oracle error-free");
}

std::vector<std::uint8_t> ReadTextureRaster(GL& gl, const std::string& name) {
    std::vector<std::uint8_t> pixels(kOracleSide * kOracleSide * 3);
    gl.PixelStorei(GL_PACK_ALIGNMENT, 1);
    gl.ReadPixels(0, 0, kOracleSide, kOracleSide, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    std::ofstream output(name + ".rgb", std::ios::binary);
    output.write(reinterpret_cast<const char*>(pixels.data()), static_cast<std::streamsize>(pixels.size()));
    Check(output.good(), "write raster bytes");
    return pixels;
}

void ReplayTextureDraw(GL& gl, const DrawPacket& draw) {
    std::vector<GLuint> temporary;
    gl.PixelStorei(GL_UNPACK_ALIGNMENT, 1);
    gl.PixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    for (std::size_t unit = 0; unit < kTextureUnits; ++unit) {
        gl.ActiveTexture(GL_TEXTURE0 + static_cast<GLenum>(unit));
        const auto& state = draw.textures[unit];
        for (std::size_t target = 0; target < textureTargets.size(); ++target) {
            const auto& object = target == 0 ? draw.resources[unit].texture2D : draw.resources[unit].rectangle;
            GLuint name = 0;
            gl.GenTextures(1, &name);
            temporary.push_back(name);
            gl.BindTexture(textureTargets[target], name);
            const auto& sampler = object->sampler;
            gl.TexParameteri(textureTargets[target], GL_TEXTURE_MIN_FILTER,
                static_cast<GLint>(filters[static_cast<std::size_t>(sampler.minification)]));
            gl.TexParameteri(textureTargets[target], GL_TEXTURE_MAG_FILTER,
                static_cast<GLint>(filters[static_cast<std::size_t>(sampler.magnification)]));
            gl.TexParameteri(textureTargets[target], GL_TEXTURE_WRAP_S,
                static_cast<GLint>(wraps[static_cast<std::size_t>(sampler.wrapS)]));
            gl.TexParameteri(textureTargets[target], GL_TEXTURE_WRAP_T,
                static_cast<GLint>(wraps[static_cast<std::size_t>(sampler.wrapT)]));
            gl.TexParameteri(textureTargets[target], GL_TEXTURE_MAX_LEVEL, sampler.maximumLevel);
            for (const auto& entry : object->levels) {
                const auto& image = *entry.second;
                Check(image.defined && image.internal != InternalFormat::ColorIndex8, "qualified replay image subset");
                gl.TexImage2D(textureTargets[target], entry.first,
                    static_cast<GLint>(internalFormats[static_cast<std::size_t>(image.internal)]),
                    image.width, image.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image.pixels.data());
            }
            const bool enabled = target == 0 ? state.enabled2D : state.enabledRectangle;
            if (enabled) {
                gl.Enable(textureTargets[target]);
            } else {
                gl.Disable(textureTargets[target]);
            }
        }
        gl.TexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE,
            static_cast<GLint>(environments[static_cast<std::size_t>(state.environment)]));
        gl.TexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
        gl.TexEnvi(GL_TEXTURE_ENV, GL_RGB_SCALE, static_cast<GLint>(state.rgbScale));
        gl.MatrixMode(GL_TEXTURE);
        gl.LoadMatrixf(draw.textureMatrices[unit].values.data());
    }
    gl.MatrixMode(GL_MODELVIEW);
    gl.LoadMatrixf(draw.modelView.values.data());
    gl.MatrixMode(GL_PROJECTION);
    gl.LoadMatrixf(draw.projection.values.data());
    const auto& raster = draw.raster;
    for (std::size_t index = 0; index < 6; ++index) {
        if (raster.enabled[index]) {
            gl.Enable(capabilities[index]);
        } else {
            gl.Disable(capabilities[index]);
        }
    }
    if (raster.enabled.back()) {
        gl.Enable(GL_POLYGON_OFFSET_FILL);
    } else {
        gl.Disable(GL_POLYGON_OFFSET_FILL);
    }
    gl.DepthMask(raster.depthWrite ? GL_TRUE : GL_FALSE);
    gl.DepthFunc(comparisons[static_cast<std::size_t>(raster.depthFunction)]);
    gl.AlphaFunc(comparisons[static_cast<std::size_t>(raster.alphaFunction)], raster.alphaReference);
    gl.BlendFunc(blendFactors[static_cast<std::size_t>(raster.blendSource)],
        blendFactors[static_cast<std::size_t>(raster.blendDestination)]);
    gl.CullFace(cullFaces[static_cast<std::size_t>(raster.cull)]);
    gl.ShadeModel(raster.shade == Shade::Smooth ? GL_SMOOTH : GL_FLAT);
    gl.FrontFace(raster.frontCounterClockwise ? GL_CCW : GL_CW);
    gl.PolygonMode(GL_FRONT, polygonModes[static_cast<std::size_t>(raster.polygonMode[0])]);
    gl.PolygonMode(GL_BACK, polygonModes[static_cast<std::size_t>(raster.polygonMode[1])]);
    gl.PolygonOffset(raster.polygonOffset[0], raster.polygonOffset[1]);
    gl.Fogi(GL_FOG_MODE, static_cast<GLint>(fogModes[static_cast<std::size_t>(raster.fogMode)]));
    gl.Fogfv(GL_FOG_COLOR, raster.fogColor.data());
    gl.Fogf(GL_FOG_START, raster.fogStart);
    gl.Fogf(GL_FOG_END, raster.fogEnd);
    gl.Fogf(GL_FOG_DENSITY, raster.fogDensity);
    gl.Hint(GL_FOG_HINT, hints[static_cast<std::size_t>(raster.fogHint)]);
    gl.Hint(GL_PERSPECTIVE_CORRECTION_HINT, hints[static_cast<std::size_t>(raster.perspectiveHint)]);
    gl.Viewport(raster.viewport[0], raster.viewport[1], raster.viewport[2], raster.viewport[3]);
    gl.Scissor(raster.scissor[0], raster.scissor[1], raster.scissor[2], raster.scissor[3]);
    gl.DepthRange(raster.depthRange[0], raster.depthRange[1]);
    gl.Begin(primitives[static_cast<std::size_t>(draw.primitive)]);
    for (const auto& vertex : draw.vertices) {
        gl.Color4f(vertex.color[0], vertex.color[1], vertex.color[2], vertex.color[3]);
        for (std::size_t unit = 0; unit < kTextureUnits; ++unit) {
            gl.MultiTexCoord2f(GL_TEXTURE0 + static_cast<GLenum>(unit),
                vertex.texcoords[unit][0], vertex.texcoords[unit][1]);
        }
        gl.Vertex4f(vertex.position[0], vertex.position[1], vertex.position[2], vertex.position[3]);
    }
    gl.End();
    gl.DeleteTextures(static_cast<GLsizei>(temporary.size()), temporary.data());
}

void TextureRasterOracle(GL& gl, int profile) {
    Recorder cpu;
    Submit(gl, cpu, Make(Op::Viewport, {0, 0, kOracleSide, kOracleSide}));
    Submit(gl, cpu, Make(Op::MatrixMode, {}, static_cast<std::uint32_t>(MatrixMode::Projection)));
    Submit(gl, cpu, Make(Op::Identity));
    Submit(gl, cpu, Make(Op::MatrixMode, {}, static_cast<std::uint32_t>(MatrixMode::ModelView)));
    Submit(gl, cpu, Make(Op::Identity));
    for (std::uint32_t unit = 0; unit < kTextureUnits; ++unit) {
        Submit(gl, cpu, Make(Op::ActiveTexture, {}, unit));
        Submit(gl, cpu, Make(Op::MatrixMode, {}, static_cast<std::uint32_t>(MatrixMode::Texture)));
        Submit(gl, cpu, Make(Op::Identity));
        Submit(gl, cpu, Make(Op::Enable, {0}, static_cast<std::uint32_t>(Capability::TextureRectangle)));
        Submit(gl, cpu, Make(Op::Enable, {unit == 0 || profile == 2 ? 1.0 : 0.0},
            static_cast<std::uint32_t>(Capability::Texture2D)));
        auto bind = Make(Op::BindTexture);
        bind.second = 201 + unit;
        Submit(gl, cpu, bind);
        auto image = Make(Op::TextureImage);
        image.upload.width = 2;
        image.upload.height = 2;
        image.upload.bytes = {255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 255, 255};
        if (unit != 0) {
            image.upload.bytes.assign(16, unit == 1 ? 192 : 128);
        }
        if (profile == 3 && unit == 0) {
            image.upload.internal = InternalFormat::RGB5A1;
            image.upload.bytes = {17, 34, 51, 127, 68, 85, 102, 255, 119, 136, 153, 128, 170, 187, 204, 255};
        }
        Submit(gl, cpu, image);
        const auto filter = profile == 3 ? Filter::Linear : Filter::Nearest;
        Submit(gl, cpu, TextureParameterCommand(TextureParameter::MinFilter, static_cast<double>(filter)));
        Submit(gl, cpu, TextureParameterCommand(TextureParameter::MagFilter, static_cast<double>(filter)));
        Submit(gl, cpu, TextureParameterCommand(TextureParameter::WrapS, static_cast<double>(Wrap::ClampToEdge)));
        Submit(gl, cpu, TextureParameterCommand(TextureParameter::WrapT, static_cast<double>(Wrap::ClampToEdge)));
        const auto mode = unit == 2 ? EnvironmentMode::Combine : profile == 0 ? EnvironmentMode::Replace :
            EnvironmentMode::Modulate;
        Submit(gl, cpu, EnvironmentCommand(EnvironmentParameter::Mode, static_cast<std::uint32_t>(mode)));
        Submit(gl, cpu, EnvironmentCommand(EnvironmentParameter::RGBScale, unit == 2 ? 2 : 1));
    }
    for (std::uint32_t capability = 0; capability < 6; ++capability) {
        Submit(gl, cpu, Make(Op::Enable, {0}, capability));
    }
    Submit(gl, cpu, Make(Op::Enable, {0}, static_cast<std::uint32_t>(Capability::PolygonOffsetFill)));
    Submit(gl, cpu, Make(Op::DepthWrite, {1}));
    Submit(gl, cpu, Make(Op::FrontFace, {}, 1));
    auto polygon = Make(Op::PolygonMode, {}, static_cast<std::uint32_t>(Cull::FrontAndBack));
    polygon.second = static_cast<std::uint32_t>(PolygonMode::Fill);
    Submit(gl, cpu, polygon);
    Submit(gl, cpu, Make(Op::Color, {1, 1, 1, 1}));
    Submit(gl, cpu, Make(Op::FogHint, {}, static_cast<std::uint32_t>(HintQuality::Nicest)));
    Submit(gl, cpu, Make(Op::PerspectiveHint, {}, static_cast<std::uint32_t>(HintQuality::Nicest)));
    if (profile == 1) {
        Submit(gl, cpu, Make(Op::Enable, {1}, static_cast<std::uint32_t>(Capability::Blend)));
        auto blend = Make(Op::BlendFunction, {}, static_cast<std::uint32_t>(BlendFactor::SourceAlpha));
        blend.second = static_cast<std::uint32_t>(BlendFactor::InverseSourceAlpha);
        Submit(gl, cpu, blend);
        Submit(gl, cpu, Make(Op::Enable, {1}, static_cast<std::uint32_t>(Capability::ScissorTest)));
        Submit(gl, cpu, Make(Op::Scissor, {8, 8, 48, 48}));
        Submit(gl, cpu, Make(Op::Enable, {1}, static_cast<std::uint32_t>(Capability::Fog)));
        Submit(gl, cpu, Make(Op::FogMode, {}, static_cast<std::uint32_t>(FogMode::Linear)));
        Submit(gl, cpu, Make(Op::FogStart, {0}));
        Submit(gl, cpu, Make(Op::FogEnd, {1}));
        Submit(gl, cpu, Make(Op::FogColor, {17.0 / 255, 34.0 / 255, 51.0 / 255, 1}));
        Submit(gl, cpu, Make(Op::Color, {1, 1, 1, 0.5}));
    }
    gl.Disable(GL_SCISSOR_TEST);
    gl.ClearColor(0, 0, 0, 1);
    gl.Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (profile == 1) {
        gl.Enable(GL_SCISSOR_TEST);
    }
    Submit(gl, cpu, Make(Op::Begin));
    constexpr std::array<std::array<float, 2>, 6> corners{{{{-1, -1}}, {{1, -1}}, {{1, 1}},
        {{-1, -1}}, {{1, 1}}, {{-1, 1}}}};
    for (const auto& corner : corners) {
        for (std::uint32_t unit = 0; unit < kTextureUnits; ++unit) {
            Submit(gl, cpu, Make(Op::TexCoord, {(corner[0] + 1) / 2, (corner[1] + 1) / 2}, unit));
        }
        Submit(gl, cpu, Make(Op::Vertex, {corner[0], corner[1], -0.5}));
    }
    Submit(gl, cpu, Make(Op::End));
    const auto name = std::string("texture-raster-") + std::to_string(profile);
    auto original = ReadTextureRaster(gl, name + "-original");
    Check(std::any_of(original.begin(), original.end(), [](std::uint8_t value) { return value != 0; }),
        "nonempty original raster");
    gl.Disable(GL_SCISSOR_TEST);
    gl.Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    // Destroy original GL/CPU objects; the captured draw still owns its original versions.
    auto remove = Make(Op::DeleteTextures);
    remove.names = {201, 202, 203};
    Submit(gl, cpu, remove);
    ReplayTextureDraw(gl, cpu.Draws().front());
    auto replay = ReadTextureRaster(gl, name + "-owned-replay");
    Check(gl.GetError() == GL_NO_ERROR, "captured texture/raster replay has no GL errors");
    Check(original == replay, "exact selected actual GL versus owned-snapshot GL raster bytes");
    std::printf("texture_raster_profile=%d exact_RGB_bytes=%zu\n", profile, replay.size());
#if defined(CSE_COMPARE_GPU_TEXTURES)
    ::CompareGpuTextureRaster(cpu.Draws().front(), original, profile);
#endif
}

void RunTextureRasters(GL& gl) {
    constexpr int kRasterProfiles = 4;
    for (int profile = 0; profile < kRasterProfiles; ++profile) {
        TextureRasterOracle(gl, profile);
    }
    std::puts("PASS selected texture raster capture; no upload/list or general pixel parity claim");
}

void RunTextures(GL& gl) {
    const auto extensions = reinterpret_cast<const char*>(gl.GetString(GL_EXTENSIONS));
    std::printf("actual_shared_texture_palette=%d palette_extension_raster_qualified=0\n",
        extensions && std::strstr(extensions, "GL_EXT_shared_texture_palette") != nullptr);
    TextureUploadOracle(gl);
    RunTextureRasters(gl);
    std::puts("PASS selected texture upload/list/raster capture; no SDL_GPU or general pixel parity claim");
}
