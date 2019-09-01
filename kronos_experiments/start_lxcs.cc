#include <iostream>
#include <fstream>
#include <string> 

#include "start_lxcs.h"

#define MAX_BUF 1024

using namespace std;
using namespace lxc_manager;

const string cmd_executor_path = "/home/kronos/ns-allinone-3.29/ns-3.29/examples/vt_experiments/common/cmd_executor";

//Creates the necessary configuration files for the LXCs
void LXCManager::createConfigFiles() {
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
			myfile << "lxc.network.ipv4 = 10.0.0." << i << "/24\n";
			myfile << "lxc.aa_profile = unconfined\n";
		        myfile << "lxc.aa_allow_incomplete = 1\n";
			myfile << "lxc.mount.auto=proc\n";
                        myfile << "lxc.environment = HOME=/home/kronos\n";
			myfile.close();
		}

		system(("sed \'s/@NODEID@/" + std::to_string(i) + "/g\' " + startupCmdsDir + "/olsrd_config.template > /tmp/lxc-" + std::to_string(i) + "/lxc-" + std::to_string(i) + "-olsrd-config").c_str());
		
		
	}
	
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
	
}

string LXCManager::wrapCmd(string origCmdContainer, bool is_file) {
	if (!is_virtual) {
		if (!is_file)
			return origCmdContainer;
		else {
			return cmd_executor_path + " -f " + origCmdContainer;
		}
	}
	if (is_file)
		return "sudo tracer -r " + std::to_string(relative_cpu_speed) + " -n " + std::to_string(num_insns_per_round) + " -f \"" + origCmdContainer + "\"";
	else
		return "sudo tracer -r " + std::to_string(relative_cpu_speed) + " -n " + std::to_string(num_insns_per_round) + " -c \"" + origCmdContainer + "\"";
}

// Starts all of the LXCs
void LXCManager::startLXCs() {
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
		default_cmd = "lxc-start -n lxc-" + std::to_string(i) + " -L " + lxc_log + " -d -- " + wrapCmd(default_cmd, is_cmd_file);
		std::cout << "Starting Command: " << default_cmd << " inside LXC: " << i << std::endl;
		system(default_cmd.c_str());
	}
}

void LXCManager::stopLXCs() {
	
	for(int i = 1; i  <=numLxcs; i++) {
		system(("rm /tmp/lxc-" + std::to_string(i) + "/olsrd.lock").c_str());
	}
	system("/tmp/teardown.sh");
}

