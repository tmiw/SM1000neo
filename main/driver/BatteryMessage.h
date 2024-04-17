/* 
 * This file is part of the ezDV project (https://github.com/tmiw/ezDV).
 * Copyright (c) 2022 Mooneer Salem
 * 
 * This program is free software: you can redistribute it and/or modify  
 * it under the terms of the GNU General Public License as published by  
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef BATTERY_MESSAGE_H
#define BATTERY_MESSAGE_H

#include "task/DVTaskMessage.h"

extern "C"
{
    DV_EVENT_DECLARE_BASE(BATTERY_MESSAGE);
}

namespace ezdv
{

namespace driver
{

using namespace ezdv::task;

enum BatteryMessageTypes
{
    BATTERY_STATE = 1,
    LOW_POWER_SHUTDOWN = 2,
    REQUEST_BATTERY_STATE = 3,
};

class BatteryStateMessage : public DVTaskMessageBase<BATTERY_STATE, BatteryStateMessage>
{
public:
    BatteryStateMessage(float voltageProvided = 0, float socProvided = 0, float socChangeRateProvided = 0, bool usbPowerProvided = false)
        : DVTaskMessageBase<BATTERY_STATE, BatteryStateMessage>(BATTERY_MESSAGE)
        , voltage(voltageProvided)
        , soc(socProvided)
        , socChangeRate(socChangeRateProvided)
        , usbPowerEnabled(usbPowerProvided)
        {}
    virtual ~BatteryStateMessage() = default;

    float voltage;
    float soc;
    float socChangeRate;
    bool usbPowerEnabled;
};

class LowBatteryShutdownMessage : public DVTaskMessageBase<LOW_POWER_SHUTDOWN, BatteryStateMessage>
{
public:
    LowBatteryShutdownMessage() : DVTaskMessageBase<LOW_POWER_SHUTDOWN, BatteryStateMessage>(BATTERY_MESSAGE)
    {}

    virtual ~LowBatteryShutdownMessage() = default;
};

class RequestBatteryStateMessage : public DVTaskMessageBase<REQUEST_BATTERY_STATE, RequestBatteryStateMessage>
{
public:
    RequestBatteryStateMessage(bool updateTempRequested = false) 
        : DVTaskMessageBase<REQUEST_BATTERY_STATE, RequestBatteryStateMessage>(BATTERY_MESSAGE)
        , updateTemp(updateTempRequested)
    {}

    virtual ~RequestBatteryStateMessage() = default;
    
    bool updateTemp;
};

}

}

#endif // BATTERY_MESSAGE_H