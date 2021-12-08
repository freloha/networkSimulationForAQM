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

#include "inet/common/INETDefs.h"
#include "inet/common/queue/CodelActiveQueue.h"
#include "math.h"

namespace inet {

Define_Module(CodelActiveQueue);
//Simulation signals (or just signals) provide a way of publish-subscribe
//communication for models. Signals are represented by the type simsignal_t,
//are emitted on a module or channel using cComponent::emit(), and propagate up in the module tree.
//At any level, one may add listeners (cIListener) with cComponent::subscribe().
simsignal_t CodelActiveQueue::queueLengthSignal = registerSignal("queueLength");
simsignal_t CodelActiveQueue::dropSojournTimeSignal = registerSignal("dropSojournTime");
simsignal_t CodelActiveQueue::virtualSojournDelaySignal = registerSignal("virtualSojournDelay");
simsignal_t CodelActiveQueue::dropCountSignal = registerSignal("dropCount");
simsignal_t CodelActiveQueue::totalDropCountSignal = registerSignal("totalDropCount");

void CodelActiveQueue::initialize()
{
    PassiveQueueBase::initialize();
    next_drop_time = 0;
    total_drop_count = 0;
    drop_state = false;

    queue.setName(par("queueName"));
    emit(queueLengthSignal, 0);
    //Emits the given object as a signal.
    //If the given signal has listeners in this component or in ancestor components, their appropriate receiveSignal() methods are called. If there are no listeners, the runtime cost is usually minimal.
    outGate = gate("out");

    // configuration
    frameCapacity = par("frameCapacity"); // number of packets
    MTU = par("MTU"); // ethernet 1500
    adapt = par("adapt");
    interval = simtime_t(par("interval")); // simtime_t 는 simulationTime을 의미
    target = simtime_t(par("target"));
    gate_period = simtime_t(par("gate_period"));
    blocking_time = simtime_t(par("gate_rate") * gate_period);
}

cMessage *CodelActiveQueue::enqueue(cMessage *msg) // 이부분을 고쳐보자
{
    if (frameCapacity && queue.getLength() >= frameCapacity) {
        EV << "Queue full, dropping packet.\n";
        return msg;
    }
    else {
        msg->setTimestamp(simTime());
        queue.insert(msg);
        emit(queueLengthSignal, queue.getLength());
        return nullptr;
    }
}

cMessage *CodelActiveQueue::dequeue()
{
    if (queue.isEmpty())
        return nullptr;

    cMessage *msg = (cMessage *)queue.pop();
    emit(queueLengthSignal, queue.getLength());

    simtime_t dequeue_time = simTime();
    simtime_t enqueue_time = msg->getTimestamp();
    simtime_t sojourn_time = dequeue_time - enqueue_time;
    int delta;

    int n2 = dequeue_time/gate_period;
    simtime_t dequeue_open_time = gate_period*(n2); //마지막으로 gate가 열린시간
    simtime_t dequeue_close_time = dequeue_open_time + blocking_time; //다음 gate가 닫힐시간
    if(adapt)
    {
        if(enqueue_time < dequeue_open_time)
        {
            int n1 = enqueue_time/gate_period;
            simtime_t enqueue_open_time = gate_period*(n1); //마지막으로 gate가 열린시간
            simtime_t enqueue_close_time = enqueue_open_time + blocking_time; //마지막으로 gate가 닫힌시간

            int n = n2 - n1 - 1; // virtual sojourn time
            if(enqueue_close_time>enqueue_time)
            {
                sojourn_time = (enqueue_close_time - enqueue_time) + (dequeue_time - dequeue_open_time) + (n * blocking_time);
            }
            else
            {
                sojourn_time = (dequeue_time - dequeue_open_time) + (n * blocking_time);
            }
        }
        else
        {
            sojourn_time = dequeue_time - enqueue_time;
        }
    }

    if(drop_state)
    {
        if(sojourn_time < target || queue.getLength() < MTU)
        {
            drop_state = false;
        }
        else
        {
            while(dequeue_time >= next_drop_time && drop_state)
            {
                numQueueDropped++;
                msg = check_and_cast<cMessage*>(queue.pop());//drop
                emit(dropPkByQueueSignal, msg);
                emit(queueLengthSignal, queue.getLength());
                enqueue_time = msg->getTimestamp();


                sojourn_time = dequeue_time - enqueue_time;

                if(adapt)
                {
                    if(enqueue_time < dequeue_open_time)
                    {
                        int n1 = enqueue_time/gate_period;
                        simtime_t enqueue_open_time = gate_period*(n1);
                        simtime_t enqueue_close_time = enqueue_open_time + blocking_time;

                        int n = n2 - n1 - 1;
                        if(enqueue_close_time>enqueue_time)
                        {
                            sojourn_time = (enqueue_close_time - enqueue_time) + (dequeue_time - dequeue_open_time) + (n * blocking_time);
                        }
                        else
                        {
                            sojourn_time = (dequeue_time - dequeue_open_time) + (n * blocking_time);
                        }
                    }
                    else
                    {
                        sojourn_time = dequeue_time - enqueue_time;
                    }
                }

                count++;
                total_drop_count++;
                emit(dropCountSignal, count);
                emit(dropSojournTimeSignal, simtime_t(dequeue_time-(msg->getTimestamp())));
                emit(totalDropCountSignal, total_drop_count);

                if(sojourn_time < target || queue.getLength() < MTU)
                {
                    drop_state = false;
                }
                else
                {
                    next_drop_time = control_law(next_drop_time, count);

                    if(adapt)
                    {
                        if(dequeue_close_time < next_drop_time)
                        {
                            int n = (next_drop_time - dequeue_close_time)/blocking_time + 1;
                            next_drop_time = next_drop_time + blocking_time*n;
                        }
                    }
                }
            }
        }
    }
    else if(sojourn_time >= target && queue.getLength() >= MTU)
    {
        numQueueDropped++;
        emit(dropPkByQueueSignal, msg);
        msg = check_and_cast<cMessage*>(queue.pop());//drop
        drop_state = true;
        delta  = count - last_count;
        count = 1;
        total_drop_count++;

        if((delta > 1) && (dequeue_time - next_drop_time < 16 * interval))
            count = delta;
        emit(dropCountSignal, count);
        emit(totalDropCountSignal, total_drop_count);
        emit(dropSojournTimeSignal, simtime_t(dequeue_time-(msg->getTimestamp())));
        next_drop_time = control_law(dequeue_time, count);
        if(adapt)
        {
            if(dequeue_close_time < next_drop_time)
            {
                int n = (next_drop_time - dequeue_close_time)/blocking_time + 1;
                next_drop_time = next_drop_time + blocking_time*n;
            }
        }
        last_count = count;
    }
    emit(virtualSojournDelaySignal, sojourn_time);
    return msg;
}

cMessage *CodelActiveQueue::getFirstMsg()
{
    return check_and_cast<cMessage*>(queue.get(0));
}

simtime_t CodelActiveQueue::control_law(simtime_t t, int count)
{
    return t + interval/sqrt(count);
}

void CodelActiveQueue::sendOut(cMessage *msg)
{
    send(msg, outGate);
}

bool CodelActiveQueue::isEmpty()
{
    return queue.isEmpty();
}

} // namespace inet

