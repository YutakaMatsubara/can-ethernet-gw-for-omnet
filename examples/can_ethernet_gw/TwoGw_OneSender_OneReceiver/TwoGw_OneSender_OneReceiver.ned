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

package inet.examples.can_ethernet_gw.TwoGw_OneSender_OneReceiver;

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

network TwoGw_OneSender_OneReceiver
{
    @display("bgb=567,385");
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
        Sender: CanStandardHost {
            @display("p=160,268");
        }
        Receiver: CanStandardHost {
            @display("p=362,262");
        }
        canEthernetGw1: CanEthernetGw {
            @display("p=160,109");
        }
        canEthernetGw2: CanEthernetGw {
            @display("p=362,109");
        }
        canBus1: CanBus {
            @display("p=149,193");
        }
        canBus2: CanBus {
            @display("p=371,189");
        }
        configurator: IPv4NetworkConfigurator {
            @display("p=269,18");
        }
        GWApp1: CanCliHost {
            @display("p=91,70");
        }
        GWApp2: CanCliHost {
            @display("p=433,61");
        }
    connections:
        canEthernetGw1.ethg++ <--> ethernet <--> canEthernetGw2.ethg++;
        Sender.ethg <--> C <--> canBus1.ethg++;
        Receiver.ethg <--> C <--> canBus2.ethg++;
        canBus1.ethg++ <--> C <--> canEthernetGw1.cang++;
        canBus2.ethg++ <--> C <--> canEthernetGw2.cang++;
        GWApp1.ethg <--> C <--> canEthernetGw1.appg;
        GWApp2.ethg <--> C <--> canEthernetGw2.appg;
}
