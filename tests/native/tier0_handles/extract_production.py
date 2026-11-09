"""Extract complete production functions; never substitute their algorithms or change C++ widths."""

from pathlib import Path
import re
import sys


def functions(source, signature):
    found = []
    pattern = r"^" + re.escape(signature) + r"\s*(?::[^\n]*\n)?\s*\{"
    for match in re.finditer(pattern, source, re.M):
        opening = source.index("{", match.start())
        depth = 1
        cursor = opening + 1
        while depth:
            if source[cursor] == "{":
                depth += 1
            elif source[cursor] == "}":
                depth -= 1
            cursor += 1
        found.append(source[match.start():cursor] + "\n")
    if not found:
        raise ValueError(f"Missing production function: {signature}")
    return found


def main():
    source, output = (Path(value).resolve() for value in sys.argv[1:])
    threads = (source / "tier0/threadtools.cpp").read_text(encoding="cp1252")
    profile = (source / "tier0/vprof.cpp").read_text(encoding="cp1252")
    helper = ""
    if "ThreadDiagnosticBits(ThreadId value)" in threads:
        # Retain complete templates, including their return-type constraints.
        for match in re.finditer(r"^template<typename ThreadId>\n(?=static[^;{]*ThreadDiagnosticBits)", threads, re.M):
            opening = threads.index("{", match.end())
            signature = threads[match.start():opening].strip()
            helper += functions(threads, signature)[0]
    names = functions(threads, "const char* CThread::GetName()")
    setters = functions(threads, "void CThread::SetName( const char* name )") if (
        "void CThread::SetName( const char* name )" in threads) else []
    setters += functions(threads, "void CThread::SetName( const char* pszName )")
    constructors = functions(threads, "CThread::CThread()")
    windows = functions(threads, "CThreadSyncObject::CThreadSyncObject()")[0]
    windows += functions(threads, "CThreadEvent::CThreadEvent( void* event, bool ownEvent )")[0]
    output.joinpath("actual_windows_diagnostics.inc").write_text(helper + windows + names[0])
    for index, platform in enumerate(("windows", "posix")):
        tls = ""
        for signature in ("CThreadLocalBase::CThreadLocalBase()", "CThreadLocalBase::~CThreadLocalBase()",
                          "void* CThreadLocalBase::Get() const", "void CThreadLocalBase::Set( void* value )"):
            tls += functions(threads, signature)[index]
        output.joinpath(f"actual_{platform}_tls.inc").write_text(tls)
        native = helper + constructors[index] + names[index] + setters[index]
        if platform == "windows":
            native += windows
        output.joinpath(f"actual_{platform}_thread.inc").write_text(native)
    serial = ""
    for signature in ("static void* EncodeThreadSerial(int serial)",
                      "static std::uint32_t DecodeThreadSerial(void* token)"):
        if signature in profile:
            serial += functions(profile, signature)[0]
    for signature in ("void CVProfile::Start()", "void CVProfile::Stop()",
                      "void CVProfile::EnterScope( const char* pName, int detailLevel, const char* pBudgetGroup, bool bAssertAccounted )",
                      "void CVProfile::EnterScope( const char* pName, int detailLevel, const char* pBudgetGroup, bool, int budgetFlags )",
                      "void CVProfile::ExitScope()"):
        serial += functions(profile, signature)[0]
    output.joinpath("actual_profile.inc").write_text(serial)
    header = (source / "public/tier0/threadtools.h").as_posix()
    output.joinpath("selected_threadtools.h").write_text(f'#include "{header}"\n')


if __name__ == "__main__":
    main()
