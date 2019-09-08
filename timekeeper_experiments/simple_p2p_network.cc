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
#include "TimeKeeper_functions.h"   
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
   bool enable_timekeeper = false;
   long run_time_secs = 10;
   float advance_ns_per_round;
   long time_elapsed = 0;
   long num_rounds_to_run;
   BridgeHelper bridge;
   int nLXCsperSwitch = 1;
   int nSwitches = 2;
   int nSimHostsperSwitch = 1;
   int percentageSinks = 10;
   int INIT_DELAY_SECS = 10;
   int numTracersperLXC = 1;
   long timeslice = 10000;
   float tdf = 1.0;


   TIMER_TYPE t0, t1, t2;
   TIMER_NOW (t0);

   CsmaHelper csma_sw_sw;
   csma_sw_sw.SetChannelAttribute ("DataRate", StringValue ("100Gbps"));
   csma_sw_sw.SetChannelAttribute ("Delay", TimeValue (MicroSeconds (10)));

   CsmaHelper csma_host_sw;
   csma_host_sw.SetChannelAttribute ("DataRate", StringValue ("10Gbps"));
   csma_host_sw.SetChannelAttribute ("Delay", TimeValue (MicroSeconds (10)));

   CommandLine cmd;
   cmd.AddValue ("timeslice", "Time slice", timeslice); 
   cmd.AddValue ("tdf", "TDF", tdf); 
   cmd.AddValue ("runTimeSecs", "Running Time Secs", run_time_secs); 
   cmd.AddValue ("enableTimeKeeper", "Enable/disable Kronos", enable_timekeeper);
   cmd.AddValue ("startupCmds", "Start up cmds directory", startup_cmds_dir); 
   cmd.AddValue ("nLXCsperSwitch", "Number of LXCs", nLXCsperSwitch); 
   cmd.AddValue ("nSimHostsperSwitch", "Number of Simulated Hosts per switch", nSimHostsperSwitch); 
   cmd.AddValue ("nSwitches", "Number of Switches", nSwitches); 
   cmd.AddValue ("logFolder", "Folder Name in logs base dir", log_folder); 
   cmd.AddValue ("numTracersperLXC", "numTracersperLXC", numTracersperLXC); 
   cmd.Parse (argc, argv);

   if (!enable_timekeeper)
        INIT_DELAY_SECS = 0;

   system("rm /tmp/trigger.txt");

   num_lxcs = nLXCsperSwitch*nSwitches;
   TkLXCManager lxcManager(num_lxcs, startup_cmds_dir, enable_timekeeper,
			timeslice, tdf);

   TapBridgeHelper tapBridge;
   Address serverAddress;
   tapBridge.SetAttribute ("Mode", StringValue ("UseBridge"));

   NodeContainer switches;
   NodeContainer simHosts;
   NodeContainer emuHosts;
   NodeContainer allHosts;

   BridgeHelper bridgeHelper;
   vectorOfNetDeviceContainer bridgedDevices(2*nSwitches);


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

   advance_ns_per_round = timeslice/tdf;



 
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
   for (int i =0; i < 2*nSwitches; i++) {
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
   /*for (int i = 0; i < nSwitches - 1; i++) {
        NetDeviceContainer sw_i_i1_link = csma_sw_sw.Install(NodeContainer(switches.Get(i), switches.Get(i+1)));
        bridgedDevices[i].Add(sw_i_i1_link.Get(0));
        bridgedDevices[i+1].Add(sw_i_i1_link.Get(1));
   }*/

   for (int i = 0; i < nSwitches; i++) {
        NetDeviceContainer sw_i_i_plus_n_link = csma_sw_sw.Install(NodeContainer(switches.Get(i), switches.Get(i+nSwitches)));
        bridgedDevices[i].Add(sw_i_i_plus_n_link.Get(0));
        bridgedDevices[i+nSwitches].Add(sw_i_i_plus_n_link.Get(1));
   }


   for (int i = 0; i < nSwitches -1; i++) {
        NetDeviceContainer sw_i_plus_n_i_plus_n1_link = csma_sw_sw.Install(NodeContainer(switches.Get(i + nSwitches), switches.Get(i + nSwitches + 1)));
        bridgedDevices[i + nSwitches].Add(sw_i_plus_n_i_plus_n1_link.Get(0));
        bridgedDevices[i + nSwitches + 1].Add(sw_i_plus_n_i_plus_n1_link.Get(1));
   }

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
   for (int i = 0; i < 2*nSwitches; i++) {
        bridgeHelper.Install(switches.Get(i), bridgedDevices[i]);
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

  NS_LOG_INFO("Starting Applications");
  Ptr<UniformRandomVariable> urng = CreateObject<UniformRandomVariable> ();
  int r1, r2;
  double start_time;
  for (int i = 0; i < nSwitches; i++)
    {
      for (int j = 0; j < nSimHostsperSwitch; j++)
        {
          int num_sinks = nSimHostsperSwitch/percentageSinks;
          if (num_sinks <= 1)
                break;

          if (j < num_sinks) {
              PacketSinkHelper sinkHelper
                ("ns3::TcpSocketFactory",
                InetSocketAddress (Ipv4Address::GetAny (), 9999));

              ApplicationContainer sinkApp =
                sinkHelper.Install (simHosts.Get (i*nSimHostsperSwitch + j));
                  
              sinkApp.Start (Seconds (INIT_DELAY_SECS));
          } else {
              r1 = urng->GetInteger(0, nSwitches - 1);
              r2 = urng->GetValue (0, num_sinks - 1);
              start_time = 10 * urng->GetValue ();
              //start_time = 0.0;
              OnOffHelper client ("ns3::TcpSocketFactory", Address ());

              AddressValue remoteAddress
                (InetSocketAddress (simHosts.Get (r1*nSimHostsperSwitch + r2)->GetObject<Ipv4>()->GetAddress (1,0).GetLocal (), 9999));

              client.SetAttribute ("Remote", remoteAddress);
              ApplicationContainer clientApp;
              clientApp.Add (client.Install (simHosts.Get (i*nSimHostsperSwitch + j)));
              clientApp.Start (Seconds ((double)INIT_DELAY_SECS + start_time));
             // std::cout << "Scheduling Client APP start at : " << (double)INIT_DELAY_SECS + start_time << " for address: " << simHosts.Get (r1*nSimHostsperSwitch + r2)->GetObject<Ipv4>()->GetAddress (1,0).GetLocal () << std::endl;
         }
        
       }
   }
   
   
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


   Ptr<OutputStreamWrapper> routingStream =
        Create<OutputStreamWrapper> ("global-routing-multi-switch-plus-router.routes", std::ios::out);
 
   Ipv4GlobalRoutingHelper g;
   g.PrintRoutingTableAllAt (Seconds (0.1), routingStream);

   TIMER_NOW (t1);
  if (enable_timekeeper) {
	std::cout << "Setting Timeslice for Tk Experiment: " << timeslice << std::endl;
	if (set_cbe_exp_timeslice(timeslice*(int)tdf) < 0) {
		std::cout << "Set Exp timeslice failed !" << std::endl;
		exit(0);
	}
       
	std::cout << "Synchronize and Freeze initiated !" << std::endl;	
	synchronizeAndFreeze();
        system("touch /tmp/trigger.txt");
	std::cout << "Synchronize and Freeze Succeeded !" << std::endl;
        std::cout << "Running Simulator" << std::endl;
        Simulator::Stop (Seconds(INIT_DELAY_SECS));
	Simulator::Run ();
	usleep(1000000);
	startExp();
	num_rounds_to_run = (run_time_secs * NS_IN_SEC)/advance_ns_per_round;
	std::cout << "Running for: " << num_rounds_to_run << " rounds !" << std::endl;
	for(int i = 0; i < num_rounds_to_run; i++) {
	       Simulator::Stop (NanoSeconds(advance_ns_per_round));
	       Simulator::Run ();
	       if (progress_exp_cbe(1) < 0) {
			std::cout << "Progress exp cbe failed ! " << std::endl;
			exit(0);
		}
               time_elapsed += (int)advance_ns_per_round;
               if (i % 1000 == 0) {
		    std::cout << "Time Elapsed: (sec): " << (float)time_elapsed/NS_IN_SEC << std::endl;
	       }
	}
	std::cout << "Resuming Exp ... " << std::endl;
	usleep(1000000);
	resume_exp_cbe();
	std::cout << "Stopping Synchronized Experiment (Waiting 10 secs) ... " << std::endl; 
	stopExp();
	usleep(10000000);

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
