/* 
 * This file is part of the ezDV project (https://github.com/tmiw/ezDV).
 * Copyright (c) 2023 Mooneer Salem
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

#include <string>
#include <sstream>
#include <unistd.h>
#include <sys/socket.h>

#include "FlexTcpTask.h"
#include "FlexKeyValueParser.h"
#include "audio/FreeDVMessage.h"
#include "network/NetworkMessage.h"
#include "network/ReportingMessage.h"

#include "esp_log.h"

#define CURRENT_LOG_TAG "FlexTcpTask"

namespace ezdv
{

namespace network
{
    
namespace flex
{

FlexTcpTask::FlexTcpTask()
    : DVTask("FlexTcpTask", 10, 4096, tskNO_AFFINITY, 32, pdMS_TO_TICKS(10))
    , reconnectTimer_(this, this, &FlexTcpTask::connect_, MS_TO_US(10000), "FlexTcpReconnectTimer") /* reconnect every 10 seconds */
    , connectionCheckTimer_(this, this, &FlexTcpTask::checkConnection_, MS_TO_US(100), "FlexTcpConnTimer") /* checks for connection every 100ms */
    , commandHandlingTimer_(this, this, &FlexTcpTask::commandResponseTimeout_, MS_TO_US(500), "FlexTcpCmdTimeout") /* time out waiting for command response after 0.5 second */
    , socket_(-1)
    , sequenceNumber_(0)
    , activeSlice_(-1)
    , txSlice_(-1)
    , isTransmitting_(false)
    , isConnecting_(false)
{
    registerMessageHandler(this, &FlexTcpTask::onFlexConnectRadioMessage_);
    registerMessageHandler(this, &FlexTcpTask::onRequestRxMessage_);
    registerMessageHandler(this, &FlexTcpTask::onRequestTxMessage_);
    registerMessageHandler(this, &FlexTcpTask::onFreeDVReceivedCallsignMessage_);
    registerMessageHandler(this, &FlexTcpTask::onFreeDVModeChange_);
    
    // Initialize filter widths. These are sent to SmartSDR on mode changes.
    filterWidths_.push_back(FilterPair_(150, 2850)); // ANA
    filterWidths_.push_back(FilterPair_(750, 2250)); // 700D - 1K width + a bit extra
    filterWidths_.push_back(FilterPair_(500, 2500)); // 700E - 1.5K width + a bit extra
    filterWidths_.push_back(FilterPair_(687, 2313)); // 1600 - 1.125K width + a bit extra
    
    // Default to ANA unless we get something better.
    currentWidth_ = filterWidths_[0];
}

FlexTcpTask::~FlexTcpTask()
{
    disconnect_();
}

void FlexTcpTask::onTaskStart_()
{
    // nothing required, just waiting for a connect request.
}

void FlexTcpTask::onTaskSleep_()
{
    // empty, we have custom actions for sleep.
}

void FlexTcpTask::onTaskSleep_(DVTask* origin, TaskSleepMessage* message)
{
    ESP_LOGI(CURRENT_LOG_TAG, "Sleeping task");
    
    if (socket_ > 0)
    {
        isConnecting_ = false;
        disconnect_();
    }
    else
    {
        DVTask::onTaskSleep_(nullptr, nullptr);
    }
}

void FlexTcpTask::onTaskTick_()
{
    if (socket_ <= 0 || isConnecting_ || !isAwake())
    {
        // Skip tick if we don't have a valid connection yet.
        return;
    }
    
    // Process if there is pending data on the socket.
    char buffer = 0;
    while (true)
    {
        auto rv = recv(socket_, &buffer, 1, 0);
        if (rv > 0)
        {
            // Append to input buffer. Then if we have a full line, we can process
            // accordingly.
            if (buffer == '\n')
            {
                std::string line = inputBuffer_.str();
                processCommand_(line);
                inputBuffer_.str("");
            }
            else
            {
                inputBuffer_.write(&buffer, rv);
            }
        }
        else if (rv == -1 && errno == EAGAIN)
        {
            // Nothing actually available on the socket, ignore.
            break;
        }
        else
        {
            ESP_LOGE(CURRENT_LOG_TAG, "Detected disconnect from socket, reattempting connect");
            socketFinalCleanup_(true);
            return;
        }
    }
}

void FlexTcpTask::socketFinalCleanup_(bool reconnect)
{
    // Report disconnection
    if (socket_ != -1)
    {
        ezdv::network::RadioConnectionStatusMessage response(false);
        publish(&response);

        close(socket_);
        socket_ = -1;
        activeSlice_ = -1;
        isLSB_ = false;
        txSlice_ = -1;

        responseHandlers_.clear();
        inputBuffer_.clear();

        commandHandlingTimer_.stop();
        connectionCheckTimer_.stop();
        isConnecting_ = false;
    }
    
    // Report sleep
    if (reconnect) reconnectTimer_.start();
    else reconnectTimer_.stop();
}

void FlexTcpTask::connect_(DVTimer*)
{
    // Stop any existing reconnection timers.
    reconnectTimer_.stop();
    
    // Clean up any existing connections before starting.
    socketFinalCleanup_(false);

    struct sockaddr_in radioAddress;
    radioAddress.sin_addr.s_addr = inet_addr(ip_.c_str());
    radioAddress.sin_family = AF_INET;
    radioAddress.sin_port = htons(4992); // hardcoded as per Flex documentation
    
    socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socket_ == -1)
    {
        auto err = errno;
        ESP_LOGE(CURRENT_LOG_TAG, "Got socket error %d (%s) while creating socket", err, strerror(err));
    }
    assert(socket_ != -1);

    // Set socket to be non-blocking.
    fcntl (socket_, F_SETFL, O_NONBLOCK);

    // Connect to the radio.
    ESP_LOGI(CURRENT_LOG_TAG, "Connecting to radio at IP %s", ip_.c_str());
    isConnecting_ = true;
    int rv = connect(socket_, (struct sockaddr*)&radioAddress, sizeof(radioAddress));
    if (rv == -1 && errno != EINPROGRESS)
    {
        auto err = errno;
        ESP_LOGE(CURRENT_LOG_TAG, "Got socket error %d (%s) while connecting", err, strerror(err));
        
        // Try again in a few seconds
        close(socket_);
        socket_ = -1;
        reconnectTimer_.start();
    }
    else if (rv == 0)
    {
        // We got an immediate connection!
        checkConnection_(nullptr);
    }
    else
    {
        connectionCheckTimer_.start();
    }
}

void FlexTcpTask::checkConnection_(DVTimer*)
{
    ESP_LOGI(CURRENT_LOG_TAG, "Checking to see if we're connected to the radio");

    fd_set writeSet;
    struct timeval tv = {0, 0};
    int err = 0;

    FD_ZERO(&writeSet);
    FD_SET(socket_, &writeSet);
    
    if (select(socket_ + 1, nullptr, &writeSet, nullptr, &tv) > 0)
    {
        int sockErrCode = 0;
        socklen_t resultLength = sizeof(sockErrCode);
        auto sockOptError = getsockopt(socket_, SOL_SOCKET, SO_ERROR, &sockErrCode, &resultLength);

        if (sockOptError < 0)
        {
            err = errno;
            goto socket_error;
        }
        else if (sockErrCode != 0 && sockErrCode != EINPROGRESS)
        {
            err = sockErrCode;
            goto socket_error;
        }
        else if (sockErrCode == 0)
        {
            isConnecting_ = false;
            connectionCheckTimer_.stop();

            ESP_LOGI(CURRENT_LOG_TAG, "Connected to radio successfully");
            sequenceNumber_ = 0;
            
            // Report successful connection
            ezdv::network::RadioConnectionStatusMessage response(true);
            publish(&response);

            // Get current FreeDV mode to ensure filters are set properly on
            // SmartSDR connection.
            audio::RequestGetFreeDVModeMessage requestGetFreeDVMode;
            publish(&requestGetFreeDVMode);
        }
    }

    return;

socket_error:
    ESP_LOGE(CURRENT_LOG_TAG, "Got socket error %d (%s) while connecting", err, strerror(err));
    
    // Try again in a few seconds
    connectionCheckTimer_.stop();
    isConnecting_ = false;
    close(socket_);
    socket_ = -1;
    reconnectTimer_.start();
}

void FlexTcpTask::disconnect_()
{
    if (socket_ > 0)
    {
        cleanupWaveform_();
    }
}

void FlexTcpTask::initializeWaveform_()
{
    // Send needed commands to initialize the waveform. This is from the reference
    // waveform implementation.
    createWaveform_("FreeDV-USB", "FDVU", "DIGU");
    createWaveform_("FreeDV-LSB", "FDVL", "LSB");
    
    // subscribe to slice updates, needed to detect when we enter FDVU/FDVL mode
    sendRadioCommand_("sub slice all");
}

void FlexTcpTask::cleanupWaveform_()
{
    std::stringstream ss;
    
    // Change mode back to something that exists.
    if (activeSlice_ >= 0)
    {
        ss << "slice set " << activeSlice_ << " mode=";
        if (isLSB_) ss << "LSB";
        else ss << "USB";

        // Ensure that we disconnect from any reporting services as appropriate
        DisableReportingMessage disableMessage;
        publish(&disableMessage);
        
        sendRadioCommand_(ss.str().c_str(), [&](unsigned int rv, std::string message) {
            // Recursively call ourselves again to actually remove the waveform
            // once we get a response for this command.
            activeSlice_ = -1;
            cleanupWaveform_();
        });
        
        return;
    }
    
    sendRadioCommand_("unsub slice all"/*);
    sendRadioCommand_("waveform remove FreeDV-USB");
    sendRadioCommand_("waveform remove FreeDV-LSB"*/, [&](unsigned int rv, std::string message) {
        // We can disconnect after we've fully unregistered the waveforms.
        socketFinalCleanup_(false);
        DVTask::onTaskSleep_(nullptr, nullptr);
    });
}

void FlexTcpTask::createWaveform_(std::string name, std::string shortName, std::string underlyingMode)
{
    ESP_LOGI(CURRENT_LOG_TAG, "Creating waveform %s (abbreviated %s in SmartSDR)", name.c_str(), shortName.c_str());
    
    // Actually create the waveform.
    std::string waveformCommand = "waveform create name=" + name + " mode=" + shortName + " underlying_mode=" + underlyingMode + " version=2.0.0";
    std::string setPrefix = "waveform set " + name + " ";
    sendRadioCommand_(waveformCommand, [&, setPrefix](unsigned int rv, std::string message) {
        if (rv == 0)
        {
            // Set the filter-related settings for the just-created waveform.
            sendRadioCommand_(setPrefix + "tx=1");
            sendRadioCommand_(setPrefix + "rx_filter depth=256");
            sendRadioCommand_(setPrefix + "tx_filter depth=256");

            // Link waveform to our UDP audio stream.
            sendRadioCommand_(setPrefix + "udpport=4992");
        }
    });
}

void FlexTcpTask::sendRadioCommand_(std::string command)
{
    sendRadioCommand_(command, std::function<void(int rv, std::string message)>());
}

void FlexTcpTask::sendRadioCommand_(std::string command, std::function<void(unsigned int rv, std::string message)> fn)
{
    int err = 0;

    if (socket_ > 0)
    {
        std::ostringstream ss;

        ESP_LOGI(CURRENT_LOG_TAG, "Sending '%s' as command %d", command.c_str(), sequenceNumber_);    
        ss << "C" << (sequenceNumber_) << "|" << command << "\n";
        
        // Make sure we can actually write to the socket
        fd_set writeSet;
        struct timeval tv = {0, 0};

        FD_ZERO(&writeSet);
        FD_SET(socket_, &writeSet);

        if (select(socket_ + 1, nullptr, &writeSet, nullptr, &tv) > 0)
        {
            int sockErrCode = 0;
            socklen_t resultLength = sizeof(sockErrCode);
            auto sockOptError = getsockopt(socket_, SOL_SOCKET, SO_ERROR, &sockErrCode, &resultLength);

            if (sockOptError < 0)
            {
                err = errno;
                goto socket_error;
            }
            else if (sockErrCode != 0)
            {
                err = sockErrCode;
                goto socket_error;
            }
            else if (sockErrCode == 0)
            {
                auto rv = write(socket_, ss.str().c_str(), ss.str().length());
                if (rv <= 0)
                {
                    err = rv;
                    goto socket_error;
                }
                else
                {
                    responseHandlers_[sequenceNumber_++] = fn;
                    commandHandlingTimer_.stop();
                    commandHandlingTimer_.start(true);
                }
            }
            
            return;
        }
        else
        {
            err = errno;
        }
    }
    else
    {
        return;
    }

socket_error:
    ESP_LOGE(CURRENT_LOG_TAG, "Failed writing command to radio!");

    // We've likely disconnected, do cleanup and re-attempt connection.
    socketFinalCleanup_(true);

    // Call event handler with failure code in case the sender needs to
    // do any additional actions.
    fn(0xFFFFFFFF, strerror(err));
}

void FlexTcpTask::commandResponseTimeout_(DVTimer*)
{
    // We timed out waiting for a response, just go ahead and call handlers so that
    // processing can continue.
    ESP_LOGW(CURRENT_LOG_TAG, "Timed out waiting for response from radio.");
    for (auto& kvp : responseHandlers_)
    {
        if (kvp.second)
        {
            ESP_LOGI(CURRENT_LOG_TAG, "Calling response handler for command %d", kvp.first);
            kvp.second(0xFFFFFFFF, "Timed out waiting for response from radio");
        }
    }
    responseHandlers_.clear();
}

void FlexTcpTask::processCommand_(std::string& command)
{
    if (command[0] == 'V')
    {
        // Version information from radio
        ESP_LOGI(CURRENT_LOG_TAG, "Radio is using protocol version %s", &command.c_str()[1]);
    }
    else if (command[0] == 'H')
    {
        // Received connection's handle. We don't currently do anything with this other
        // than trigger waveform creation.
        ESP_LOGI(CURRENT_LOG_TAG, "Connection handle is %s", &command.c_str()[1]);
        initializeWaveform_();
    }
    else if (command[0] == 'R')
    {
        ESP_LOGI(CURRENT_LOG_TAG, "Received response %s", command.c_str());
        
        // Received response for a command.
        command.erase(0, 1);
        std::stringstream ss(command);
        int seq = 0;
        unsigned int rv = 0;
        char temp = 0;
        
        // Get sequence number and return value
        ss >> seq >> temp >> std::hex >> rv;
        
        if (rv != 0)
        {
            ESP_LOGE(CURRENT_LOG_TAG, "Command %d returned error %x", seq, rv);
        }
        
        // If we have a valid command handler, call it now
        if (responseHandlers_[seq])
        {
            responseHandlers_[seq](rv, ss.str());
        }
        responseHandlers_.erase(seq);

        // Stop timer if we're not waiting for any more responses.
        if (responseHandlers_.size() == 0)
        {
            commandHandlingTimer_.stop();
        }
    }
    else if (command[0] == 'S')
    {
        ESP_LOGI(CURRENT_LOG_TAG, "Received status update %s", command.c_str());
        
        command.erase(0, 1);
        std::stringstream ss(command);
        unsigned int clientId = 0;
        std::string statusName;
        
        ss >> std::hex >> clientId;
        char pipe = 0;
        ss >> pipe >> statusName;
        
        if (statusName == "slice")
        {
            ESP_LOGI(CURRENT_LOG_TAG, "Detected slice update");
            
            int sliceId = 0;
            ss >> std::dec >> sliceId;
            
            auto parameters = FlexKeyValueParser::GetCommandParameters(ss);

            auto tx = parameters.find("tx");
            if (tx != parameters.end() && tx->second == "1")
            {
                txSlice_ = sliceId;
            }

            auto rfFrequency = parameters.find("RF_frequency");
            if (rfFrequency != parameters.end())
            {
                sliceFrequencies_[sliceId] = rfFrequency->second;

                // Report new frequency to any listening reporters
                if (activeSlice_ == sliceId)
                {
                    // Frequency reported by Flex is in MHz but reporters expect
                    // it in Hz.
                    uint64_t freqHz = atof(rfFrequency->second.c_str()) * 1000000;

                    ReportFrequencyChangeMessage freqChangeMessage(freqHz);
                    publish(&freqChangeMessage);
                }
            }
            
            auto isActive = parameters.find("in_use");
            if (isActive != parameters.end())
            {
                activeSlices_[sliceId] = isActive->second == "1" ? true : false;
                if (sliceId == activeSlice_ && !activeSlices_[sliceId])
                {
                    // Ensure that we disconnect from any reporting services as appropriate
                    DisableReportingMessage disableMessage;
                    publish(&disableMessage);

                    activeSlice_ = -1;
                }
            }
            
            auto mode = parameters.find("mode");
            if (mode != parameters.end())
            {
                if (mode->second == "FDVU" || mode->second == "FDVL")
                {
                    if (sliceId != activeSlice_)
                    {
                        ESP_LOGI(CURRENT_LOG_TAG, "Swtiching slice %d to FreeDV mode", sliceId);
                        
                        if (activeSlice_ == -1)
                        {
                            // Don't enable reporting if we've already done so.
                            ESP_LOGI(CURRENT_LOG_TAG, "Enabling FreeDV reporting for slice %d", sliceId);
                            EnableReportingMessage enableMessage;
                            publish(&enableMessage);
                        }
                        else 
                        {
                            ESP_LOGW(CURRENT_LOG_TAG, "Attempted to activate FDVU/FDVL from a second slice (id = %d, active = %d)", sliceId, activeSlice_);
                        }

                        // User wants to use the waveform.
                        activeSlice_ = sliceId;
                        isLSB_ = mode->second == "FDVL";

                        // Set the filter corresponding to the current mode.
                        setFilter_(currentWidth_.first, currentWidth_.second);

                        // Ensure that we connect to any reporting services as appropriate
                        uint64_t freqHz = atof(sliceFrequencies_[activeSlice_].c_str()) * 1000000;
                        ReportFrequencyChangeMessage freqChangeMessage(freqHz);
                        publish(&freqChangeMessage);
                    }
                }
                else if (sliceId == activeSlice_)
                {
                    // Ensure that we disconnect from any reporting services as appropriate
                    DisableReportingMessage disableMessage;
                    publish(&disableMessage);

                    activeSlice_ = -1;
                }
            }
        }
        else if (statusName == "interlock")
        {
            ESP_LOGI(CURRENT_LOG_TAG, "Detected interlock update");
            
            auto parameters = FlexKeyValueParser::GetCommandParameters(ss);
            auto state = parameters.find("state");
            auto source = parameters.find("source");
            
            if (state != parameters.end() && state->second == "PTT_REQUESTED" &&
                activeSlice_ == txSlice_ && source->second != "TUNE")
            {
                // Going into transmit mode
                ESP_LOGI(CURRENT_LOG_TAG, "Radio went into transmit");
                isTransmitting_ = true;
                audio::RequestTxMessage message;
                publish(&message);
            }
            else if (state != parameters.end() && state->second == "UNKEY_REQUESTED")
            {
                // Going back into receive
                ESP_LOGI(CURRENT_LOG_TAG, "Radio went out of transmit");
                isTransmitting_ = false;
                audio::RequestRxMessage message;
                publish(&message);
            }
        }
        else
        {
            ESP_LOGW(CURRENT_LOG_TAG, "Unknown status update type %s", statusName.c_str());
        }
    }
    else
    {
        ESP_LOGW(CURRENT_LOG_TAG, "Got unhandled command %s", command.c_str());
    }
}

void FlexTcpTask::onFlexConnectRadioMessage_(DVTask* origin, FlexConnectRadioMessage* message)
{
    ESP_LOGI(CURRENT_LOG_TAG, "Received radio connect message");
    ip_ = message->ip;
    connect_(nullptr);
}

void FlexTcpTask::onRequestTxMessage_(DVTask* origin, audio::RequestTxMessage* message)
{
    if (activeSlice_ >= 0 && !isTransmitting_)
    {
        isTransmitting_ = true;
        sendRadioCommand_("xmit 1");
    }
}

void FlexTcpTask::onRequestRxMessage_(DVTask* origin, audio::TransmitCompleteMessage* message)
{
    if (activeSlice_ >= 0 && isTransmitting_)
    {
        isTransmitting_ = false;
        sendRadioCommand_("xmit 0");
    }
}

void FlexTcpTask::onFreeDVReceivedCallsignMessage_(DVTask* origin, audio::FreeDVReceivedCallsignMessage* message)
{
    if (activeSlice_ >= 0 && strlen(message->callsign) > 0)
    {
        std::stringstream ss;
        ss << "spot add rx_freq=" << sliceFrequencies_[activeSlice_] << " callsign=" << message->callsign << " mode=FREEDV timestamp=" << time(NULL); //lifetime_seconds=300";
        sendRadioCommand_(ss.str());
    }
}

void FlexTcpTask::onFreeDVModeChange_(DVTask* origin, audio::SetFreeDVModeMessage* message)
{
    currentWidth_ = filterWidths_[message->mode];
    setFilter_(currentWidth_.first, currentWidth_.second);
}

void FlexTcpTask::setFilter_(int low, int high)
{
    if (activeSlice_ >= 0)
    {
        int low_cut = low;
        int high_cut = high;

        if (isLSB_)
        {
            low_cut = -high;
            high_cut = -low;
        }

        std::stringstream ss;
        ss << "filt " << activeSlice_ << " " << low_cut << " " << high_cut;
        sendRadioCommand_(ss.str());
    }
}
    
}

}

}