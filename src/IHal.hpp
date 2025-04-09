#pragma once
#include <time.h>

class IHalInterface {
   public:
    virtual ~IHalInterface() = default;

    virtual void init(bool synchronStart) = 0;

    virtual int32_t getBatteryLevel() { return -1; }

    virtual tm getTime() = 0;

    virtual bool isDisplayReady() = 0;

    virtual bool isNetworkConnected() = 0;
    
    virtual bool shouldFactoryReset() { return false; };

    virtual void startSleep() = 0;
};