"""The actual shared and CS radio images reserve and paint the same scaled bounds."""
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[3]
OUTPUT = ROOT / 'build/zbot-layout/radio-images'
OUTPUT.mkdir(parents=True, exist_ok=True)

HARNESS = r'''
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

void Require(bool condition, const char* message) {
    if (!condition) { std::fprintf(stderr, "%s\n", message); std::exit(1); }
}
struct Color { Color(int, int, int, int) {} Color() = default; };
namespace vgui2 {
struct IScheme {};
struct IImage {
    virtual ~IImage() = default;
    virtual void Paint() = 0;
    virtual void SetPos(int x, int y) = 0;
    virtual void GetSize(int& width, int& height) = 0;
    virtual void SetSize(int width, int height) = 0;
};
struct Image : IImage {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    void SetPos(int left, int top) override { x = left; y = top; }
    void GetPos(int& left, int& top) { left = x; top = y; }
    void GetSize(int& wide, int& tall) override { wide = width; tall = height; }
    void SetSize(int wide, int tall) override { width = wide; height = tall; }
};
struct Bitmap : Image {
    int paintedX = 0;
    int paintedY = 0;
    int paintedWidth = 0;
    int paintedHeight = 0;
    Bitmap() { width = 21; height = 21; }
    void Paint() override {
        paintedX = x; paintedY = y; paintedWidth = width; paintedHeight = height;
    }
};
Bitmap selected;
Bitmap unselected;
struct Schemes {
    IImage* GetImage(const char* name, bool) {
        return std::strcmp(name, "resource/icon_radiosel") == 0 ? &selected : &unselected;
    }
} schemes;
Schemes* scheme() { return &schemes; }
struct RadioButton {
    int scale = 1;
    bool selected = false;
    int GetProportionalScaledValue(int value) { return value * scale; }
    Color GetSchemeColor(const char*, Color fallback, IScheme*) { return fallback; }
    bool IsSelected() { return selected; }
};
}
'''

CASES = r'''
int main() {
    vgui2::RadioButton button;
    // Resource factories construct controls before parenting and proportional setup.
    RadioImage image(&button);
    for (int scale : {1, 2, 3}) {
        button.scale = scale;
        image.ApplySchemeSettings(nullptr, true);
        for (bool selected : {false, true}) {
            button.selected = selected;
            image.SetPos(7, 11);
            image.Paint();
            const auto& bitmap = selected ? vgui2::selected : vgui2::unselected;
            int width = 0;
            int height = 0;
            image.GetSize(width, height);
            Require(width > bitmap.paintedWidth, "radio layout must reserve the scaled icon and text gap");
            Require(height == bitmap.paintedHeight, "radio layout must reserve the rendered icon height");
            Require(bitmap.paintedX == 7 && bitmap.paintedY == 11,
                    "radio bitmap must use the centered position assigned by Label");
            Require(bitmap.width == 21 && bitmap.height == 21, "shared bitmap size must be restored after paint");
        }
    }
}
'''

for owner in ('shared', 'cs'):
    path = ROOT / ('src/vgui2/controls/RadioButton.cpp' if owner == 'shared'
                   else 'src/cstrike/vgui2/controls/RadioButton.cpp')
    source = path.read_text(encoding='latin-1')
    body = source[source.index('class RadioImage'):source.index('namespace vgui2')]
    probe = OUTPUT / (owner + '.cpp')
    probe.write_text(HARNESS + body + CASES)
    executable = OUTPUT / owner
    subprocess.run(['c++', '-std=c++17', '-fsanitize=address,undefined', str(probe), '-o', str(executable)], check=True)
    subprocess.run([str(executable)], check=True)
    print(owner + ': radio image scaled layout and paint position passed')
