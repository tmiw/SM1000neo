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

#ifndef USER_INTERFACE_TASK_H
#define USER_INTERFACE_TASK_H

#include "task/DVTask.h"
#include "task/DVTimer.h"
#include "audio/FreeDVMessage.h"
#include "audio/VoiceKeyerMessage.h"
#include "driver/BatteryMessage.h"
#include "driver/ButtonMessage.h"
#include "driver/TLV320Message.h"
#include "network/NetworkMessage.h"
#include "storage/SettingsMessage.h"

namespace ezdv
{

namespace ui
{

using namespace ezdv::task;

class UserInterfaceTask : public DVTask
{
public:
    UserInterfaceTask();
    virtual ~UserInterfaceTask();

protected:
    virtual void onTaskStart_() override;
    virtual void onTaskSleep_() override;

private:
    DVTimer volHoldTimer_;
    DVTimer networkFlashTimer_;
    DVTimer timeOutTimer_;
    audio::FreeDVMode currentMode_;
    bool isTransmitting_;
    bool isActive_;
    int8_t leftVolume_;
    int8_t rightVolume_;
    int8_t volIncrement_;
    bool netLedStatus_;
    bool radioStatus_;
    bool voiceKeyerRunning_;
    bool voiceKeyerEnabled_;
    int lastBatteryLevel_;
    bool sleepPending_;
    bool allowHeadsetPtt_;

    // Button handling
    void onButtonShortPressedMessage_(DVTask* origin, driver::ButtonShortPressedMessage* message);
    void onButtonLongPressedMessage_(DVTask* origin, driver::ButtonLongPressedMessage* message);
    void onButtonReleasedMessage_(DVTask* origin, driver::ButtonReleasedMessage* message);

    // Sync state handling
    void onFreeDVSyncStateMessage_(DVTask* origin, audio::FreeDVSyncStateMessage* message);

    // Storage update handling
    void onLeftChannelVolumeMessage_(DVTask* origin, storage::LeftChannelVolumeMessage* message);
    void onRightChannelVolumeMessage_(DVTask* origin, storage::RightChannelVolumeMessage* message);

    // Network state handling
    void onNetworkStateChange_(DVTask* origin, network::WirelessNetworkStatusMessage* message);
    void onRadioStateChange_(DVTask* origin, network::RadioConnectionStatusMessage* message);
    void flashNetworkLight_(DVTimer*);

    // Voice keyer handling
    void onRequestTxMessage_(DVTask* origin, audio::RequestTxMessage* message);
    void onRequestRxMessage_(DVTask* origin, audio::RequestRxMessage* message);
    void onVoiceKeyerSettingsMessage_(DVTask* origin, storage::VoiceKeyerSettingsMessage* message);
    void onVoiceKeyerCompleteMessage_(DVTask* origin, audio::VoiceKeyerCompleteMessage* message);
    void onRequestStartStopKeyerMessage_(DVTask* origin, audio::RequestStartStopKeyerMessage* message);
    void onGetKeyerStateMessage_(DVTask* origin, audio::GetKeyerStateMessage* message);
    void startTx_();
    void stopTx_(DVTimer*);

    // ADC overload handling
    void onADCOverload_(DVTask* origin, driver::OverloadStateMessage* message);

    // Headset button press handling
    void onHeadsetButtonPressed_(DVTask* origin, driver::HeadsetButtonPressMessage* message);

    // Timer handling
    void updateVolumeCommon_(DVTimer*);

    // Battery state handling
    void onBatteryStateUpdate_(DVTask* origin, driver::BatteryStateMessage* message);

    // Mode handling
    void onRequestSetFreeDVModeMessage_(DVTask* origin, audio::RequestSetFreeDVModeMessage* message);

    // Radio settings handling
    void onRadioSettingsMessage_(DVTask* origin, storage::RadioSettingsMessage* message);
    
    // IP address assignment handling
    void onIpAddressAssignedMessage_(DVTask* origin, network::IpAddressAssignedMessage* message);
};

}

}

#endif // USER_INTERFACE_TASK_H