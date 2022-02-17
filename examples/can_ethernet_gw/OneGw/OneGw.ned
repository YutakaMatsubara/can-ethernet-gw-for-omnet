//
// Copyright (C) 2003 CTIE, Monash University
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//

package inet.examples.can_ethernet_gw.OneGw;

import inet.applications.can.CanAppCli;
import inet.applications.can.CanAppSrv;
import inet.linklayer.ethernet.EtherBus;
import inet.nodes.can.CanCliHost;
import inet.nodes.can.CanStandardHost;
import inet.nodes.ethernet.EtherHost;
import inet.nodes.ethernet.EtherSwitch;
import inet.nodes.gateway.CanEthernetGw;
import inet.nodes.inet.StandardHost;
import ned.DatarateChannel;
import inet.util.ThruputMeteringChannel;
import inet.nodes.can.CanSrvHost;
import inet.nodes.gateway.CanEthernetGw;
import inet.linklayer.can.CanBus;
import inet.networklayer.autorouting.ipv4.IPv4NetworkConfigurator;

//
// TODO documentation
//
network OneGw
{
    @display("bgb=494,405");
    types:
        channel ethernet extends DatarateChannel
        {
            delay = normal(0.00015s, 0.00005s);
            datarate = 100Mbps;
        }
        channel C extends DatarateChannel
        {
            delay = 0.1us;
            datarate = 1Mbps;
        }
    submodules:
        Sender: CanSrvHost {
            @display("p=106,121");
        }
        Receiver: CanCliHost {
            @display("p=192,113");
        }
        canEthernetGw: CanEthernetGw {
            @display("p=338,169");
        }
        CAN1: CanBus {
            parameters:
                @display("p=245,64;b=400,6;o=#408060");
            gates:
               // ethg[2];
        }
        EtherHost: StandardHost {
            @display("p=396,332");
        }
        configurator: IPv4NetworkConfigurator {
            config = xmldoc("config.xml");
            @display("p=275,26");
        }
        CGWAppSrv: CanAppSrv {
            @display("p=428,142");
        }
    connections allowunconnected:
        EtherHost.ethg++ <--> ethernet <--> canEthernetGw.ethg++;
        CAN1.ethg++ <--> C <--> Sender.ethg;
        CAN1.ethg++ <--> C <--> Receiver.ethg;
        CAN1.ethg++ <--> C <--> canEthernetGw.cang++;
        CGWAppSrv.in <-- canEthernetGw.appg$o;
        CGWAppSrv.out --> canEthernetGw.appg$i;
}

