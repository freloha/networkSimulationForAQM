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

#include "inet/common/queue/GatedScheduler.h"
#include "inet/linklayer/ethernet/EtherFrame.h"
#include "inet/common/queue/CodelActiveQueue.h"

namespace inet {

Define_Module(GatedScheduler);

simsignal_t GatedScheduler::unvfgtTimeSignal = registerSignal("unvfgtTime");
simsignal_t GatedScheduler::utilRateSignal = registerSignal("utilRate");
simsignal_t GatedScheduler::outTimeSignal = registerSignal("outTime");

void GatedScheduler::initialize() {
    SchedulerBase::initialize();
    slot = par("slot");
    gate_period = simtime_t(par("gate_period"));
    gatetime = simtime_t(par("gate_rate") * gate_period); // 절대값
    gate = true;
    delayed_count = 0;
}

void GatedScheduler::handleMessage(cMessage *msg) {
    if (msg->isSelfMessage()) { // 다 보낸 것이 아니라면, is posted by scheduleAt
        if (msg->getKind() == 0) { // 아직 보낼 것이 있음
            if (packetsToBeRequestedFromInputs > 0) {
                bool success = schedulePacket();
                if (success) //requestPacket(), 만일 slot < 0 이면 계속 이 구문만 반복됨
                    packetsToBeRequestedFromInputs--;
            } else if (packetsRequestedFromUs == 0)
                notifyListeners();
        } else { // 보낼 것이 없음
            emit(outTimeSignal, 0); // 아무것도 안함
        }
        msg->~cMessage();
    } else { // 다 보낸것이라면
        ASSERT(packetsRequestedFromUs > 0);
        packetsRequestedFromUs--;
        sendOut(msg);

        cPacket *packet = dynamic_cast<cPacket *>(msg);
        int64_t length = packet->getBitLength(); // 패킷의 bit
        double length4 = ((length + 3) / 4);
        simtime_t duration = simtime_t(length4) / 25000000; // 패킷을 보내는데 걸리는 시간
        emit(outTimeSignal, duration); // duration만큼 패킷을 보냄
        cMessage *outendEvent = new cMessage("outend", 1); // 패킷을 다 보냈다는 신호, 일종의 CRC, SMD_C의 마지막과 같은 역할, 1이면 끝, 0이면 남아있다
        scheduleAt(simTime() + duration, outendEvent);
    }
}

bool GatedScheduler::schedulePacket() { // gate_period = 0.01s, gate_rate = 0.1
    start: simtime_t current_t = simTime();
    int a = current_t / gate_period; // 현재 시간 * 100
    deqtime = current_t - gate_period * a; // dequeue가 가능한 시간
    simtime_t next_t = gate_period * (a + 1); // 다음 dequeue시간??
    saved = 0;

    if (slot < 0) { // gated가 아니므로 실시간으로 패킷을 요청한다
        for (auto inputQueue : inputQueues) {

            if (!inputQueue->isEmpty()) {
                inputQueue->requestPacket();
                return true;
            }
        }
    } else { // gated인 상태
        for (auto inputQueue : inputQueues) { // inputQueues = object, inputQueue = name
            if (!inputQueue->isEmpty()) {

                if (deqtime < gatetime) { // gated 시간에 도달하지 않았다면, deqtime = current_t - gate_period * a;
                    gate = true; // gate를 열고
                    aqueue = check_and_cast<CodelActiveQueue *>(inputQueue);
                    cMessage *msg = aqueue->getFirstMsg(); // queue에서 첫번째 메세지를 가지고 나옴
                    cPacket *packet = dynamic_cast<cPacket *>(msg); // queue에서 빼오는 것
                    int64_t length = packet->getBitLength();
                    double length4 = ((length + 3) / 4);
                    simtime_t duration = simtime_t(length4) / 25000000;
                    if (deqtime + duration < gatetime) { // 패킷 전송 시간동안 gate가 열려있다면
                        aqueue->requestPacket(); // requestPacket을 해야 dequeue가 이루어지며 패킷전송이 시작됨
                        return true;
                    } else if (inputQueues.back() == inputQueue) { //inputQueues의 마지막 포인터가 현재 값과 같다면, 즉 이 부분을 고쳐야함
                        delayed_count++;


                        if (saved > 0) {
                            gatetime -= saved;
                            saved = 0;
                            return false;
                        } else {
                            int64_t newL = ((length + 99) / 4);
                            simtime_t next = simtime_t(newL) / 25000000;
                            next_t = next_t + next;
                            saved = (deqtime + next) - gatetime;
                        }


                        emit(unvfgtTimeSignal, simtime_t(gatetime - deqtime));
                        emit(utilRateSignal, deqtime / gatetime); // 주어진 시간동안 얼마나 사용되고 있는지
                    }
                }

                gate = false;
                cMessage *delayEvent = new cMessage("delay", 0); // 보낼 segment가 남아있다, ("message", kind)
                scheduleAt(next_t, delayEvent); // next_t로 gated와 non_gated를 구분하는 것 같음
                return false;

            } // !queue.isEmpty()구문의 끝
        } // for문
    }
    return false;
}

void GatedScheduler::refreshDisplay() const {
    char buf[100];
    sprintf(buf, "gate: %s\nq delayed: %d\np req: %d", gate ? "open" : "close",
            delayed_count, packetsToBeRequestedFromInputs);
    getDisplayString().setTagArg("t", 0, buf);
}

} // namespace inet

