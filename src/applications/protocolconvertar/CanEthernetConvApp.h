/*
 * Copyright (C) 2003 Andras Varga; CTIE, Monash University, Australia
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */
#ifndef __INET_CanEthernetConvApp_H
#define __INET_CanEthernetConvApp_H


#include <vector>
#include <list>
#include "UDPSocket.h"
#include "INETDefs.h"
#include "IPassiveQueue.h"
#include "CanFrame_m.h"
#include "CanEthernet_m.h"


// Self-message kind values
#define CONVERT_REQUEST  1000

/**
 * Simple traffic generator for the Cannet model.
 */
class INET_API CanEthernetConvApp : public cSimpleModule, public cListener
{
 protected:
	struct SendMessageInfo{ //送信メッセージ情報の構造体
		int ID;
		int DLC;
		double SendInterval;
		std::vector<double> SendTime;
		double Offset;
		double gwdeadline;// 最悪応答時間解析値から算出したゲートウェイ内デッドライン(2013/01/22 add)
	};
	std::map<int,SendMessageInfo> SendMessageInfoList; // キー：メッセージID，値：送信メッセージ情報の構造体　とするmap

	class MessageQueue
	{
	protected:
		cQueue queue;
		int queueLimit;               // max queue length
		
	protected:
		static int packetCompare(cObject *a, cObject *b);  // PAUSE frames have higher priority
		
	public:
	MessageQueue(const char* name = NULL, int limit = 0) : queue(name, packetCompare), queueLimit(limit) {}
		void insertFrame(cObject *obj) { queue.insert(obj); }
		cObject *pop() { return queue.pop(); }
		cObject *getQueueFirst() { return queue.front(); }
		bool empty() const { return queue.empty(); }
		int getQueueLimit() const { return queueLimit; }
		bool isFull() const { return queueLimit != 0 && queue.length() >= queueLimit; }
		int length() const { return queue.length(); }
		void insertFrameBefore(cObject *where, cObject *obj) { queue.insertBefore(where, obj);}
		void insertFrameAfter(cObject *where, cObject *obj) { queue.insertAfter(where, obj);}
		void removeFrame(cObject *obj){queue.remove(obj);}
	};

	// UDPBasicApp.hよりコピー
    UDPSocket socket;
    int localPort, destPort;
    IPvXAddress destAddr;
    std::vector<IPvXAddress> destAddresses;
    simtime_t stopTime;

	// the CanMessage queue
	MessageQueue txQueue;

	// gate
	 cGate *upperLayerInGate;              // pointer to the "upperLayerIn" gate
	 cGate *upperLayerOutGate;             // pointer to the "upperLayerOut" gate

	// convert parameters
    cPar *convInterval;
    std::list<CanFrame *> canframes;

	// self messages
    cMessage *convReqMsg;
    simtime_t nextconvTime;

    // 4種アルゴリズムのための変数
    bool rdctConvPeriodMode;
    double rdctMaxTime;
    int urgencyPri;
    double graceTime;
    double graceRate;

	// receive statistics
    long packetsSent;
    long packetsReceived;
    static simsignal_t sentPkSignal;
    static simsignal_t rcvdPkSignal;
    static simsignal_t queueLengthSignal;
    static simsignal_t queueLengthZeroSignal;

	// statistics
	unsigned long countEthernetSent;	// Ethernet送信カウンタ
	unsigned long countEthernetReceive; // Ethernet受信カウンタ
	unsigned long countCanSent;			// CAN送信カウンタ
	unsigned long countCanReceive;		// CAN受信カウンタ
	unsigned long countAckSent;			// アクノリッジ送信カウンタ
	unsigned long countAckReceive;		// アクノリッジ受信カウンタ
	unsigned long countAckNG;			// アクノリッジ異常カウンタ

	unsigned long serialNum;
	unsigned long C2serialNum; // 先回受信したコマンド2フレームのシリアル番号
	
 public:
    CanEthernetConvApp();
    ~CanEthernetConvApp();

 protected:
    virtual void initialize(int stage);
    virtual void handleMessage(cMessage *msg);
    virtual void finish();

    virtual void receiveCanFrame(CanFrame *msg);
    virtual void receiveCommand1(CanEthernetCommand1 *frames);
    virtual void receiveCommand2(CanEthernetCommand2 *frames);
    virtual void receiveCommand3(CanEthernetCommand3 *frames);
    virtual void receiveCommand4(CanEthernetCommand4 *frames);
	virtual void printState();
	virtual unsigned long ToDec(const char str[ ]);
	
    // chooses random destination address
    virtual IPvXAddress chooseDestAddr();
    virtual CanEthernetCommand1 *createPacket();
    virtual void sendPacket();
    virtual void setSocketOptions();

 private:
	void handleSelfConvRequest();
};

#endif
