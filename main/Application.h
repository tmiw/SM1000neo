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

#ifndef EZDV_APPLICATION_H
#define EZDV_APPLICATION_H

#include "task/DVTask.h"
#include "task/DVTimer.h"
#include "network/WirelessTask.h"

using namespace ezdv::task;

namespace ezdv
{

class App : public DVTask
{
public:
    App();

protected:
    virtual void onTaskStart_() override;
    virtual void onTaskSleep_() override;
    virtual void onTaskTick_() override;
    
private:
    network::WirelessTask* wirelessTask_;
};

}

#endif // EZDV_APPLICATION_H
