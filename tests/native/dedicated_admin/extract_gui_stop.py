import pathlib
import sys
text = pathlib.Path(sys.argv[1]).read_text()
start = text.index('void NativeGui::Stop() noexcept {')
end = text.index('void NativeGui::Close() noexcept {', start)
pathlib.Path(sys.argv[2]).write_text('''
struct ControlledPanel {
    int m_hResourceWait = 0;
    int GetVPanel() { return 0; }
};
struct ControlledGui {
    void RemoveTickSignal(int) {}
};
struct ControlledFiles {
    void CancelWaitForResources(int) {}
};
struct ControlledAdminModule {
    void Deactivate() { RemoteServer().Shutdown(); }
};
namespace vgui2 {
ControlledGui gui;
ControlledFiles files;
ControlledGui* ivgui() { return &gui; }
ControlledFiles* filesystem() { return &files; }
}
namespace cse::dedicated {
struct NativeGui {
    ControlledPanel* panel_ = nullptr;
    ControlledAdminModule* adminModule_ = nullptr;
    bool bound_ = false;
    void Stop() noexcept;
};
''' + text[start:end] + '\n}\n')
