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
    gatetime = simtime_t(par("gate_rate") * gate_period); // ���밪
    gate = true;
    delayed_count = 0;
}

void GatedScheduler::handleMessage(cMessage *msg) {
    if (msg->isSelfMessage()) { // �� ���� ���� �ƴ϶��, is posted by scheduleAt
        if (msg->getKind() == 0) { // ���� ���� ���� ����
            if (packetsToBeRequestedFromInputs > 0) {
                bool success = schedulePacket();
                if (success) //requestPacket(), ���� slot < 0 �̸� ��� �� ������ �ݺ���
                    packetsToBeRequestedFromInputs--;
            } else if (packetsRequestedFromUs == 0)
                notifyListeners();
        } else { // ���� ���� ����
            emit(outTimeSignal, 0); // �ƹ��͵� ����
        }
        msg->~cMessage();
    } else { // �� �������̶��
        ASSERT(packetsRequestedFromUs > 0);
        packetsRequestedFromUs--;
        sendOut(msg);

        cPacket *packet = dynamic_cast<cPacket *>(msg);
        int64_t length = packet->getBitLength(); // ��Ŷ�� bit
        double length4 = ((length + 3) / 4);
        simtime_t duration = simtime_t(length4) / 25000000; // ��Ŷ�� �����µ� �ɸ��� �ð�
        emit(outTimeSignal, duration); // duration��ŭ ��Ŷ�� ����
        cMessage *outendEvent = new cMessage("outend", 1); // ��Ŷ�� �� ���´ٴ� ��ȣ, ������ CRC, SMD_C�� �������� ���� ����, 1�̸� ��, 0�̸� �����ִ�
        scheduleAt(simTime() + duration, outendEvent);
    }
}

bool GatedScheduler::schedulePacket() { // gate_period = 0.01s, gate_rate = 0.1
    start: simtime_t current_t = simTime();
    int a = current_t / gate_period; // ���� �ð� * 100
    deqtime = current_t - gate_period * a; // dequeue�� ������ �ð�
    simtime_t next_t = gate_period * (a + 1); // ���� dequeue�ð�??
    saved = 0;

    if (slot < 0) { // gated�� �ƴϹǷ� �ǽð����� ��Ŷ�� ��û�Ѵ�
        for (auto inputQueue : inputQueues) {

            if (!inputQueue->isEmpty()) {
                inputQueue->requestPacket();
                return true;
            }
        }
    } else { // gated�� ����
        for (auto inputQueue : inputQueues) { // inputQueues = object, inputQueue = name
            if (!inputQueue->isEmpty()) {

                if (deqtime < gatetime) { // gated �ð��� �������� �ʾҴٸ�, deqtime = current_t - gate_period * a;
                    gate = true; // gate�� ����
                    aqueue = check_and_cast<CodelActiveQueue *>(inputQueue);
                    cMessage *msg = aqueue->getFirstMsg(); // queue���� ù��° �޼����� ������ ����
                    cPacket *packet = dynamic_cast<cPacket *>(msg); // queue���� ������ ��
                    int64_t length = packet->getBitLength();
                    double length4 = ((length + 3) / 4);
                    simtime_t duration = simtime_t(length4) / 25000000;
                    if (deqtime + duration < gatetime) { // ��Ŷ ���� �ð����� gate�� �����ִٸ�
                        aqueue->requestPacket(); // requestPacket�� �ؾ� dequeue�� �̷������ ��Ŷ������ ���۵�
                        return true;
                    } else if (inputQueues.back() == inputQueue) { //inputQueues�� ������ �����Ͱ� ���� ���� ���ٸ�, �� �� �κ��� ���ľ���
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
                        emit(utilRateSignal, deqtime / gatetime); // �־��� �ð����� �󸶳� ���ǰ� �ִ���
                    }
                }

                gate = false;
                cMessage *delayEvent = new cMessage("delay", 0); // ���� segment�� �����ִ�, ("message", kind)
                scheduleAt(next_t, delayEvent); // next_t�� gated�� non_gated�� �����ϴ� �� ����
                return false;

            } // !queue.isEmpty()������ ��
        } // for��
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

