"""ZBot availability follows the build option and stages its complete runtime data."""
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


def function(source, signature):
    start = source.index(signature)
    body = source.index('{', start)
    depth = 1
    end = body + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


class ZBotBuildTests(unittest.TestCase):
    def run_command(self, *arguments):
        result = subprocess.run(arguments, text=True, capture_output=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        return result

    def compile_probe(self, source, enabled):
        with tempfile.TemporaryDirectory(prefix='cse-zbot-probe-') as directory:
            path = Path(directory)
            (path / 'probe.cpp').write_text(source)
            if sys.platform == 'win32':
                executable = path / 'probe.exe'
                self.run_command('cl', '/nologo', '/EHsc', '/std:c++17', '/DCSE_ENABLE_ZBOT=' + str(enabled),
                                 str(path / 'probe.cpp'), '/Fe:' + str(executable), '/Fo:' + str(path / 'probe.obj'))
            else:
                executable = path / 'probe'
                self.run_command('c++', '-std=c++17', '-DCSE_ENABLE_ZBOT=' + str(enabled),
                                 str(path / 'probe.cpp'), '-o', str(executable))
            self.run_command(str(executable))

    def test_cvars_are_available_in_cstrike_only_when_enabled(self):
        source = (ROOT / 'src/cstrike/dlls/bot/cs_bot_init.cpp').read_text()
        body = function(source, 'void Bot_RegisterCvars()')
        probe = r'''
#include <cstring>
int registrations = 0;
bool quota = false;
bool UTIL_IsGame(const char* game) { return std::strcmp(game, "cstrike") == 0; }
void Register(const char* name) {
    ++registrations;
    quota |= std::strcmp(name, "&cv_bot_quota") == 0;
}
#define CVAR_REGISTER(variable) Register(#variable)
''' + body + r'''
int main() {
    Bot_RegisterCvars();
    return CSE_ENABLE_ZBOT ? !(registrations > 0 && quota) : registrations != 0;
}
'''
        for enabled in (1, 0):
            with self.subTest(enabled=enabled):
                self.compile_probe(probe, enabled)

    def test_server_registers_bot_commands_only_when_enabled(self):
        source = (ROOT / 'src/cstrike/dlls/bot/cs_bot_manager.cpp').read_text()
        body = function(source, 'void CCSBotManager::AddServerCommands()')
        probe = r'''
#include <cstring>
bool botAdd = false;
int commands = 0;
bool UTIL_IsGame(const char* game) { return std::strcmp(game, "cstrike") == 0; }
struct CCSBotManager {
    void AddServerCommands();
    void AddServerCommand(const char* command) {
        ++commands;
        botAdd |= std::strcmp(command, "bot_add") == 0;
    }
};
''' + body + r'''
int main() {
    CCSBotManager manager;
    manager.AddServerCommands();
    return CSE_ENABLE_ZBOT ? !(commands > 0 && botAdd) : commands != 0;
}
'''
        for enabled in (1, 0):
            with self.subTest(enabled=enabled):
                self.compile_probe(probe, enabled)

    def test_ui_availability_keeps_half_life_without_bot_controls(self):
        source = (ROOT / 'src/GameUI/ModInfo.cpp').read_bytes().decode('latin-1')
        self.assertIn('bool CModInfo::SupportsBots()', source)
        body = function(source, 'bool CModInfo::SupportsBots()')
        probe = r'''
#include <cstring>
#define stricmp strcmp
const char* gameDirectory = "cstrike";
const char* GameDirectory() { return gameDirectory; }
struct Engine { const char* (*pfnGetGameDirectory)(); } engineAPI = {GameDirectory};
Engine* engine = &engineAPI;
struct CModInfo { bool SupportsBots(); };
''' + body + r'''
int main() {
    CModInfo info;
    for (const char* game : {"cstrike", "czero", "valve"}) {
        gameDirectory = game;
        const bool expected = CSE_ENABLE_ZBOT && std::strcmp(game, "valve") != 0;
        if (info.SupportsBots() != expected) { return 1; }
    }
}
'''
        probe = '#include <initializer_list>\n' + probe
        for enabled in (1, 0):
            with self.subTest(enabled=enabled):
                self.compile_probe(probe, enabled)

    def test_staging_restores_resources_and_off_does_not_stage(self):
        with tempfile.TemporaryDirectory(prefix='cse-zbot-assets-') as directory:
            path = Path(directory)
            project = path / 'project'
            project.mkdir()
            (project / 'CMakeLists.txt').write_text(f'''
cmake_minimum_required(VERSION 3.25)
project(ZBotStaging NONE)
set(CSE_SDK_DIR "{ROOT.as_posix()}/src")
set(CSE_OUTPUT_DIRECTORY "${{CMAKE_BINARY_DIR}}/runtime")
include("{ROOT.as_posix()}/cmake/ZBot.cmake")
add_custom_target(server ALL)
cse_zbot_assets(server)
''')
            for enabled in ('default', 'OFF'):
                build = path / enabled
                option = [] if enabled == 'default' else ['-DCSE_ENABLE_ZBOT=OFF']
                self.run_command('cmake', '-S', str(project), '-B', str(build), '-G', 'Ninja', *option)
                self.run_command('cmake', '--build', str(build))
                destination = build / 'runtime/cstrike'
                if enabled == 'OFF':
                    self.assertFalse(destination.exists())
                    continue
                profile = destination / 'BotProfile.db'
                self.assertTrue(profile.is_file())
                database = (destination / 'BotChatter.db').read_text()
                database = re.sub(r'//[^\n]*', '', database)
                waves = set(re.findall(r'[^\s"]+\.wav', database, re.IGNORECASE))
                self.assertGreater(len(waves), 400)
                for wave in waves:
                    self.assertTrue((destination / 'sound' / wave.replace('\\', '/')).is_file(), wave)
                self.assertTrue((destination / 'maps/de_dust2.nav').is_file())
                self.assertTrue((destination / 'resource/CreateMultiplayerGameBotPage.res').is_file())
                stamp = profile.stat().st_mtime_ns
                wave_path = destination / 'sound' / sorted(waves)[0].replace('\\', '/')
                wave_path.unlink()
                self.run_command('cmake', '--build', str(build))
                self.assertTrue(wave_path.is_file())
                self.assertEqual(profile.stat().st_mtime_ns, stamp)


if __name__ == '__main__':
    unittest.main()
