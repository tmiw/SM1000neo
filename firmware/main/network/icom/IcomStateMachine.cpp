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

#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/select.h>

#include "IcomStateMachine.h"
#include "IcomProtocolState.h"
#include "IcomPacket.h"

namespace ezdv
{

namespace network
{

namespace icom
{

void* IcomStateMachine::operator new(size_t size)
{
    return heap_caps_calloc(size, 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_32BIT);
}

void IcomStateMachine::operator delete(void* p)
{
    heap_caps_free(p);
}

IcomStateMachine::IcomStateMachine(DVTask* owner)
    : StateMachine(owner)
    , socket_(0)
    , ourIdentifier_(0)
    , theirIdentifier_(0)
    , port_(0)
    , localPort_(0)
    , packetReadTimer_(owner, this, &IcomStateMachine::readPendingPackets_, MS_TO_US(10), "IcomPacketReadTimer")
{
    owner->registerMessageHandler(this, &IcomStateMachine::onSendPacket_);
    owner->registerMessageHandler(this, &IcomStateMachine::onReceivePacket_);
    owner->registerMessageHandler(this, &IcomStateMachine::onCloseSocket_);
}

std::string IcomStateMachine::getUsername()
{
    return username_;
}

std::string IcomStateMachine::getPassword()
{
    return password_;
}

uint32_t IcomStateMachine::getOurIdentifier()
{
    return ourIdentifier_;
}

uint32_t IcomStateMachine::getTheirIdentifier()
{
    return theirIdentifier_;
}

void IcomStateMachine::setTheirIdentifier(uint32_t id)
{
    theirIdentifier_ = id;
}

void IcomStateMachine::sendUntracked(IcomPacket& packet)
{
    auto task = getTask();

    if (!task->canPostMessage())
    {
        // something's gone very wrong, just skip sending the packet
        // until our queue clears up.
        return;
    }

    auto allocPacket = new IcomPacket(std::move(packet));
    assert(allocPacket != nullptr);

    SendPacketMessage message(allocPacket);
    task->post(&message);
}

void IcomStateMachine::start(std::string ip, uint16_t port, std::string username, std::string password, int localPort)
{
    ip_ = ip;
    port_ = port;
    username_ = username;
    password_ = password;

    // Create and bind UDP socket to force the specified local port number.
    if (localPort == 0)
    {
        localPort_ = port_;
    }
    else
    {
        localPort_ = localPort;
    }

    openSocket_();

    // We're now connected, start running the state machine.
    transitionState(IcomProtocolState::ARE_YOU_THERE);
    
    packetReadTimer_.start();
}

void IcomStateMachine::openSocket_()
{
    if (socket_ > 0)
    {
        close(socket_);
    }

    struct sockaddr_in radioAddress;
    radioAddress.sin_addr.s_addr = inet_addr(ip_.c_str());
    radioAddress.sin_family = AF_INET;
    radioAddress.sin_port = htons(port_);
    
    socket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socket_ == -1)
    {
        auto err = errno;
        ESP_LOGE(getName().c_str(), "Got socket error %d (%s) while creating socket", err, strerror(err));
    }
    assert(socket_ != -1);

    struct sockaddr_in ourSocketAddress;
    memset((char *) &ourSocketAddress, 0, sizeof(ourSocketAddress));

    ourSocketAddress.sin_family = AF_INET;
    ourSocketAddress.sin_port = htons(localPort_);
    ourSocketAddress.sin_addr.s_addr = htonl(INADDR_ANY);
        
    auto rv = bind(socket_, (struct sockaddr*)&ourSocketAddress, sizeof(ourSocketAddress));
    if (rv == -1)
    {
        auto err = errno;
        ESP_LOGE(getName().c_str(), "Got socket error %d (%s) while binding", err, strerror(err));
    }
    assert(rv != -1);

    // Generate our identifier by concatenating the last two octets of our IP
    // with the port we're using to connect. We bind to this port ourselves prior
    // to connection.
    uint32_t localIp = radioAddress.sin_addr.s_addr;
    ourIdentifier_ = 
        (((localIp >> 8) & 0xFF) << 24) | 
        ((localIp & 0xFF) << 16) |
        (localPort_ & 0xFFFF);

    // Connect to the radio.
    rv = connect(socket_, (struct sockaddr*)&radioAddress, sizeof(radioAddress));
    if (rv == -1)
    {
        auto err = errno;
        ESP_LOGE(getName().c_str(), "Got socket error %d (%s) while connecting", err, strerror(err));
    }
    assert(rv != -1);
    
    // Initialize Wi-Fi prioritization
    const int precedenceVI = 6;
    const int precedenceOffset = 7;
    int priority = (precedenceVI << precedenceOffset);
    setsockopt(socket_, IPPROTO_IP, IP_TOS, &priority, sizeof(priority));

    // Use non-blocking sockets
    fcntl(socket_, F_SETFL, O_NONBLOCK);
}

void IcomStateMachine::onTransitionComplete_()
{
    if (getCurrentState() == nullptr && socket_ != 0)
    {
        ESP_LOGI(getName().c_str(), "Disconnecting");
        
        // Close the socket after we send out everything pending.
        CloseSocketMessage message;
        getTask()->post(&message);
        
        // Send disconnected message to indicate that we're done.
        DisconnectedRadioMessage disconnectedMessage;
        getTask()->post(&disconnectedMessage);
    }
}

void IcomStateMachine::readPendingPackets_(DVTimer*)
{
    auto state = getProtocolState_();
    
    // Skip processing if we're not connected yet.
    if (state == nullptr)
    {
        return;
    }
       
    // Process if there are pending datagrams in the buffer
    char buffer[MAX_PACKET_SIZE];
    auto rv = recv(socket_, buffer, MAX_PACKET_SIZE, 0);
    if (rv > 0)
    {
        auto packet = new IcomPacket(buffer, rv);
        assert(packet != nullptr);

        // Queue up packet for future processing.
        ReceivePacketMessage message(packet);
        getTask()->post(&message);
    }
}

IcomProtocolState* IcomStateMachine::getProtocolState_()
{
    return static_cast<IcomProtocolState*>(getCurrentState());
}

void IcomStateMachine::onSendPacket_(DVTask* owner, SendPacketMessage* message)
{
    const int MAX_RETRY_TIME_MS = 25;
    const int EXPIRE_TIME_MS = 500;
    
    auto packet = message->packet;
    assert(packet != nullptr);

    if (socket_ > 0 && (esp_timer_get_time() - message->sendTime)/1000 <= EXPIRE_TIME_MS)
    {
        auto startTime = esp_timer_get_time();
        int tries = 1;
        int rv = send(socket_, packet->getData(), packet->getSendLength(), 0);
        auto totalTimeMs = (esp_timer_get_time() - startTime)/1000;
        while (rv == -1 && totalTimeMs < MAX_RETRY_TIME_MS)
        {
            auto err = errno;
            if (err == ENOMEM)
            {
                // Wait a bit and try again; the Wi-Fi subsystem isn't ready yet.
                vTaskDelay(5);
                tries++;
                rv = send(socket_, packet->getData(), packet->getSendLength(), 0);
                totalTimeMs = (esp_timer_get_time() - startTime)/1000;
            }
            else
            {
                // TBD: close/reopen connection
                ESP_LOGE(
                    getName().c_str(),
                    "Got socket error %d (%s) while sending", 
                    err, strerror(err));
                break;
            }
        }
        
        if (totalTimeMs >= MAX_RETRY_TIME_MS)
        {
            ESP_LOGE(getName().c_str(), "Wi-Fi subsystem took too long to become ready, dropping packet");
        }
        else if (tries > 1)
        {
            ESP_LOGW(getName().c_str(), "Needed %d tries to send a packet", tries);
        }

        // Read any packets that are available from the radio
        readPendingPackets_(nullptr);
    }
    
    delete packet;
}

void IcomStateMachine::onReceivePacket_(DVTask* origin, ReceivePacketMessage* message)
{
    assert(message->packet != nullptr);

    // Forward packet to current state for processing.
    auto state = static_cast<IcomProtocolState*>(getCurrentState());
    if (state != nullptr)
    {
        state->onReceivePacket(*message->packet);
    }
    delete message->packet;
}

void IcomStateMachine::onCloseSocket_(DVTask* owner, CloseSocketMessage* message)
{
    ESP_LOGI(getName().c_str(), "Closing UDP socket");

    // We're fully shut down now, so close the socket.
    close(socket_);
    socket_ = 0;
    
    packetReadTimer_.stop();
}

}

}

}