//
// Copyright (C) 2012 Opensim Ltd.
// Author: Tamas Borbely
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program; if not, see <http://www.gnu.org/licenses/>.
//

#ifndef __INET_GATEDSCHEDULER_H
#define __INET_GATEDSCHEDULER_H

#include "inet/common/INETDefs.h"
#include "inet/common/queue/SchedulerBase.h"
#include "inet/common/queue/CodelActiveQueue.h"

namespace inet {

class INET_API GatedScheduler : public SchedulerBase
{
  protected:
    int slot;
    bool safe;
    int delayed_count;
    simtime_t deqtime;
    simtime_t gatetime;
    simtime_t gate_period;
    simtime_t saved;
    bool gate;
    CodelActiveQueue *aqueue;

    static simsignal_t unvfgtTimeSignal;
    static simsignal_t utilRateSignal;
    static simsignal_t outTimeSignal;

  protected:
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;
    virtual bool schedulePacket() override;
    virtual void refreshDisplay() const override;
    bool schedulePacket(bool safe);
};

} // namespace inet

#endif // ifndef __INET_GATEDSCHEDULER_H

