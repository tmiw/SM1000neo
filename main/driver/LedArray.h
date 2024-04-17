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

#ifndef LED_ARRAY_H
#define LED_ARRAY_H

#include "storage/SettingsMessage.h"
#include "task/DVTask.h"
#include "LedMessage.h"
#include "OutputGPIO.h"

namespace ezdv
{

namespace driver
{

using namespace ezdv::task;

class LedArray : public DVTask
{
public:
    LedArray();
    virtual ~LedArray();

protected:
    virtual void onTaskStart_() override;
    virtual void onTaskSleep_() override;

private:
    OutputGPIO syncLed_;
    OutputGPIO overloadLed_;
    OutputGPIO pttLed_;
    OutputGPIO pttNpmLed_;
    OutputGPIO networkLed_;

    void onSetLedState_(DVTask* origin, SetLedStateMessage* message);
    void onLedBrightnessSettingsMessage_(DVTask* origin, storage::LedBrightnessSettingsMessage* message);
};

}

}

#endif // LED_ARRAY_H