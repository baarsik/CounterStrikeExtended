#ifndef CSEXTENDED_FEATURE_HOOKS_H
#define CSEXTENDED_FEATURE_HOOKS_H

#include "amxxmodule.h"

void StatsAttach();
void StatsServerActivate();
void StatsServerActivated();
void StatsServerDeactivate();
void StatsStartFrame();
void StatsClientEnter(edict_t* entity);
void StatsClientLeave(edict_t* entity);

void FormsAttach();
void FormsPluginsLoaded();
void FormsPluginsUnloading();
void FormsServerActivate();
void FormsServerDeactivate();
void FormsStartFrame();
void FormsClientEnter(edict_t* entity);
void FormsClientLeave(edict_t* entity);

void WeaponsAttach();
void WeaponsPluginsLoaded();
void WeaponsPluginsUnloading();
void WeaponsServerActivate();
void WeaponsServerDeactivate();
void WeaponsStartFrame();
void WeaponsClientLeave(edict_t* entity);

#endif
