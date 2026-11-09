#include "legacy_commands.h"
#include "legacy_texture.h"
#include <cstdio>
#include <cstdlib>
#include <string>

using namespace cse::render;

namespace {
void Check(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "FAIL texture capture: %s\n", message);
        std::exit(EXIT_FAILURE);
    }
}

Command Bind(std::uint32_t name, TextureTarget target = TextureTarget::Texture2D) {
    Command command;
    command.op = Op::BindTexture;
    command.argument = static_cast<std::uint32_t>(target);
    command.second = name;
    return command;
}

void Send(Recorder& recorder, const Command& command) {
    Check(recorder.Submit(command) == Status::Ok, "valid command");
}

void Triangle(Recorder& recorder) {
    Command command;
    command.op = Op::Begin;
    Send(recorder, command);
    command.op = Op::Vertex;
    Send(recorder, command);
    command.values[0] = 1;
    Send(recorder, command);
    command.values[1] = 1;
    Send(recorder, command);
    command.op = Op::End;
    Send(recorder, command);
}

Command Image() {
    Command command;
    command.op = Op::TextureImage;
    command.upload.width = 2;
    command.upload.height = 1;
    command.upload.bytes = {255, 0, 0, 255, 0, 255, 0, 255};
    return command;
}

void Names() {
    Recorder recorder;
    std::vector<std::uint32_t> names;
    Check(recorder.GenerateTextures(2, names) == Status::Ok && names.size() == 2, "reserve names");
    Check(names[0] != 0 && names[1] != 0 && names[0] != names[1], "unique nonzero names");
    Check(!recorder.Textures().IsTexture(names[0]), "reserved name is not yet a texture");
    Send(recorder, Bind(names[0]));
    Check(recorder.Textures().IsTexture(names[0]), "binding realizes texture");
    Check(recorder.Submit(Bind(names[0], TextureTarget::Rectangle)) == Status::InvalidOperation,
        "realized target is immutable");
    Send(recorder, Image());
    Triangle(recorder);
    const auto old = recorder.Draws()[0].resources[0].texture2D;
    Command remove;
    remove.op = Op::DeleteTextures;
    remove.names = {0, names[0], names[0], 123456};
    Send(recorder, remove);
    Check(!recorder.Textures().IsTexture(names[0]) && recorder.State().textures[0].texture2D == 0,
        "delete ignores absent names and unbinds");
    Send(recorder, Bind(names[0]));
    Triangle(recorder);
    Check(old->generation != recorder.Draws()[1].resources[0].texture2D->generation,
        "reused name has new identity");
    Check(old->levels.at(0)->pixels == std::vector<std::uint8_t>({255, 0, 0, 255, 0, 255, 0, 255}),
        "deleted snapshot owns old image");
    Send(recorder, Bind(0xf1234567));
    Check(recorder.State().textures[0].texture2D == 0xf1234567, "raw GL name keeps all 32 bits");
}

void Uploads() {
    Recorder recorder;
    Send(recorder, Bind(7));
    Command command = Image();
    command.upload.width = 1;
    command.upload.height = 2;
    command.upload.format = PixelFormat::RGB;
    command.upload.bytes = {1, 2, 3, 99, 4, 5, 6};
    Send(recorder, command);
    command.upload.bytes.assign(7, 77);
    Triangle(recorder);
    Check(recorder.Draws()[0].resources[0].texture2D->levels.at(0)->pixels ==
        std::vector<std::uint8_t>({1, 2, 3, 255, 4, 5, 6, 255}), "default alignment4 excludes padding");
    Command pixel;
    pixel.op = Op::PixelStore;
    pixel.argument = static_cast<std::uint32_t>(PixelStoreParameter::UnpackRowLength);
    pixel.values[0] = 3;
    Send(recorder, pixel);
    command.upload.format = PixelFormat::BGRA;
    command.upload.bytes = {3, 2, 1, 4, 90, 90, 90, 90, 91, 91, 91, 91, 7, 6, 5, 8};
    Send(recorder, command);
    Triangle(recorder);
    Check(recorder.Draws()[1].resources[0].texture2D->levels.at(0)->pixels ==
        std::vector<std::uint8_t>({1, 2, 3, 4, 5, 6, 7, 8}), "BGRA row-length stride");
    command.op = Op::TextureSubImage;
    command.upload.height = 1;
    command.upload.y = 1;
    command.upload.format = PixelFormat::RGBA;
    command.upload.bytes = {9, 10, 11, 12};
    Send(recorder, command);
    Triangle(recorder);
    Check(recorder.Draws()[2].resources[0].texture2D->levels.at(0)->pixels ==
        std::vector<std::uint8_t>({1, 2, 3, 4, 9, 10, 11, 12}), "subimage selected rectangle");
    command.upload.x = 1;
    Check(recorder.Submit(command) == Status::InvalidValue, "subimage bounds checked");
    Check(recorder.Draws()[1].resources[0].texture2D->levels.at(0)->pixels[4] == 5,
        "later subimage cannot change prior draw");
    Recorder copy = recorder;
    copy.ClearDraws();
    command.upload.x = 0;
    command.upload.bytes[0] = 13;
    Send(copy, command);
    Check(recorder.Textures().Bound(recorder.State(), TextureTarget::Texture2D)->levels.at(0)->pixels[4] == 9,
        "copied context mutations independent");
}

void Palettes() {
    Recorder recorder;
    Send(recorder, Bind(3));
    Command image = Image();
    image.upload.internal = InternalFormat::ColorIndex8;
    image.upload.format = PixelFormat::ColorIndex;
    image.upload.bytes = {0, 1};
    Send(recorder, image);
    Command palette;
    palette.op = Op::SharedPalette;
    palette.upload.bytes.assign(kPaletteBytes, 0);
    palette.upload.bytes[0] = 255;
    palette.upload.bytes[4] = 255;
    Send(recorder, palette);
    Triangle(recorder);
    palette.upload.bytes[0] = 13;
    Send(recorder, palette);
    Triangle(recorder);
    Check((*recorder.Draws()[0].palette)[0] == 255 && (*recorder.Draws()[1].palette)[0] == 13,
        "shared palette changes preserve past draw");
    EngineTextureCache cache;
    auto decision = cache.Bind(0x00020007);
    Check(decision.bind && decision.name == 7 && decision.paletteIndex == 1, "upper16 palette encoding");
    decision = cache.Bind(0x00030007);
    Check(!decision.bind && decision.paletteIndex == -1, "same low16 skips palette change as source");
    Check(cache.Select(1) == Status::Ok, "engine cache unit select");
    Check(cache.Bind(0x00030007).paletteIndex == 2, "per-unit cached names");
    Check(cache.Bind(0x80000008).paletteIndex == -1, "negative encoded handle has no palette selection");
}

void ListsAndBounds() {
    CaptureLimits limits;
    limits.textureStorageBytes = 16;
    Recorder recorder(limits);
    Send(recorder, Bind(4));
    Check(recorder.BeginList(1, false) == Status::Ok, "compile upload list");
    Send(recorder, Image());
    Triangle(recorder);
    Check(recorder.EndList() == Status::Ok, "owned upload list");
    Command pixel;
    pixel.op = Op::PixelStore;
    pixel.argument = static_cast<std::uint32_t>(PixelStoreParameter::UnpackRowLength);
    pixel.values[0] = 100;
    Send(recorder, pixel);
    Command call;
    call.op = Op::CallList;
    call.argument = 1;
    Send(recorder, call);
    Check(recorder.Draws()[0].resources[0].texture2D->levels.at(0)->pixels.size() == 8,
        "list upload retains capture-time unpack state and bytes");
    Send(recorder, Image());
    Check(recorder.Submit(Image()) == Status::LimitExceeded, "retained images consume bounded storage");
    recorder.ClearDraws();
    Check(recorder.Submit(Image()) == Status::Ok, "released snapshot restores storage budget");
    pixel.values[0] = -1;
    Check(recorder.Submit(pixel) == Status::InvalidValue, "negative row length rejected");
}

void Formats() {
    Recorder recorder;
    Send(recorder, Bind(8));
    Command pixel;
    pixel.op = Op::PixelStore;
    pixel.values[0] = 1;
    Send(recorder, pixel);
    auto image = Image();
    image.upload.width = 1;
    image.upload.format = PixelFormat::Alpha;
    image.upload.internal = InternalFormat::Alpha8;
    image.upload.bytes = {17};
    Send(recorder, image);
    auto texture = [&recorder]() {
        return recorder.Textures().Bound(recorder.State(), TextureTarget::Texture2D)->levels.at(0);
    };
    Check(texture()->pixels == std::vector<std::uint8_t>({255, 255, 255, 17}), "alpha sampling components");
    image.upload.format = PixelFormat::Luminance;
    image.upload.internal = InternalFormat::Intensity8;
    Send(recorder, image);
    Check(texture()->pixels == std::vector<std::uint8_t>({17, 17, 17, 17}), "intensity sampling components");
    image.upload.internal = InternalFormat::Luminance8;
    Send(recorder, image);
    Check(texture()->pixels == std::vector<std::uint8_t>({17, 17, 17, 255}), "luminance sampling components");
    image.upload.internal = InternalFormat::RGBA8;
    image.upload.format = PixelFormat::LuminanceAlpha;
    image.upload.bytes = {17, 34};
    Send(recorder, image);
    Check(texture()->pixels == std::vector<std::uint8_t>({17, 17, 17, 34}), "luminance-alpha source");
    image.upload.format = PixelFormat::RGBA;
    image.upload.internal = InternalFormat::RGB;
    image.upload.bytes = {17, 34, 51, 68};
    Send(recorder, image);
    Check(texture()->internal == InternalFormat::RGB && texture()->pixels[3] == 255,
        "unsized request remains distinct; backend storage precision is not invented");
    image.upload.internal = InternalFormat::RGB5A1;
    image.upload.bytes = {17, 34, 51, 68};
    Send(recorder, image);
    Check(texture()->internal == InternalFormat::RGB5A1 && texture()->pixels[3] == 68,
        "requested low precision retained without inventing CPU quantization");
    image.upload.internal = InternalFormat::RGBA8;
    image.upload.allocateOnly = true;
    image.upload.bytes.clear();
    Send(recorder, image);
    Check(!texture()->defined, "null allocation never promises initialized pixels");
    image.op = Op::TextureSubImage;
    image.upload.allocateOnly = false;
    image.upload.bytes = {1, 2, 3, 4};
    Send(recorder, image);
    Check(texture()->defined && texture()->pixels[0] == 1, "full update defines null-allocated image");
    image.op = Op::TextureImage;
    image.upload.level = 1;
    Send(recorder, image);
    Check(recorder.Textures().Bound(recorder.State(), TextureTarget::Texture2D)->levels.size() == 2,
        "independent explicit mip images");
    Send(recorder, Bind(9, TextureTarget::Rectangle));
    image.argument = static_cast<std::uint32_t>(TextureTarget::Rectangle);
    Check(recorder.Submit(image) == Status::InvalidValue, "rectangle rejects nonzero mip level");
    image.upload.level = 0;
    Send(recorder, image);
    Check(recorder.Textures().Bound(recorder.State(), TextureTarget::Rectangle)->levels.at(0)->pixels[0] == 1,
        "rectangle image separate from 2D binding");
}

void RasterAndSampler() {
    Recorder recorder;
    Send(recorder, Bind(7));
    Command command;
    command.op = Op::TextureParameter;
    command.second = static_cast<std::uint32_t>(TextureParameter::MinFilter);
    command.values[0] = static_cast<double>(Filter::LinearMipmapLinear);
    Send(recorder, command);
    command.second = static_cast<std::uint32_t>(TextureParameter::MagFilter);
    Check(recorder.Submit(command) == Status::InvalidValue, "magnification rejects mip filter");
    command.second = static_cast<std::uint32_t>(TextureParameter::Anisotropy);
    command.values[0] = 4;
    Send(recorder, command);
    Triangle(recorder);
    command.values[0] = 8;
    Send(recorder, command);
    Check(recorder.Draws()[0].resources[0].texture2D->sampler.requestedAnisotropy == 4,
        "sampler mutation preserves prior draw; hardware clamp remains backend policy");
    command.op = Op::ActiveTexture;
    command.argument = 2;
    Send(recorder, command);
    command.op = Op::TextureEnvironment;
    command.argument = static_cast<std::uint32_t>(EnvironmentParameter::Mode);
    command.second = static_cast<std::uint32_t>(EnvironmentMode::Combine);
    Send(recorder, command);
    command.argument = static_cast<std::uint32_t>(EnvironmentParameter::RGBScale);
    command.second = 2;
    Send(recorder, command);
    Check(recorder.State().textures[2].rgbScale == 2 && recorder.State().textures[0].rgbScale == 1,
        "detail environment is per unit");
    command.op = Op::FogMode;
    command.argument = static_cast<std::uint32_t>(FogMode::ExponentialSquared);
    Send(recorder, command);
    command.op = Op::FogDensity;
    command.values[0] = 0.25;
    Send(recorder, command);
    command.values[0] = -1;
    Check(recorder.Submit(command) == Status::InvalidValue && recorder.State().raster.fogDensity == 0.25f,
        "negative fog density leaves state unchanged");
    command.op = Op::PolygonMode;
    command.argument = static_cast<std::uint32_t>(Cull::Front);
    command.second = static_cast<std::uint32_t>(PolygonMode::Line);
    Send(recorder, command);
    command.op = Op::PolygonOffset;
    command.values[0] = -1;
    command.values[1] = -2;
    Send(recorder, command);
    command.op = Op::Enable;
    command.argument = static_cast<std::uint32_t>(Capability::PolygonOffsetFill);
    command.values[0] = 1;
    Send(recorder, command);
    Triangle(recorder);
    const auto& raster = recorder.Draws().back().raster;
    Check(raster.fogMode == FogMode::ExponentialSquared && raster.fogDensity == 0.25f,
        "fog captured with draw");
    Check(raster.polygonMode[0] == PolygonMode::Line && raster.polygonMode[1] == PolygonMode::Fill &&
        raster.enabled.back() && raster.polygonOffset == std::array<float, 2>{{-1, -2}}, "polygon state capture");
}

void Lifetime() {
    constexpr int kLifetimeCycles = 128;
    DrawPacket retained;
    for (int cycle = 0; cycle < kLifetimeCycles; ++cycle) {
        Recorder recorder;
        Send(recorder, Bind(17));
        Send(recorder, Image());
        Triangle(recorder);
        retained = recorder.Draws()[0];
        Command remove;
        remove.op = Op::DeleteTextures;
        remove.names = {17};
        Send(recorder, remove);
        recorder.ClearDraws();
        Check(recorder.Textures().StorageBytes() == 8, "detached draw remains storage owner");
    }
    Check(retained.resources[0].texture2D->levels.at(0)->pixels[0] == 255,
        "snapshot survives recorder and native-name lifetime");
    Recorder recorder;
    Send(recorder, Bind(17));
    Send(recorder, Image());
    Command remove;
    remove.op = Op::DeleteTextures;
    remove.names = {17};
    Send(recorder, remove);
    Check(recorder.Textures().StorageBytes() == 0, "unretained deleted storage released");
}
}

int main(int argc, char** argv) {
    Check(argc == 2, "case required");
    const std::string test = argv[1];
    if (test == "names") {
        Names();
    } else if (test == "uploads") {
        Uploads();
    } else if (test == "palettes") {
        Palettes();
    } else if (test == "lists-bounds") {
        ListsAndBounds();
    } else if (test == "formats") {
        Formats();
    } else if (test == "raster") {
        RasterAndSampler();
    } else if (test == "lifetime") {
        Lifetime();
    } else {
        Check(false, "unknown case");
    }
    std::printf("PASS texture capture %s\n", test.c_str());
}
