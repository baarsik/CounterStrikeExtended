// Compile the WHOLE original consumer, including its real static CDummyApp.
#include CSE_APP_COLD_ROOT_SOURCE
#include "fixture.h"
extern "C" bool FixtureObserveProvider(vgui::App*, ColdObservation*);
extern "C" bool FixtureCleanupProvider(vgui::App*);
extern "C" bool FixtureObserveRoot(ColdObservation* output) {
    return static_cast<vgui::App*>(&theApp) == vgui::App::getInstance()
        && sizeof(CDummyApp) == sizeof(vgui::App) && FixtureObserveProvider(&theApp, output);
}
extern "C" bool FixtureCleanupRoot() {
    return FixtureCleanupProvider(&theApp);
}
