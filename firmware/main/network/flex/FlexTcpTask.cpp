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
#include "audio/FreeDVMessage.h"
#include "network/NetworkMessage.h"

#include "esp_log.h"

#define CURRENT_LOG_TAG "FlexTcpTask"

namespace ezdv
{

namespace network
{
    
namespace flex
{

FlexTcpTask::FlexTcpTask()
    : DVTask("FlexTcpTask", 10 /* TBD */, 8192, tskNO_AFFINITY, 1024, pdMS_TO_TICKS(10))
    , reconnectTimer_(this, std::bind(&FlexTcpTask::connect_, this), 10000000) /* reconnect every 10 seconds */
    , socket_(-1)
    , sequenceNumber_(0)
    , activeSlice_(-1)
    , isSleeping_(true)
    , txSlice_(-1)
{
    registerMessageHandler(this, &FlexTcpTask::onFlexConnectRadioMessage_);
    registerMessageHandler(this, &FlexTcpTask::onRequestRxMessage_);
    registerMessageHandler(this, &FlexTcpTask::onRequestTxMessage_);
    registerMessageHandler(this, &FlexTcpTask::onFreeDVReceivedCallsignMessage_);
}

FlexTcpTask::~FlexTcpTask()
{
    disconnect_();
}

void FlexTcpTask::onTaskStart_()
{
    // nothing required, just waiting for a connect request.
    isSleeping_ = false;
}

void FlexTcpTask::onTaskWake_()
{
    // nothing required, just waiting for a connect request.
    isSleeping_ = false;
}

void FlexTcpTask::onTaskSleep_()
{
    // empty, we have custom actions for sleep.
}

void FlexTcpTask::onTaskSleep_(DVTask* origin, TaskSleepMessage* message)
{
    ESP_LOGI(CURRENT_LOG_TAG, "Sleeping task");
    isSleeping_ = true;
    
    if (socket_ > 0)
    {
        disconnect_();
    }
    else
    {
        DVTask::onTaskSleep_(nullptr, nullptr);
    }
}

void FlexTcpTask::onTaskTick_()
{
    if (socket_ <= 0)
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
        else if (!isSleeping_)
        {
            ESP_LOGE(CURRENT_LOG_TAG, "Detected disconnect from socket, reattempting connect");
            disconnect_();
            reconnectTimer_.start();
            return;
        }
    }
}

void FlexTcpTask::connect_()
{
    // Stop any existing reconnection timers.
    reconnectTimer_.stop();
    
    // Clean up any existing connections before starting.
    disconnect_();

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

    // Connect to the radio.
    ESP_LOGI(CURRENT_LOG_TAG, "Connecting to radio at IP %s", ip_.c_str());
    int rv = connect(socket_, (struct sockaddr*)&radioAddress, sizeof(radioAddress));
    if (rv == -1)
    {
        auto err = errno;
        ESP_LOGE(CURRENT_LOG_TAG, "Got socket error %d (%s) while connecting", err, strerror(err));
        
        // Try again in a few seconds
        close(socket_);
        socket_ = -1;
        reconnectTimer_.start();
    }
    else
    {
        ESP_LOGI(CURRENT_LOG_TAG, "Connected to radio successfully");
        sequenceNumber_ = 0;
        
        // Set socket to be non-blocking.
        fcntl (socket_, F_SETFL, O_NONBLOCK);
        
        // Report successful connection
        ezdv::network::RadioConnectionStatusMessage response(true);
        publish(&response);
    }
}

void FlexTcpTask::disconnect_()
{
    // Report disconnection
    ezdv::network::RadioConnectionStatusMessage response(false);
    publish(&response);

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
    createWaveform_("FreeDV-LSB", "FDVL", "DIGL");
    
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
        
        sendRadioCommand_(ss.str().c_str(), [&](unsigned int rv, std::string message) {
            // Recursively call ourselves again to actually remove the waveform
            // once we get a response for this command.
            activeSlice_ = -1;
            cleanupWaveform_();
        });
        
        return;
    }
    
    sendRadioCommand_("unsub slice all");
    sendRadioCommand_("waveform remove FreeDV-USB");
    sendRadioCommand_("waveform remove FreeDV-LSB", [&](unsigned int rv, std::string message) {
        // We can disconnect after we've fully unregistered the waveforms.
        close(socket_);
        socket_ = -1;
        activeSlice_ = -1;
        isLSB_ = false;
        txSlice_ = -1;
    
        responseHandlers_.clear();
        inputBuffer_.clear();
        
        // Report sleep
        if (isSleeping_) DVTask::onTaskSleep_(nullptr, nullptr);
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
            sendRadioCommand_(setPrefix + "udpport=14992");
        }
    });
}

void FlexTcpTask::sendRadioCommand_(std::string command)
{
    sendRadioCommand_(command, std::function<void(int rv, std::string message)>());
}

void FlexTcpTask::sendRadioCommand_(std::string command, std::function<void(unsigned int rv, std::string message)> fn)
{
    std::ostringstream ss;
    
    responseHandlers_[sequenceNumber_] = fn;

    ESP_LOGI(CURRENT_LOG_TAG, "Sending '%s' as command %d", command.c_str(), sequenceNumber_);    
    ss << "C" << (sequenceNumber_++) << "|" << command << "\n";
    
    write(socket_, ss.str().c_str(), ss.str().length());
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
            responseHandlers_.erase(seq);
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

            if (command.find("tx=1") != std::string::npos)
            {
                txSlice_ = sliceId;
            }

            std::string toFind = "RF_frequency=";
            auto freqLoc = command.find(toFind);
            if (freqLoc != std::string::npos && activeSlice_ == sliceId)
            {
                auto endFreqLoc = command.find(" ", freqLoc);
                if (endFreqLoc != std::string::npos)
                {
                    sliceFrequency_ = command.substr(freqLoc + toFind.length(), endFreqLoc);
                }
                else
                {
                    sliceFrequency_ = command.substr(freqLoc + toFind.length());
                }
            }
            
            if (command.find("mode=") != std::string::npos)
            {
                if (command.find("mode=FDV") != std::string::npos)
                {
                    // User wants to use the waveform.
                    activeSlice_ = sliceId;
                    isLSB_ = command.find("mode=FDVL") != std::string::npos;
                }
            }
        }
        else if (statusName == "interlock")
        {
            ESP_LOGI(CURRENT_LOG_TAG, "Detected interlock update");
            
            if (command.find("state=PTT_REQUESTED") != std::string::npos &&
                activeSlice_ == txSlice_)
            {
                // Going into transmit mode
                ESP_LOGI(CURRENT_LOG_TAG, "Radio went into transmit");
                audio::RequestTxMessage message;
                publish(&message);
            }
            else if (command.find("state=UNKEY_REQUESTED") != std::string::npos)
            {
                // Going back into receive
                ESP_LOGI(CURRENT_LOG_TAG, "Radio went out of transmit");
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
    ip_ = message->ip;
    connect_();
}

void FlexTcpTask::onRequestTxMessage_(DVTask* origin, audio::RequestTxMessage* message)
{
    if (activeSlice_ >= 0)
    {
        sendRadioCommand_("xmit 1");
    }
}

void FlexTcpTask::onRequestRxMessage_(DVTask* origin, audio::RequestRxMessage* message)
{
    if (activeSlice_ >= 0)
    {
        sendRadioCommand_("xmit 0");
    }
}

void FlexTcpTask::onFreeDVReceivedCallsignMessage_(DVTask* origin, audio::FreeDVReceivedCallsignMessage* message)
{
    if (activeSlice_ >= 0)
    {
        std::stringstream ss;
        ss << "spot add rx_freq=" << sliceFrequency_ << " callsign=" << message->callsign << " mode=FREEDV"; //lifetime_seconds=300";
        sendRadioCommand_(ss.str());
    }
}
    
}

}

}