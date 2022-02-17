//
// Copyright (C) 2000 Institut fuer Telematik, Universitaet Karlsruhe
// Copyright (C) 2004,2011 Andras Varga
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
#include "UDPCanframeSrv.h"
Define_Module(UDPCanframeSrv);

int UDPCanframeSrv::counter;
simsignal_t UDPCanframeSrv::sentPkSignal = SIMSIGNAL_NULL;
simsignal_t UDPCanframeSrv::rcvdPkSignal = SIMSIGNAL_NULL;

void UDPCanframeSrv::initialize(int stage)
{
    // because of IPvXAddressResolver, we need to wait until interfaces are registered,
    // address auto-assignment takes place etc.
    if (stage != 3)
        return;

    counter = 0;
    numSent = 0;
    numReceived = 0;
    serialNum = 0;
    WATCH(numSent);
    WATCH(numReceived);
    sentPkSignal = registerSignal("sentPk");
    rcvdPkSignal = registerSignal("rcvdPk");

    localPort = par("localPort");
    destPort = par("destPort");

    // 例えば、destAddresses="10 100"
    const char *destAddrs = par("destAddresses");
    cStringTokenizer tokenizer(destAddrs);
    const char *token;

    while ((token = tokenizer.nextToken()) != NULL)
        destAddresses.push_back(IPvXAddressResolver().resolve(token));

    socket.setOutputGate(gate("udpOut"));
    socket.bind(localPort);
    setSocketOptions();

    if (destAddresses.empty())
        return;

    stopTime = par("stopTime").doubleValue();
    simtime_t startTime = par("startTime").doubleValue();
    if (stopTime != 0 && stopTime <= startTime)
        error("Invalid startTime/stopTime parameters");

    cMessage *timerMsg = new cMessage("sendTimer");
    scheduleAt(startTime, timerMsg);
}

void UDPCanframeSrv::finish()
{
    recordScalar("packets sent", numSent);
    recordScalar("packets received", numReceived);
}

void UDPCanframeSrv::setSocketOptions()
{
    int timeToLive = par("timeToLive");
    if (timeToLive != -1)
        socket.setTimeToLive(timeToLive);

    int typeOfService = par("typeOfService");
    if (typeOfService != -1)
        socket.setTypeOfService(typeOfService);

    const char *multicastInterface = par("multicastInterface");
    if (multicastInterface[0])
    {
        IInterfaceTable *ift = InterfaceTableAccess().get(this);
        InterfaceEntry *ie = ift->getInterfaceByName(multicastInterface);
        if (!ie)
            throw cRuntimeError("Wrong multicastInterface setting: no interface named \"%s\"", multicastInterface);
        socket.setMulticastOutputInterface(ie->getInterfaceId());
    }

    bool receiveBroadcast = par("receiveBroadcast");
    if (receiveBroadcast)
        socket.setBroadcast(true);

    bool joinLocalMulticastGroups = par("joinLocalMulticastGroups");
    if (joinLocalMulticastGroups)
        socket.joinLocalMulticastGroups();
}

IPvXAddress UDPCanframeSrv::chooseDestAddr()
{
    // destaddressesに指定されたアドレス群から、ランダムでアドレスを取得する
    int k = intrand(destAddresses.size());
    return destAddresses[k];
}

CanEthernetCommand1 *UDPCanframeSrv::createPacket()
{
    int frameNum = 5;// UDPパケットに含むCANメッセージの数（TODO:ユーザーが入力できるように）
    int i;
    char msgName[32];
    sprintf(msgName, "UDPCanframeSrvData-%d", counter++);
    CanEthernetCommand1 *payload = new CanEthernetCommand1(msgName);
    payload->setType(CANETHERNET_COMMAND1);
    CanFrame *temp;

    // CANメッセージを生成し，UDPパケットのデータ部分へ入れる
    EV << "now packing ...." << endl;
    payload->setMessageIDArraySize(frameNum);
    payload->setDataArraySize(frameNum);
    payload->setFrameByteLengthArraySize(frameNum);
    for (i = 0 ; i < frameNum ; i++) {
    // CANメッセージを生成（ID：ランダム、Data：10、DLC：8）
    	payload->setMessageID(i, 1 + normal(10, 2));
    	payload->setData(i, 10);
    	payload->setFrameByteLength(i, 8);
    	EV << "CanAppSrv: Generating CAN Message `" << payload->getData(i) << payload->getMessageID(i) << "'\n";

    	temp = new CanFrame("hoge");
    	temp->setMessageID(payload->getMessageID(i));
        temp->setData(payload->getData(i));
        temp->setFrameByteLength(payload->getFrameByteLength(i));
    	// CANフレームへのポインタをリスト管理する
		(payload->getCanframes()).push_back(temp);
		//   	payload->insertCanFrame(temp);
    }
    payload->setFrameNum(frameNum);
    payload->setSerialNum(serialNum);
    serialNum++;
    if ( serialNum > 256) serialNum = 0;

    payload->setByteLength(par("messageLength").longValue());
  // payload->encapsulate(frames);
    EV << "finished packing ...." << endl;

    return payload;
}

void UDPCanframeSrv::sendPacket()
{
    CanEthernetCommand1 *payload = createPacket(); // パケット生成
    IPvXAddress destAddr = chooseDestAddr(); // 送信先アドレス決定

    emit(sentPkSignal, payload);
    socket.sendTo(payload, destAddr, destPort); // 送信
    numSent++;
}

void UDPCanframeSrv::handleMessage(cMessage *msg)
{
    if (msg->isSelfMessage())
    {
        // send, then reschedule next sending
        sendPacket();
        simtime_t d = simTime() + par("sendInterval").doubleValue();
        if (stopTime == 0 || d < stopTime)
            scheduleAt(d, msg);
        else
            delete msg;
    }
    else if (msg->getKind() == UDP_I_DATA)
    {
    	CanEthernetCommandTraffic *canethernetcommand_msg = check_and_cast<CanEthernetCommandTraffic *>(msg);
    	if (canethernetcommand_msg->getType() == CANETHERNET_COMMAND2) {
    		EV << "command2 receive.\n";
    		receiveCommand2(check_and_cast<CanEthernetCommand2*>(msg));
    	} else {
    		// process incoming packet
    		processPacket(PK(msg));
    	}
    }
    else if (msg->getKind() == UDP_I_ERROR)
    {
        EV << "Ignoring UDP error report\n";
        delete msg;
    }
    else
    {
        error("Unrecognized message (%s)%s", msg->getClassName(), msg->getName());
    }

    if (ev.isGUI())
    {
        char buf[40];
        sprintf(buf, "rcvd: %d pks\nsent: %d pks", numReceived, numSent);
        getDisplayString().setTagArg("t", 0, buf);
    }
}

void UDPCanframeSrv::processPacket(cPacket *pk)
{
    emit(rcvdPkSignal, pk);
    EV << "Received packet: " << UDPSocket::getReceivedPacketInfo(pk) << endl;
    delete pk;
    numReceived++;
}

void UDPCanframeSrv::receiveCommand2(CanEthernetCommand2 *pk)
{
	IPvXAddress destAddr = chooseDestAddr();
	CanEthernetCommand2 *frame = pk->dup();
	delete pk;
	emit(sentPkSignal, frame);
	socket.sendTo(frame, destAddr, destPort);
}
