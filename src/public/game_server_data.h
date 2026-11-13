#pragma once
#include "interface_registry.h"
class IGameServerData : public IBaseInterface {
public:
    virtual void WriteDataRequest(const void* buffer, int bufferSize) = 0;
    virtual int ReadDataResponse(void* buffer, int bufferSize) = 0;
};
