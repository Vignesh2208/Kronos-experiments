/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
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
  *
  */
 

#include <cstdlib>
#include <sys/time.h>
#include <fstream>
#include <vector>

#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/on-off-helper.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/mpi-interface.h"
#include "ns3/ipv4-nix-vector-helper.h"
#include "ns3/csma-module.h"
#include "ns3/bridge-module.h"
 #include "ns3/applications-module.h"

#include "ns3/core-module.h"
#include "ns3/tap-bridge-module.h"
#include "start_lxcs.h"

#define NS_IN_SEC 1000000000
#define NS_IN_MS 1000000

extern "C"
{
#include <Kronos_functions.h>   
}

using namespace ns3;
using namespace lxc_manager;
 
 // Network topology (default)
 //
 //            n2     +          +     n3          .
 //             | ... |\        /| ... |           .
 //             ======= \      / =======           .
 //              CSMA    \    /   CSMA             .
 //                       \  /                     .
 //            n1     +--- n0 ---+     n4          .
 //             | ... |   /  \   | ... |           .
 //             =======  /    \  =======           .
 //              CSMA   /      \  CSMA             .
 //                    /        \                  .
 //            n6     +          +     n5          .
 //             | ... |          | ... |           .
 //             =======          =======           .
 //              CSMA             CSMA             .
 //
 
 using namespace ns3;

 typedef struct timeval TIMER_TYPE;
 #define TIMER_NOW(_t) gettimeofday (&_t,NULL);
 #define TIMER_SECONDS(_t) ((double)(_t).tv_sec + (_t).tv_usec*1e-6)
 #define TIMER_DIFF(_t1, _t2) (TIMER_SECONDS (_t1)-TIMER_SECONDS (_t2))
 
 NS_LOG_COMPONENT_DEFINE ("CsmaStar");
 
 int 
 main (int argc, char *argv[])
 {

   typedef std::vector<NodeContainer> vectorOfNodeContainer;
   typedef std::vector<NetDeviceContainer> vectorOfNetDeviceContainer;
 
   LogComponentEnable ("CsmaStar", LOG_LEVEL_INFO);
   LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
   LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);
   //
   // Default number of nodes in the star.  Overridable by command line argument.
   //
   int nSpokes = 2;

   int num_lxcs;
   string startup_cmds_dir = "/home/kronos/ns-allinone-3.29/ns-3.29/examples/vt_experiments/common/startup_cmds/campus_network";
   bool enable_kronos = false;
   long num_insns_per_round = 1000000;
   float relative_cpu_speed = 1.0;
   long run_time_secs = 10;
   float advance_ns_per_round;
   long time_elapsed = 0;
   long num_rounds_to_run;


   TIMER_TYPE t0, t1, t2;
   TIMER_NOW (t0);

   int nLANClients = 1 , nLXCsperLAN = 1, lxc_counter = 1;

   CommandLine cmd;
   cmd.AddValue ("LAN", "Number of nodes per LAN [42]", nLANClients);
   cmd.AddValue ("nLXCsperLAN", "Number of VMs/LXCs per LAN", nLXCsperLAN); 
   cmd.AddValue ("numInsnsPerRound", "Number insns per round", num_insns_per_round); 
   cmd.AddValue ("relCpuSpeed", "Relative CPU speed", relative_cpu_speed); 
   cmd.AddValue ("runTimeSecs", "Running Time Secs", run_time_secs); 
   cmd.AddValue ("enableKronos", "Enable/disable Kronos", enable_kronos);
   cmd.AddValue ("startupCmds", "Start up cmds directory", startup_cmds_dir); 
   cmd.AddValue ("nSpokes", "Number of spoke nodes to place in the star", nSpokes);
   cmd.Parse (argc, argv);

   cmd.Parse (argc,argv);

   num_lxcs = nLXCsperLAN*nSpokes;
   LXCManager lxcManager(num_lxcs, startup_cmds_dir, enable_kronos,
			num_insns_per_round, relative_cpu_speed);


   vectorOfNodeContainer LanNodes(nSpokes);
   vectorOfNodeContainer BridgeNodes(nSpokes);
   NodeContainer AllNodes;
   NetDeviceContainer AllDevices;
   vectorOfNetDeviceContainer LanDevices(nSpokes);
   vectorOfNetDeviceContainer BridgeDevices(nSpokes);
   BridgeHelper bridge;
   Ptr<Node> common_node = CreateObject<Node> ();
   AllNodes.Add(common_node);
   TapBridgeHelper tapBridge;
   Address serverAddress;
   tapBridge.SetAttribute ("Mode", StringValue ("UseLocal"));

   int n_seen_nodes = 0;
   for (int i = 0; i < nSpokes; ++i) {
          n_seen_nodes += (nLXCsperLAN + nLANClients + 1)*i;
          for (int j = 0; j < nLXCsperLAN; j++) {
              std::string lxc_ip_address;
              //lxc_ip_address = "10.1." + std::to_string(i+1) + "." + std::to_string(2 + nLANClients + j);
              lxc_ip_address = "10.1.1." + std::to_string(n_seen_nodes + 2 + nLANClients + j);
              lxcManager.lxcIPs.push_back(lxc_ip_address);
          }
   }
   lxcManager.createConfigFiles();
   std::cout << "Starting LXCs ... " << std::endl;
   lxcManager.startLXCs();

   advance_ns_per_round = num_insns_per_round/relative_cpu_speed;
 
   NS_LOG_INFO ("Build star topology.");
   CsmaHelper csma;
   csma.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
   csma.SetChannelAttribute ("Delay", StringValue ("1ms"));

 
   for (int i = 0; i < nSpokes; i++) {
       Ptr<Node> bridge_node = CreateObject<Node> ();
       BridgeNodes[i].Add(bridge_node);
       LanNodes[i].Add (common_node);
       for(int j = 0; j < nLXCsperLAN + nLANClients; j++) {
           Ptr<Node> lan_node = CreateObject<Node> ();
           LanNodes[i].Add(lan_node);
           AllNodes.Add(lan_node);
       }
       for(int j = 0; j < nLXCsperLAN + nLANClients + 1; j++) {
           NetDeviceContainer link = csma.Install (NodeContainer (LanNodes[i].Get (j), BridgeNodes[i].Get(0)));
           LanDevices[i].Add (link.Get (0));
           AllDevices.Add(link.Get(0));
           BridgeDevices[i].Add (link.Get (1));
       }
       bridge.Install(BridgeNodes[i].Get(0), BridgeDevices[i]);
   }

   InternetStackHelper internet;
   internet.Install (AllNodes);
   std::string AddrBase = "10.1.1.0";
   Ipv4AddressHelper ipv4;
   //std::string AddrBase = "10.1." + std::to_string(i + 1) + ".0";
   ipv4.SetBase (AddrBase.c_str(), "255.255.255.0");
   Ipv4InterfaceContainer intf = ipv4.Assign (AllDevices);
   serverAddress = Address(intf.GetAddress (1));

   for (int i = 0; i < nSpokes; i++) {
        

        for (int j = nLANClients + 1; j < nLXCsperLAN + nLANClients + 1; j++) {
                tapBridge.SetAttribute ("DeviceName", StringValue ("tap-" + std::to_string(lxc_counter)));
                tapBridge.Install (LanNodes[i].Get (j), LanDevices[i].Get (j));
                Ptr<Ipv4> ipv4 = LanNodes[i].Get (j)->GetObject<Ipv4>();
                Ipv4InterfaceAddress iaddr = ipv4->GetAddress (1,0);
                Ipv4Address addri = iaddr.GetLocal ();
                std::cout << "IP ADDress of lxc " << lxc_counter << " is: " << addri << std::endl; 
                lxc_counter += 1;
        }
   }

    NS_LOG_INFO ("Create Applications.");
 //
 // Create a UdpEchoServer application on node one.
 //
   uint16_t port = 9;  // well-known echo port number
   UdpEchoServerHelper server (port);
   ApplicationContainer apps = server.Install (LanNodes[0].Get (1));
   apps.Start (Seconds (0.0));
   apps.Stop (Seconds (1.0));
 
 //
 // Create a UdpEchoClient application to send UDP datagrams from node zero to
 // node one.
 //
   uint32_t packetSize = 1024;
   uint32_t maxPacketCount = 100;
   Time interPacketInterval = Seconds (0.1);
   UdpEchoClientHelper client (serverAddress, port);
   client.SetAttribute ("MaxPackets", UintegerValue (maxPacketCount));
   client.SetAttribute ("Interval", TimeValue (interPacketInterval));
   client.SetAttribute ("PacketSize", UintegerValue (packetSize));
   apps = client.Install (LanNodes[1].Get(1));
   apps.Start (Seconds (0.1));
   apps.Stop (Seconds (1.0));
 
   NS_LOG_INFO ("Configure Tracing.");
 
   //
   // Configure tracing of all enqueue, dequeue, and NetDevice receive events.
   // Trace output will be sent to the file "csma-bridge-one-hop.tr"
   //
   //AsciiTraceHelper ascii;
   //csma.EnableAsciiAll (ascii.CreateFileStream ("csma-bridge-one-hop.tr"));
 
   //
   // Also configure some tcpdump traces; each interface will be traced.
   // The output files will be named:
   //     csma-bridge-one-hop-<nodeId>-<interfaceId>.pcap
   // and can be read by the "tcpdump -r" command (use "-tt" option to
   // display timestamps correctly)
   //
   csma.EnablePcapAll ("csma-bridge", false);



   // Simulation control
   Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
 
   NS_LOG_INFO ("Enable pcap tracing.");
 
   std::cout << "Created " << NodeList::GetNNodes () << " nodes." << std::endl;
   TIMER_TYPE routingStart;
   TIMER_NOW (routingStart);

   // Calculate routing tables
   std::cout << "Populating Routing tables..." << std::endl;
   Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

   TIMER_TYPE routingEnd;
   TIMER_NOW (routingEnd);
   std::cout << "Routing tables population took "
        << TIMER_DIFF (routingEnd, routingStart) << std::endl;

   std::cout << "Running simulator..." << std::endl;
   TIMER_NOW (t1);
   if (enable_kronos) {
	std::cout << "Synchronize and Freeze initiated !" << std::endl;	
	while(synchronizeAndFreeze(num_lxcs) <= 0) {
		usleep(1000000);
	}
	std::cout << "Synchronize and Freeze Succeeded !" << std::endl;
	startExp();
	num_rounds_to_run = (run_time_secs * NS_IN_SEC)/advance_ns_per_round;
	std::cout << "Running for: " << num_rounds_to_run << " rounds !" << std::endl;
	for(int i = 0; i < num_rounds_to_run; i++) {
	       Simulator::Stop (NanoSeconds(advance_ns_per_round));
	       Simulator::Run ();
	       progress_n_rounds(1);
               time_elapsed += (int) advance_ns_per_round;
               std::cout << "Virtual Time Elapsed: (sec): " << (float)time_elapsed/NS_IN_SEC << std::endl;
	}
	stopExp();

    } else {    
	    for (int i = 0 ; i < run_time_secs * 100; i++) {
	       //std::cout << " ############ Running Round: " << i + 1 << "############### " << std::endl;
	       Simulator::Stop (MilliSeconds(10));
	       Simulator::Run ();
	       usleep(10000);
	    }
    }
    
   TIMER_NOW (t2);
   std::cout << "Simulator finished." << std::endl;
   Simulator::Destroy ();
   std::cout << "Stopping LXCs ..." << std::endl;
   lxcManager.stopLXCs();
   double d1 = TIMER_DIFF (t1, t0), d2 = TIMER_DIFF (t2, t1);
   std::cout << "-----" << std::endl << "Runtime Stats:" << std::endl;
   std::cout << "Simulator init time: " << d1 << std::endl;
   std::cout << "Simulator run time: " << d2 << std::endl;
   std::cout << "Total elapsed time: " << d1 + d2 << std::endl;
   return 0;
 
   return 0;
 }


