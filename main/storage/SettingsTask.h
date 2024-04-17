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

#ifndef SETTINGS_TASK_H
#define SETTINGS_TASK_H

#include <memory>

#include "nvs_flash.h"
#include "nvs.h"
#include "nvs_handle.hpp"

#include "task/DVTask.h"
#include "task/DVTimer.h"

#include "SettingsMessage.h"
#include "audio/FreeDVMessage.h"

namespace ezdv
{

namespace storage
{

using namespace ezdv::task;

class SettingsTask : public DVTask
{
public:
    SettingsTask();
    virtual ~SettingsTask() = default;

protected:
    virtual void onTaskStart_() override;
    virtual void onTaskSleep_() override;

private:
    int8_t leftChannelVolume_;
    int8_t rightChannelVolume_;
    
    bool wifiEnabled_;
    WifiMode wifiMode_;
    WifiSecurityMode wifiSecurity_;
    int wifiChannel_;
    char wifiSsid_[WifiSettingsMessage::MAX_STR_SIZE];
    char wifiPassword_[WifiSettingsMessage::MAX_STR_SIZE];
    char wifiHostname_[WifiSettingsMessage::MAX_STR_SIZE];
    
    bool headsetPtt_;
    int timeOutTimer_;
    bool radioEnabled_;
    int radioType_;
    char radioHostname_[RadioSettingsMessage::MAX_STR_SIZE];
    int radioPort_;
    char radioUsername_[RadioSettingsMessage::MAX_STR_SIZE];
    char radioPassword_[RadioSettingsMessage::MAX_STR_SIZE];
    
    char callsign_[ReportingSettingsMessage::MAX_STR_SIZE];
    char gridSquare_[ReportingSettingsMessage::MAX_STR_SIZE];
    char message_[ReportingSettingsMessage::MAX_MSG_SIZE];
    bool forceReporting_;
    uint64_t freqHz_;

    bool enableVoiceKeyer_;
    int voiceKeyerNumberTimesToTransmit_;
    int voiceKeyerSecondsToWaitAfterTransmit_;
    
    int ledDutyCycle_;
    int lastMode_;

    DVTimer commitTimer_;
    std::shared_ptr<nvs::NVSHandle> storageHandle_;
    
    void onRequestWifiSettingsMessage_(DVTask* origin, RequestWifiSettingsMessage* message);
    void onSetWifiSettingsMessage_(DVTask* origin, SetWifiSettingsMessage* message);
    
    void onRequestRadioSettingsMessage_(DVTask* origin, RequestRadioSettingsMessage* message);
    void onSetRadioSettingsMessage_(DVTask* origin, SetRadioSettingsMessage* message);

    void onRequestVoiceKeyerSettingsMessage_(DVTask* origin, RequestVoiceKeyerSettingsMessage* message);
    void onSetVoiceKeyerSettingsMessage_(DVTask* origin, SetVoiceKeyerSettingsMessage* message);
    
    void onRequestReportingSettingsMessage_(DVTask* origin, RequestReportingSettingsMessage* message);
    void onSetReportingSettingsMessage_(DVTask* origin, SetReportingSettingsMessage* message);

    void onSetLeftChannelVolume_(DVTask* origin, SetLeftChannelVolumeMessage* message);
    void onSetRightChannelVolume_(DVTask* origin, SetRightChannelVolumeMessage* message);
    
    void onRequestLedBrightness_(DVTask* origin, RequestLedBrightnessSettingsMessage* message);
    void onSetLedBrightness_(DVTask* origin, SetLedBrightnessSettingsMessage* message);
    
    void onChangeFreeDVMode_(DVTask* origin, audio::SetFreeDVModeMessage* message);
    
    void onRequestVolumeSettings_(DVTask* origin, RequestVolumeSettingsMessage* message);
    
    void loadAllSettings_();
    void commit_();
    
    void setLeftChannelVolume_(int8_t vol);
    void setRightChannelVolume_(int8_t vol);
    void setWifiSettings_(bool enabled, WifiMode mode, WifiSecurityMode security, int channel, const char* ssid, const char* password, const char* hostname, bool force = false);
    void setRadioSettings_(bool headsetPtt, int timeOutTimer, bool enabled, int type, const char* host, int port, const char* username, const char* password, bool force = false);
    void setVoiceKeyerSettings_(bool enabled, int timesToTransmit, int secondsToWait, bool force = false);
    void setReportingSettings_(const char* callsign, const char* gridSquare, bool forceReporting, uint64_t freqHz, const char* message, bool force = false);
    void setLedBrightness_(int dutyCycle);
    void setLastMode_(int lastMode);

    void initializeVolumes_();
    void initializeWifi_();
    void initializeRadio_();
    void initialzeVoiceKeyer_();
    void initializeReporting_();
    void initializeLedBrightness_();
    void initializeLastMode_();
};

} // namespace storage

} // namespace ezdv


#endif // SETTINGS_TASK_H