#!/usr/bin/env python3
"""Execute actual adaptive browser owners with recorded actual bitmap/native metrics."""
from pathlib import Path
import argparse
import csv
import hashlib
import json
import subprocess
p = argparse.ArgumentParser()
p.add_argument('--metrics', type=Path, required=True)
p.add_argument('--evidence', type=Path, required=True)
a = p.parse_args()
r = Path(__file__).resolve().parents[3]
out = a.evidence.resolve(); out.mkdir(parents=True, exist_ok=True)
source = (r / 'src/cl_dll/vgui_ServerBrowser.cpp').read_text(encoding='cp1252')
def function(signature):
    start = source.index(signature); opening = source.index('{', start); depth = 1; end = opening + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}'); end += 1
    return source[start:end]
rows = list(csv.DictReader(a.metrics.open(), delimiter='\t'))
groups = {}
for row in rows:
    key = (row['language'], row['resolution'], row['provider'])
    if row['context'] == 'browser-button-75px':
        groups.setdefault(key, {})[row['token']] = (int(row['width']), int(row['height']))
cases = []
for (language, resolution, provider), metrics in groups.items():
    width, height = map(int, resolution.split('x'))
    scoreboard = next(row for row in rows if (row['language'], row['resolution'], row['provider'], row['context']) ==
        (language, resolution, provider, 'browser-first-cell-status'))
    pairs = ','.join('{%d,%d}' % metrics['CSE_UI_' + token] for token in ['Connect', 'Refresh', 'Stop', 'Close'])
    cases.append('Case{"%s/%s/%s",%d,%d,%s,{{%s}}}' %
        (language, resolution, provider, width, height, scoreboard['height'], pairs))
prelude = r'''
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>
int ScreenWidth = 640;
int ScreenHeight = 480;
#define XRES(x) ((x) * (static_cast<float>(ScreenWidth) / 640))
#define YRES(y) ((y) * (static_cast<float>(ScreenHeight) / 480))
#define HEADER_SIZE_Y YRES(18)
#define CELL_HEIGHT YRES(15)
struct Panel {
    int x = 0, y = 0, w = 0, h = 0;
    void getSize(int& width, int& height) { width = w; height = h; }
    void setSize(int width, int height) { w = width; h = height; }
    void setBounds(int left, int top, int width, int height) { x = left; y = top; w = width; h = height; }
};
struct CommandButton : Panel {
    int textWidth = 0, textHeight = 0, refreshed = 0;
    void getTextSize(int& width, int& height) { width = textWidth; height = textHeight; }
    void RefreshLanguage() { ++refreshed; }
};
struct Font { int tall = 0; int getTall() { return tall; } } font;
struct Scheme { enum { sf_primary2 }; Font* getFont(int) { return &font; } } scheme;
struct App { static App* getInstance() { static App app; return &app; } Scheme* getScheme() { return &scheme; } };
struct ServerBrowserTablePanel : Panel {
    static constexpr int CellGridPixels = 2;
    int getRowCount();
    int getCellTall(int);
};
struct ServerBrowser : Panel {
    Panel* _headerPanel;
    Panel* _tablePanel;
    CommandButton* _connectButton;
    CommandButton* _refreshButton;
    CommandButton* _stopButton;
    CommandButton* _cancelButton;
    int queryGeneration = 71, callbacks = 9, selectedRow = 14;
    std::string filter = "#typed", address = "server:27015";
    void LayoutPresentation();
    void RefreshLanguage();
    void setSize(int, int);
};
struct Metric { int width, height; };
struct Case { const char* name; int width, height, rowFontHeight; std::array<Metric, 4> metrics; };
'''
methods = '\n'.join(function(name) for name in ['void ServerBrowser::LayoutPresentation()',
    'void ServerBrowser::RefreshLanguage()', 'void ServerBrowser::setSize('])
methods += '\n' + function('int getRowCount() override').replace('int getRowCount() override',
    'int ServerBrowserTablePanel::getRowCount()')
methods += '\n' + function('int getCellTall(int) override').replace('int getCellTall(int) override',
    'int ServerBrowserTablePanel::getCellTall(int)')
main = r'''
int main() {
    const std::vector<Case> cases = {CASES};
    for (const auto& item : cases) {
        ScreenWidth = item.width;
        ScreenHeight = item.height;
        font.tall = item.rowFontHeight;
        Panel header;
        ServerBrowserTablePanel table;
        std::array<CommandButton, 4> buttons{};
        ServerBrowser browser;
        browser._headerPanel = &header;
        browser._tablePanel = &table;
        browser._connectButton = &buttons[0]; browser._refreshButton = &buttons[1];
        browser._stopButton = &buttons[2]; browser._cancelButton = &buttons[3];
        for (std::size_t index = 0; index < buttons.size(); ++index) {
            buttons[index].textWidth = item.metrics[index].width;
            buttons[index].textHeight = item.metrics[index].height;
        }
        browser.setSize(item.width, item.height);
        for (int revision = 0; revision < 80; ++revision) browser.RefreshLanguage();
        for (std::size_t index = 0; index < buttons.size(); ++index) {
            const auto& button = buttons[index];
            assert(button.w >= button.textWidth + 8 && button.h >= button.textHeight + 4);
            assert(button.x >= 0 && button.y >= 0 && button.x + button.w <= item.width);
            assert(button.y + button.h <= item.height && table.y + table.h < button.y);
            assert(button.refreshed == 80);
            for (std::size_t other = index + 1; other < buttons.size(); ++other) {
                const auto& neighbor = buttons[other];
                const bool separated = button.x + button.w <= neighbor.x || neighbor.x + neighbor.w <= button.x ||
                    button.y + button.h <= neighbor.y || neighbor.y + neighbor.h <= button.y;
                assert(separated);
            }
        }
        assert(table.getCellTall(0) >= font.tall + 2 && table.getRowCount() > 0);
        assert(browser.queryGeneration == 71 && browser.callbacks == 9 && browser.selectedRow == 14);
        assert(browser.filter == "#typed" && browser.address == "server:27015");
        std::printf("%s footer/table/row-font fit; session preserved over80 refreshes: PASS\n", item.name);
    }
}
'''.replace('CASES', ',\n'.join(cases))
cpp = out / 'layout.cpp'; cpp.write_text(prelude + methods + main)
command = ['c++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined', str(cpp), '-o', str(out / 'layout')]
subprocess.run(command, check=True)
result = subprocess.run([str(out / 'layout')], text=True, capture_output=True)
(out / 'run.log').write_text(result.stdout + result.stderr)
(out / 'receipt.json').write_text(json.dumps({'source': hashlib.sha256(source.encode('cp1252')).hexdigest(),
    'metrics': hashlib.sha256(a.metrics.read_bytes()).hexdigest(), 'cases': len(cases), 'command': command}, indent=2))
print(result.stdout[-1700:] + result.stderr)
result.check_returncode()
assert 'LayoutPresentation();' in function('ServerBrowser::ServerBrowser(')
print(len(cases), 'actual-metric cases, 80 refreshes each: PASS')
