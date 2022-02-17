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

package inet.nodes.gateway;

import inet.applications.protocolconvertar.CanEthernetConvApp;
import inet.linklayer.can.CanController;
import inet.applications.can.CanMessageRouter;
import inet.applications.can.CanAppSrv;
import inet.linklayer.ethernet.EthernetInterface;
import inet.nodes.inet.NetworkLayer;
import inet.transport.udp.UDP;

import inet.util.PcapRecorder;
import inet.networklayer.ipv4.RoutingTable;
import inet.networklayer.common.InterfaceTable;
import inet.mobility.IMobility;
import inet.linklayer.IWirelessNic;
import inet.linklayer.IWiredNic;
import inet.linklayer.IExternalNic;
import inet.base.NotificationBoard;

import inet.applications.IPingApp;
import inet.applications.ISCTPApp;
import inet.applications.ITCPApp;
import inet.applications.IUDPApp;
import inet.transport.ISCTP;
import inet.transport.ITCP;
import inet.transport.IUDP;

//
// TODO auto-generated type
//
module CanEthernetGw
{
    parameters:
        string queueType = default(""); // DropTailQueue, DropTailQoSQueue, REDQueue, or empty to use the internal queue
        @node();
        @labels(node,can-node);
        @display("i=device/pc2;bgb=362,515");
        int numBuses = default(0);
        string udpType = default(firstAvailable("UDP","UDP_None"));
        bool IPForward = default(false);  // disable routing by default
        networkLayer.proxyARP = default(false);
        string routingFile = default("");
        bool forwardMulticast = default(false);
    gates:
        inout cang[] @labels(CanFrame-conn);
        inout ethg[] @labels(EtherFrame-conn);
        inout appg;
    submodules:
        notificationBoard: NotificationBoard {
            parameters:
                @display("p=53,212");
        }
        routingTable: RoutingTable {
            parameters:
                @display("p=53,315");
                IPForward = IPForward;
                forwardMulticast = forwardMulticast;
                routingFile = routingFile;
        }
         // linklayer
        interfaceTable: InterfaceTable {
            parameters:
                @display("p=53,414");

        }
        router: CanMessageRouter {
            parameters:
                @display("p=133,125,col");
            gates:
        	    //ethg[2];
        }
        ctl[sizeof(cang)]: CanController {
            parameters:
          //      queueModule = default(queueType == "" ? "" : "queue");
          		queueKind = default(1);
                @display("p=133,393,r,100;q=txQueue");
        }
        canEthernetConvApp: CanEthernetConvApp {
            parameters:
                @display("p=297,150");
            gates:
                upperLayerIn;
                upperLayerOut;
        }
        udp: <udpType> like IUDP {
            parameters:
                @display("p=297,227");
        }
        networkLayer: NetworkLayer {
            @display("p=297,312");
        }
        eth[sizeof(ethg)]: <default("EthernetInterface")> like IWiredNic {
            parameters:
                @display("p=297,393,row,90;q=txQueue");
        }
    connections allowunconnected:
        router.out++ --> appg$o;
        router.in++ <-- appg$i;
        for i=0..sizeof(cang)-1 {
            router.out++ --> ctl[i].upperLayerIn++;
            router.in++ <-- ctl[i].upperLayerOut++;
            ctl[i].phys <--> cang[i];
        }
        for i=0..sizeof(ethg)-1 {
            ethg[i] <--> eth[i].phys;
            eth[i].upperLayerIn <-- networkLayer.ifOut++; // for INET 2.3.0
            eth[i].upperLayerOut --> networkLayer.ifIn++; // for INET 2.3.0
//            eth[i].netwOut --> networkLayer.ifIn++; 
//            eth[i].netwIn <-- networkLayer.ifOut++;
        }

        udp.ipOut --> networkLayer.transportIn++; // for INET 2.3.0
        udp.ipIn <-- networkLayer.transportOut++; // for INET 2.3.0
//        udp.ipOut --> networkLayer.udpIn;
//        udp.ipIn <-- networkLayer.udpOut;

        canEthernetConvApp.udpOut --> udp.appIn++;
        canEthernetConvApp.udpIn <-- udp.appOut++;
        canEthernetConvApp.upperLayerOut --> router.in++;
        router.out++ --> canEthernetConvApp.upperLayerIn;
}
