"""Compile real owner declarations; import exact bodies and emit typed test bindings.

The generated Panel/service definitions are controlled test behavior, not production
Panel/UI qualification. There is no class rewriting or vptr construction.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shlex
import subprocess
from fixture_bindings import method_body, service_body


def digest(data):
    return hashlib.sha256(data).hexdigest()


def method(source, signature):
    start = source.index(signature)
    brace = source.index("{", start)
    # Actual retained method bodies do not contain unmatched braces in literals.
    depth = 1
    cursor = brace + 1
    while depth:
        if source[cursor] == "{":
            depth += 1
        elif source[cursor] == "}":
            depth -= 1
        cursor += 1
    return source[start:cursor]


def documents(text):
    decoder = json.JSONDecoder()
    cursor = 0
    while cursor < len(text):
        while cursor < len(text) and text[cursor].isspace():
            cursor += 1
        if cursor == len(text):
            return
        value, cursor = decoder.raw_decode(text, cursor)
        yield value


def record(documents_, name):
    matches = [d for d in documents_ if d.get("kind") == "CXXRecordDecl"
               and d.get("name") == name and d.get("completeDefinition")]
    if len(matches) != 1:
        raise RuntimeError((name, "unknown or ambiguous declaring report", len(matches)))
    return matches[0]


def signature(node, owner, interface=False):
    name = node["name"]
    parameters = []
    for index, parameter in enumerate(x for x in node.get("inner", []) if x["kind"] == "ParmVarDecl"):
        spelling = parameter["type"]["qualType"]
        if "(" in spelling or "[" in spelling:
            raise RuntimeError((owner, name, "unsupported parameter", spelling))
        parameters.append(spelling + " " + parameter.get("name", "arg" + str(index)))
    function_type = node["type"]["qualType"]
    split = function_type.index("(")
    return_type = function_type[:split].strip()
    if owner == "Panel" and return_type in ("PinCorner_e", "AutoResize_e", "eHDProportionalStyle"):
        return_type = "Panel::" + return_type
    if owner == "KeyValues" and return_type == "types_t":
        return_type = "KeyValues::types_t"
    suffix = " const" if function_type.endswith(" const") else ""
    if function_type.endswith(" noexcept") and node["kind"] != "CXXDestructorDecl":
        suffix += " noexcept"
    scope = "" if interface else owner + "::"
    if node["kind"] in ("CXXConstructorDecl", "CXXDestructorDecl"):
        return_type = ""
    return (return_type + " " + scope + name + "(" + ", ".join(parameters) + ")" + suffix).strip()


def ignore_parameters(node, indentation):
    rows = []
    for index, parameter in enumerate(x for x in node.get("inner", []) if x["kind"] == "ParmVarDecl"):
        rows.append(indentation + "static_cast<void>(" + parameter.get("name", "arg" + str(index)) + ");")
    return "\n".join(rows) + ("\n" if rows else "")


def reject_body(node, label, indentation):
    type_ = node["type"]["qualType"]
    result = type_[:type_.index("(")].strip()
    if "&" in result:
        raise RuntimeError((label, "unhandled reference return"))
    final = "return;" if result == "void" or node["kind"] == "CXXDestructorDecl" else "return {};"
    return indentation + 'RejectUnexpected("' + label + '");\n' + indentation + final


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--database", type=Path, required=True)
    parser.add_argument("--owner", choices=["common", "cs"], required=True)
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)
    owner_file = args.repo / ("GoldSrc-SDK/vgui2/controls/TextEntry.cpp" if args.owner == "common"
                              else "GoldSrc-SDK/cstrike/vgui2/controls/TextEntry.cpp")
    target = "gameui" if args.owner == "common" else "cs_client"
    entries = [e for e in json.loads(args.database.read_text()) if e["file"] == str(owner_file)
               and (target + ".dir") in e["output"]]
    if len(entries) != 1:
        raise RuntimeError("owning context not unique")
    entry = entries[0]
    flags = []
    skip = False
    for token in shlex.split(entry["command"]):
        if skip:
            skip = False
            continue
        if token == "-o":
            skip = True
        elif token not in ("-c", entry["file"]):
            flags.append(token)
    header = "TextEntry.h" if args.owner == "common" else "vgui_controls/TextEntry.h"
    prefix = '#include "' + header + '"\n'
    prefix += '\n'.join('#include "' + n + '"' for n in
                        ["vgui/ISystem.h", "vgui/IPanel.h", "vgui/IInput.h", "vgui/KeyValues.h",
                         "IKeyValues.h", "controls.h" if args.owner == "common" else "Controls.h",
                         "vgui/IScheme.h", str(args.repo / "GoldSrc-SDK/engine/common.h"),
                         str(args.repo / "GoldSrc-SDK/engine/controls.h"),
                         str(args.repo / "GoldSrc-SDK/common/TextEntryClipboard.h"),
                         "vgui/IVGui.h", "vgui/Cursor.h", "Menu.h", "ScrollBar.h"])
    source_probe = args.out / "declarations.cpp"
    source_probe.write_text(prefix + "\n")
    reports = {}
    commands = []
    for name in ["Panel", "TextEntry", "ISystem", "IPanel", "IInput", "ISurface", "IKeyValues", "KeyValues"]:
        qualified = name if name in ("IKeyValues", "KeyValues") else "vgui2::" + name
        command = flags + ["-fsyntax-only", "-Xclang", "-ast-dump=json", "-Xclang",
                           "-ast-dump-filter=" + qualified, str(source_probe)]
        result = subprocess.run(command, cwd=entry["directory"], capture_output=True, check=False)
        (args.out / (name + "-ast.log")).write_bytes(result.stderr)
        if result.returncode:
            raise RuntimeError(result.stderr.decode(errors="replace"))
        (args.out / (name + "-ast.json")).write_bytes(result.stdout)
        reports[name] = record(list(documents(result.stdout.decode())), name)
        commands.append({"command": command, "exit": result.returncode})
    (args.out / "declaring-commands.json").write_text(json.dumps(commands, indent=2) + "\n")
    imports = []
    source = owner_file.read_text(encoding="latin1")
    selected = ["TextEntry::TextEntry( Panel*", "TextEntry::~TextEntry()", "void TextEntry::Paste()",
                "void TextEntry::CopySelected()", "void TextEntry::InsertChar( wchar_t",
                "void TextEntry::SetCharAt( wchar_t", "void TextEntry::SaveUndoState()",
                "void TextEntry::FireActionSignal()", "bool TextEntry::GetSelectedRange(",
                "bool TextEntry::IsEditable()", "void TextEntry::SetEditable(",
                "void TextEntry::SetMaximumCharCount(", "void TextEntry::SetAllowNonAsciiCharacters(",
                "void TextEntry::SetAllowNumericInputOnly(", "void TextEntry::SetMultiline("]
    selected += ["void TextEntry::SetAutoProgressOnHittingCharLimit(", "void TextEntry::SetWrap(",
                 "void TextEntry::SetTextHidden(", "void TextEntry::OnKeyCodeTyped(",
                 "void TextEntry::DeleteSelected()", "void TextEntry::SelectNone()"]
    bodies = []
    imported_names = set()
    for sig in selected:
        body = method(source, sig)
        bodies.append(body)
        imported_names.add(re.search(r"TextEntry::(~?\w+)\(", body)[1])
        imports.append({"source": str(owner_file), "signature": sig, "sha256": digest(body.encode("latin1"))})
    # Common owner has out-of-line registration constructors; CS macros define them inline.
    if args.owner == "common":
        begin = source.index("void TextEntry::ChainToMap()")
        end = source.index("// Purpose: Constructor", begin)
        registrations = source[begin:end]
        registrations = registrations[:registrations.rfind("//-----------------------------------------------------------------------------")]
        bodies.insert(0, registrations)
        imported_names.add("ChainToMap")
    (args.out / "actual_textentry.inc").write_bytes(("\n\n".join(bodies) + "\n").encode("latin1"))
    (args.out / "imports.json").write_text(json.dumps(imports, indent=2) + "\n")
    stubs = []
    for name in ["Panel", "TextEntry"]:
        for node in reports[name].get("inner", []):
            if node["kind"] not in ("CXXMethodDecl", "CXXConstructorDecl", "CXXDestructorDecl"):
                continue
            if node.get("isImplicit") or node.get("explicitlyDefaulted"):
                continue
            if any(x["kind"] == "CompoundStmt" for x in node.get("inner", [])):
                continue
            if name == "TextEntry" and node["name"] in imported_names:
                continue
            if name == "TextEntry" and node["name"] == "GetMessageMap":
                continue  # Actual common header defines it after the class.
            if name == "Panel" and node["kind"] == "CXXConstructorDecl":
                continue
            if node.get("storageClass") == "static":
                continue
            if name == "Panel" and node["name"] == "OnMessage":
                continue
            body = method_body(name, node["name"], node["type"]["qualType"])
            if body is None:
                body = reject_body(node, name + "::" + node["name"], "    ")
            stubs.append(signature(node, name) + " {\n" + ignore_parameters(node, "    ") + body + "\n}")
    (args.out / "unrelated_methods.inc").write_text("\n".join(stubs) + "\n")
    services = []
    for name in ["ISystem", "IPanel", "IInput", "ISurface", "IKeyValues"]:
        base = name if name == "IKeyValues" else "vgui2::" + name
        services.append("class Controlled" + name + " : public " + base + " {\npublic:")
        for node in reports[name].get("inner", []):
            if node["kind"] != "CXXMethodDecl" or node.get("isImplicit"):
                continue
            if not node.get("virtual") and not node.get("pure"):
                continue
            body = service_body(name, node["name"], node["type"]["qualType"])
            if body is None:
                body = reject_body(node, name + "::" + node["name"], "        ")
            services.append("    " + signature(node, name, True) + " override {\n"
                            + ignore_parameters(node, "        ") + body + "\n    }")
        services.append("};")
    (args.out / "controlled_services.inc").write_text("\n".join(services) + "\n")
    base_bodies = []
    for node in reports["IKeyValues"].get("inner", []):
        if node["kind"] == "CXXMethodDecl" and node.get("virtual") and not node.get("isImplicit"):
            base_bodies.append(signature(node, "IKeyValues") + " {\n" + ignore_parameters(node, "    ")
                               + reject_body(node, "base IKeyValues", "    ") + '\n}')
    (args.out / "keyvalues_base.inc").write_text("\n".join(base_bodies) + "\n")
    panel_source = (args.repo / "GoldSrc-SDK/vgui2/controls/Panel.cpp").read_text(encoding="latin1")
    (args.out / "actual_onmessage.inc").write_text(method(panel_source, "void Panel::OnMessage(") + "\n")
    (args.out / "actual_prepare_map.inc").write_text(method(panel_source, "void PreparePanelMessageMap(") + "\n")
    kv_source = (args.repo / "GoldSrc-SDK/engine/KeyValues.cpp").read_text(encoding="latin1")
    kv_signatures = ["KeyValues::KeyValues( const char* name )",
                     "KeyValues::KeyValues( const char* name, const char* firstKey, const char* firstValue )",
                     "void KeyValues::Init(",
                     "KeyValues::~KeyValues()", "void* KeyValues::operator new( std::size_t size )",
                     "void KeyValues::operator delete( void* memory )", "void KeyValues::FreeAllocatedValue()",
                     "const char* KeyValues::GetName() const", "int KeyValues::GetNameSymbol() const",
                     "void KeyValues::deleteThis()"]
    kv_bodies = [method(kv_source, s) for s in kv_signatures]
    kv_names = {re.search(r"KeyValues::([^\(]+)\(", s)[1].strip() for s in kv_bodies}
    kv_stubs = []
    for node in reports["KeyValues"].get("inner", []):
        if node["kind"] != "CXXMethodDecl" or node.get("isImplicit") or node.get("storageClass") == "static":
            continue
        if node["name"] in kv_names or any(x["kind"] == "CompoundStmt" for x in node.get("inner", [])):
            continue
        kv_stubs.append(signature(node, "KeyValues") + '\n{\n' + ignore_parameters(node, "    ")
                        + reject_body(node, "KeyValues::" + node["name"], "    ") + '\n}')
    (args.out / "actual_keyvalues.inc").write_text("\n\n".join(kv_bodies + kv_stubs) + "\n")
    helper = (args.repo / "GoldSrc-SDK/common/TextEntryClipboard.cpp").read_text()
    default_allocator = method(helper, "Allocator DefaultAllocator()")
    binding = """Allocator DefaultAllocator() {
    return {&::ControlledTextAllocate, &::ControlledTextRelease};
}"""
    helper = helper.replace(default_allocator, binding, 1)
    declarations = """#include <cstddef>
void* ControlledTextAllocate(std::size_t bytes);
void ControlledTextRelease(void* memory);
"""
    (args.out / "actual_clipboard_helper.cpp").write_text(declarations + helper)
    (args.out / "allocator-binding.json").write_text(json.dumps({
        "status": "explicit controlled allocator backend only; actual Buffer/Frame implementations unchanged",
        "production_source_sha256": digest((args.repo / "GoldSrc-SDK/common/TextEntryClipboard.cpp").read_bytes()),
        "production_default_allocator_sha256": digest(default_allocator.encode()),
        "bound_source_sha256": digest((declarations + helper).encode())}, indent=2) + "\n")
    (args.out / "owning-context.json").write_text(json.dumps({"entry": entry, "flags": flags,
                                                            "owner": args.owner}, indent=2) + "\n")
    print(args.owner, "actual declaration reports/imports generated")


if __name__ == "__main__":
    main()
