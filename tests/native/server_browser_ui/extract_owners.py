"""Extract complete real owning bodies, with immutable source/body receipts.

This tool generates fixtures only when explicitly run after compiler/source admission.
It does not rewrite statements or substitute a test-owned state machine.
"""
import argparse
import hashlib
import json
import re
from pathlib import Path


def closing_brace(source, opening):
    depth = 0
    state = "code"
    index = opening
    while index < len(source):
        character = source[index]
        following = source[index:index + 2]
        if state == "line":
            if character == "\n":
                state = "code"
        elif state == "block":
            if following == "*/":
                state = "code"
                index += 1
        elif state in ('"', "'"):
            if character == "\\":
                index += 1
            elif character == state:
                state = "code"
        elif following == "//":
            state = "line"
            index += 1
        elif following == "/*":
            state = "block"
            index += 1
        elif character in ('"', "'"):
            state = character
        elif character == "{":
            depth += 1
        elif character == "}":
            depth -= 1
            if depth == 0:
                return index + 1
        index += 1
    raise ValueError("unterminated owning body")


def extract(source, symbol, occurrence, expected_count):
    pattern = re.compile(r"^[\w:*<> \t]+\b" + re.escape(symbol) + r"\s*\([^;{}]*\)\s*(?:const\s*)?\{", re.M)
    matches = list(pattern.finditer(source))
    if len(matches) != expected_count:
        raise ValueError(f"{symbol}: expected {expected_count} real bodies, found {len(matches)}; update ownership audit")
    match = matches[occurrence]
    end = closing_brace(source, source.index("{", match.start()))
    return source[match.start():end], source.count("\n", 0, match.start()) + 1


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    groups = {
        "retirement.inc": [
            ("engine/vgui_intwrap.cpp", "cse::TryShutdownNativeVGuiWrap1", 0, 1),
            ("engine/vgui_intwrap.cpp", "VGuiWrap_Shutdown", 0, 1),
            ("engine/vgui_intwrap2.cpp", "cse::TryShutdownNativeVGuiWrap2", 0, 1),
            ("engine/vgui_int.cpp", "cse::TryShutdownNativeVGui", 0, 1),
            ("engine/host.cpp", "cse::SetNativeHostInitPhase", 0, 1),
            ("engine/host.cpp", "cse::ResetNativeHostShutdown", 0, 1),
            ("engine/host.cpp", "cse::TryShutdownNativeHost", 0, 1),
            ("engine/sys_dll2.cpp", "cse::TryShutdownNativeGame", 0, 1),
            ("engine/sys_engine.cpp", "CEngine::TryUnload", 0, 1),
            ("engine/vgui_intwrap2.cpp", "VGuiWrap2_Shutdown", 0, 1),
            ("engine/vgui_int.cpp", "VGui_Shutdown", 0, 1),
            ("engine/host.cpp", "Host_Shutdown", 0, 1),
            ("engine/sys_dll2.cpp", "Sys_ShutdownGame", 1, 2),
            ("engine/sys_engine.cpp", "CEngine::Unload", 0, 1),
        ],
        "startup_sw.inc": [("engine/sys_dll2.cpp", "Sys_InitGame", 0, 2)],
        "startup_hw.inc": [("engine/sys_dll2.cpp", "Sys_InitGame", 1, 2)],
        "loader.inc": [
            ("GameUI/VGuiSystemModuleLoader.cpp", "CVGuiSystemModuleLoader::" + name, 0, 1)
            for name in ("GetModuleFactory", "InitializeAllModules", "LoadPlatformModules", "ShutdownPlatformModules",
                         "UnloadPlatformModules", "RunFrame", "TryRetireNativeModules", "DeactivatePlatformModules",
                         "ReactivatePlatformModules", "PostMessageToAllModules")
        ],
    }
    groups["gameui_shutdown.inc"] = [("GameUI/GameUI_Interface.cpp", "CGameUI::Shutdown", 0, 1)]
    groups["gameui_panels.inc"] = [
        ("GameUI/GameConsole.cpp", "CGameConsole::TryRetireNativePanel", 0, 1),
        ("GameUI/GameConsole.cpp", "CGameConsole::IsNativePanelRetired", 0, 1),
        ("GameUI/GameUI_Interface.cpp", "cse::server_browser::TryRetireGameUIPanels", 0, 1),
        ("GameUI/GameUI_Interface.cpp", "cse::server_browser::AreGameUIPanelsRetired", 0, 1),
    ]
    groups["baseui_shutdown.inc"] = [
        ("engine/vgui2/BaseUI_Interface.cpp", "CBaseUI::TryRetireGameUI", 0, 1),
        ("engine/vgui2/BaseUI_Interface.cpp", "CBaseUI::TryShutdown", 0, 1),
    ]
    groups["baseui_surface_retirement.inc"] = [
        ("engine/vgui2/BaseUISurface.cpp", "BaseUISurface::SolveTraverse", 0, 1),
        ("engine/vgui2/BaseUISurface.cpp", "BaseUISurface::SetEmbeddedPanel", 0, 1),
        ("engine/vgui2/BaseUISurface.cpp", "BaseUISurface::GetEmbeddedPanel", 0, 1),
    ]
    groups["entry_sw.inc"] = [("engine/sys_dll2.cpp", "CEngineAPI::Run", 0, 2)]
    groups["entry_hw.inc"] = [("engine/sys_dll2.cpp", "CEngineAPI::Run", 1, 2)]
    groups["host_init.inc"] = [("engine/host.cpp", "Host_Init", 0, 1)]
    groups["accepted_connection.inc"] = [
        ("GameUI/GameUI_Interface.cpp", "CGameUI::ConnectToServer", 0, 1),
        ("GameUI/GameUI_Interface.cpp", "CGameUI::DisconnectFromServer", 0, 1),
    ]
    groups["listen_sw.inc"] = [("engine/sys_dll2.cpp", "RunListenServer", 0, 2)]
    groups["listen_hw.inc"] = [("engine/sys_dll2.cpp", "RunListenServer", 1, 2)]
    groups["listen_retirement.inc"] = [
        ("engine/sys_dll2.cpp", "cse::BeginNativeListenShutdown", 0, 1),
        ("engine/sys_dll2.cpp", "cse::FinishNativeListenShutdown", 0, 1),
    ]
    receipt = []
    state_regions = [
        ("engine/vgui_intwrap.cpp", "namespace {\ncse_vgui1_native::Result nativeVGui1RetirementResult"),
        ("engine/vgui_int.cpp", "namespace {\nenum class NativeVGuiShutdownPhase"),
        ("engine/host.cpp", "namespace {\nenum class NativeHostShutdownPhase"),
    ]
    loader_path = args.repo / "src/GameUI/VGuiSystemModuleLoader.cpp"
    loader_source = loader_path.read_text(errors="surrogateescape")
    marker = "namespace {\nclass NativeModuleCallScope final"
    start = loader_source.index(marker)
    end = closing_brace(loader_source, loader_source.index("{", start))
    guard = loader_source[start:end]
    (args.output / "loader_guard.inc").write_text(guard)
    receipt.append(dict(source="src/GameUI/VGuiSystemModuleLoader.cpp", symbol="actual module call guard",
                        sourceSha256=hashlib.sha256(loader_path.read_bytes()).hexdigest(),
                        bodySha256=hashlib.sha256(guard.encode()).hexdigest()))
    listen_path = args.repo / "src/engine/sys_dll2.cpp"
    listen_source = listen_path.read_text(errors="surrogateescape")
    start = listen_source.index("namespace {\nenum class NativeListenShutdownPhase")
    end = closing_brace(listen_source, listen_source.index("{", start))
    listen_state = listen_source[start:end]
    (args.output / "listen_state.inc").write_text(listen_state)
    receipt.append(dict(source="src/engine/sys_dll2.cpp", symbol="actual outer listen phase state",
                        sourceSha256=hashlib.sha256(listen_path.read_bytes()).hexdigest(),
                        bodySha256=hashlib.sha256(listen_state.encode()).hexdigest()))
    states = []
    for relative, marker in state_regions:
        raw = (args.repo / "src" / relative).read_bytes()
        source = raw.decode("utf-8")
        if source.count(marker) != 1:
            raise ValueError(f"{relative}: actual private phase region missing or ambiguous")
        start = source.index(marker)
        end = closing_brace(source, source.index("{", start))
        body = source[start:end]
        line = source.count("\n", 0, start) + 1
        declaration = f'#line {line} "src/{relative}"\n{body}\n'
        if relative == "engine/vgui_intwrap.cpp":
            declaration = "#if defined(__APPLE__)\n" + declaration + "#endif\n"
        states.append(declaration)
        receipt.append({"source": "src/" + relative, "symbol": "private phase state", "line": line,
                        "sourceSha256": hashlib.sha256(raw).hexdigest(),
                        "bodySha256": hashlib.sha256(body.encode()).hexdigest()})
    (args.output / "retirement_state.inc").write_text("\n".join(states))
    panel_states = []
    for relative, marker in [
        ("GameUI/GameUI_Interface.cpp", "namespace {\nvgui2::DHANDLE<CBasePanel> nativeGameUIRoot"),
        ("GameUI/GameConsole.cpp", "namespace {\nvgui2::DHANDLE<CGameConsoleDialog> nativeConsolePanel"),
    ]:
        raw = (args.repo / "src" / relative).read_bytes()
        source = raw.decode("utf-8", errors="surrogateescape")
        start = source.index(marker)
        end = closing_brace(source, source.index("{", start))
        body = source[start:end]
        panel_states.append(body)
        receipt.append(dict(source="src/" + relative, symbol="actual panel phase state",
                            sourceSha256=hashlib.sha256(raw).hexdigest(),
                            bodySha256=hashlib.sha256(body.encode()).hexdigest()))
    (args.output / "panel_state.inc").write_text("\n".join(panel_states))
    fixture_path = args.repo / "tests/native/server_browser/model_tests.cpp"
    fixture_raw = fixture_path.read_bytes()
    fixture_source = fixture_raw.decode("utf-8")
    fixture_marker = "\nvoid Addresses()"
    if fixture_source.count(fixture_marker) != 1:
        raise ValueError("Task 1 typed fixture boundary changed; inspect before extraction")
    fixture_prefix = fixture_source[:fixture_source.index(fixture_marker)]
    (args.output / "model_fixture.inc").write_text(fixture_prefix + "\n")
    receipt.append({"source": "tests/native/server_browser/model_tests.cpp", "symbol": "typed fixture prefix",
                    "line": 1, "sourceSha256": hashlib.sha256(fixture_raw).hexdigest(),
                    "bodySha256": hashlib.sha256(fixture_prefix.encode()).hexdigest()})
    favorite_path = args.repo / "tests/native/server_browser/favorite_adapter_tests.cpp"
    favorite_raw = favorite_path.read_bytes()
    favorite_prefix = favorite_raw.decode().split("\nint main()", 1)[0]
    (args.output / "matchmaking_fixture.inc").write_text(favorite_prefix)
    receipt.append(dict(source="tests/native/server_browser/favorite_adapter_tests.cpp", symbol="typed matchmaking fixture",
                        sourceSha256=hashlib.sha256(favorite_raw).hexdigest(),
                        bodySha256=hashlib.sha256(favorite_prefix.encode()).hexdigest()))
    for output_name, declarations in groups.items():
        bodies = []
        for relative, symbol, occurrence, expected_count in declarations:
            path = args.repo / "src" / relative
            raw = path.read_bytes()
            source = raw.decode("utf-8", errors="surrogateescape")
            body, line = extract(source, symbol, occurrence, expected_count)
            bodies.append(f'#line {line} "src/{relative}"\n{body}\n')
            receipt.append({"source": "src/" + relative, "symbol": symbol,
                            "occurrence": occurrence, "line": line,
                            "sourceSha256": hashlib.sha256(raw).hexdigest(),
                            "bodySha256": hashlib.sha256(body.encode("utf-8", errors="surrogateescape")).hexdigest()})
        (args.output / output_name).write_text("\n".join(bodies), errors="surrogateescape")
    (args.output / "owner-extraction.json").write_text(json.dumps(receipt, indent=2) + "\n")


if __name__ == "__main__":
    main()
