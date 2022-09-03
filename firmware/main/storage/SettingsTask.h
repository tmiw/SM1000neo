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
    virtual void onTaskStart_(DVTask* origin, TaskStartMessage* message) override;
    virtual void onTaskWake_(DVTask* origin, TaskWakeMessage* message) override;
    virtual void onTaskSleep_(DVTask* origin, TaskSleepMessage* message) override;

private:
    int8_t leftChannelVolume_;
    int8_t rightChannelVolume_;
    DVTimer commitTimer_;
    std::shared_ptr<nvs::NVSHandle> storageHandle_;

    void loadAllSettings_();
    void commit_();
    void setLeftChannelVolume_(int8_t vol);
    void setRightChannelVolume_(int8_t vol);
};

} // namespace storage

} // namespace ezdv


#endif // SETTINGS_TASK_H