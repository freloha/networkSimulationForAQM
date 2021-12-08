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

#ifndef __INET_CodelActiveQueue_H
#define __INET_CodelActiveQueue_H

#include "inet/common/INETDefs.h"
#include "inet/common/queue/PassiveQueueBase.h"

namespace inet {

class INET_API CodelActiveQueue : public PassiveQueueBase
{
    protected:
      // configuration
      int frameCapacity;
      simtime_t interval;
      simtime_t target;
      int MTU;
      int count = 0;
      int last_count = 0;
      int total_drop_count = 0;
      simtime_t next_drop_time = 0;
      bool drop_state = false;

      // for gated adopt codel
      int adapt;
      simtime_t blocking_time;
      simtime_t gate_period;

      // state
      cQueue queue;
      cGate *outGate;

      // statistics
      static simsignal_t queueLengthSignal;
      static simsignal_t dropSojournTimeSignal;
      static simsignal_t virtualSojournDelaySignal;
      static simsignal_t dropCountSignal;
      static simsignal_t totalDropCountSignal;

    protected:
      virtual void initialize() override;
      /**
       * Redefined from PassiveQueueBase.
       */
      virtual cMessage *enqueue(cMessage *msg) override;

      /**
       * Redefined from PassiveQueueBase.
       */
      virtual cMessage *dequeue() override;

      virtual simtime_t control_law(simtime_t t, int count);

      /**
       * Redefined from PassiveQueueBase.
       */
      virtual void sendOut(cMessage *msg) override;

      /**
       * Redefined from IPassiveQueue.
       */
      virtual bool isEmpty() override;

    public:
      cMessage *getFirstMsg();
};

} // namespace inet

#endif // ifndef __INET_CodelActiveQueue_H

