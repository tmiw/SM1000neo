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

#include "esp_log.h"
#include "SettingsTask.h"

#define CURRENT_LOG_TAG ("SettingsTask")

#define LEFT_CHAN_VOL_ID ("lfChanVol")
#define RIGHT_CHAN_VOL_ID ("rtChanVol")

#define WIFI_ENABLED_ID ("wifiEn")
#define WIFI_MODE_ID ("wifiMode")
#define WIFI_SECURITY_ID ("wifiSec")
#define WIFI_CHANNEL_ID ("wifiChan")
#define WIFI_SSID_ID ("wifiSsid")
#define WIFI_PASSWORD_ID ("wifiPass")
#define WIFI_HOSTNAME_ID ("wifiHost")

#define HEADSET_PTT_ID ("headPtt")
#define TIME_OUT_TIMER_ID ("tot")
#define DEFAULT_TIME_OUT_TIMER_SEC (120)
#define RADIO_ENABLED_ID ("radioEn")
#define RADIO_TYPE_ID ("radioType")
#define RADIO_HOSTNAME_ID ("radioHost")
#define RADIO_PORT_ID ("radioPort")
#define RADIO_USERNAME_ID ("radioUser")
#define RADIO_PASSWORD_ID ("radioPass")

#define VOICE_KEYER_ENABLED_ID ("vkEnable")
#define VOICE_KEYER_TIMES_TO_TRANSMIT ("vkTimesTX")
#define VOICE_KEYER_SECONDS_TO_WAIT_AFTER_TRANSMIT ("vkSecWait")

#define REPORTING_CALLSIGN_ID ("repCall")
#define REPORTING_GRID_SQUARE_ID ("repGrid")
#define REPORTING_FORCE_ID ("repForce")
#define REPORTING_FREQ_ID ("repFreq")
#define REPORTING_MSG_ID ("repMsg")

#define LED_DUTY_CYCLE_ID ("ledDtyCyc")

#define DEFAULT_WIFI_ENABLED (false)
#define DEFAULT_WIFI_MODE (WifiMode::ACCESS_POINT)
#define DEFAULT_WIFI_SECURITY (WifiSecurityMode::NONE)
#define DEFAULT_WIFI_CHANNEL (1)
#define DEFAULT_WIFI_SSID ("")
#define DEFAULT_WIFI_PASSWORD ("")
#define DEFAULT_WIFI_HOSTNAME ("ezdv")

#define DEFAULT_VOICE_KEYER_ENABLE (false)
#define DEFAULT_VOICE_KEYER_TIMES_TO_TRANSMIT (10)
#define DEFAULT_VOICE_KEYER_SECONDS_TO_WAIT (5)

#define DEFAULT_REPORTING_CALLSIGN ("")
#define DEFAULT_REPORTING_GRID_SQUARE ("UN00KN")
#define DEFAULT_REPORTING_FORCE (false)
#define DEFAULT_REPORTING_FREQ (14236000)
#define DEFAULT_REPORTING_MSG ("")

#define DEFAULT_RADIO_ENABLED (false)
#define DEFAULT_RADIO_HEADSET_PTT_ENABLED (false)
#define DEFAULT_RADIO_PORT (50001)
#define DEFAULT_RADIO_TYPE (0)
#define DEFAULT_RADIO_USERNAME ("")
#define DEFAULT_RADIO_PASSWORD ("")
#define DEFAULT_RADIO_HOSTNAME ("")

#define DEFAULT_LED_DUTY_CYCLE (8192)

#define LAST_MODE_ID ("lastMode")
#define DEFAULT_LAST_MODE (1) /* Default to 700D */

namespace ezdv
{

namespace storage
{

SettingsTask::SettingsTask()
    : DVTask("SettingsTask", 2, 4096, tskNO_AFFINITY, 32)
    , leftChannelVolume_(0)
    , rightChannelVolume_(0)
    , wifiEnabled_(false)
    , wifiMode_(WifiMode::ACCESS_POINT)
    , wifiSecurity_(WifiSecurityMode::NONE)
    , headsetPtt_(false)
    , timeOutTimer_(0)
    , radioEnabled_(false)
    , radioType_(0)
    , radioPort_(0)
    , forceReporting_(false)
    , freqHz_(0)
    , enableVoiceKeyer_(false)
    , voiceKeyerNumberTimesToTransmit_(0)
    , voiceKeyerSecondsToWaitAfterTransmit_(0)
    , ledDutyCycle_(0)
    , lastMode_(0)
    , commitTimer_(this, [this](DVTimer*) { commit_(); }, 1000000, "SettingsCommitTimer")
{
    memset(wifiSsid_, 0, WifiSettingsMessage::MAX_STR_SIZE);
    memset(wifiPassword_, 0, WifiSettingsMessage::MAX_STR_SIZE);
    memset(wifiHostname_, 0, WifiSettingsMessage::MAX_STR_SIZE);
    
    memset(radioHostname_, 0, RadioSettingsMessage::MAX_STR_SIZE);
    memset(radioUsername_, 0, RadioSettingsMessage::MAX_STR_SIZE);
    memset(radioPassword_, 0, RadioSettingsMessage::MAX_STR_SIZE);
    
    memset(callsign_, 0, ReportingSettingsMessage::MAX_STR_SIZE);

    // Subscribe to messages
    registerMessageHandler(this, &SettingsTask::onSetLeftChannelVolume_);
    registerMessageHandler(this, &SettingsTask::onSetRightChannelVolume_);
    registerMessageHandler(this, &SettingsTask::onRequestWifiSettingsMessage_);
    registerMessageHandler(this, &SettingsTask::onSetWifiSettingsMessage_);
    registerMessageHandler(this, &SettingsTask::onRequestRadioSettingsMessage_);
    registerMessageHandler(this, &SettingsTask::onSetRadioSettingsMessage_);
    registerMessageHandler(this, &SettingsTask::onRequestVoiceKeyerSettingsMessage_);
    registerMessageHandler(this, &SettingsTask::onSetVoiceKeyerSettingsMessage_);
    registerMessageHandler(this, &SettingsTask::onRequestReportingSettingsMessage_);
    registerMessageHandler(this, &SettingsTask::onSetReportingSettingsMessage_);
    registerMessageHandler(this, &SettingsTask::onRequestLedBrightness_);
    registerMessageHandler(this, &SettingsTask::onSetLedBrightness_);
    registerMessageHandler(this, &SettingsTask::onChangeFreeDVMode_);
    registerMessageHandler(this, &SettingsTask::onRequestVolumeSettings_);
}

void SettingsTask::onTaskStart_()
{
    // Initialize NVS
    ESP_LOGI(CURRENT_LOG_TAG, "Initializing NVS.");
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(CURRENT_LOG_TAG, "erasing NVS");
        
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK( err );
    
    // Open NVS handle.
    ESP_LOGI(CURRENT_LOG_TAG, "Opening NVS handle.");
    esp_err_t result;
    storageHandle_ = nvs::open_nvs_handle("storage", NVS_READWRITE, &result);
    if (result != ESP_OK)
    {
        ESP_LOGE(CURRENT_LOG_TAG, "error opening NVS handle: %s", esp_err_to_name(result));
        ESP_LOGW(CURRENT_LOG_TAG, "settings will not be saved.");
        storageHandle_ = nullptr;
    }
    
    loadAllSettings_();
}

void SettingsTask::onTaskSleep_()
{
    // none
}

void SettingsTask::onRequestWifiSettingsMessage_(DVTask* origin, RequestWifiSettingsMessage* message)
{
    // Publish current Wi-Fi settings to everyone who may care.
    WifiSettingsMessage* response = new WifiSettingsMessage(
        wifiEnabled_,
        wifiMode_,
        wifiSecurity_,
        wifiChannel_,
        wifiSsid_,
        wifiPassword_,
        wifiHostname_
    );
    assert(response != nullptr);
    if (origin != nullptr)
    {
        origin->post(response);
    }
    delete response;
}

void SettingsTask::onRequestRadioSettingsMessage_(DVTask* origin, RequestRadioSettingsMessage* message)
{
    // Publish current radio settings to everyone who may care.
    RadioSettingsMessage* response = new RadioSettingsMessage(
        headsetPtt_,
        timeOutTimer_,
        radioEnabled_,
        radioType_,
        radioHostname_,
        radioPort_,
        radioUsername_,
        radioPassword_
    );
    assert(response != nullptr);
    if (origin != nullptr)
    {
        origin->post(response);
    }
    delete response;
}

void SettingsTask::onRequestVoiceKeyerSettingsMessage_(DVTask* origin, RequestVoiceKeyerSettingsMessage* message)
{
    // Publish current voice keyer settings to everyone who may care.
    VoiceKeyerSettingsMessage* response = new VoiceKeyerSettingsMessage(
        enableVoiceKeyer_,
        voiceKeyerNumberTimesToTransmit_,
        voiceKeyerSecondsToWaitAfterTransmit_
    );
    assert(response != nullptr);
    if (origin != nullptr)
    {
        origin->post(response);
    }
    delete response;
}

void SettingsTask::onRequestReportingSettingsMessage_(DVTask* origin, RequestReportingSettingsMessage* message)
{
    // Publish current reporting settings to everyone who may care.
    ReportingSettingsMessage* response = new ReportingSettingsMessage(
        callsign_,
        gridSquare_,
        forceReporting_,
        freqHz_,
        message_
    );

    assert(response != nullptr);
    if (origin != nullptr)
    {
        origin->post(response);
    }
    delete response;
}

void SettingsTask::onRequestLedBrightness_(DVTask* origin, RequestLedBrightnessSettingsMessage* message)
{
    // Publish current reporting settings to everyone who may care.
    LedBrightnessSettingsMessage* response = new LedBrightnessSettingsMessage(
        ledDutyCycle_
    );

    assert(response != nullptr);
    if (origin != nullptr)
    {
        origin->post(response);
    }
    delete response;
}

void SettingsTask::onSetLeftChannelVolume_(DVTask* origin, SetLeftChannelVolumeMessage* message)
{
    setLeftChannelVolume_(message->volume);
}

void SettingsTask::onSetRightChannelVolume_(DVTask* origin, SetRightChannelVolumeMessage* message)
{
    setRightChannelVolume_(message->volume);
}

void SettingsTask::loadAllSettings_()
{
    if (storageHandle_)
    {
        initializeVolumes_();
        initializeWifi_();
        initializeRadio_();
        initialzeVoiceKeyer_();
        initializeLedBrightness_();
        initializeLastMode_();
        initializeReporting_();
    }
}

void SettingsTask::initializeVolumes_()
{
    esp_err_t result = storageHandle_->get_item(LEFT_CHAN_VOL_ID, leftChannelVolume_);
    if (result == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGW(CURRENT_LOG_TAG, "leftChannelVolume not found, will set to defaults");
        setLeftChannelVolume_(0);
    }
    else if (result != ESP_OK)
    {
        ESP_LOGE(CURRENT_LOG_TAG, "error retrieving leftChannelVolume: %s", esp_err_to_name(result));
    }
    else
    {
        ESP_LOGI(CURRENT_LOG_TAG, "leftChannelVolume: %d", leftChannelVolume_);

        // Broadcast volume so that other components can initialize themselves with it.
        LeftChannelVolumeMessage* message = new LeftChannelVolumeMessage();
        assert(message != nullptr);

        message->volume = leftChannelVolume_;
        publish(message);
        delete message;
    }
    
    result = storageHandle_->get_item(RIGHT_CHAN_VOL_ID, rightChannelVolume_);
    if (result == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGW(CURRENT_LOG_TAG, "rightChannelVolume not found, will set to defaults");
        setRightChannelVolume_(0);
    }
    else if (result != ESP_OK)
    {
        ESP_LOGE(CURRENT_LOG_TAG, "error retrieving rightChannelVolume: %s", esp_err_to_name(result));
    }
    else
    {
        ESP_LOGI(CURRENT_LOG_TAG, "rightChannelVolume: %d", rightChannelVolume_);

        // Broadcast volume so that other components can initialize themselves with it.
        RightChannelVolumeMessage* message = new RightChannelVolumeMessage();
        assert(message != nullptr);

        message->volume = rightChannelVolume_;
        publish(message);
        delete message;
    }
}

void SettingsTask::initializeWifi_()
{
    bool resaveSettings = false;
    esp_err_t result = storageHandle_->get_item(WIFI_ENABLED_ID, wifiEnabled_);
    if (result == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGW(CURRENT_LOG_TAG, "wifiEnabled not found, will set to default");
        wifiEnabled_ = DEFAULT_WIFI_ENABLED;
        resaveSettings = true;
    }
    else if (result != ESP_OK)
    {
        ESP_LOGE(CURRENT_LOG_TAG, "error retrieving wifiEnabled: %s", esp_err_to_name(result));
    }
    else
    {
        ESP_LOGI(CURRENT_LOG_TAG, "wifiEnabled: %d", wifiEnabled_);
    }
    
    result = storageHandle_->get_item(WIFI_MODE_ID, wifiMode_);
    if (result == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGW(CURRENT_LOG_TAG, "wifiMode not found, will set to default");
        wifiMode_ = DEFAULT_WIFI_MODE;
        resaveSettings = true;
    }
    else if (result != ESP_OK)
    {
        ESP_LOGE(CURRENT_LOG_TAG, "error retrieving wifiMode: %s", esp_err_to_name(result));
    }
    else
    {
        ESP_LOGI(CURRENT_LOG_TAG, "wifiMode: %d", wifiMode_);
    }
    
    result = storageHandle_->get_item(WIFI_SECURITY_ID, wifiSecurity_);
    if (result == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGW(CURRENT_LOG_TAG, "wifiSecurity not found, will set to default");
        wifiSecurity_ = DEFAULT_WIFI_SECURITY;
        resaveSettings = true;
    }
    else if (result != ESP_OK)
    {
        ESP_LOGE(CURRENT_LOG_TAG, "error retrieving wifiSecurity: %s", esp_err_to_name(result));
    }
    else
    {
        ESP_LOGI(CURRENT_LOG_TAG, "wifiSecurity: %d", wifiSecurity_);
    }
    
    result = storageHandle_->get_item(WIFI_CHANNEL_ID, wifiChannel_);
    if (result == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGW(CURRENT_LOG_TAG, "wifiChannel not found, will set to default");
        wifiChannel_ = DEFAULT_WIFI_CHANNEL;
        resaveSettings = true;
    }
    else if (result != ESP_OK)
    {
        ESP_LOGE(CURRENT_LOG_TAG, "error retrieving wifiChannel: %s", esp_err_to_name(result));
    }
    else
    {
        ESP_LOGI(CURRENT_LOG_TAG, "wifiChannel: %d", wifiChannel_);
    }
    
    result = storageHandle_->get_string(WIFI_SSID_ID, wifiSsid_, WifiSettingsMessage::MAX_STR_SIZE);
    if (result == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGW(CURRENT_LOG_TAG, "wifiSsid not found, will set to default");
        memset(wifiSsid_, 0, WifiSettingsMessage::MAX_STR_SIZE);
        strncpy(wifiSsid_, DEFAULT_WIFI_SSID, WifiSettingsMessage::MAX_STR_SIZE - 1);
        resaveSettings = true;
    }
    else if (result != ESP_OK)
    {
        ESP_LOGE(CURRENT_LOG_TAG, "error retrieving wifiSsid: %s", esp_err_to_name(result));
    }
    else
    {
        ESP_LOGI(CURRENT_LOG_TAG, "wifiSsid: %s", wifiSsid_);
    }
    
    result = storageHandle_->get_string(WIFI_PASSWORD_ID, wifiPassword_, WifiSettingsMessage::MAX_STR_SIZE);
    if (result == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGW(CURRENT_LOG_TAG, "wifiPassword not found, will set to default");
        memset(wifiPassword_, 0, WifiSettingsMessage::MAX_STR_SIZE);
        strncpy(wifiPassword_, DEFAULT_WIFI_PASSWORD, WifiSettingsMessage::MAX_STR_SIZE - 1);
        resaveSettings = true;
    }
    else if (result != ESP_OK)
    {
        ESP_LOGE(CURRENT_LOG_TAG, "error retrieving wifiPassword: %s", esp_err_to_name(result));
    }
    else
    {
        ESP_LOGI(CURRENT_LOG_TAG, "wifiPassword: ********");
    }

    result = storageHandle_->get_string(WIFI_HOSTNAME_ID, wifiHostname_, WifiSettingsMessage::MAX_STR_SIZE);
    if (result == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGW(CURRENT_LOG_TAG, "wifiHostname not found, will set to default");
        memset(wifiHostname_, 0, WifiSettingsMessage::MAX_STR_SIZE);
        strncpy(wifiHostname_, DEFAULT_WIFI_HOSTNAME, WifiSettingsMessage::MAX_STR_SIZE - 1);
        resaveSettings = true;
    }
    else if (result != ESP_OK)
    {
        ESP_LOGE(CURRENT_LOG_TAG, "error retrieving wifiHostname: %s", esp_err_to_name(result));
    }
    else
    {
        ESP_LOGI(CURRENT_LOG_TAG, "wifiHostname: %s", wifiHostname_);
    }

    if (resaveSettings)
    {
        // setWifiSettings_ below will automatically publish WifiSettingsMessage.
        setWifiSettings_(
            wifiEnabled_, wifiMode_, wifiSecurity_, 
            wifiChannel_, wifiSsid_, wifiPassword_,
            wifiHostname_, true);
    }
    else
    {
        // Publish current Wi-Fi settings to everyone who may care.
        WifiSettingsMessage* message = new WifiSettingsMessage(
            wifiEnabled_,
            wifiMode_,
            wifiSecurity_,
            wifiChannel_,
            wifiSsid_,
            wifiPassword_,
            wifiHostname_
        );
        assert(message != nullptr);
        publish(message);
        delete message;
    }
}

void SettingsTask::initializeRadio_()
{
    bool resaveSettings = false;

    esp_err_t result = storageHandle_->get_item(RADIO_ENABLED_ID, radioEnabled_);
    if (result == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGW(CURRENT_LOG_TAG, "radioEnabled not found, will set to default");
        radioEnabled_ = DEFAULT_RADIO_ENABLED;
        resaveSettings = true;
    }
    else if (result != ESP_OK)
    {
        ESP_LOGE(CURRENT_LOG_TAG, "error retrieving radioEnabled: %s", esp_err_to_name(result));
    }
    else
    {
        ESP_LOGI(CURRENT_LOG_TAG, "radioEnabled: %d", radioEnabled_);
    }

    result = storageHandle_->get_item(HEADSET_PTT_ID, headsetPtt_);
    if (result == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGW(CURRENT_LOG_TAG, "headsetPtt not found, will set to default");
        headsetPtt_ = DEFAULT_RADIO_HEADSET_PTT_ENABLED;
        resaveSettings = true;
    }
    else if (result != ESP_OK)
    {
        ESP_LOGE(CURRENT_LOG_TAG, "error retrieving headsetPtt: %s", esp_err_to_name(result));
    }
    else
    {
        ESP_LOGI(CURRENT_LOG_TAG, "headsetPtt: %d", headsetPtt_);
    }

    result = storageHandle_->get_item(TIME_OUT_TIMER_ID, timeOutTimer_);
    if (result == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGW(CURRENT_LOG_TAG, "timeOutTimer not found, will set to default");
        timeOutTimer_ = DEFAULT_TIME_OUT_TIMER_SEC;
        resaveSettings = true;
    }
    else if (result != ESP_OK)
    {
        ESP_LOGE(CURRENT_LOG_TAG, "error retrieving timeOutTimer: %s", esp_err_to_name(result));
    }
    else
    {
        ESP_LOGI(CURRENT_LOG_TAG, "timeOutTimer: %d", timeOutTimer_);
    }
    
    result = storageHandle_->get_item(RADIO_TYPE_ID, radioType_);
    if (result == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGW(CURRENT_LOG_TAG, "radioType not found, will set to default");
        radioType_ = DEFAULT_RADIO_TYPE;
        resaveSettings = true;
    }
    else if (result != ESP_OK)
    {
        ESP_LOGE(CURRENT_LOG_TAG, "error retrieving radioType: %s", esp_err_to_name(result));
    }
    else
    {
        ESP_LOGI(CURRENT_LOG_TAG, "radioType: %d", radioType_);
    }
    
    result = storageHandle_->get_string(RADIO_HOSTNAME_ID, radioHostname_, RadioSettingsMessage::MAX_STR_SIZE);
    if (result == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGW(CURRENT_LOG_TAG, "radioHostname not found, will set to default");
        memset(radioHostname_, 0, RadioSettingsMessage::MAX_STR_SIZE);
        strncpy(radioHostname_, DEFAULT_RADIO_HOSTNAME, RadioSettingsMessage::MAX_STR_SIZE - 1);
        resaveSettings = true;
    }
    else if (result != ESP_OK)
    {
        ESP_LOGE(CURRENT_LOG_TAG, "error retrieving radioHostname: %s", esp_err_to_name(result));
    }
    else
    {
        ESP_LOGI(CURRENT_LOG_TAG, "radioHostname: %s", radioHostname_);
    }
    
    result = storageHandle_->get_item(RADIO_PORT_ID, radioPort_);
    if (result == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGW(CURRENT_LOG_TAG, "radioPort not found, will set to default");
        radioPort_ = DEFAULT_RADIO_PORT;
        resaveSettings = true;
    }
    else if (result != ESP_OK)
    {
        ESP_LOGE(CURRENT_LOG_TAG, "error retrieving radioPort: %s", esp_err_to_name(result));
    }
    else
    {
        ESP_LOGI(CURRENT_LOG_TAG, "radioPort: %d", radioPort_);
    }

    if (radioPort_ == 0)
    {
        // We shouldn't use 0 for the default port as most IC-705s
        // will default to 50001.
        radioPort_ = DEFAULT_RADIO_PORT;
        resaveSettings = true;
    }
    
    result = storageHandle_->get_string(RADIO_USERNAME_ID, radioUsername_, RadioSettingsMessage::MAX_STR_SIZE);
    if (result == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGW(CURRENT_LOG_TAG, "radioUsername not found, will set to default");
        memset(radioUsername_, 0, RadioSettingsMessage::MAX_STR_SIZE);
        strncpy(radioUsername_, DEFAULT_RADIO_USERNAME, RadioSettingsMessage::MAX_STR_SIZE - 1);
        resaveSettings = true;
    }
    else if (result != ESP_OK)
    {
        ESP_LOGE(CURRENT_LOG_TAG, "error retrieving radioUsername: %s", esp_err_to_name(result));
    }
    else
    {
        ESP_LOGI(CURRENT_LOG_TAG, "radioUsername: %s", radioUsername_);
    }
    
    result = storageHandle_->get_string(RADIO_PASSWORD_ID, radioPassword_, RadioSettingsMessage::MAX_STR_SIZE);
    if (result == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGW(CURRENT_LOG_TAG, "radioPassword_ not found, will set to default");
        memset(radioPassword_, 0, RadioSettingsMessage::MAX_STR_SIZE);
        strncpy(radioPassword_, DEFAULT_RADIO_PASSWORD, RadioSettingsMessage::MAX_STR_SIZE - 1);
        resaveSettings = true;
    }
    else if (result != ESP_OK)
    {
        ESP_LOGE(CURRENT_LOG_TAG, "error retrieving radioPassword: %s", esp_err_to_name(result));
    }
    else
    {
        ESP_LOGI(CURRENT_LOG_TAG, "radioPassword: ********");
    }

    if (resaveSettings)
    {
        // setRadioSettings_ sends RadioSettingsMessage on completion.
        setRadioSettings_(
            headsetPtt_, timeOutTimer_, radioEnabled_, radioType_, 
            radioHostname_, radioPort_, radioUsername_, radioPassword_,
            true);
    }
    else
    {
        // Publish current Wi-Fi settings to everyone who may care.
        RadioSettingsMessage* message = new RadioSettingsMessage(
            headsetPtt_,
            timeOutTimer_,
            radioEnabled_,
            radioType_,
            radioHostname_,
            radioPort_,
            radioUsername_,
            radioPassword_
        );
        assert(message != nullptr);
        publish(message);
        delete message;
    }
}

void SettingsTask::initialzeVoiceKeyer_()
{
    bool resaveSettings = false;

    esp_err_t result = storageHandle_->get_item(VOICE_KEYER_ENABLED_ID, enableVoiceKeyer_);
    if (result == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGW(CURRENT_LOG_TAG, "enableVoiceKeyer not found, will set to default");
        enableVoiceKeyer_ = DEFAULT_VOICE_KEYER_ENABLE;
        resaveSettings = true;
    }
    else if (result != ESP_OK)
    {
        ESP_LOGE(CURRENT_LOG_TAG, "error retrieving enableVoiceKeyer: %s", esp_err_to_name(result));
    }
    else
    {
        ESP_LOGI(CURRENT_LOG_TAG, "enableVoiceKeyer: %d", enableVoiceKeyer_);
    }
    
    result = storageHandle_->get_item(VOICE_KEYER_TIMES_TO_TRANSMIT, voiceKeyerNumberTimesToTransmit_);
    if (result == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGW(CURRENT_LOG_TAG, "voiceKeyerNumberTimesToTransmit not found, will set to default");
        voiceKeyerNumberTimesToTransmit_ = DEFAULT_VOICE_KEYER_TIMES_TO_TRANSMIT;
        resaveSettings = true;
    }
    else if (result != ESP_OK)
    {
        ESP_LOGE(CURRENT_LOG_TAG, "error retrieving voiceKeyerNumberTimesToTransmit: %s", esp_err_to_name(result));
    }
    else
    {
        ESP_LOGI(CURRENT_LOG_TAG, "voiceKeyerNumberTimesToTransmit: %d", voiceKeyerNumberTimesToTransmit_);
    }

    result = storageHandle_->get_item(VOICE_KEYER_SECONDS_TO_WAIT_AFTER_TRANSMIT, voiceKeyerSecondsToWaitAfterTransmit_);
    if (result == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGW(CURRENT_LOG_TAG, "voiceKeyerSecondsToWaitAfterTransmit_ not found, will set to default");
        voiceKeyerSecondsToWaitAfterTransmit_ = DEFAULT_VOICE_KEYER_SECONDS_TO_WAIT;
        resaveSettings = true;
    }
    else if (result != ESP_OK)
    {
        ESP_LOGE(CURRENT_LOG_TAG, "error retrieving voiceKeyerSecondsToWaitAfterTransmit: %s", esp_err_to_name(result));
    }
    else
    {
        ESP_LOGI(CURRENT_LOG_TAG, "voiceKeyerSecondsToWaitAfterTransmit: %d", voiceKeyerSecondsToWaitAfterTransmit_);
    }
    
    if (resaveSettings)
    {
        // setVoiceKeyerSettings_ will broadcast VoiceKeyerSettingsMessage on completion.
        setVoiceKeyerSettings_(
            enableVoiceKeyer_, voiceKeyerNumberTimesToTransmit_, 
            voiceKeyerSecondsToWaitAfterTransmit_, true);
    }
    else
    {
        // Publish current voice keyer settings to everyone who may care.
        VoiceKeyerSettingsMessage* message = new VoiceKeyerSettingsMessage(
            enableVoiceKeyer_,
            voiceKeyerNumberTimesToTransmit_,
            voiceKeyerSecondsToWaitAfterTransmit_
        );
        assert(message != nullptr);
        publish(message);
        delete message;
    }
}

void SettingsTask::initializeReporting_()
{
    bool resaveSettings = false;

    esp_err_t result = storageHandle_->get_string(REPORTING_CALLSIGN_ID, callsign_, ReportingSettingsMessage::MAX_STR_SIZE);
    if (result == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGW(CURRENT_LOG_TAG, "callsign not found, will set to default");
        memset(callsign_, 0, ReportingSettingsMessage::MAX_STR_SIZE);
        strncpy(callsign_, DEFAULT_REPORTING_CALLSIGN, ReportingSettingsMessage::MAX_STR_SIZE - 1);
        resaveSettings = true;
    }
    else if (result != ESP_OK)
    {
        ESP_LOGE(CURRENT_LOG_TAG, "error retrieving callsign: %s", esp_err_to_name(result));
    }
    else
    {
        ESP_LOGI(CURRENT_LOG_TAG, "callsign: %s", callsign_);
    }
    
    result = storageHandle_->get_string(REPORTING_GRID_SQUARE_ID, gridSquare_, ReportingSettingsMessage::MAX_STR_SIZE);
    if (result == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGW(CURRENT_LOG_TAG, "gridSquare not found, will set to default");
        memset(gridSquare_, 0, ReportingSettingsMessage::MAX_STR_SIZE);
        strncpy(gridSquare_, DEFAULT_REPORTING_GRID_SQUARE, ReportingSettingsMessage::MAX_STR_SIZE - 1);
        resaveSettings = true;
    }
    else if (result != ESP_OK)
    {
        ESP_LOGE(CURRENT_LOG_TAG, "error retrieving grid square: %s", esp_err_to_name(result));
    }
    else
    {
        ESP_LOGI(CURRENT_LOG_TAG, "gridSquare: %s", gridSquare_);
    }
    
    result = storageHandle_->get_item(REPORTING_FORCE_ID, forceReporting_);
    if (result == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGW(CURRENT_LOG_TAG, "forceReporting not found, will set to default");
        forceReporting_ = DEFAULT_REPORTING_FORCE;
        resaveSettings = true;
    }
    else if (result != ESP_OK)
    {
        ESP_LOGE(CURRENT_LOG_TAG, "error retrieving force reporting value: %s", esp_err_to_name(result));
    }
    else
    {
        ESP_LOGI(CURRENT_LOG_TAG, "forceReporting: %d", forceReporting_);
    }
    
    result = storageHandle_->get_item(REPORTING_FREQ_ID, freqHz_);
    if (result == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGW(CURRENT_LOG_TAG, "freqHz not found, will set to default");
        freqHz_ = DEFAULT_REPORTING_FREQ;
        resaveSettings = true;
    }
    else if (result != ESP_OK)
    {
        ESP_LOGE(CURRENT_LOG_TAG, "error retrieving reporting frequency: %s", esp_err_to_name(result));
    }
    else
    {
        ESP_LOGI(CURRENT_LOG_TAG, "freqHz: %" PRIu64, freqHz_);
    }

    result = storageHandle_->get_string(REPORTING_MSG_ID, message_, ReportingSettingsMessage::MAX_MSG_SIZE);
    if (result == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGW(CURRENT_LOG_TAG, "message not found, will set to default");
        memset(message_, 0, ReportingSettingsMessage::MAX_MSG_SIZE);
        strncpy(message_, DEFAULT_REPORTING_MSG, ReportingSettingsMessage::MAX_MSG_SIZE - 1);
        resaveSettings = true;
    }
    else if (result != ESP_OK)
    {
        ESP_LOGE(CURRENT_LOG_TAG, "error retrieving reporting message: %s", esp_err_to_name(result));
    }
    else
    {
        ESP_LOGI(CURRENT_LOG_TAG, "reportMsg: %s", message_);
    }
    
    if (resaveSettings)
    {
        // setReportingSettings_ will broadcast ReportingSettingsMessage when done.
        setReportingSettings_(
            callsign_, gridSquare_, forceReporting_, freqHz_, message_, true
        );
    }
    else
    {
        // Publish current reporting settings to everyone who may care.
        ReportingSettingsMessage* message = new ReportingSettingsMessage(
            callsign_,
            gridSquare_,
            forceReporting_,
            freqHz_,
            message_
        );
        assert(message != nullptr);
        publish(message);
        delete message;
    }
}

void SettingsTask::initializeLedBrightness_()
{
    esp_err_t result = storageHandle_->get_item(LED_DUTY_CYCLE_ID, ledDutyCycle_);
    if (result == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGW(CURRENT_LOG_TAG, "LED brightness settings not found, will set to defaults");
        setLedBrightness_(DEFAULT_LED_DUTY_CYCLE);
    }
    else if (result != ESP_OK)
    {
        ESP_LOGE(CURRENT_LOG_TAG, "error retrieving ledDutyCycle: %s", esp_err_to_name(result));
    }
    else
    {
        ESP_LOGI(CURRENT_LOG_TAG, "ledDutyCycle: %d", ledDutyCycle_);
    }
}

void SettingsTask::initializeLastMode_()
{
    esp_err_t result = storageHandle_->get_item(LAST_MODE_ID, lastMode_);
    if (result == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGW(CURRENT_LOG_TAG, "Last mode not found, will set to defaults");
        setLastMode_(DEFAULT_LAST_MODE);
    }
    else if (result != ESP_OK)
    {
        ESP_LOGE(CURRENT_LOG_TAG, "error retrieving lastMode: %s", esp_err_to_name(result));
    }
    else
    {
        ESP_LOGI(CURRENT_LOG_TAG, "lastMode: %d", lastMode_);
    }
    
    // Request mode change to previous mode
    audio::RequestSetFreeDVModeMessage reqMsg((audio::FreeDVMode)lastMode_);
    publish(&reqMsg);
}

void SettingsTask::commit_()
{
    ESP_LOGI(CURRENT_LOG_TAG, "Committing pending settings to flash.");
        
    esp_err_t err = storageHandle_->commit();
    if (err != ESP_OK)
    {
        ESP_LOGE(CURRENT_LOG_TAG, "error committing settings: %s", esp_err_to_name(err));
    }
}

void SettingsTask::setLeftChannelVolume_(int8_t vol)
{
    if (vol <= -127) vol = -127;
    else if (vol >= 48) vol = 48;
    
    bool valueChanged = vol != leftChannelVolume_;
    leftChannelVolume_ = vol;
    
    if (storageHandle_)
    {
        if (valueChanged)
        {
            esp_err_t result = storageHandle_->set_item(LEFT_CHAN_VOL_ID, vol);
            if (result != ESP_OK)
            {
                ESP_LOGE(CURRENT_LOG_TAG, "error setting leftChannelVolume: %s", esp_err_to_name(result));
            }
            else
            {
                commitTimer_.stop();
                commitTimer_.start(true);
            }
        }

        // Publish new volume setting to everyone who may care.
        LeftChannelVolumeMessage* message = new LeftChannelVolumeMessage();
        assert(message != nullptr);

        message->volume = vol;
        publish(message);
        delete message;
    }
}

void SettingsTask::setRightChannelVolume_(int8_t vol)
{
    if (vol <= -127) vol = -127;
    else if (vol >= 48) vol = 48;
    
    bool valueChanged = vol != rightChannelVolume_;
    rightChannelVolume_ = vol;
    
    if (storageHandle_)
    {
        if (valueChanged)
        {
            esp_err_t result = storageHandle_->set_item(RIGHT_CHAN_VOL_ID, vol);
            if (result != ESP_OK)
            {
                ESP_LOGE(CURRENT_LOG_TAG, "error setting rightChannelVolume: %s", esp_err_to_name(result));
            }
            commitTimer_.stop();
            commitTimer_.start(true);
        }

        // Publish new volume setting to everyone who may care.
        RightChannelVolumeMessage* message = new RightChannelVolumeMessage();
        assert(message != nullptr);

        message->volume = vol;
        publish(message);
        delete message;
    }
}

void SettingsTask::onSetWifiSettingsMessage_(DVTask* origin, SetWifiSettingsMessage* message)
{
    setWifiSettings_(
        message->enabled, message->mode, message->security, message->channel, message->ssid, 
        message->password, message->hostname);
}

void SettingsTask::setWifiSettings_(
    bool enabled, WifiMode mode, WifiSecurityMode security, int channel, const char* ssid,
    const char* password, const char* hostname, bool force)
{
    ESP_LOGI(CURRENT_LOG_TAG, "Saving Wi-Fi settings");

    bool valuesChanged =
        force ||
        wifiEnabled_ != enabled ||
        wifiMode_ != mode ||
        wifiSecurity_ != security ||
        wifiChannel_ != channel ||
        (strcmp(wifiSsid_, ssid) != 0) ||
        (strcmp(wifiPassword_, password) != 0) ||
        (strcmp(wifiHostname_, hostname) != 0);

    wifiEnabled_ = enabled;
    wifiMode_ = mode;
    wifiSecurity_ = security;
    wifiChannel_ = channel;
    
    memset(wifiSsid_, 0, WifiSettingsMessage::MAX_STR_SIZE);
    memset(wifiPassword_, 0, WifiSettingsMessage::MAX_STR_SIZE);
    memset(wifiHostname_, 0, WifiSettingsMessage::MAX_STR_SIZE);
    
    strncpy(wifiSsid_, ssid, WifiSettingsMessage::MAX_STR_SIZE - 1);
    strncpy(wifiPassword_, password, WifiSettingsMessage::MAX_STR_SIZE - 1);
    strncpy(wifiHostname_, hostname, WifiSettingsMessage::MAX_STR_SIZE - 1);

    if (storageHandle_)
    {
        if (valuesChanged)
        {
            esp_err_t result = storageHandle_->set_item(WIFI_ENABLED_ID, wifiEnabled_);
            if (result != ESP_OK)
            {
                ESP_LOGE(CURRENT_LOG_TAG, "error setting wifiEnabled: %s", esp_err_to_name(result));
            }
            result = storageHandle_->set_item(WIFI_MODE_ID, wifiMode_);
            if (result != ESP_OK)
            {
                ESP_LOGE(CURRENT_LOG_TAG, "error setting wifiMode: %s", esp_err_to_name(result));
            }
            result = storageHandle_->set_item(WIFI_SECURITY_ID, wifiSecurity_);
            if (result != ESP_OK)
            {
                ESP_LOGE(CURRENT_LOG_TAG, "error setting wifiSecurity: %s", esp_err_to_name(result));
            }
            result = storageHandle_->set_item(WIFI_CHANNEL_ID, wifiChannel_);
            if (result != ESP_OK)
            {
                ESP_LOGE(CURRENT_LOG_TAG, "error setting wifiChannel: %s", esp_err_to_name(result));
            }
            result = storageHandle_->set_string(WIFI_SSID_ID, wifiSsid_);
            if (result != ESP_OK)
            {
                ESP_LOGE(CURRENT_LOG_TAG, "error setting wifiSsid: %s", esp_err_to_name(result));
            }
            result = storageHandle_->set_string(WIFI_PASSWORD_ID, wifiPassword_);
            if (result != ESP_OK)
            {
                ESP_LOGE(CURRENT_LOG_TAG, "error setting wifPassword: %s", esp_err_to_name(result));
            }
            result = storageHandle_->set_string(WIFI_HOSTNAME_ID, wifiHostname_);
            if (result != ESP_OK)
            {
                ESP_LOGE(CURRENT_LOG_TAG, "error setting wifiHostname: %s", esp_err_to_name(result));
            }
                
            commitTimer_.stop();
            commitTimer_.start(true);
        }

        // Publish new Wi-Fi settings to everyone who may care.
        WifiSettingsMessage* message = new WifiSettingsMessage(
            wifiEnabled_,
            wifiMode_,
            wifiSecurity_,
            wifiChannel_,
            wifiSsid_,
            wifiPassword_,
            wifiHostname_
        );
        assert(message != nullptr);
        publish(message);
        delete message;
        
        WifiSettingsSavedMessage response;
        publish(&response);
    }
}

void SettingsTask::onSetRadioSettingsMessage_(DVTask* origin, SetRadioSettingsMessage* message)
{
    setRadioSettings_(message->headsetPtt, message->timeOutTimer, message->enabled, message->type, message->host, message->port, message->username, message->password);
}

void SettingsTask::setRadioSettings_(
    bool headsetPtt, int timeOutTimer, bool enabled, int type, 
    const char* host, int port, 
    const char* username, const char* password, bool force)
{
    bool valuesChanged = 
        force ||
        headsetPtt_ != headsetPtt ||
        timeOutTimer_ != timeOutTimer ||
        radioEnabled_ != enabled ||
        radioPort_ != port ||
        radioType_ != type ||
        (strcmp(radioHostname_, host) != 0) ||
        (strcmp(radioUsername_, username) != 0) ||
        (strcmp(radioPassword_, password) != 0);

    headsetPtt_ = headsetPtt;
    timeOutTimer_ = timeOutTimer;
    radioEnabled_ = enabled;
    radioPort_ = port;
    radioType_ = type;
    
    memset(radioHostname_, 0, RadioSettingsMessage::MAX_STR_SIZE);
    memset(radioUsername_, 0, RadioSettingsMessage::MAX_STR_SIZE);
    memset(radioPassword_, 0, RadioSettingsMessage::MAX_STR_SIZE);
    
    strncpy(radioHostname_, host, RadioSettingsMessage::MAX_STR_SIZE - 1);
    strncpy(radioUsername_, username, RadioSettingsMessage::MAX_STR_SIZE - 1);
    strncpy(radioPassword_, password, RadioSettingsMessage::MAX_STR_SIZE - 1);
    
    if (storageHandle_)
    {
        if (valuesChanged)
        {
            esp_err_t result = storageHandle_->set_item(HEADSET_PTT_ID, headsetPtt_);
            if (result != ESP_OK)
            {
                ESP_LOGE(CURRENT_LOG_TAG, "error setting headsetPtt: %s", esp_err_to_name(result));
            }
            result = storageHandle_->set_item(TIME_OUT_TIMER_ID, timeOutTimer_);
            if (result != ESP_OK)
            {
                ESP_LOGE(CURRENT_LOG_TAG, "error setting timeOutTimer: %s", esp_err_to_name(result));
            }
            result = storageHandle_->set_item(RADIO_ENABLED_ID, radioEnabled_);
            if (result != ESP_OK)
            {
                ESP_LOGE(CURRENT_LOG_TAG, "error setting radioEnabled: %s", esp_err_to_name(result));
            }
            result = storageHandle_->set_item(RADIO_TYPE_ID, radioType_);
            if (result != ESP_OK)
            {
                ESP_LOGE(CURRENT_LOG_TAG, "error setting radioType: %s", esp_err_to_name(result));
            }
            result = storageHandle_->set_string(RADIO_HOSTNAME_ID, radioHostname_);
            if (result != ESP_OK)
            {
                ESP_LOGE(CURRENT_LOG_TAG, "error setting radioHostname: %s", esp_err_to_name(result));
            }
            result = storageHandle_->set_item(RADIO_PORT_ID, radioPort_);
            if (result != ESP_OK)
            {
                ESP_LOGE(CURRENT_LOG_TAG, "error setting radioPort: %s", esp_err_to_name(result));
            }
            result = storageHandle_->set_string(RADIO_USERNAME_ID, radioUsername_);
            if (result != ESP_OK)
            {
                ESP_LOGE(CURRENT_LOG_TAG, "error setting radioUsername: %s", esp_err_to_name(result));
            }
            result = storageHandle_->set_string(RADIO_PASSWORD_ID, radioPassword_);
            if (result != ESP_OK)
            {
                ESP_LOGE(CURRENT_LOG_TAG, "error setting radioPassword: %s", esp_err_to_name(result));
            }
                
            commitTimer_.stop();
            commitTimer_.start(true);
        }

        // Publish new Wi-Fi settings to everyone who may care.
        RadioSettingsMessage* message = new RadioSettingsMessage(
            headsetPtt_,
            timeOutTimer_,
            radioEnabled_,
            radioType_,
            radioHostname_,
            radioPort_,
            radioUsername_,
            radioPassword_
        );
        assert(message != nullptr);
        publish(message);
        delete message;
        
        RadioSettingsSavedMessage response;
        publish(&response);
    }
}

void SettingsTask::onSetVoiceKeyerSettingsMessage_(DVTask* origin, SetVoiceKeyerSettingsMessage* message)
{
    // Minor wait in case someone else wants to wait for our response.
    // XXX -- this shouldn't be needed.
    vTaskDelay(pdMS_TO_TICKS(50));
    
    setVoiceKeyerSettings_(message->enabled, message->timesToTransmit, message->secondsToWait);
}

void SettingsTask::setVoiceKeyerSettings_(bool enabled, int timesToTransmit, int secondsToWait, bool force)
{
    bool valuesChanged =
        force ||
        enableVoiceKeyer_ != enabled ||
        voiceKeyerNumberTimesToTransmit_ != timesToTransmit ||
        voiceKeyerSecondsToWaitAfterTransmit_ != secondsToWait;

    enableVoiceKeyer_ = enabled;
    voiceKeyerNumberTimesToTransmit_ = timesToTransmit;
    voiceKeyerSecondsToWaitAfterTransmit_ = secondsToWait;
    
    if (storageHandle_)
    {
        if (valuesChanged)
        {
            esp_err_t result = storageHandle_->set_item(VOICE_KEYER_ENABLED_ID, enableVoiceKeyer_);
            if (result != ESP_OK)
            {
                ESP_LOGE(CURRENT_LOG_TAG, "error setting enableVoiceKeyer: %s", esp_err_to_name(result));
            }
            result = storageHandle_->set_item(VOICE_KEYER_TIMES_TO_TRANSMIT, voiceKeyerNumberTimesToTransmit_);
            if (result != ESP_OK)
            {
                ESP_LOGE(CURRENT_LOG_TAG, "error setting voiceKeyerNumberTimesToTransmit: %s", esp_err_to_name(result));
            }
            result = storageHandle_->set_item(VOICE_KEYER_SECONDS_TO_WAIT_AFTER_TRANSMIT, voiceKeyerSecondsToWaitAfterTransmit_);
            if (result != ESP_OK)
            {
                ESP_LOGE(CURRENT_LOG_TAG, "error setting voiceKeyerSecondsToWaitAfterTransmit: %s", esp_err_to_name(result));
            }

            commitTimer_.stop();
            commitTimer_.start(true);
        }

        // Publish new voice keyer settings to everyone who may care.
        VoiceKeyerSettingsMessage* message = new VoiceKeyerSettingsMessage(
            enableVoiceKeyer_,
            voiceKeyerNumberTimesToTransmit_,
            voiceKeyerSecondsToWaitAfterTransmit_
        );
        assert(message != nullptr);
        publish(message);
        delete message;
        
        VoiceKeyerSettingsSavedMessage response;
        publish(&response);
    }
}

void SettingsTask::onSetReportingSettingsMessage_(DVTask* origin, SetReportingSettingsMessage* message)
{
    setReportingSettings_(message->callsign, message->gridSquare, message->forceReporting, message->freqHz, message->message);
}

void SettingsTask::setReportingSettings_(
    const char* callsign, const char* gridSquare, bool forceReporting, uint64_t freqHz, const char* message, bool force)
{
    bool valuesChanged = 
        force ||
        (strcmp(callsign_, callsign) != 0) ||
        (strcmp(gridSquare_, gridSquare) != 0) ||
        (strcmp(message_, message) != 0) ||
        forceReporting_ != forceReporting ||
        freqHz_ != freqHz;

    memset(callsign_, 0, ReportingSettingsMessage::MAX_STR_SIZE);    
    strncpy(callsign_, callsign, ReportingSettingsMessage::MAX_STR_SIZE - 1);
    
    memset(gridSquare_, 0, ReportingSettingsMessage::MAX_STR_SIZE);    
    strncpy(gridSquare_, gridSquare, ReportingSettingsMessage::MAX_STR_SIZE - 1);
    
    memset(message_, 0, ReportingSettingsMessage::MAX_MSG_SIZE);
    strncpy(message_, message, ReportingSettingsMessage::MAX_MSG_SIZE - 1);

    forceReporting_ = forceReporting;
    freqHz_ = freqHz;
    
    if (storageHandle_)
    {
        if (valuesChanged)
        {
            esp_err_t result = storageHandle_->set_string(REPORTING_CALLSIGN_ID, callsign_);
            if (result != ESP_OK)
            {
                ESP_LOGE(CURRENT_LOG_TAG, "error setting callsign: %s", esp_err_to_name(result));
            }
            
            result = storageHandle_->set_string(REPORTING_GRID_SQUARE_ID, gridSquare_);
            if (result != ESP_OK)
            {
                ESP_LOGE(CURRENT_LOG_TAG, "error setting gridSquare: %s", esp_err_to_name(result));
            }
            
            result = storageHandle_->set_item(REPORTING_FORCE_ID, forceReporting_);
            if (result != ESP_OK)
            {
                ESP_LOGE(CURRENT_LOG_TAG, "error setting forceReporting: %s", esp_err_to_name(result));
            }
            
            result = storageHandle_->set_item(REPORTING_FREQ_ID, freqHz_);
            if (result != ESP_OK)
            {
                ESP_LOGE(CURRENT_LOG_TAG, "error setting freqHz: %s", esp_err_to_name(result));
            }

            result = storageHandle_->set_string(REPORTING_MSG_ID, message_);
            if (result != ESP_OK)
            {
                ESP_LOGE(CURRENT_LOG_TAG, "error setting reporting message: %s", esp_err_to_name(result));
            }
                
            commitTimer_.stop();
            commitTimer_.start(true);
        }

        // Publish new Wi-Fi settings to everyone who may care.
        ReportingSettingsMessage* message = new ReportingSettingsMessage(
            callsign_,
            gridSquare_,
            forceReporting_,
            freqHz_,
            message_
        );
        assert(message != nullptr);
        publish(message);
        delete message;
        
        ReportingSettingsSavedMessage response;
        publish(&response);
    }
}

void SettingsTask::onSetLedBrightness_(DVTask* origin, SetLedBrightnessSettingsMessage* message)
{
    // Minor wait in case someone else wants to wait for our response.
    // XXX -- this shouldn't be needed.
    vTaskDelay(pdMS_TO_TICKS(50));
    
    setLedBrightness_(message->dutyCycle);
}

void SettingsTask::setLedBrightness_(int dutyCycle)
{
    bool valueChanged = ledDutyCycle_ != dutyCycle;

    ledDutyCycle_ = dutyCycle;
    
    if (storageHandle_)
    {        
        if (valueChanged)
        {
            esp_err_t result = storageHandle_->set_item(LED_DUTY_CYCLE_ID, ledDutyCycle_);
            if (result != ESP_OK)
            {
                ESP_LOGE(CURRENT_LOG_TAG, "error setting ledDutyCycle: %s", esp_err_to_name(result));
            }

            commitTimer_.stop();
            commitTimer_.start(true);
        }

        // Publish new voice keyer settings to everyone who may care.
        LedBrightnessSettingsMessage* message = new LedBrightnessSettingsMessage(
            ledDutyCycle_
        );
        assert(message != nullptr);
        publish(message);
        delete message;
        
        LedBrightnessSettingsSavedMessage response;
        publish(&response);
    }
}

void SettingsTask::onChangeFreeDVMode_(DVTask* origin, audio::SetFreeDVModeMessage* message)
{
    setLastMode_(message->mode);
}

void SettingsTask::setLastMode_(int lastMode)
{
    if (lastMode_ != lastMode)
    {
        lastMode_ = lastMode;
        if (storageHandle_)
        {        
            esp_err_t result = storageHandle_->set_item(LAST_MODE_ID, lastMode_);
            if (result != ESP_OK)
            {
                ESP_LOGE(CURRENT_LOG_TAG, "error setting lastMode: %s", esp_err_to_name(result));
            }

            commitTimer_.stop();
            commitTimer_.start(true);
        
            // Note: don't report mode changes on update. We're only interested
            // in the last used mode on bootup.
        }
    }
}

void SettingsTask::onRequestVolumeSettings_(DVTask* origin, RequestVolumeSettingsMessage* message)
{
    ESP_LOGI(CURRENT_LOG_TAG, "onRequestVolumeSettings_");
    
    LeftChannelVolumeMessage* leftMessage = new LeftChannelVolumeMessage();
    assert(leftMessage != nullptr);

    leftMessage->volume = leftChannelVolume_;
    publish(leftMessage);
    
    RightChannelVolumeMessage* rightMessage = new RightChannelVolumeMessage();
    assert(rightMessage != nullptr);

    rightMessage->volume = rightChannelVolume_;
    publish(rightMessage);
}

}

}