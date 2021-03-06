 /* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
 /*
  * Copyright (c) 2008-2009 Strasbourg University
  *
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
  * Author: David Gross <gdavid.devel@gmail.com>
  *         Sebastien Vincent <vincent@clarinet.u-strasbg.fr>
  */
 
 // Network topology
 // //
 // //             n0   r    n1
 // //             |    _    |
 // //             ====|_|====
 // //                router
 // //
 // // - Tracing of queues and packet receptions to file "simple-routing-ping6.tr"
 
#include <fstream>
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-apps-module.h"
#include "ns3/ipv4-static-routing-helper.h"
 
#include "ns3/ipv4-routing-table-entry.h"
 

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


typedef struct timeval TIMER_TYPE;
#define TIMER_NOW(_t) gettimeofday (&_t,NULL);
#define TIMER_SECONDS(_t) ((double)(_t).tv_sec + (_t).tv_usec*1e-6)
#define TIMER_DIFF(_t1, _t2) (TIMER_SECONDS (_t1)-TIMER_SECONDS (_t2))

 
NS_LOG_COMPONENT_DEFINE ("SimpleRoutingPing6Example");

 int main (int argc, char** argv)
 {

   // Set up some default values for the simulation.
   Config::SetDefault ("ns3::OnOffApplication::PacketSize", UintegerValue (1024));
   Config::SetDefault ("ns3::OnOffApplication::DataRate", StringValue ("10Mb/s"));

   typedef std::vector<NetDeviceContainer> vectorOfNetDeviceContainer;
 
   int num_lxcs = 2;
   string startup_cmds_dir = "/home/kronos/ns-allinone-3.29/ns-3.29/examples/vt_experiments/common/startup_cmds/simple_p2p_network";
   string logs_base_dir = "/home/kronos/ns-allinone-3.29/ns-3.29/examples/vt_experiments/NSDI-2020/experiment_logs/simple_p2p_network";
   string log_folder = "test_logs";
   bool enable_kronos = false;
   long num_insns_per_round = 10000;
   float relative_cpu_speed = 0.18;
   long run_time_secs = 10;
   float advance_ns_per_round;
   long time_elapsed = 0;
   long num_rounds_to_run;
   BridgeHelper bridge;
   int nLXCsperSwitch = 4;
   int nSwitches = 3;
   int nSimHostsperSwitch = 1;
   int INIT_DELAY_SECS = 10;
   int numTracersperLXC = 1;
   double tElapsed;


   TIMER_TYPE t0, t1, t2, t3, t4;
   TIMER_NOW (t0);

   CsmaHelper csma_sw_sw;
   csma_sw_sw.SetChannelAttribute ("DataRate", StringValue ("100Gbps"));
   csma_sw_sw.SetChannelAttribute ("Delay", TimeValue (MicroSeconds (10)));

   CsmaHelper csma_host_sw;
   csma_host_sw.SetChannelAttribute ("DataRate", StringValue ("10Gbps"));
   csma_host_sw.SetChannelAttribute ("Delay", TimeValue (MicroSeconds (10)));

   CommandLine cmd;
   cmd.AddValue ("numInsnsPerRound", "Number insns per round", num_insns_per_round); 
   cmd.AddValue ("relCpuSpeed", "Relative CPU speed", relative_cpu_speed); 
   cmd.AddValue ("runTimeSecs", "Running Time Secs", run_time_secs); 
   cmd.AddValue ("enableKronos", "Enable/disable Kronos", enable_kronos);
   cmd.AddValue ("startupCmds", "Start up cmds directory", startup_cmds_dir); 
   cmd.AddValue ("nLXCsperSwitch", "Number of LXCs", nLXCsperSwitch); 
   cmd.AddValue ("nSimHostsperSwitch", "Number of Simulated Hosts per switch", nSimHostsperSwitch); 
   cmd.AddValue ("nSwitches", "Number of Switches", nSwitches); 
   cmd.AddValue ("logFolder", "Folder Name in logs base dir", log_folder); 
   cmd.AddValue ("numTracersperLXC", "numTracersperLXC", numTracersperLXC); 
   cmd.Parse (argc, argv);

   if (!enable_kronos)
        INIT_DELAY_SECS = 0;

   num_lxcs = nLXCsperSwitch*nSwitches;
   LXCManager lxcManager(num_lxcs, startup_cmds_dir, enable_kronos,
			num_insns_per_round, relative_cpu_speed);

   TapBridgeHelper tapBridge;
   Address serverAddress;
   tapBridge.SetAttribute ("Mode", StringValue ("UseBridge"));

   NodeContainer switches;
   NodeContainer simHosts;
   NodeContainer emuHosts;
   NodeContainer allHosts;

   BridgeHelper bridgeHelper;
   vectorOfNetDeviceContainer bridgedDevices(nSwitches + 1);


   int network_start = 1, num_nodes_assigned = 1;
   for (int i = 0; i < num_lxcs; i++) {
      if (num_nodes_assigned > 255) {
          network_start ++;
          num_nodes_assigned = 0;
      }
      std::string ip_address = "10.1." + std::to_string(network_start) + "." + std::to_string(num_nodes_assigned); 
      lxcManager.lxcIPs.push_back(ip_address);
      num_nodes_assigned ++;
   }

   advance_ns_per_round = num_insns_per_round;



 
   NS_LOG_INFO ("Create nodes.");
   for (int i =0; i < nSimHostsperSwitch*nSwitches; i++) {
        Ptr<Node> node = CreateObject<Node> ();
        simHosts.Add(node);
        allHosts.Add(node);
   }
   for (int i =0; i < nLXCsperSwitch*nSwitches; i++) {
        Ptr<Node> node = CreateObject<Node> ();
        emuHosts.Add(node);
        allHosts.Add(node);
   }
   for (int i =0; i < nSwitches + 1; i++) {
        Ptr<Node> node = CreateObject<Node> ();
        switches.Add(node);
   }
 
   
   NS_LOG_INFO ("Create IPv4 Internet Stack");
   InternetStackHelper internetv4;
   internetv4.Install (allHosts);

   NetDeviceContainer simHostDevices;
   NetDeviceContainer emuHostDevices;
   NetDeviceContainer allHostDevices;
 
 
   NS_LOG_INFO ("Create channels.");

   NS_LOG_INFO("Creating backbone links");

  
        NetDeviceContainer sw_1_link = csma_sw_sw.Install(NodeContainer(switches.Get(0), switches.Get(nSwitches)));
        bridgedDevices[0].Add(sw_1_link.Get(0));
        bridgedDevices[nSwitches].Add(sw_1_link.Get(1));

        NetDeviceContainer sw_2_link = csma_sw_sw.Install(NodeContainer(switches.Get(1), switches.Get(nSwitches)));
        bridgedDevices[1].Add(sw_2_link.Get(0));
        bridgedDevices[nSwitches].Add(sw_2_link.Get(1));

        NetDeviceContainer sw_3_link = csma_sw_sw.Install(NodeContainer(switches.Get(2), switches.Get(nSwitches)));
        bridgedDevices[2].Add(sw_3_link.Get(0));
        bridgedDevices[nSwitches].Add(sw_3_link.Get(1));
  

   NS_LOG_INFO("Creating SimHost-Switch links");
   for (int i = 0; i < nSwitches; i++) {
        for (int j = 0; j < nSimHostsperSwitch; j++) {
                NetDeviceContainer host_j_sw_i_link = csma_host_sw.Install(NodeContainer(simHosts.Get(i*nSimHostsperSwitch + j), switches.Get(i)));
                bridgedDevices[i].Add(host_j_sw_i_link.Get(1));
                simHostDevices.Add(host_j_sw_i_link.Get(0));
        }
   }

   NS_LOG_INFO("Creating LXCHost-Switch links");
   for (int i = 0; i < nSwitches; i++) {
        for (int j = 0; j < nLXCsperSwitch; j++) {
                NetDeviceContainer host_j_sw_i_link = csma_host_sw.Install(NodeContainer(emuHosts.Get(i*nLXCsperSwitch + j), switches.Get(i)));
                bridgedDevices[i].Add(host_j_sw_i_link.Get(1));
                emuHostDevices.Add(host_j_sw_i_link.Get(0));
        }
   }

   for (int i = 0; i < nLXCsperSwitch*nSwitches; i++) {
        allHostDevices.Add(emuHostDevices.Get(i));
   }
   
   for (int i = 0; i < nSimHostsperSwitch*nSwitches; i++) {
        allHostDevices.Add(simHostDevices.Get(i));
   }


   NS_LOG_INFO ("Create networks and assign IPv6 Addresses.");
   Ipv4AddressHelper ipv4;
   ipv4.SetBase ("10.1.0.0", "255.255.0.0", "0.0.1.1");
   ipv4.Assign (allHostDevices);

   NS_LOG_INFO("Bridging Switch Devices");
   for (int i = 0; i < nSwitches + 1; i++) {
        bridgeHelper.Install(switches.Get(i), bridgedDevices[i]);
   }

   if (enable_kronos) {
	if (initializeExp(1) < 0) {
		std::cout << "Kronos initialization failed !. Make sure Kronos is loaded ! " << std::endl;
	}
   }
   
   lxcManager.createConfigFiles();
   std::cout << "Starting LXCs ... " << std::endl;
   lxcManager.startLXCs();

   NS_LOG_INFO("Setting TapBridges");
   int lxc_counter = 1;
   for (int i = 0; i < nSwitches; i++) {
        for (int j = 0; j < nLXCsperSwitch; j++) {
                tapBridge.SetAttribute ("DeviceName", StringValue ("tap-" + std::to_string(lxc_counter)));
                tapBridge.Install (emuHosts.Get(lxc_counter - 1), emuHostDevices.Get(lxc_counter - 1));
                Ptr<Ipv4> ipv4_add = emuHosts.Get(lxc_counter - 1)->GetObject<Ipv4>();
                Ipv4InterfaceAddress iaddr = ipv4_add->GetAddress (1,0);
                Ipv4Address addri = iaddr.GetLocal ();
                std::cout << "IP ADDress of lxc-" << lxc_counter << " is: " << addri << std::endl; 
                std::cout << "MAC address is: " << emuHostDevices.Get(lxc_counter - 1)->GetAddress() << std::endl;
                lxc_counter ++;
        }
   }

  std::cout << "Created " << NodeList::GetNNodes () << " nodes." << std::endl; 
  RngSeedManager::SetSeed (12);
  RngSeedManager::SetRun(101);
   
   TIMER_TYPE routingStart;
   TIMER_NOW (routingStart);

   // Calculate routing tables
   std::cout << "Populating Routing tables..." << std::endl;
   Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

   
   //Ipv4NixVectorHelper nixRouting;
   //internetv4.SetRoutingHelper (nixRouting); // has effect on the next Install ()
   

   TIMER_TYPE routingEnd;
   TIMER_NOW (routingEnd);
   std::cout << "Routing tables population took "
        << TIMER_DIFF (routingEnd, routingStart) << std::endl;

   std::cout << "Running simulator..." << std::endl;

   TIMER_NOW (t1);
        tElapsed = 0.0;
   if (enable_kronos) {
        // Need to run the Simulator first for a bit to flush out all the unwanted multi cast router solicitation messages
        // which are sent upon lxc startup.
        Simulator::Stop (Seconds(INIT_DELAY_SECS));
        Simulator::Run ();

	std::cout << "Synchronize and Freeze initiated !" << std::endl;	
	while(synchronizeAndFreeze(num_lxcs*numTracersperLXC) <= 0) {
		usleep(1000000);
	}
	std::cout << "Synchronize and Freeze Succeeded !" << std::endl;
	startExp();
	num_rounds_to_run = (run_time_secs * NS_IN_SEC)/advance_ns_per_round;
	std::cout << "Running for: " << num_rounds_to_run << " rounds !" << std::endl;
	for(int i = 0; i < num_rounds_to_run; i++) {
	       Simulator::Stop (NanoSeconds(advance_ns_per_round));
	       Simulator::Run ();
                TIMER_NOW (t3);
	       progress_n_rounds(1);
                TIMER_NOW (t4);
               time_elapsed += (int) advance_ns_per_round;
                tElapsed += TIMER_DIFF (t4, t3);
               if (i % 1000 == 0) {
                      tElapsed = tElapsed/1000.0;
                std::cout << "Virtual Time Elapsed: (sec): " << (float)time_elapsed/NS_IN_SEC << ", Kronos overhead: " << tElapsed << std::endl;
                        tElapsed = 0.0;
                }
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
   
   double d1 = TIMER_DIFF (t1, t0), d2 = TIMER_DIFF (t2, t1);
   std::cout << "-----" << std::endl << "Runtime Stats:" << std::endl;
   std::cout << "Simulator init time: " << d1 << std::endl;
   std::cout << "Simulator run time: " << d2 << std::endl;
   std::cout << "Total elapsed time: " << d1 + d2 << std::endl;

   system(("sudo mkdir -p " + logs_base_dir + "/" + log_folder).c_str());
   system(("sudo cp -R /tmp/lxc* " + logs_base_dir + "/" + log_folder).c_str());
   system(("sudo chmod -R 777 " + logs_base_dir).c_str());

   std::cout << "Stopping LXCs ..." << std::endl;
   lxcManager.stopLXCs();

   std::cout << "Simulator finished." << std::endl;
   Simulator::Destroy ();
   
   return 0;
 

 }
