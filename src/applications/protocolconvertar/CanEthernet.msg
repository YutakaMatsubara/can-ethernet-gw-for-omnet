//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
// 
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
// 

cplusplus {{
    #include "INETDefs.h"
    #include "CanFrame_m.h"
    #include <list>
typedef std::list<CanFrame *> CanFrameList;
}}


class noncobject CanFrameList;

enum CanEthernetCommandTrafficType
{
	CANETHERNET_COMMAND1 = 0x01;
	CANETHERNET_COMMAND2 = 0x02;
	CANETHERNET_COMMAND3 = 0x03;
	CANETHERNET_COMMAND4 = 0x04;
}


//
// This class should never be instantiated by the models, only specific
// subclasses: CanJam, CanIFG and submodules of ~CanFrame.
//
packet CanEthernetCommandTraffic
{
    short type enum(CanEthernetCommandTrafficType);
}

//
// TODO generated message class
//
packet CanEthernetCommand1 extends CanEthernetCommandTraffic {
    int serialNum;
    int frameNum;
    int MessageID[];
	int Data[];
   	int frameByteLength[];
   	simtime_t CreationTime[];

	CanFrameList canframes;
}

packet CanEthernetCommand2 extends CanEthernetCommandTraffic {
    int serialNum;    
    int frameNum;
}

packet CanEthernetCommand3 extends CanEthernetCommandTraffic {
}

packet CanEthernetCommand4 extends CanEthernetCommandTraffic {
    unsigned long countEthernetSent; // Ethernet送信カウンタ
	unsigned long countEthernetReceive; // Ethernet受信カウンタ
	unsigned long countCanSent; // CAN送信カウンタ
	unsigned long countCanReceive; // CAN受信カウンタ
	unsigned long countAckSent; // アクノリッジ送信カウンタ
	unsigned long countAckReceive; // アクノリッジ受信カウンタ
	unsigned long countAckNG; // アクノリッジ異常カウンタ
}
