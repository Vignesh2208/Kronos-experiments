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
 * (c) 2009, GTech Systems, Inc. - Alfred Park <park@gtech-systems.com>
 *
 * DARPA NMS Campus Network Model
 *
 * This topology replicates the original NMS Campus Network model
 * with the exception of chord links (which were never utilized in the
 * original model)
 * Link Bandwidths and Delays may not be the same as the original
 * specifications
 *
 * Modified for distributed simulation by Josh Pelkey <jpelkey@gatech.edu>
 *
 * The fundamental unit of the NMS model consists of a campus network. The
 * campus network topology can been seen here:
 * http://www.nsnam.org/~jpelkey3/nms.png
 * The number of hosts (default 42) is variable.  Finally, an arbitrary
 * number of these campus networks can be connected together (default 2)
 * to make very large simulations.
 */

// for timing functions
#include <cstdlib>
#include <sys/time.h>
#include <fstream>
#include <vector>
#include <map>

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
#define TIMER_SECONDS(_t) ((double)(_t).tv_sec + (_t).tv_usec * 1e-6)
#define TIMER_DIFF(_t1, _t2) (TIMER_SECONDS (_t1) - TIMER_SECONDS (_t2))

NS_LOG_COMPONENT_DEFINE ("CampusNetworkModel");

int
main (int argc, char *argv[])
{

  typedef std::vector<NetDeviceContainer> vectorOfNetDeviceContainer;

  int num_lxcs;
  string startup_cmds_dir = "/home/kronos/ns-allinone-3.29/ns-3.29/examples/vt_experiments/common/startup_cmds/campus_network";
  string logs_base_dir = "/home/kronos/ns-allinone-3.29/ns-3.29/examples/vt_experiments/NSDI-2020/experiment_logs/simple_p2p_network";
  string log_folder = "test_logs";
  bool enable_kronos = false;
  long num_insns_per_round = 1000000;
  float relative_cpu_speed = 1.0;
  long run_time_secs = 10;
  float advance_ns_per_round;
  long time_elapsed = 0;
  long num_rounds_to_run;
  int percentageSinks = 10;
  int INIT_DELAY_SECS = 10;

  int nLXCsperSwitch = 1;
  int nTotalSwitches = 25;
  int nLANSwitches = 12;
  int nSimHostsperSwitch = 1;
  std::map<std::string, int> idx_track;
  std::map<int, std::string> lanSwitchToBackboneSwitch;

  TIMER_TYPE t0, t1, t2;
  TIMER_NOW (t0);
  std::cout << " ==== DARPA NMS CAMPUS NETWORK SIMULATION ====" << std::endl;

  bool nix = false;

  CsmaHelper csma_sw_sw;
  csma_sw_sw.SetChannelAttribute ("DataRate", StringValue ("1Gbps"));
  csma_sw_sw.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (1)));

   // Set up some default values for the simulation.
   Config::SetDefault ("ns3::OnOffApplication::PacketSize", UintegerValue (1024));
   Config::SetDefault ("ns3::OnOffApplication::DataRate", StringValue ("10Mb/s"));

  CsmaHelper csma_host_sw;
  csma_host_sw.SetChannelAttribute ("DataRate", StringValue ("1Gbps"));
  csma_host_sw.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (1)));

  CommandLine cmd;
  cmd.AddValue ("LAN", "Number of simulated nodes per LAN Switch [1]", nSimHostsperSwitch);
  cmd.AddValue ("nix", "Toggle the use of nix-vector or global routing", nix);
  cmd.AddValue ("nLXCsperLAN", "Number of VMs/LXCs per LAN Switch", nLXCsperSwitch); 
  cmd.AddValue ("numInsnsPerRound", "Number insns per round", num_insns_per_round); 
  cmd.AddValue ("relCpuSpeed", "Relative CPU speed", relative_cpu_speed); 
  cmd.AddValue ("runTimeSecs", "Running Time Secs", run_time_secs); 
  cmd.AddValue ("enableKronos", "Enable/disable Kronos", enable_kronos);
  cmd.AddValue ("startupCmds", "Start up cmds directory", startup_cmds_dir);
  cmd.AddValue ("logFolder", "Folder Name in logs base dir", log_folder); 
  cmd.Parse (argc,argv);


  num_lxcs = nLXCsperSwitch*nLANSwitches;
  LXCManager lxcManager(num_lxcs, startup_cmds_dir, enable_kronos,
			num_insns_per_round, relative_cpu_speed);

  if (!enable_kronos)
        INIT_DELAY_SECS = 0;


  TapBridgeHelper tapBridge;
  Address serverAddress;
  tapBridge.SetAttribute ("Mode", StringValue ("UseBridge"));

  NodeContainer switches;
  NodeContainer simHosts;
  NodeContainer emuHosts;
  NodeContainer allHosts;

  BridgeHelper bridgeHelper;
  vectorOfNetDeviceContainer bridgedDevices(nTotalSwitches);

  

  int network_start = 1, num_nodes_assigned = 1;
  for (int i = 0; i < num_lxcs; i++) {
     if (num_nodes_assigned > 255) {
         network_start ++;
         num_nodes_assigned = 1;
     }
     std::string ip_address = "10.1." + std::to_string(network_start) + "." + std::to_string(num_nodes_assigned); 
     lxcManager.lxcIPs.push_back(ip_address);
     num_nodes_assigned ++;
  }

  advance_ns_per_round = num_insns_per_round/relative_cpu_speed;
  if (enable_kronos) {
       if (initializeExp(1) < 0) {
	        std::cout << "Kronos initialization failed !. Make sure Kronos is loaded ! " << std::endl;
        }
  }
  


   NS_LOG_INFO ("Create nodes.");
   for (int i =0; i < nSimHostsperSwitch*nLANSwitches; i++) {
        Ptr<Node> node = CreateObject<Node> ();
        simHosts.Add(node);
        allHosts.Add(node);
   }
   for (int i =0; i < nLXCsperSwitch*nLANSwitches; i++) {
        Ptr<Node> node = CreateObject<Node> ();
        emuHosts.Add(node);
        allHosts.Add(node);
   }
   for (int i =0; i < nTotalSwitches; i++) {
        Ptr<Node> node = CreateObject<Node> ();
        switches.Add(node);
   }
 
   
   NS_LOG_INFO ("Create IPv4 Internet Stack");
   InternetStackHelper internetv4;
   internetv4.Install (allHosts);

   if (nix) {
     Ipv4NixVectorHelper nixRouting;
     internetv4.SetRoutingHelper (nixRouting); // has effect on the next Install ()
   }


   NetDeviceContainer simHostDevices;
   NetDeviceContainer emuHostDevices;
   NetDeviceContainer allHostDevices;
 
 
   NS_LOG_INFO ("Create channels.");

   NS_LOG_INFO("Creating backbone links");
   
   // For Switch_00
   idx_track["000"] = 0;
   idx_track["001"] = 1;
   idx_track["20"] = 2;
   idx_track["21"] = 3;
   idx_track["22"] = 4;
   idx_track["23"] = 5;
   idx_track["24"] = 6;
   idx_track["25"] = 7;
   idx_track["26"] = 8;
   idx_track["30"] = 9;
   idx_track["31"] = 10;
   idx_track["32"] = 11;
   idx_track["33"] = 12;

   lanSwitchToBackboneSwitch[0] = "22";
   lanSwitchToBackboneSwitch[1] = "23";
   lanSwitchToBackboneSwitch[2] = "24";
   lanSwitchToBackboneSwitch[3] = "25";
   lanSwitchToBackboneSwitch[4] = "26";
   lanSwitchToBackboneSwitch[5] = "26";
   lanSwitchToBackboneSwitch[6] = "26";
   lanSwitchToBackboneSwitch[7] = "30";
   lanSwitchToBackboneSwitch[8] = "30";
   lanSwitchToBackboneSwitch[9] = "32";
   lanSwitchToBackboneSwitch[10] = "33";
   lanSwitchToBackboneSwitch[11] = "33";
   
   // sw_000 sw_001
   auto link = csma_sw_sw.Install(NodeContainer(switches.Get(idx_track["000"]), switches.Get(idx_track["001"])));
   bridgedDevices[idx_track["000"]].Add(link.Get(0));
   bridgedDevices[idx_track["001"]].Add(link.Get(1));

   //sw_000 sw_20
   link = csma_sw_sw.Install(NodeContainer(switches.Get(idx_track["000"]), switches.Get(idx_track["20"])));
   bridgedDevices[idx_track["000"]].Add(link.Get(0));
   bridgedDevices[idx_track["20"]].Add(link.Get(1));

   //sw_000 sw_21
   link = csma_sw_sw.Install(NodeContainer(switches.Get(idx_track["000"]), switches.Get(idx_track["21"])));
   bridgedDevices[idx_track["000"]].Add(link.Get(0));
   bridgedDevices[idx_track["21"]].Add(link.Get(1));

   //sw_001 sw_30
   link = csma_sw_sw.Install(NodeContainer(switches.Get(idx_track["001"]), switches.Get(idx_track["30"])));
   bridgedDevices[idx_track["001"]].Add(link.Get(0));
   bridgedDevices[idx_track["30"]].Add(link.Get(1));


   //sw_001 sw_31
   link = csma_sw_sw.Install(NodeContainer(switches.Get(idx_track["001"]), switches.Get(idx_track["31"])));
   bridgedDevices[idx_track["001"]].Add(link.Get(0));
   bridgedDevices[idx_track["31"]].Add(link.Get(1));


   //sw_20 sw_21
   //link = csma_sw_sw_1.Install(NodeContainer(switches.Get(idx_track["20"]), switches.Get(idx_track["21"])));
   //bridgedDevices[idx_track["20"]].Add(link.Get(0));
   //bridgedDevices[idx_track["21"]].Add(link.Get(1));

   //sw_20 sw_22
   link = csma_sw_sw.Install(NodeContainer(switches.Get(idx_track["20"]), switches.Get(idx_track["22"])));
   bridgedDevices[idx_track["20"]].Add(link.Get(0));
   bridgedDevices[idx_track["22"]].Add(link.Get(1));


   //sw_21 sw_23
   link = csma_sw_sw.Install(NodeContainer(switches.Get(idx_track["21"]), switches.Get(idx_track["23"])));
   bridgedDevices[idx_track["21"]].Add(link.Get(0));
   bridgedDevices[idx_track["23"]].Add(link.Get(1));

   //sw_22 sw_23
   //link = csma_sw_sw.Install(NodeContainer(switches.Get(idx_track["22"]), switches.Get(idx_track["23"])));
   //bridgedDevices[idx_track["22"]].Add(link.Get(0));
   //bridgedDevices[idx_track["23"]].Add(link.Get(1));

   //sw_22 sw_24
   link = csma_sw_sw.Install(NodeContainer(switches.Get(idx_track["22"]), switches.Get(idx_track["24"])));
   bridgedDevices[idx_track["22"]].Add(link.Get(0));
   bridgedDevices[idx_track["24"]].Add(link.Get(1));

   //sw_23 sw_25
   link = csma_sw_sw.Install(NodeContainer(switches.Get(idx_track["23"]), switches.Get(idx_track["25"])));
   bridgedDevices[idx_track["23"]].Add(link.Get(0));
   bridgedDevices[idx_track["25"]].Add(link.Get(1));

   //sw_24 sw_25
   //link = csma_sw_sw.Install(NodeContainer(switches.Get(idx_track["24"]), switches.Get(idx_track["25"])));
   //bridgedDevices[idx_track["24"]].Add(link.Get(0));
   //bridgedDevices[idx_track["25"]].Add(link.Get(1));

   //sw_25 sw_26
   link = csma_sw_sw.Install(NodeContainer(switches.Get(idx_track["25"]), switches.Get(idx_track["26"])));
   bridgedDevices[idx_track["25"]].Add(link.Get(0));
   bridgedDevices[idx_track["26"]].Add(link.Get(1));

   //sw_30 sw_31
   //link = csma_sw_sw.Install(NodeContainer(switches.Get(idx_track["30"]), switches.Get(idx_track["31"])));
   //bridgedDevices[idx_track["30"]].Add(link.Get(0));
   //bridgedDevices[idx_track["31"]].Add(link.Get(1));

   //sw_31 sw_32
   link = csma_sw_sw.Install(NodeContainer(switches.Get(idx_track["31"]), switches.Get(idx_track["32"])));
   bridgedDevices[idx_track["31"]].Add(link.Get(0));
   bridgedDevices[idx_track["32"]].Add(link.Get(1));

   //sw_31 sw_33
   link = csma_sw_sw.Install(NodeContainer(switches.Get(idx_track["31"]), switches.Get(idx_track["33"])));
   bridgedDevices[idx_track["31"]].Add(link.Get(0));
   bridgedDevices[idx_track["33"]].Add(link.Get(1));

   //sw_32 sw_33
   //link = csma_sw_sw.Install(NodeContainer(switches.Get(idx_track["32"]), switches.Get(idx_track["33"])));
   //bridgedDevices[idx_track["32"]].Add(link.Get(0));
   //bridgedDevices[idx_track["33"]].Add(link.Get(1));

   NS_LOG_INFO("Creating LAN Switches and Backbone Switch Links");
   for (int i = 0; i < nLANSwitches; i++) {
        auto mappedSwitchIdx = idx_track[lanSwitchToBackboneSwitch[i]];
        auto link = csma_sw_sw.Install(NodeContainer(switches.Get(13 + i), switches.Get(mappedSwitchIdx) ));
        bridgedDevices[13 + i].Add(link.Get(0));
        bridgedDevices[mappedSwitchIdx].Add(link.Get(1));
   }

   NS_LOG_INFO("Creating SimHost-LANSwitch links");
   for (int i = 0; i < nLANSwitches; i++) {
        for (int j = 0; j < nSimHostsperSwitch; j++) {
                NetDeviceContainer host_j_sw_i_link = csma_host_sw.Install(NodeContainer(simHosts.Get(i*nSimHostsperSwitch + j), switches.Get(13 + i)));
                bridgedDevices[13 + i].Add(host_j_sw_i_link.Get(1));
                simHostDevices.Add(host_j_sw_i_link.Get(0));
        }
   }

   NS_LOG_INFO("Creating LXCHost-LANSwitch links");
   for (int i = 0; i < nLANSwitches; i++) {
        for (int j = 0; j < nLXCsperSwitch; j++) {
                NetDeviceContainer host_j_sw_i_link = csma_host_sw.Install(NodeContainer(emuHosts.Get(i*nLXCsperSwitch + j), switches.Get(13 + i)));
                bridgedDevices[13 + i].Add(host_j_sw_i_link.Get(1));
                emuHostDevices.Add(host_j_sw_i_link.Get(0));
        }
   }

   for (int i = 0; i < nLXCsperSwitch*nLANSwitches; i++) {
        allHostDevices.Add(emuHostDevices.Get(i));
   }
   
   for (int i = 0; i < nSimHostsperSwitch*nLANSwitches; i++) {
        allHostDevices.Add(simHostDevices.Get(i));
   }


   NS_LOG_INFO ("Create networks and assign IPv6 Addresses.");
   Ipv4AddressHelper ipv4;
   ipv4.SetBase ("10.1.0.0", "255.255.0.0", "0.0.1.1");
   ipv4.Assign (allHostDevices);

   NS_LOG_INFO("Bridging Switch Devices");
   for (int i = 0; i < nTotalSwitches; i++) {
        bridgeHelper.Install(switches.Get(i), bridgedDevices[i]);
   }


  TIMER_TYPE routingStart;
  TIMER_NOW (routingStart);

  if (!nix) {
        // Calculate routing tables
        std::cout << "Populating Routing tables..." << std::endl;
        Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
  }

  TIMER_TYPE routingEnd;
  TIMER_NOW (routingEnd);
  std::cout << "Routing tables population took "
            << TIMER_DIFF (routingEnd, routingStart) << std::endl;


   lxcManager.createConfigFiles();
   std::cout << "Starting LXCs ... " << std::endl;
   lxcManager.startLXCs();

   NS_LOG_INFO("Setting TapBridges");
   int lxc_counter = 1;
   for (int i = 0; i < nLANSwitches; i++) {
        for (int j = 0; j < nLXCsperSwitch; j++) {
                tapBridge.SetAttribute ("DeviceName", StringValue ("tap-" + std::to_string(lxc_counter)));
                tapBridge.Install (emuHosts.Get(lxc_counter - 1), emuHostDevices.Get(lxc_counter - 1));
                //Ptr<Ipv4> ipv4_add = emuHosts.Get(lxc_counter - 1)->GetObject<Ipv4>();
                //Ipv4InterfaceAddress iaddr = ipv4_add->GetAddress (1,0);
                //Ipv4Address addri = iaddr.GetLocal ();
                //std::cout << "IP ADDress of lxc-" << lxc_counter << " is: " << addri << std::endl; 
                //std::cout << "MAC address is: " << emuHostDevices.Get(lxc_counter - 1)->GetAddress() << std::endl;
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
  for (int i = 0; i < nLANSwitches; ++i)
    {
      for (int j = 0; j < nSimHostsperSwitch; ++j)
        {
          int num_sinks = nSimHostsperSwitch/percentageSinks;
          if (num_sinks <= 0)
                break;
          if (j < num_sinks) {
              PacketSinkHelper sinkHelper
                ("ns3::TcpSocketFactory",
                InetSocketAddress (Ipv4Address::GetAny (), 9999));

              ApplicationContainer sinkApp =
                sinkHelper.Install (simHosts.Get (i*nSimHostsperSwitch + j));

              sinkApp.Start (Seconds (INIT_DELAY_SECS));
          } else {
              r1 = urng->GetInteger(0, nLANSwitches - 1);
              r2 = urng->GetValue (0, num_sinks - 1);
              start_time = 10 * urng->GetValue ();
              OnOffHelper client ("ns3::TcpSocketFactory", Address ());

              AddressValue remoteAddress
                (InetSocketAddress (simHosts.Get (r1*nSimHostsperSwitch + r2)->GetObject<Ipv4>()->GetAddress (1,0).GetLocal (), 9999));

              client.SetAttribute ("Remote", remoteAddress);
              ApplicationContainer clientApp;
              clientApp.Add (client.Install (simHosts.Get (i*nSimHostsperSwitch + j)));
              clientApp.Start (Seconds ((double)INIT_DELAY_SECS + start_time));
         }
        
       }
   }
   


  

  std::cout << "Running simulator..." << std::endl;
  TIMER_NOW (t1);
  if (enable_kronos) {
        // Need to run the Simulator first for a bit to flush out all the unwanted multi cast router solicitation messages
        // which are sent upon lxc startup.
        Simulator::Stop (Seconds(INIT_DELAY_SECS));
        Simulator::Run ();

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
               if (i % 1000 == 0)
                        std::cout << "Virtual Time Elapsed: (sec): " << (float)time_elapsed/NS_IN_SEC << std::endl;
	}
	stopExp();

    } else {    
	    for (int i = 0 ; i < run_time_secs * 100; i++) {
	       std::cout << " ############ Running Round: " << i + 1 << "############### " << std::endl;
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
  // Exit the parallel execution environment
  //MpiInterface::Disable ();
  double d1 = TIMER_DIFF (t1, t0), d2 = TIMER_DIFF (t2, t1);
  std::cout << "-----" << std::endl << "Runtime Stats:" << std::endl;
  std::cout << "Simulator init time: " << d1 << std::endl;
  std::cout << "Simulator run time: " << d2 << std::endl;
  std::cout << "Total elapsed time: " << d1 + d2 << std::endl;

   system(("sudo mkdir -p " + logs_base_dir + "/" + log_folder).c_str());
   system(("sudo cp -R /tmp/lxc* " + logs_base_dir + "/" + log_folder).c_str());
   system(("sudo chmod -R 777 " + logs_base_dir).c_str());
  return 0;
}

