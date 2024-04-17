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

#include "IcomAudioStateMachine.h"

namespace ezdv
{

namespace network
{

namespace icom
{
    
IcomAudioStateMachine::IcomAudioStateMachine(DVTask* owner)
    : IcomStateMachine(owner)
    , areYouThereState_(this)
    , areYouReadyState_(this)
    , audioState_(this)
{
    addState_(IcomProtocolState::ARE_YOU_THERE, &areYouThereState_);
    addState_(IcomProtocolState::ARE_YOU_READY, &areYouReadyState_);
    addState_(IcomProtocolState::AUDIO, &audioState_);
}

std::string IcomAudioStateMachine::getName_()
{
    return "IcomAudio";
}
    
}

}

}