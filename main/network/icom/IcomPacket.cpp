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

#include "esp_heap_caps.h"

#include <random>
#include <cstring>
#include "IcomPacket.h"

namespace ezdv
{

namespace network
{

namespace icom
{

void* IcomPacket::operator new(std::size_t count)
{
    return heap_caps_malloc(sizeof(IcomPacket)*count, MALLOC_CAP_SPIRAM | MALLOC_CAP_32BIT);
}

void IcomPacket::operator delete(void* ptr)
{
    heap_caps_free(ptr);
}

IcomPacket::IcomPacket()
    : rawPacket_(nullptr)
    , size_(0)
{
    // empty
}

IcomPacket::IcomPacket(char* existingPacket, int size)
    : rawPacket_((char*)heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_32BIT))
    , size_(size)
{
    assert(rawPacket_ != nullptr);
    memcpy(rawPacket_, existingPacket, size_);
}

IcomPacket::IcomPacket(int size)
    : rawPacket_((char*)heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_32BIT))
    , size_(size)
{
    assert(rawPacket_ != nullptr);
    memset(rawPacket_, 0, size);
}
    
IcomPacket::IcomPacket(const IcomPacket& packet)
    : rawPacket_((char*)heap_caps_malloc(packet.size_, MALLOC_CAP_SPIRAM | MALLOC_CAP_32BIT))
    , size_(packet.size_)
{
    assert(rawPacket_ != nullptr);
    memcpy(rawPacket_, packet.rawPacket_, size_);
}

IcomPacket::IcomPacket(IcomPacket&& packet)
    : rawPacket_(packet.rawPacket_)
    , size_(packet.size_)
{
    packet.rawPacket_ = nullptr;
    packet.size_ = 0;
}

IcomPacket::~IcomPacket()
{
    heap_caps_free(rawPacket_);
}

int IcomPacket::getSendLength()
{
    //ESP_LOGI("IcomPacket", "Sending packet of size %d", size_);
    return size_;
}

const uint8_t* IcomPacket::getData()
{
    return (const uint8_t*)rawPacket_;
}

IcomPacket& IcomPacket::operator=(const IcomPacket& packet)
{
    heap_caps_free(rawPacket_);
    
    rawPacket_ = (char*)heap_caps_malloc(packet.size_, MALLOC_CAP_SPIRAM | MALLOC_CAP_32BIT);
    assert(rawPacket_ != nullptr);
    memcpy(rawPacket_, packet.rawPacket_, packet.size_);
    size_ = packet.size_;
    
    return *this;
}

IcomPacket& IcomPacket::operator=(IcomPacket&& packet)
{
    heap_caps_free(rawPacket_);
    
    rawPacket_ = packet.rawPacket_;
    size_ = packet.size_;
    packet.rawPacket_ = nullptr;
    packet.size_ = 0;
    
    return *this;
}

IcomPacket IcomPacket::CreateAreYouTherePacket(uint32_t ourId, uint32_t theirId)
{
    static_assert(CONTROL_SIZE == sizeof(control_packet));
    constexpr uint16_t packetType = 0x03;
    
    IcomPacket result(sizeof(control_packet));
    auto packet = result.getTypedPacket<control_packet>();
    packet->len = sizeof(control_packet);
    packet->type = packetType;
    packet->seq = 0; // always the first packet, so no need for a sequence number
    packet->sentid = ourId;
    packet->rcvdid = theirId;
    
    return result;
}

IcomPacket IcomPacket::CreateAreYouReadyPacket(uint32_t ourId, uint32_t theirId)
{    
    static_assert(CONTROL_SIZE == sizeof(control_packet));
    constexpr uint16_t packetType = 0x06;
    
    IcomPacket result(sizeof(control_packet));
    auto packet = result.getTypedPacket<control_packet>();
    packet->len = sizeof(control_packet);
    packet->type = packetType;
    packet->seq = 1; // always the second packet, so no need for a sequence number
    packet->sentid = ourId;
    packet->rcvdid = theirId;
    
    return result;
}

IcomPacket IcomPacket::CreateLoginPacket(uint16_t authSeq, uint32_t ourId, uint32_t theirId, std::string username, std::string password, std::string computerName)
{
    static_assert(LOGIN_SIZE == sizeof(login_packet));
    
    // Generate random token
    std::minstd_rand generator;
    std::uniform_int_distribution<uint16_t> uniform_dist(0, UINT16_MAX);
    uint16_t tokRequest = uniform_dist(generator);
    
    IcomPacket result(sizeof(login_packet));
    auto packet = result.getTypedPacket<login_packet>();
    packet->len = sizeof(login_packet);
    packet->sentid = ourId;
    packet->rcvdid = theirId;
    
    // Payload size needs to be manually forced to be correctly sent. ESP issue??
    uint16_t tmp = ToBigEndian((uint16_t)(sizeof(login_packet) - 0x10)); // ?? why subtract 16?
    ((uint8_t*)packet)[0x12] = *((uint8_t*)&tmp);
    ((uint8_t*)packet)[0x13] = *((uint8_t*)&tmp + 1);

    packet->requesttype = 0x00;
    packet->requestreply = 0x01;
    packet->innerseq = ToBigEndian(authSeq);
    packet->tokrequest = tokRequest;
    
    EncodePassword_(username, packet->username);
    EncodePassword_(password, packet->password);
    memcpy(packet->name, computerName.c_str(), computerName.size());
    
    return result;
}

IcomPacket IcomPacket::CreateTokenAckPacket(uint16_t authSeq, uint16_t tokenRequest, uint32_t token, uint32_t ourId, uint32_t theirId)
{
    static_assert(TOKEN_SIZE == sizeof(token_packet));
    
    IcomPacket result(sizeof(token_packet));
    auto packet = result.getTypedPacket<token_packet>();
    packet->len = sizeof(token_packet);
    
    packet->sentid = ourId;
    packet->rcvdid = theirId;
    packet->payloadsize = ToBigEndian((uint16_t)(sizeof(token_packet) - 0x10));
    packet->requesttype = 0x02;
    packet->requestreply = 0x01;
    packet->innerseq = ToBigEndian(authSeq);
    packet->tokrequest = tokenRequest;
    packet->token = token;

    return result;
}

IcomPacket IcomPacket::CreatePingPacket(uint16_t pingSeq, uint32_t ourId, uint32_t theirId)
{
    static_assert(PING_SIZE == sizeof(ping_packet));
    constexpr uint16_t packetType = 0x07;
    
    IcomPacket result(sizeof(ping_packet));
    auto packet = result.getTypedPacket<ping_packet>();
    packet->len = sizeof(ping_packet);
    packet->type = packetType;
    packet->seq = pingSeq;
    packet->sentid = ourId;
    packet->rcvdid = theirId;
    packet->time = time(NULL); // wfview used milliseconds since start of day, not sure that matters
    return result;
}

IcomPacket IcomPacket::CreatePingAckPacket(uint16_t theirPingSeq, uint32_t ourId, uint32_t theirId)
{
    static_assert(PING_SIZE == sizeof(ping_packet));
    constexpr uint16_t packetType = 0x07;
    
    IcomPacket result(sizeof(ping_packet));
    auto packet = result.getTypedPacket<ping_packet>();
    packet->len = sizeof(ping_packet);
    packet->type = packetType;
    packet->seq = theirPingSeq;
    packet->sentid = ourId;
    packet->rcvdid = theirId;
    packet->time = time(NULL); // wfview used milliseconds since start of day, not sure that matters
    packet->reply = 0x1;
    return result;
}

IcomPacket IcomPacket::CreateIdlePacket(uint16_t ourSeq, uint32_t ourId, uint32_t theirId)
{
    static_assert(CONTROL_SIZE == sizeof(control_packet));
    constexpr uint16_t packetType = 0x00;
    
    IcomPacket result(sizeof(control_packet));
    auto packet = result.getTypedPacket<control_packet>();
    packet->len = sizeof(control_packet);
    packet->type = packetType;
    packet->seq = ourSeq;
    packet->sentid = ourId;
    packet->rcvdid = theirId;
    
    return result;
}

IcomPacket IcomPacket::CreateRetransmitRequest(uint32_t ourId, uint32_t theirId, std::vector<uint16_t, util::PSRamAllocator<uint16_t>> packetIdsToRetransmit)
{
    static_assert(CONTROL_SIZE == sizeof(control_packet));
    constexpr uint16_t packetType = 0x01;
    
    size_t numBytesAtEnd = sizeof(uint16_t) * (packetIdsToRetransmit.size());
    IcomPacket result(sizeof(control_packet) + numBytesAtEnd);
    auto packet = result.getTypedPacket<control_packet>();
    packet->len = sizeof(control_packet) + numBytesAtEnd;
    packet->type = packetType;
    packet->seq = 0; // no sequence number for retransmit packets by default
    packet->sentid = ourId;
    packet->rcvdid = theirId;
    
    // If only one packet to resend, we can use the sequence number field to store the ID.
    if (packetIdsToRetransmit.size() == 1)
    {
        packet->seq = ToBigEndian(packetIdsToRetransmit[0]);
    }
    else
    {
        uint16_t* pos = (uint16_t*)((uint8_t*)result.getData() + sizeof(control_packet));
        for (auto& id : packetIdsToRetransmit)
        {
            *(pos++) = ToBigEndian(id);
        }
    }
    
    return result;
}

IcomPacket IcomPacket::CreateTokenRenewPacket(uint16_t authSeq, uint16_t tokenRequest, uint32_t token, uint32_t ourId, uint32_t theirId)
{
    static_assert(TOKEN_SIZE == sizeof(token_packet));
    
    IcomPacket result(sizeof(token_packet));
    auto packet = result.getTypedPacket<token_packet>();
    packet->len = sizeof(token_packet);
    
    packet->sentid = ourId;
    packet->rcvdid = theirId;
    packet->payloadsize = ToBigEndian((uint16_t)(sizeof(token_packet) - 0x10));
    packet->requesttype = 0x05;
    packet->requestreply = 0x01;
    packet->innerseq = ToBigEndian(authSeq);
    packet->tokrequest = tokenRequest;
    packet->token = token;

    return result;
}

IcomPacket IcomPacket::CreateTokenRemovePacket(uint16_t authSeq, uint16_t tokenRequest, uint32_t token, uint32_t ourId, uint32_t theirId)
{
    static_assert(TOKEN_SIZE == sizeof(token_packet));
    
    IcomPacket result(sizeof(token_packet));
    auto packet = result.getTypedPacket<token_packet>();
    packet->len = sizeof(token_packet);
    
    packet->sentid = ourId;
    packet->rcvdid = theirId;
    packet->payloadsize = ToBigEndian((uint16_t)(sizeof(token_packet) - 0x10));
    packet->requesttype = 0x01;
    packet->requestreply = 0x01;
    packet->innerseq = ToBigEndian(authSeq);
    packet->tokrequest = tokenRequest;
    packet->token = token;

    return result;
}

IcomPacket IcomPacket::CreateDisconnectPacket(uint32_t ourId, uint32_t theirId)
{
    static_assert(CONTROL_SIZE == sizeof(control_packet));
    constexpr uint16_t packetType = 0x05;
    
    IcomPacket result(sizeof(control_packet));
    auto packet = result.getTypedPacket<control_packet>();
    packet->len = sizeof(control_packet);
    packet->type = packetType;
    packet->seq = 0; // always the first packet, so no need for a sequence number
    packet->sentid = ourId;
    packet->rcvdid = theirId;
    
    return result;
}

IcomPacket IcomPacket::CreateCIVPacket(uint32_t ourId, uint32_t theirId, uint16_t sendSeq, uint8_t* civData, uint16_t civLength)
{
    IcomPacket result(0x15 + civLength);
    auto packet = result.getTypedPacket<data_packet>();
    packet->len = 0x15 + civLength;
    
    packet->sentid = ourId;
    packet->rcvdid = theirId;
    
    packet->reply = (uint8_t)0xc1;
    packet->datalen = civLength;
    packet->sendseq = ToBigEndian(sendSeq);
    memcpy(const_cast<uint8_t*>(result.getData()) + 0x15, civData, civLength);

    return result;
}

IcomPacket IcomPacket::CreateCIVOpenClosePacket(uint16_t civSeq, uint32_t ourId, uint32_t theirId, bool close)
{
    static_assert(OPENCLOSE_SIZE == sizeof(openclose_packet));
    
    IcomPacket result(sizeof(openclose_packet));
    auto packet = result.getTypedPacket<openclose_packet>();
    packet->len = sizeof(openclose_packet);
    
    packet->sentid = ourId;
    packet->rcvdid = theirId;
    
    packet->data = 0x01c0;
    packet->magic = close ? 0x00 : 0x04;
    
    packet->sendseq = ToBigEndian(civSeq);
    
    return result;   
}

IcomPacket IcomPacket::CreateAudioPacket(uint16_t audioSeq, uint32_t ourId, uint32_t theirId, short* audio, uint16_t len)
{
    IcomPacket result(0x18 + len * sizeof(short));
    auto packet = result.getTypedPacket<audio_packet>();
    packet->len = 0x18 + len * sizeof(short);
    
    packet->sentid = ourId;
    packet->rcvdid = theirId;
    
    // Other arguments need to be manually forced to be correctly sent. ESP issue??
    
    // packet->ident = 0x0080
    ((uint8_t*)packet)[0x10] = 0x80;
    ((uint8_t*)packet)[0x11] = 0x00;
    
    // packet->sendseq = ToBigEndian(audioSeq);
    uint16_t tmp = ToBigEndian(audioSeq);
    ((uint8_t*)packet)[0x12] = *((uint8_t*)&tmp);
    ((uint8_t*)packet)[0x13] = *((uint8_t*)&tmp + 1);

    // packet->datalen = ToBigEndian(len * sizeof(short)); 
    tmp = len * sizeof(short);
    tmp = ToBigEndian(tmp);
    ((uint8_t*)packet)[0x16] = *((uint8_t*)&tmp);
    ((uint8_t*)packet)[0x17] = *((uint8_t*)&tmp + 1);
    
    memcpy(const_cast<uint8_t*>(result.getData()) + 0x18, audio, len * sizeof(short));
    
    //ESP_LOGI("CreateAudioPacket", "len: %d", len);
    
    return result;
}

bool IcomPacket::isIAmHere(uint32_t& theirId)
{
    if (size_ == CONTROL_SIZE)
    {
        auto typedPacket = getTypedPacket<control_packet>();
        theirId = typedPacket->sentid;
        
        return typedPacket->type == 0x04;
    }
    
    return false;
}

bool IcomPacket::isIAmReady()
{
    if (size_ == CONTROL_SIZE)
    {
        auto typedPacket = getTypedPacket<control_packet>();            
        return typedPacket->type == 0x06;
    }
    
    return false;
}

bool IcomPacket::isLoginResponse(std::string& connectionType, bool& isInvalidPassword, uint16_t& tokenReq, uint32_t& radioToken)
{
    bool ret = false;
    
    if (size_ == LOGIN_RESPONSE_SIZE)
    {
        auto typedPacket = getConstTypedPacket<login_response_packet>();
        if (typedPacket->type != 0x01) // XXX -- what does 0x01 mean? from wfview source code
        {
            connectionType = typedPacket->connection;
            isInvalidPassword = typedPacket->error == 0xfeffffff;
            
            tokenReq = typedPacket->tokrequest;
            radioToken = typedPacket->token;
            
            ret = true;
        }
    }
    
    return ret;
}

bool IcomPacket::isPingRequest(uint16_t& pingSequence)
{
    bool ret = false;
    
    if (size_ == PING_SIZE)
    {
        auto typedPacket = getConstTypedPacket<ping_packet>();
        ret = typedPacket->reply == 0;
        pingSequence = typedPacket->seq;
    }
    
    return ret;
}

bool IcomPacket::isPingResponse(uint16_t& pingSequence)
{
    bool ret = false;
    
    if (size_ == PING_SIZE)
    {
        auto typedPacket = getConstTypedPacket<ping_packet>();
        ret = typedPacket->reply == 1;
        pingSequence = typedPacket->seq;
    }
    
    return ret;
}

bool IcomPacket::isCapabilitiesPacket(std::vector<radio_cap_packet_t, util::PSRamAllocator<radio_cap_packet_t>>& radios)
{
    auto rawData = getData();
    
    bool result = false;
    if ((size_ - CAPABILITIES_SIZE) % RADIO_CAP_SIZE == 0)
    {
        for (int index = CAPABILITIES_SIZE; index < size_; index += RADIO_CAP_SIZE)
        {
            radios.push_back((radio_cap_packet_t)(rawData + index));
        }
        
        result = true;
    }

    return result;
}

bool IcomPacket::isRetransmitPacket(std::vector<uint16_t, util::PSRamAllocator<uint16_t>>& retryPackets)
{
    bool result = false;
    
    if (size_ >= CONTROL_SIZE)
    {
        auto typedPacket = getConstTypedPacket<control_packet>();
        if (typedPacket->type == 0x01)
        {
            result = true;
            
            if (size_ == CONTROL_SIZE)
            {
                // only one packet to resend
                retryPackets.push_back(typedPacket->seq);
            }
            else
            {
                uint16_t* ids = (uint16_t*)(getData() + CONTROL_SIZE);
                for (int sz = CONTROL_SIZE; sz < size_; sz += sizeof(uint16_t))
                {
                    retryPackets.push_back(*(ids++));
                }
            }
        }
    }
    
    return result;
}

bool IcomPacket::isConnInfoPacket(std::string& name, uint32_t& ip, bool& isBusy)
{
    bool result = false;
    
    if (size_ == CONNINFO_SIZE)
    {
        auto typedPacket = getConstTypedPacket<conninfo_packet>();
        result = true;
        
        auto data = getData();
        
        name = typedPacket->name;
        ip = *(uint32_t*)(data + 0x84);
        isBusy = *(uint32_t*)(data + 0x60);
    }
    
    return result;
}

bool IcomPacket::isStatusPacket(bool& connSuccessful, bool& disconnected, uint16_t& civPort, uint16_t& audioPort)
{
    bool result = false;
    
    if (size_ == STATUS_SIZE)
    {
        auto typedPacket = getConstTypedPacket<status_packet>();
        result = true;
        
        connSuccessful = !(typedPacket->error == 0xffffffff);
        disconnected = typedPacket->error == 0 && typedPacket->disc == 1;
        
        civPort = ToLittleEndian(typedPacket->civport);
        audioPort = ToLittleEndian(typedPacket->audioport);
    }
    
    return result;
}

bool IcomPacket::isAudioPacket(uint16_t& seq, short** dataStart)
{
    bool result = false;
    
    auto typedPacket = getConstTypedPacket<control_packet>();
    if (typedPacket->type != 0x01 && typedPacket->len >= 0x20)
    {
        // TBD -- we might need to do some reordering by virtue of how UDP works.
        result = true;
        seq = ToLittleEndian(typedPacket->seq);
        *dataStart = (short*)(getData() + 0x18);
    }
    
    return result;
}

bool IcomPacket::isCivPacket(uint8_t** civPacket, uint16_t* civPacketLength)
{
    bool result = false;
    
    auto typedPacket = getConstTypedPacket<data_packet>();
    if (typedPacket->len > 0x15 && typedPacket->type != 0x01 && (typedPacket->datalen + 0x15) == typedPacket->len)
    {
        result = true;
        *civPacket = const_cast<uint8_t*>(getData() + 0x15);
        *civPacketLength = typedPacket->datalen;
    }
    
    return result;
}

void IcomPacket::EncodePassword_(std::string str, char* output)
{
    const uint8_t sequence[] =
    {
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0x47,0x5d,0x4c,0x42,0x66,0x20,0x23,0x46,0x4e,0x57,0x45,0x3d,0x67,0x76,0x60,0x41,0x62,0x39,0x59,0x2d,0x68,0x7e,
        0x7c,0x65,0x7d,0x49,0x29,0x72,0x73,0x78,0x21,0x6e,0x5a,0x5e,0x4a,0x3e,0x71,0x2c,0x2a,0x54,0x3c,0x3a,0x63,0x4f,
        0x43,0x75,0x27,0x79,0x5b,0x35,0x70,0x48,0x6b,0x56,0x6f,0x34,0x32,0x6c,0x30,0x61,0x6d,0x7b,0x2f,0x4b,0x64,0x38,
        0x2b,0x2e,0x50,0x40,0x3f,0x55,0x33,0x37,0x25,0x77,0x24,0x26,0x74,0x6a,0x28,0x53,0x4d,0x69,0x22,0x5c,0x44,0x31,
        0x36,0x58,0x3b,0x7a,0x51,0x5f,0x52,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0

    };

    uint8_t* ascii = (uint8_t*)str.c_str();
    for (int i = 0; i < str.size() && i < 16; i++)
    {
        int p = ascii[i] + i;
        if (p > 126) 
        {
            p = 32 + p % 127;
        }
        
        *output++ = (char)sequence[p];
    }
}

}

}

}
