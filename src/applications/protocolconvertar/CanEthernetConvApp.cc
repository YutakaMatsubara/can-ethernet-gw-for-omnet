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

#include <stdio.h>
#include <string.h>
#include <math.h>

#include "CanEthernetConvApp.h"
#include "UDPControlInfo_m.h"
#include "IPvXAddressResolver.h"
#include "InterfaceTableAccess.h"

Define_Module(CanEthernetConvApp);

simsignal_t CanEthernetConvApp::sentPkSignal = SIMSIGNAL_NULL;
simsignal_t CanEthernetConvApp::rcvdPkSignal = SIMSIGNAL_NULL;
simsignal_t CanEthernetConvApp::queueLengthSignal = SIMSIGNAL_NULL;
simsignal_t CanEthernetConvApp::queueLengthZeroSignal = SIMSIGNAL_NULL;

CanEthernetConvApp::CanEthernetConvApp()
{
	convReqMsg = NULL;
}

CanEthernetConvApp::~CanEthernetConvApp()
{
	cancelAndDelete(convReqMsg);
}

void CanEthernetConvApp::initialize(int stage)
{
	// statistics
	packetsSent = packetsReceived = 0;
	convInterval = &par("convInterval");

	sentPkSignal = registerSignal("sentPk");
	rcvdPkSignal = registerSignal("rcvdPk");

	queueLengthSignal = registerSignal("queueLength");
    queueLengthZeroSignal = registerSignal("queueLengthZero");
	
	upperLayerInGate = gate("upperLayerIn");
	upperLayerOutGate = gate("upperLayerOut");

	countEthernetSent = 0;		// Ethernet送信カウンタ
	countEthernetReceive = 0;	// Ethernet受信カウンタ
	countCanSent = 0;			// CAN送信カウンタ
	countCanReceive = 0;		// CAN受信カウンタ
	countAckSent = 0;			// アクノリッジ送信カウンタ
	countAckReceive = 0;		// アクノリッジ受信カウンタ
	countAckNG = 0;				// アクノリッジ異常カウンタ

	WATCH(countEthernetSent);
	WATCH(countEthernetReceive);
	WATCH(countCanSent);
	WATCH(countCanReceive);
	WATCH(countAckSent);
	WATCH(countAckReceive);

	serialNum = 0;
	C2serialNum = 0;

	// メッセージの情報（現在必要なのは周期のみ）をXMLから読み込む
	// CanAppsrv.ccの初期化部分より流用
	/******************************************/
	/* XMLの例
	 *
	 * <NodeInfo>
	 * 	   <Node ID="ECU1">
	 *	  	  <SendMessage SendTime="0" ID="15a" DLC="8" SendInterval="10" Offset="1000" gwdeadline="6000"/>
	 *	  	  <SendMessage SendTime="0" ID="20b" DLC="8" SendInterval="2" Offset="1000"/>
	 *	  	  <SendMessage SendTime="1 10 100" ID="25c" DLC="8" SendInterval="0" Offset="0"/>
	 * 	   </Node>
	 * </NodeInfo
	 *
	 */
	/******************************************/
	// 自ECUの送信メッセージリストを読み込む
	cXMLElementList nodes = ((par("NodeInfo").xmlValue())->getChildrenByTagName("Node"));
	for (int j = 0; j < (int)nodes.size(); j++) {
		cXMLElementList messages = ((nodes[j])->getChildrenByTagName("SendMessage"));

		// 送信メッセージ1つ1つに初期化処理をしていく
		for (int i = 0; i < (int)messages.size(); i++) {
			cXMLElement *message = messages[i];
			SendMessageInfo smInfo; // メッセージ情報を格納する構造体

			// 送信メッセージ情報の生成
			smInfo.ID = ToDec(message->getAttribute("ID"));
			smInfo.DLC = atoi(message->getAttribute("DLC"));
			smInfo.SendInterval = atof(message->getAttribute("SendInterval")) / 1000; // エクセルでの単位はミリ秒
			smInfo.Offset = atof(message->getAttribute("Offset")) / 1000000; // エクセルでの単位はマイクロ秒

			smInfo.gwdeadline = atof(message->getAttribute("gwdeadline"))/1000000; // エクセルでの単位はマイクロ秒（デッドライン型変換プロトコルでは指定する必要がある．それ以外のプロトコルではコメントアウトする）
	    	EV << "ID : " << smInfo.ID << "DLC : " << smInfo.DLC << " SendInterval : " << smInfo.SendInterval << " gwdeadline: " << smInfo.gwdeadline ;

	    	// イベント送信の絶対送信時刻SendTime="1 10 100"をint型の配列にする
	    	const char *SendTimes = message->getAttribute("SendTime");
	    	smInfo.SendTime = cStringTokenizer(SendTimes).asDoubleVector();
	    	cStringTokenizer tokenizer(SendTimes);
	    	const char *token;
	    	// 配列の要素数を数える
	    	int numsendTime = 0;
	    	while ((token = tokenizer.nextToken()) != NULL)	{ 
				numsendTime++;
			}
	    	EV << " SendTime : ";
	    	for (int j = 0; j < numsendTime; j++) {
	    		smInfo.SendTime[j] = smInfo.SendTime[j] / 1000;	// sendTimeはミリ秒
	    		EV << smInfo.SendTime[j] << " ";
	    	}
	    	EV << endl;

	    	//　データ構造(IDをキー，構造体を値とするハッシュ)に入れる
	    	SendMessageInfoList.insert(std::map<int,SendMessageInfo>::value_type(smInfo.ID,smInfo));
		}
	}

	// the CanMessage queue
	txQueue = CanEthernetConvApp::MessageQueue("GwQueue", par("queueLimit"));

	localPort = par("localPort");
	destPort = par("destPort");
	rdctConvPeriodMode = par("rdctConvPeriodMode");
	rdctMaxTime = par("rdctMaxTime");
	urgencyPri = par("urgencyPri");
	graceTime = par("graceTime"); // 許容猶予時間をINIファイルで定義しておき、ここで読み込む
	graceRate = par("graceRate");

	// 例えば、destAddresses="10 100"
    const char *destAddrs = par("destAddresses");
    cStringTokenizer tokenizer(destAddrs);
    const char *token;

    while ((token = tokenizer.nextToken()) != NULL) {
        destAddresses.push_back(IPvXAddressResolver().resolve(token));
	}

    socket.setOutputGate(gate("udpOut"));
    socket.bind(localPort);
    setSocketOptions();

    if (destAddresses.empty()) {
        return;
	}

	// initialize self messages
	convReqMsg = new cMessage("ConvertRequest", CONVERT_REQUEST);

	WATCH(packetsSent);
    WATCH(packetsReceived);

	// generate first message
	simtime_t startTime = par("startTime");
	scheduleAt(startTime, convReqMsg);
}

///////////////////////////////////////////////////////////////
// ここからUDPBasicApp.ccよりコピー
void CanEthernetConvApp::setSocketOptions()
{
    int timeToLive = par("timeToLive");
    if (timeToLive != -1) {
        socket.setTimeToLive(timeToLive);
	}

    int typeOfService = par("typeOfService");
    if (typeOfService != -1) {
        socket.setTypeOfService(typeOfService);
	}

    const char *multicastInterface = par("multicastInterface");
    if (multicastInterface[0]) {
        IInterfaceTable *ift = InterfaceTableAccess().get(this);
        InterfaceEntry *ie = ift->getInterfaceByName(multicastInterface);
        if (!ie) {
            throw cRuntimeError("Wrong multicastInterface setting: no interface named \"%s\"", multicastInterface);
		}
        socket.setMulticastOutputInterface(ie->getInterfaceId());
    }

    bool receiveBroadcast = par("receiveBroadcast");
    if (receiveBroadcast) {
        socket.setBroadcast(true);
	}

    bool joinLocalMulticastGroups = par("joinLocalMulticastGroups");
    if (joinLocalMulticastGroups) {
        socket.joinLocalMulticastGroups();
	}
}

IPvXAddress CanEthernetConvApp::chooseDestAddr()
{
	// 送信先アドレスをアドレス群から無作為に抽出
    int k = intrand(destAddresses.size());
    return destAddresses[k];
}

CanEthernetCommand1 *CanEthernetConvApp::createPacket()
{
	// 画面に表示されるメッセージの名前とバイト長のみを設定した空UDPパケットを生成
    char msgName[32];
    sprintf(msgName, "CanEthernetConvAppData-");

    CanEthernetCommand1 *payload = new CanEthernetCommand1(msgName);
    payload->setType(CANETHERNET_COMMAND1);
    payload->setByteLength(par("messageLength").longValue());
    return payload;
}

void CanEthernetConvApp::sendPacket()
{
	CanEthernetCommand1 *payload = createPacket();// パケット生成
	IPvXAddress destAddr = chooseDestAddr();// アドレス決定

    emit(sentPkSignal, payload);
    socket.sendTo(payload, destAddr, destPort);// 送信
   // numSent++;
}
///////////////////////////////////////////////////////////////
// ここまでUDPBasicApp.ccよりコピー

void CanEthernetConvApp::handleMessage(cMessage *msg)
{
	EV << "CanEthernetConvApp: hogehoge" << msg->getName() << " foo"<< endl;

	if (!msg->isSelfMessage()) {

		if ( dynamic_cast<CanFrame *>(msg) != NULL){
			// CANメッセージの場合（到着ゲートで分岐してもいいかも）
			receiveCanFrame(check_and_cast<CanFrame*>(msg));
		} else {
			//　Etherパケットの場合
			CanEthernetCommandTraffic *canethernetcommand_msg = check_and_cast<CanEthernetCommandTraffic *>(msg);
			switch (canethernetcommand_msg->getType()) {
			case CANETHERNET_COMMAND1:
			    // コマンド1フレーム受信時
				receiveCommand1(check_and_cast<CanEthernetCommand1*>(msg));
				break;

			case CANETHERNET_COMMAND2:
                // コマンド2フレーム受信時
				receiveCommand2(check_and_cast<CanEthernetCommand2*>(msg));
				break;

			case CANETHERNET_COMMAND3:
                // コマンド3フレーム受信時
				receiveCommand3(check_and_cast<CanEthernetCommand3*>(msg));
				break;

			case CANETHERNET_COMMAND4:
                // コマンド4フレーム受信時
				receiveCommand4(check_and_cast<CanEthernetCommand4*>(msg));
				break;

			default:
				error("Message with unexpected message type %d", canethernetcommand_msg->getType());
			}
		}
	}
	else {
		EV << "CanEthernetConvApp: Self-message " << msg << msg->getKind() << " received\n";

		switch (msg->getKind()) {
		case CONVERT_REQUEST:
			// 1ms周期の変換処理メッセージの場合
			handleSelfConvRequest();
			break;
		default:
			error("self-message with unexpected message kind %d", msg->getKind());

		}
	}
}

void CanEthernetConvApp::receiveCanFrame(CanFrame *msg)
{
	EV << "Received CAN Frame `" << msg->getName() << "'\n";

	printState();
	
	if (!txQueue.isFull()) {
		// キューに空きがある場合,キューに追加する
		EV << "CanEthernetConvApp: Queue is empty....\n";
		txQueue.insertFrame(check_and_cast<CanFrame *>(msg));
		//packetsReceived++;
		//    emit(rcvdPkSignal, msg);

		//（キューが満杯になった場合）
		//　現在出している周期変換メッセージをキャンセル（cancelEvent()をつかう
		//　変換処理関数handleSelfConvRequest()を呼び出す　// ここでバッファフル時の処理が終了
		//
		//（キューが満杯にはならなかった場合）
		//　（優先度を考慮する場合）
		//　　convinterval - 優先度*重み計数 ＞　現在時刻　なら
		//　　　現在出している周期変換メッセージをキャンセル
		//　　　convinterval - 優先度*重み計数 の時間を引数に新しくscheduleAtで変換メッセージを送る
		//　　convinterval - 優先度*重み計数　＜＝　現在時刻なら
		//　　　現在出してい周期変換メッセージをキャンセル
		//　　　変換処理関数handleSelfCOnvrequest()を呼び出す
		//　（優先度を考慮しない場合）

		//　　何もしない

		// NextConvTime：次の変換処理される時間（予定）を保持する変数　　　を追加する必要あり

		// 上のコメントを実装
		 if(txQueue.isFull()) {
			cancelEvent(convReqMsg);
			handleSelfConvRequest();
		 } else {
			 // ここにデッドライン型のIF文を記述すればよい
			 /* （処理）
			  * if nextconvtime > 生成時刻＋受信したメッセージの周期ー許容猶予時間graceTime（INIファイルで定義、NEDでデフォルト値を定義）
			  * 	新しく変換要求メッセージを送る（時間はIF判定式の右辺の時間値)
			  * 	未実装[案１：ひとつだけ時間が迫ってるやつを取り出して送信する]
			  * 	まずはこちらを実装[案２：時間が迫っているやつがきた時点でバッファにたまっているものをまとめてパッキングして送信する]
			  * else
			  * 	なにもしない
			  */
			 // MessageIDをキーとして連想配列から値を読みだし、各メッセージの絶対時刻デッドラインを算出
		     // --> GW内デッドラインを静的に計算し、ルーティングテーブルと一緒に保持するように変更。(2013/01/22 add)
		     // --> 変数gwdeadlineに連想配列から読みだしたGW内デッドラインを読込む
			 std::map<int,SendMessageInfo>::iterator itr;
			 itr = SendMessageInfoList.find(msg->getMessageID());
			 SendMessageInfo value;
			 double deadline;
			 double gwdeadline;
			 double latencyTime;
			 if (itr != SendMessageInfoList.end()) // マップが end では無い場合（つまりキーにヒットする値が存在した場合）
			 {
				 // 値も取得します。
				 value = itr->second;
				 deadline = value.SendInterval;

				 //              gwdeadline = value.gwdeadline; // 最悪応答時間解析値を用いる場合
				 latencyTime = deadline*graceRate; // デッドラインまでの割合を用いる場合
			 }
			 // ヒットする値がなかった場合
			 else
			 {
				 EV << "ヒットする値が見つかりませんでした。" << endl;
			 }

			 EV << "猶予時間:" << msg->getCreationTime() + deadline - latencyTime ;
			 EV << "GW内デッドライン:" << gwdeadline + simTime();
			 EV << "次回変換予定時間：" << nextconvTime << endl;
			 /*************************************************************************************/
			 // CANメッセージに関わらず一定値の場合
			 if(graceTime > 0 && simTime() >= msg->getCreationTime() + deadline - graceTime){//GWデッドラインが現在時刻よりも手前を示しているとき
			     EV << "graceTime : Deadline is now!! \n";
			     // 急ぐメッセージをキューの先頭につける
			     if(txQueue.length() > 1){
			         txQueue.removeFrame(msg);
			         txQueue.insertFrameBefore(txQueue.getQueueFirst(),msg);
			     }

			     cancelEvent(convReqMsg);//現在出している変換要求メッセージを取消
			     handleSelfConvRequest();//すぐに変換処理にはいる
			 }else if(graceTime > 0 && nextconvTime > msg->getCreationTime() + deadline - graceTime){//GWデッドラインが次回変換予定時刻より手前を示しているとき
			     EV << "graceTime : Deadline is approaching more!! \n";
			     // 急ぐメッセージをキューの先頭につける
			     if(txQueue.length() > 1){
			         txQueue.removeFrame(msg);
			         txQueue.insertFrameBefore(txQueue.getQueueFirst(),msg);
			     }
			     cancelEvent(convReqMsg);//現在出している変換要求メッセージを取消
			     scheduleAt(msg->getCreationTime() + deadline - graceTime, convReqMsg);//GWデッドラインの時刻に次回の変換を行うように再設定
			     nextconvTime = msg->getCreationTime() + deadline - graceTime;
			 }
             /*************************************************************************************/

			 /*************************************************************************************/
			 // 周期に対して一 定割合の場合(例えば周期の50%など)
			 if(graceRate > 0 && simTime() >= msg->getCreationTime() + deadline - latencyTime){
				 EV << "graceTime : Deadline is now!! \n";
				 // 急ぐメッセージをキューの先頭につける
				 if(txQueue.length() > 1){
					 txQueue.removeFrame(msg);
					 txQueue.insertFrameBefore(txQueue.getQueueFirst(),msg);
				 }

				 cancelEvent(convReqMsg);
				 handleSelfConvRequest();
			 }else if(graceRate > 0 && nextconvTime > msg->getCreationTime() + deadline - latencyTime){
				 EV << "graceTime : Deadline is approaching more!! \n";
				 // 急ぐメッセージをキューの先頭につける
				 if(txQueue.length() > 1){
					 txQueue.removeFrame(msg);
					 txQueue.insertFrameBefore(txQueue.getQueueFirst(),msg);
				 }
				 cancelEvent(convReqMsg);
				 scheduleAt(msg->getCreationTime() + deadline - latencyTime, convReqMsg);
				 nextconvTime = msg->getCreationTime() + deadline - latencyTime;
			 }
             /*************************************************************************************/

			 // 緊急型の場合の処理
			 if(msg->getMessageID() < urgencyPri){//緊急メッセージに指定されているメッセージが到着した場合
				 EV << "!!! urgency-message !!!\n";
					cancelEvent(convReqMsg);//現在出している変換要求メッセージを取消
					handleSelfConvRequest();//すぐに変換処理にとりかかる
			 }else if(rdctMaxTime > 0){
					EV << "reduction ConvPeriod : true  \n";
					EV << "current simulation time : " << simTime() << endl;
					EV << "nextConvTime : " << nextconvTime << endl;
					EV << "msg->getMwssageID" << msg->getMessageID() << endl;
					EV << "rdctMaxTime/msg->getMessageID()" << rdctMaxTime / msg->getMessageID() << endl;
					EV << "nextconvTime" << nextconvTime - rdctMaxTime / msg->getMessageID() << endl;
					if( nextconvTime - rdctMaxTime / msg->getMessageID() > simTime()){
						EV << "rdctconvTime : true \n";
						cancelEvent(convReqMsg);
						scheduleAt((nextconvTime - rdctMaxTime / msg->getMessageID()), convReqMsg);
						nextconvTime = nextconvTime - rdctMaxTime / msg->getMessageID();
					} else {
						EV << "rdctconvTime : false -> packing Start!! \n";
						cancelEvent(convReqMsg);
						handleSelfConvRequest();
					}

			 }
		}

	}else{
		// キューが満杯の場合，到着したCANメッセージを破棄する
		EV << "CanEthernetConvApp: Queue is full!!!!\n";
		delete msg;
	}
}


void CanEthernetConvApp::handleSelfConvRequest()
{
	int i;
	EV << "Start Message-Conversion!!\n";

	// 定期処理
	// キューの中身をチェック
	if (txQueue.empty()) {
		EV << "Queue is empty!" << endl;
        emit(queueLengthZeroSignal,txQueue.length()); // 変換時のキューの長さをシグナル出力する

		// 中身がなければ空パケットを作成、Ethernet側へ送る
		sendPacket();

	} else {
		EV << "Queue has some can-message!" << endl;
		emit(queueLengthSignal,txQueue.length()); // 変換時のキューの長さをシグナル出力する

		// 中身があれば以下の処理をする
		// キュー内CANメッセージをUDPパケットにまとめる
		// Ethernet側へ送る

		while(txQueue.getQueueFirst() != NULL){
			int msgLength = 0;
			char msgName[32];
			sprintf(msgName, "CanEthernetConvAppData-");

			CanEthernetCommand1 *payload = new CanEthernetCommand1(msgName);
			//CanEthernetCommand1 *frames = new CanEthernetCommand1(); // CanFrameの各要素を配列で持つクラス
			payload->setType(CANETHERNET_COMMAND1);
			CanFrame *temp ;

			int frameNum;

			IPvXAddress destAddr = chooseDestAddr();
			//payload = new CanEthernetCommand1(msgName);

			EV << "now packing ...." << endl;
			frameNum = txQueue.length();
			// キューの長さ(格納されてるCANメッセージ数)だけ配列を確保する（上限256）（TODO：256を超えた場合の残りCANメッセージの扱いを実装）
			if (frameNum > 256) {
				payload->setMessageIDArraySize(256);
				payload->setDataArraySize(256);
				payload->setFrameByteLengthArraySize(256);
				payload->setCreationTimeArraySize(256);
			} else {
				payload->setMessageIDArraySize(frameNum);
				payload->setDataArraySize(frameNum);
				payload->setFrameByteLengthArraySize(frameNum);
				payload->setCreationTimeArraySize(frameNum);
			}
			payload->setFrameNum(frameNum);
			serialNum++;
			if ( serialNum > 256) serialNum = 0;
			payload->setSerialNum(serialNum);

			// CANメッセージを一つずつUDPパケットの中へパッキングしていく
			for (i = 0 ; i < frameNum ; i++) {
				//msgLength = msgLength + (check_and_cast<cMessage*>(txQueue.pop())).getByteLength();
				temp = check_and_cast<CanFrame *>(txQueue.pop());
				payload->setMessageID(i,temp->getMessageID());
				payload->setData(i,temp->getData());
				payload->setFrameByteLength(i,temp->getFrameByteLength());
				payload->setCreationTime(i,temp->getCreationTime());//メッセージの生成時刻を情報として保持させる
				EV << "frame[" << i << "] MessageID Data FrameByteLength" << payload->getMessageID(i) << payload->getData(i) << payload->getFrameByteLength(i) << endl;

				// CANフレームへのポインタをリスト管理する
				// insertCanFrame(temp);
				(payload->getCanframes()).push_back(temp);
				//take(temp);
				//frames.push_back(temp);

				//delete temp;
				if(txQueue.getQueueFirst() == NULL) 	break;
			}
			// CANメッセージを複数保有するframesをカプセル化しデータとして乗せる
			//payload2->encapsulate(frames);

			EV << "finished packing ....(msgLength = " << msgLength << " ) "<< endl;
			msgLength = 4 + 16 * frameNum;
			payload->setByteLength(msgLength);
			emit(sentPkSignal, payload);
			socket.sendTo(payload, destAddr, destPort);

			// カウンタインクリメント
			countEthernetSent++; // Ethernet送信カウンタ
			countCanSent = countCanSent + frameNum; // CAN送信カウンタ
		}
	}
	// 次のメッセージ変換のための変換要求を自分に送る．
	scheduleAt((simTime() + convInterval->doubleValue()), convReqMsg);
	nextconvTime = simTime() + convInterval->doubleValue();
}

void CanEthernetConvApp::receiveCommand1(CanEthernetCommand1 *frames)
{
	EV << "CanEthernetConvApp: receiveCommand1"<< endl;
	printState();
	int i, fNum, sNum;

	// コマンド1
	// CANメッセージに分割しCANメッセージルータへ送信
	fNum = frames->getFrameNum();
	sNum = frames->getSerialNum();

	for (i = 0 ; i < fNum ; i++) {
		CanFrame *frame = (frames->getCanframes()).front();
		EV << "frame data : " << frame->getData() << endl;
		(frames->getCanframes()).remove(frame);

		//		frame->setData(frame->getData() + 1);//GW通過したことが分かるようにDataを1だけインクリメントする

		CanFrame *msg = new CanFrame("hoge");
		msg->setMessageID(frames->getMessageID(i));
		msg->setData(frames->getData(i));
		msg->setFrameByteLength(frames->getFrameByteLength(i));	//msgLength = msgLength + (check_and_cast<cMessage*>(txQueue.pop())).getByteLength();
		char canmsgName[32];
		sprintf(canmsgName,"0x%04x",msg->getMessageID());
		msg->setName(canmsgName);

		EV << "CanEthernetConvApp: Generating CAN Message `" << frame->getName() << frame->getMessageID() << "'\n";
		emit(sentPkSignal, msg);
		send(msg, upperLayerOutGate);
		EV << "CanEthernetConvApp: Sent CAN Message `" << frame->getName() << "'\n";
	}

	delete frames;
	// カウンタインクリメント
	countEthernetReceive++; // Ethernet受信カウンタ
	countCanReceive = countCanReceive + fNum; // CAN受信カウンタ

	// ACK送信
	char msgName[32];
	sprintf(msgName, "CanEthernetConvAppData-");

	CanEthernetCommand2 *frames2 = new CanEthernetCommand2(); // ACK
	frames2->setType(CANETHERNET_COMMAND2);
	IPvXAddress destAddr = chooseDestAddr();

	frames2->setSerialNum(sNum);
	frames2->setFrameNum(fNum);
	//emit(sentPkSignal, frames2);
	socket.sendTo(frames2, destAddr, destPort);

	// ACKカウンタインクリメント
	countAckSent++; // アクノリッジ送信カウンタ

}

void CanEthernetConvApp::receiveCommand2(CanEthernetCommand2 *frames)
{
	EV << "CanEthernetConvApp: receiveCommand2"<< endl;
	printState();
	//CanEthernetCommand2 *frames = check_and_cast<CanEthernetCommand2 *>(pk->decapsulate());
	int sNum;
	// コマンド2
	countAckReceive++; // アクノリッジ受信カウンタ

	sNum = frames->getSerialNum();
	if ((sNum == 0)&&(C2serialNum == 0)) EV << "ack OK\n";
	else if ( (sNum - C2serialNum) != 1 ) countAckNG++; // アクノリッジ異常カウンタ

	C2serialNum = sNum;

	delete frames;
}

void CanEthernetConvApp::receiveCommand3(CanEthernetCommand3 *frames)
{
	EV << "CanEthernetConvApp: receiveCommand3"<< endl;
	printState();

	// コマンド3（ステータス要求はシミュレータ上で確認できるため、何もしない）
}

void CanEthernetConvApp::receiveCommand4(CanEthernetCommand4 *frames)
{
	EV << "CanEthernetConvApp: receiveCommand4"<< endl;
	printState();

	// コマンド4（何もしない）
}

void CanEthernetConvApp::finish()
{
}


int CanEthernetConvApp::MessageQueue::packetCompare(cObject *a, cObject *b)
{
    int ap = (dynamic_cast<CanFrame*>(a) == NULL) ? 1 : 0;
    int bp = (dynamic_cast<CanFrame*>(b) == NULL) ? 1 : 0;
    return ap - bp;
}

void CanEthernetConvApp::printState()
{
	EV << "********printState()**************************" << endl;
	EV << " countEthernetSent: " << countEthernetSent << endl;
	EV << " countEthernetReceive: " << countEthernetReceive << endl;
	EV << " countCanSent: " << countCanSent << endl;
	EV << " countCanReceive: " << countCanReceive << endl;
	EV << " countAckSent: " << countAckSent << endl;
	EV << " countAckReceive: " << countAckReceive << endl;
	EV << " countAckNG: " << countAckNG << endl;

	EV << "queueLength: " << txQueue.length() << endl;
	EV << "********printState()**************************" << endl;
}

/* 16 進文字列を 10 進数に変換する */
unsigned long CanEthernetConvApp::ToDec(const char str[ ])
{
   short i = 0;        /* 配列の添字として使用 */
   short n;
   unsigned long x = 0;
   char c;

   while (str[i] != '\0') {        /* 文字列の末尾でなければ */

           /* '0' から '9' の文字なら */
       if ('0' <= str[i] && str[i] <= '9')
           n = str[i] - '0';        /* 数字に変換 */

           /* 'a' から 'f' の文字なら */
       else if ('a' <= (c = tolower(str[i])) && c <= 'f')
           n = c - 'a' + 10;        /* 数字に変換 */

       else {        /* それ以外の文字なら */
    	   printf("%s",str);
           printf("無効な文字です。\n");
           exit(0);        /* プログラムを終了させる */
       }
       i++;        /* 次の文字を指す */

       x = x *16 + n;    /* 桁上がり */
   }
   return (x);
}

