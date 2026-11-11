#pragma once
#include "ISystemModule.h"
struct client_data_s;
struct event_args_s;

class IEngineWrapper : public virtual ISystemModule {
public:
    virtual ~IEngineWrapper() = default;

    virtual bool GetViewOrigin(float *origin) = 0;
    virtual bool GetViewAngles(float *angles) = 0;
    virtual int GetTraceEntity() = 0;

    virtual float GetCvarFloat(char *name) = 0;
    virtual char *GetCvarString(char *name) = 0;
    virtual void SetCvar(char *name, char *value) = 0;

    virtual void Cbuf_AddText(char *text) = 0;

    virtual void DemoUpdateClientData(client_data_s *data) = 0;
    virtual void CL_QueueEvent(int flags, int index, float delay, event_args_s *args) = 0;
    virtual void HudWeaponAnim(int animation, int body) = 0;
    virtual void CL_DemoPlaySound(int channel, char *sample, float attenuation, float volume, int flags, int pitch) = 0;
    virtual void ClientDLL_ReadDemoBuffer(int size, unsigned char *buffer) = 0;

    virtual bool ValidStuffText(const char *message) = 0;
    virtual void Cbuf_AddFilteredText(char *text) = 0;
};
