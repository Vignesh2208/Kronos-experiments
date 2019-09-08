#include <iostream>
#include <fstream>
#include <string> 
#include <algorithm>

#include "start_lxcs.h"



#define MAX_BUF 1024

using namespace std;
using namespace lxc_manager;

const string cmd_executor_path = "/home/kronos/ns-allinone-3.29/ns-3.29/examples/vt_experiments/common/cmd_executor_tk";

//Creates the necessary configuration files for the LXCs
void TkLXCManager::createConfigFiles() {
	int i;
	string file_name;
	for (i = 1; i <= numLxcs; i++) {
		system(("mkdir -p /tmp/lxc-" + std::to_string(i)).c_str());
		file_name = "/tmp/lxc-" + std::to_string(i) + "/lxc-" + std::to_string(i) + ".conf";
		ofstream myfile;
		myfile.open(file_name, std::ofstream::out);
		if (myfile.is_open()) {
			myfile << "lxc.utsname = lxc-" << i << "\n";
			myfile << "lxc.network.type = veth\n";
			myfile << "lxc.network.flags = up\n";
			myfile << "lxc.network.link = br-" << i << "\n";
			if (lxcIPs.empty()) {
			    myfile << "lxc.network.ipv4 = 10.0.0." << i << "/24\n";
                        } else {
			    myfile << "lxc.network.ipv4 = " << lxcIPs[i-1] << "/16\n";
			}
                        myfile << "lxc.network.hwaddr = 02:00:00:00:" << std::hex << (int)(i / 256) << ":" << (i % 256) << "\n";
			myfile << "lxc.aa_profile = unconfined\n";
		        myfile << "lxc.aa_allow_incomplete = 1\n";
			myfile << "lxc.mount.auto=proc\n";
                        myfile << "lxc.environment = HOME=/home/kronos\n";
			myfile.close();
		}

		system(("sed \'s/@NODEID@/" + std::to_string(i) + "/g\' " + startupCmdsDir + "/olsrd_config.template > /tmp/lxc-" + std::to_string(i) + "/lxc-" + std::to_string(i) + "-olsrd-config").c_str());
		
		
	}

        file_name = "/tmp/arp.txt";
	ofstream arpfile;
	arpfile.open(file_name, std::ofstream::out);
	if (arpfile.is_open()) {
		for (i = 1; i <= numLxcs; i++) {
		
		                arpfile << lxcIPs[i-1] << " 02:00:00:00:" << std::hex << (int)(i / 256) << ":" << (i % 256) << "\n";
		
		}
	}
	arpfile.close();

	
	file_name = "/tmp/startup.sh";
	ofstream startup_file;
	startup_file.open(file_name, std::ofstream::out);
	if (startup_file.is_open()) {
		startup_file << "#!/bin/bash\n";
		for (i = 1; i <= numLxcs; i++) {
			startup_file << "brctl addbr br-" << i << "\n";
			startup_file << "tunctl -t tap-" << i << "\n";
			startup_file << "ifconfig tap-" << i << " 0.0.0.0 promisc up\n";
			startup_file << "brctl addif br-" << i <<" tap-" << i << "\n";
			startup_file << "ifconfig br-" << i <<" up\n";
			startup_file << "lxc-create -n lxc-"<< i << " -t none -f /tmp/lxc-" << i << "/lxc-"<< i <<".conf\n\n";
		}
		startup_file.close();
	}

	system("chmod +x /tmp/startup.sh");

	file_name = "/tmp/teardown.sh";
	ofstream teardown_file;
	teardown_file.open(file_name, std::ofstream::out);
	if (teardown_file.is_open()) {
		teardown_file << "#!/bin/bash\n";
		for (i = 1; i <= numLxcs; i++) {
			teardown_file << "lxc-stop -n lxc-"<< i <<" -k\n";
			teardown_file << "lxc-destroy -n lxc-"<< i <<"\n";
			teardown_file << "ifconfig br-"<< i <<" down\n";
			teardown_file << "brctl delif br-"<< i <<" tap-"<< i <<"\n";
			teardown_file << "brctl delbr br-"<< i << "\n";
			teardown_file << "ifconfig tap-"<< i <<" down\n";
			teardown_file << "tunctl -d tap-"<< i <<"\n\n";
		}
		teardown_file.close();
	}
	system("chmod +x /tmp/teardown.sh");

	string copy_cmd = "cp " + startupCmdsDir + "/* /tmp";
	system(copy_cmd.c_str());
	for (i = 1; i <= numLxcs; i++) {
		std::ifstream infile;
		string lxc_cmds_file = "/tmp/lxc-" + std::to_string(i) + "-cmds.txt";
		infile.open(lxc_cmds_file);
		if (infile.is_open()) {
			this->lxcNumToStartupCmd.insert(std::make_pair(i, lxc_cmds_file));
		}
	}

	if (is_virtual) {
		std::cout << "Removing Previously inserted Tk Module " << std::endl;
		system("rmmod /home/kronos/TimeKeeper/dilation-code/build/TimeKeeper.ko");
		usleep(1000000);
		
		std::cout << "Inserting Tk Module " << std::endl;
		system("insmod /home/kronos/TimeKeeper/dilation-code/build/TimeKeeper.ko");
		usleep(1000000);

	}	
}

string TkLXCManager::wrapCmd(string origCmdContainer, string pidLog) {
	
	return cmd_executor_path + " -f " + origCmdContainer + " -l " + pidLog;
}

// Starts all of the LXCs
void TkLXCManager::startLXCs() {
	int i;
	char cwd[MAX_BUF];
	if (getcwd(cwd, sizeof(cwd)) == NULL)
            perror("getcwd() error");
	system("/tmp/startup.sh");
	for (i = 1; i <= numLxcs; i++) {
		string default_cmd = "sleep 10";
		bool is_cmd_file = false;
		auto got = this->lxcNumToStartupCmd.find(i);
		if (got != this->lxcNumToStartupCmd.end()) {
			default_cmd = got->second;
			is_cmd_file = true;
		}
		
		std::string lxc_log = "/tmp/lxc-" + std::to_string(i) + "/lxc-" + std::to_string(i) + ".log";
		ofstream myfile;
		myfile.open(lxc_log, std::ofstream::out);
		myfile.close();

		std::string lxc_startup ="/tmp/lxc-" + std::to_string(i) + "/lxc-" + std::to_string(i) + "-startup.sh";
		std::string pid_log = "/tmp/lxc-" + std::to_string(i) + "/lxc-" + std::to_string(i) + "-pid.log";
		myfile.open(lxc_startup, std::ofstream::out);
		myfile << "#!/bin/bash\n";
		myfile << "arp -f /tmp/arp.txt\n";
		if (!is_cmd_file) {
			myfile << default_cmd + "\n";
		} else {
			myfile << wrapCmd(default_cmd, pid_log) + "\n";
		}
		myfile.close();
		system(("chmod +x " + lxc_startup).c_str());



		default_cmd = "lxc-start -n lxc-" + std::to_string(i) + " -L " + lxc_log + " -d -- " + lxc_startup;
		std::cout << "Starting Command: " << default_cmd << " inside LXC: " << i << std::endl;
		system(default_cmd.c_str());
		string lxc_name = "lxc-" + std::to_string(i);
		int pid = getpidfromname((char *)lxc_name.c_str());
		std::cout << "LXC-PID: " << pid << std::endl;
		usleep(1000000);
		std::ifstream infile(pid_log);
		std::string line;
		while (std::getline(infile, line)) {
			int child_pid = std::stoi(line);
			if (child_pid > 1 && is_virtual) {
				std::cout << "Dilating Child: " << child_pid << " of LXC: " << lxc_name << " with TDF: " << tdf << std::endl;
				dilate_all(child_pid, tdf);
				addToExp(child_pid, -1);
			} else if (child_pid > 1) {
				std::cout << "Child: " << child_pid << " belongs to LXC: " << lxc_name << std::endl;
			}
		}
		infile.close();
	}
}

void TkLXCManager::stopLXCs() {
	
	for(int i = 1; i  <=numLxcs; i++) {
		system(("rm /tmp/lxc-" + std::to_string(i) + "/olsrd.lock").c_str());
	}
	system("/tmp/teardown.sh");
}

