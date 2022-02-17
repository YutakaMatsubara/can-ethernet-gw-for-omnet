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

package inet.applications.protocolconvertar;


simple CanEthernetConvApp
{
    parameters:
        //string destAddress = default("");  // destination MAC address, or module path name of destination station; empty means off
        string destAddresses = default(""); // list of IP addresses, separated by spaces ("": don't send)
        int localPort = default(-1);
        int destPort;
        double startTime @unit(s) = default(this.convInterval);  // time of sending the first request
        double stopTime @unit(s) = default(0);  // time of finish sending, 0 means forever
        volatile double convInterval @unit(s);  // interval between converting requests
        volatile int reqLength @unit(B) = default(100B);  // length of request packets
        volatile int respLength @unit(B) = default(1KiB);  // length of response packets
        bool registerSAP = default(false);   // whether to sent IEEE802CTRL_REGISTER_DSAP on startup
    	volatile int messageLength @unit(B); // length of messages to generate, in bytes
        int timeToLive = default(-1); // if not -1, set the TTL (IPv4) or Hop Limit (IPv6) field of sent packets to this value
        int typeOfService = default(-1); // if not -1, set the ToS (IPv4) or Traffic Class (IPv6) field of sent packets to this value
        string multicastInterface = default("");  // if not empty, set the multicast output interface option on the socket (interface name expected) 
        bool receiveBroadcast = default(false); // if true, makes the socket receive broadcast packets
        bool joinLocalMulticastGroups = default(false); // if true, makes the socket receive packets from all multicast groups set on local interfaces
        bool rdctConvPeriodMode = default(false);
        double rdctMaxTime = default(10);
    	int queueLimit = default(100);
        int urgencyPri = default(0);
        double graceTime = default(0);
        double graceRate = default(0);
//        xml NodeInfo = default(xmldoc("RoutingMap.xml"));//GWでメッセージの周期を読込むため、メッセージセットのXMLをGWでも読み込む必要がある
        xml NodeInfo = default(xmldoc("RoutingMap_matsumura.xml"));//GWでメッセージの周期を読込むため、メッセージセットのXMLをGWでも読み込む必要がある
        
        @display("i=block/app");
        @signal[sentPk](type=cPacket);
        @signal[rcvdPk](type=cPacket);
        @signal[queueLength](type=long);
        @signal[queueLengthZero](type=long);
        
        @statistic[rcvdPk](title="packets received"; source=rcvdPk; record=count,"sum(packetBytes)","vector(packetBytes)"; interpolationmode=none);
        @statistic[sentPk](title="packets sent"; source=sentPk; record=count,"sum(packetBytes)","vector(packetBytes)"; interpolationmode=none);
        @statistic[endToEndDelay](title="end-to-end delay"; source="messageAge(rcvdPk)"; unit=s; record=histogram,vector; interpolationmode=none);
        @statistic[queueLength](title="queue length"; source=queueLength; record=vector,histogram;interpolationmode=none);
        @statistic[queueLengthZero](title="queue length(Zero)"; source=queueLengthZero; record=vector,histogram;interpolationmode=none);
        
    gates:
        input upperLayerIn @labels(CanFrame);    // to ~EtherLLC or ~EtherEncap or ~MACRelayUnitPP
        output upperLayerOut @labels(CanFrame);  // to ~EtherLLC or ~EtherEncap or ~MACRelayUnitPP
        //inout phys @labels(CanFrame-conn); // to physical layer or the network
        input udpIn @labels(UDPControlInfo/up) ;
        output udpOut @labels(UDPControlInfo/down);
}