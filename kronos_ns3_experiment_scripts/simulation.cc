
#include <iostream> 
#include <csignal> 
#include <ctime>
#include <ratio>
#include <chrono>
#include <thread>
#include <unistd.h>
#include <vector>
#include <algorithm>
#include <unistd.h>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/tap-bridge-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "start_lxcs.h"

#define NS_IN_SEC 1000000000
#define NS_IN_MS 1000000

extern "C"
{
#include <Kronos_functions.h>   
}

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TapCsmaVirtualMachineExample");


using namespace std;
using namespace lxc_manager;

class InputParser{
    public:
        InputParser (int &argc, char **argv){
            for (int i=1; i < argc; ++i)
                this->tokens.push_back(std::string(argv[i]));
        }
        const std::string& getCmdOption(const std::string &option) const{
            auto itr =  std::find(this->tokens.begin(), this->tokens.end(), option);
            if (itr != this->tokens.end() && ++itr != this->tokens.end()){
                return *itr;
            }
            static const std::string empty_string("");
            return empty_string;
        }
        bool cmdOptionExists(const std::string &option) const{
            return std::find(this->tokens.begin(), this->tokens.end(), option)
                   != this->tokens.end();
        }
    private:
        std::vector <std::string> tokens;
};


int main(int argc, char **argv) {

     
    InputParser input(argc, argv);
    int num_lxcs;
    string startup_cmds_dir;
    bool enable_kronos = false;
    bool enable_wifi = false;
    long num_insns_per_round = 1000000;
    float relative_cpu_speed = 1.0;
    long run_time_secs = 10;
    float advance_ns_per_round;
    long time_elapsed = 0;
    long num_rounds_to_run;


    if (!input.cmdOptionExists("-f") || !input.cmdOptionExists("-n")) {
        std::cout << "ERROR: Missing arguments! " << std::endl;
        std::cout << "Usage: simulation -f <lxc startup cmds directory>"
                " -n <num_lxcs> -w <1 - enable wifi, 0- enable csma> -r <run time secs>"
		" [-e <1 or 0 to enable/disable kronos>] "
                " [-i <num_insns_per_round - only valid if Kronos is enabled >] "
                " [-s <relative cpu speed - only valid if Kronos is enabled>]"

                << std::endl;
        exit(0);
    }


    
    num_lxcs = std::stol(input.getCmdOption("-n"));        
    startup_cmds_dir = input.getCmdOption("-f");

    if (input.cmdOptionExists("-e")) {
        string is_virtual = input.getCmdOption("-e");

        if (is_virtual == "1") {
            std::cout << "Starting under virtual time ..." << std::endl;
            enable_kronos = true;
        }
        
    }

    if (input.cmdOptionExists("-w")) {
        string wifi_enable = input.getCmdOption("-w");

        if (wifi_enable == "1") {
            std::cout << "Starting WIFI network ..." << std::endl;
            enable_wifi = true;
        }
        
    }


    if (input.cmdOptionExists("-i")) {
        num_insns_per_round = std::stol(input.getCmdOption("-i"));        
    }

    if (input.cmdOptionExists("-r")) {
        run_time_secs = std::stol(input.getCmdOption("-r"));        
    }


    if (input.cmdOptionExists("-s")) {
        relative_cpu_speed = std::stof(input.getCmdOption("-s"));
        if (relative_cpu_speed <= 0.0) 
            relative_cpu_speed = 1.0;        
    }

    LXCManager lxcManager(num_lxcs, startup_cmds_dir, enable_kronos,
			  num_insns_per_round, relative_cpu_speed);
    lxcManager.createConfigFiles();

    NodeContainer nodes;
    nodes.Create (num_lxcs);
    advance_ns_per_round = num_insns_per_round/relative_cpu_speed;

    if (enable_kronos) {
	if (initializeExp(1) < 0) {
		std::cout << "Kronos initialization failed !. Make sure Kronos is loaded ! " << std::endl;
	}
    }


    /* For CSMA networks */
    if (!enable_wifi) {  
	    CsmaHelper csma;
	    NetDeviceContainer devices = csma.Install (nodes);
	    TapBridgeHelper tapBridge;
	    tapBridge.SetAttribute ("Mode", StringValue ("UseBridge"));

	    std::cout << "Starting LXCs ... " << std::endl;
	    lxcManager.startLXCs();

	    for (int i = 0 ; i < num_lxcs; i++) {
	      tapBridge.SetAttribute ("DeviceName", StringValue ("tap-" + std::to_string(i + 1)));
	      tapBridge.Install (nodes.Get (i), devices.Get (i));
	    }

	    
    } else {
	int Max_X = 100;
	int Max_Y = 100;
	float step_size = 1.0;
	WifiHelper wifi;
	wifi.SetStandard (WIFI_PHY_STANDARD_80211a);
	wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode", StringValue ("OfdmRate54Mbps"));

	// No Access Points. We'll use an ad-hoc network
	WifiMacHelper wifiMac;
	wifiMac.SetType ("ns3::AdhocWifiMac");

	// Configure the physical layer.
	YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
	YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
	wifiPhy.SetChannel (wifiChannel.Create ());

	// Install the wireless devices onto our ghost nodes.
	NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);

	// We need location information since we are talking about wifi, so add a
	// constant position to the ghost nodes.
	MobilityHelper mobility;
	Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();

	// set node positions here ...
	for (int i =0; i < num_lxcs; i++) {
		int x_coord =  i / Max_X;
		int y_coord = i % Max_Y;
		positionAlloc->Add (Vector (x_coord*step_size, y_coord*step_size, 0.0));
	}

	mobility.SetPositionAllocator (positionAlloc);
	mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
	mobility.Install (nodes);
        TapBridgeHelper tapBridge;
	tapBridge.SetAttribute ("Mode", StringValue ("UseLocal"));
	//Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

	std::cout << "Starting LXCs ... " << std::endl;
	lxcManager.startLXCs();

	for (int i = 0 ; i < num_lxcs; i++) {
		tapBridge.SetAttribute ("DeviceName", StringValue ("tap-" + std::to_string(i + 1)));
		tapBridge.Install (nodes.Get (i), devices.Get (i));
	}

    }

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
               //if (time_elapsed % 10*NS_IN_MS == 0) {
			std::cout << "Virtual Time Elapsed: (sec): " << (float)time_elapsed/NS_IN_SEC << std::endl;
	       //}
	}
	stopExp();

    } else {    
	    for (int i = 0 ; i < 1000; i++) {
	       std::cout << " ############ Running Round: " << i + 1 << "############### " << std::endl;
	       Simulator::Stop (MilliSeconds(10));
	       Simulator::Run ();
	       usleep(10000);
	    }
    }
    Simulator::Destroy ();
    std::cout << "Stopping LXCs ..." << std::endl;
    lxcManager.stopLXCs();


    return 0;
}
