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

#include "DVTimer.h"

extern "C"
{
    DV_EVENT_DEFINE_BASE(DV_TASK_TIMER_MESSAGE);
}

namespace ezdv
{

namespace task
{
    
DVTimer::DVTimer(DVTask* owner, TimerHandlerFn fn, uint64_t intervalInMicroseconds, const char* timerName)
    : owner_(owner)
    , intervalInMicroseconds_(intervalInMicroseconds)
    , running_(false)
    , once_(false)
{
    fn_ = new TimerHandlerFnForwarder(fn);
    assert(fn_ != nullptr);
    
    esp_timer_create_args_t args = {
        .callback = &OnESPTimerFire_,
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "DVTimer",
        .skip_unhandled_events = true,
    };
    
    if (timerName != nullptr)
    {
        args.name = timerName;
    }
    
    ESP_ERROR_CHECK(
        esp_timer_create(
            &args, &timerHandle_
        )
    );
    
    owner->registerMessageHandler(this, &DVTimer::onTimerFire_);
}

DVTimer::~DVTimer()
{
    stop();
    
    ESP_ERROR_CHECK(
        esp_timer_delete(timerHandle_)
    );

    delete fn_;
}

void DVTimer::changeInterval(uint64_t intervalInMicroseconds)
{
    bool isRunning = running_;

    if (isRunning)
    {
        stop();
    }

    intervalInMicroseconds_ = intervalInMicroseconds;

    if (isRunning)
    {
        start(once_);
    }
}

void DVTimer::start(bool once)
{
    if (!running_)
    {
        if (once)
        {
            once_ = true;
            ESP_ERROR_CHECK(esp_timer_start_once(timerHandle_, intervalInMicroseconds_));
        }
        else
        {
            ESP_ERROR_CHECK(esp_timer_start_periodic(timerHandle_, intervalInMicroseconds_));
        }
        
        running_ = true;
    }
}

void DVTimer::stop()
{
    if (running_)
    {
        ESP_ERROR_CHECK(esp_timer_stop(timerHandle_));
        running_ = false;
        once_ = false;
    }
}

void DVTimer::onTimerFire_(DVTask* origin, TimerFireMessage* message)
{
    if (message->timer == this)
    {
        fn_->call(this);
    }
}

void DVTimer::OnESPTimerFire_(void* ptr)
{
    DVTimer* obj = (DVTimer*)ptr;
    
    if (obj->once_)
    {
        obj->once_ = false;
        obj->running_ = false;
    }

    TimerFireMessage message(obj);
    obj->owner_->postTimer(&message);
}

}

}