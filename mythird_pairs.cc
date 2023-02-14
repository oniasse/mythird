/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ssid.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/animation-interface.h"
#include "ns3/netanim-module.h"

// Default Network Topology
//
//   Wifi 10.1.3.0
//                 AP
//  *    *    *    *
//  |    |    |    |    10.1.1.0
// n5   n6   n7   n0 -------------- n1   n2   n3   n4
//                   point-to-point  |    |    |    |
//                                   ================
//                                     LAN 10.1.2.0

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ThirdScriptExample");

int main(int argc, char* argv[])
{
    bool verbose = true;
    uint32_t nCsma;
    uint32_t nWifi;
    bool tracing = true;

    CommandLine cmd(__FILE__);
    cmd.AddValue("nCsma", "Number of \"extra\" CSMA nodes/devices", nCsma);
    //cmd.AddValue("nWifi", "Number of wifi STA devices", nWifi);         //don't allow nWifi as cmd line argument
    cmd.AddValue("verbose", "Tell echo applications to log if true", verbose);
    cmd.AddValue("tracing", "Enable pcap tracing", tracing);

    cmd.Parse(argc, argv);
    
    nWifi = nCsma;  //pair number of servers to clients

    if (nCsma > 200)
    {
        std::cout << "nCsma should be 200 or less"  //for IP assignment limitation
                  << std::endl;
        return 1;
    }

    if (verbose)
    {
        LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
        LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    }

    NodeContainer p2pNodes;  //create p2p node container
    p2pNodes.Create(2);      //put 2 p2p nodes in above container

    //set attributes of p2p nodes/devices
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer p2pDevices;  //create p2p net device container
    p2pDevices = pointToPoint.Install(p2pNodes);  //install devices with above attributes on p2p nodes and channel beween them

    NodeContainer csmaNodes;  //create container for csma nodes
    csmaNodes.Add(p2pNodes.Get(1));  //add p2p node to csma node container
    csmaNodes.Create(nCsma);  //add 'n' more csma nodes to csma node container

    //set attributes of csma nodes/devices
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

    NetDeviceContainer csmaDevices;  //create csma net device container
    csmaDevices = csma.Install(csmaNodes);  //install devices with above attributes on p2p nodes and channel beween them

    NodeContainer wifiStaNodes;  //create container for wifi nodes
    wifiStaNodes.Create(nWifi);  //create wifi nodes
    NodeContainer wifiApNode = p2pNodes.Get(0);  //add p2p node to wifiAP node container

    //construct wifi devices and channel between wifi nodes
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();  //construct wifi channel helper
    YansWifiPhyHelper phy;  //construct wifi physical helper
    phy.SetChannel(channel.Create());  //create a channel object and associate it to our physical layer object manager to make sure that all the physical layer objects created share the same underlying channel

    WifiMacHelper mac;  //used to set MAC parameters
    Ssid ssid = Ssid("ns-3-ssid");  //set the value of the SSID attribute of the MAC layer implementation

    WifiHelper wifi;  //default configuration WiFi 6 and rate control algorithm IdealWifiManager

    NetDeviceContainer staDevices;  //create wifi net device containers for station nodes
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
    staDevices = wifi.Install(phy, mac, wifiStaNodes);  //install net devices on wifi stations

    NetDeviceContainer apDevices;  //create wifi net device container for ap node
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    apDevices = wifi.Install(phy, mac, wifiApNode);  //install net devices on wifi AP

    MobilityHelper mobility;  //create helper for defining a boundary box for mobile wifi stations

    //tells mobility helper to use a two-dimensional grid to initially place wifi station nodes
    //explore Doxygen for class ns3::GridPositionAllocator to see exactly what is being done
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));

    //tells nodes to move in a random direction at a random speed inside bounding box
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel", "Bounds",
                              RectangleValue(Rectangle(-1000, 1000, -1000, 1000)));
    mobility.Install(wifiStaNodes);  //install mobility model on the wifi station nodes

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");  //switch to constant position model
    mobility.Install(wifiApNode);  //apply constant position model to AP wifi node
    mobility.Install(csmaNodes);   //apply constant position model to csma nodes

    //install Internet Protocol stacks
    InternetStackHelper stack;
    stack.Install(csmaNodes);
    stack.Install(wifiApNode);
    stack.Install(wifiStaNodes);

    Ipv4AddressHelper address;  //used to assign IP addresses to our net device interfaces

    //automatically assigns IP to net devices on the "10.1.1.0" network using subnet mask (p2p)
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer p2pInterfaces;
    p2pInterfaces = address.Assign(p2pDevices);

    //automatically assigns IP to net devices on the "10.1.2.0" network using subnet mask (csma)
    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer csmaInterfaces;
    csmaInterfaces = address.Assign(csmaDevices);

    //automatically assigns IP to net devices on the "10.1.3.0" network using subnet mask (wifi)
    address.SetBase("10.1.3.0", "255.255.255.0");
    address.Assign(staDevices);
    address.Assign(apDevices);

    UdpEchoServerHelper echoServer(9);  //echo server on port 9
    
    while(nCsma > 0)
    {
    ApplicationContainer serverApps = echoServer.Install(csmaNodes.Get(nCsma));
    serverApps.Start (Seconds (1.0));
    serverApps.Stop (Seconds (100.0));
    
    UdpEchoClientHelper echoClient(csmaInterfaces.GetAddress(nCsma--), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue (10000));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1250));
    ApplicationContainer clientApps = echoClient.Install(wifiStaNodes.Get(nWifi-- - 1));    
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(100.0));
    }



   // while(nWifi > 0)
    //{
    
    
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();  //enable internet routing

    Simulator::Stop(Seconds(100.0));  //simulator stop time (needed to avoid infinite loop)

    if (tracing)
    {
        phy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
        pointToPoint.EnablePcapAll("third_p2p");
        phy.EnablePcap("third_wifi", apDevices.Get(0));
        csma.EnablePcap("third_csma", csmaDevices.Get(0), true);
    }
    
     AnimationInterface anim ("third.xml");
     	 anim.SetConstantPosition (csmaNodes.Get(1), 6, 10);
 	 anim.SetConstantPosition (csmaNodes.Get(2), 9, 10);
 	 anim.SetConstantPosition (csmaNodes.Get(3), 12, 10);

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}
