"""Explicit controlled bodies for unrelated base/UI operations and typed services."""


def method_body(owner, name, type_):
    common = {
        ("Panel", "GetVPanel"): "    return _vpanel;",
        ("Panel", "GetVParent"): "    return ParentOf(this);",
        ("Panel", "~Panel"): "    UnregisterPanel(this);",
        ("Panel", "GetName"): "    return _panelName;",
        ("Panel", "IsEnabled"): "    return true;",
        ("Panel", "HasFocus"): "    return fixtureFocused == GetVPanel();",
        ("Panel", "IsProportional"): "    return false;",
        ("Panel", "SetCursor"): "    _cursor = cursor;",
        ("Panel", "Repaint"): '    LayoutCall(this, "Repaint");',
        ("Panel", "InvalidateLayout"): '    LayoutCall(this, "InvalidateLayout");',
        ("Panel", "RequestFocus"): '    Event(this, "Focus");',
        ("Panel", "RequestFocusNext"): '    Event(this, "Next");\n    AdvanceFocus(this);\n    return true;',
        ("Panel", "PostActionSignal"): '    Event(this, "Action");\n    message->deleteThis();',
        ("TextEntry", "GotoTextEnd"): "    _cursorPos = m_TextStream.Count();",
    }
    body = common.get((owner, name))
    if body is not None:
        return body
    if owner == "TextEntry" and name in ("ScrollRight", "LayoutVerticalScrollBarSlider", "ResetCursorBlink",
                                         "CalcBreakIndex", "RecalculateLineBreaks"):
        return '    LayoutCall(this, "' + name + '");'
    return None


def service_body(owner, name, type_):
    if owner == "ISystem":
        if name == "GetTimeMillis":
            return "        return 0;"
        if name == "GetClipboardTextCount":
            return '        ApiEvent("Count");\n        return CountHint();'
        if name == "GetClipboardText" and "wchar_t *" in type_:
            return "        return ReadClipboard(offset, buf, bufLen);"
        if name == "SetClipboardText" and "wchar_t *" in type_:
            return "        PublishClipboard(text, textLen);"
    if owner == "IPanel":
        if name == "GetCurrentKeyFocus":
            return '        ApiEvent("QueryFocus");\n        return QueryFocus();'
        if name == "SendMessage":
            return "        DeliverMessage(vguiPanel, params, ifrompanel);"
    if owner == "ISurface" and name == "PlaySound":
        return '        ApiEvent("Warning");'
    if owner == "IInput" and name == "IsKeyDown":
        return "        return KeyDown(code);"
    if owner == "IKeyValues":
        bodies = {
            "RegisterSizeofKeyValues": "        if (size != sizeof(KeyValues)) { throw Unexpected(\"KV size\"); }",
            "AllocKeyValuesMemory": "        return AllocateKeyValues(size);",
            "FreeKeyValuesMemory": "        FreeKeyValues(memory);",
            "GetSymbolForString": "        return SymbolFor(name);",
            "GetStringForSymbol": "        return StringFor(symbol);",
            "AddKeyValuesToMemoryLeakList": "        return;",
            "RemoveKeyValuesFromMemoryLeakList": "        return;",
        }
        if name in bodies:
            return bodies[name]
    return None
