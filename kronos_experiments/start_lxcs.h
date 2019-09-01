#ifndef __LXC_MANAGER_H
#define __LXC_MANAGER_H

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <readline/readline.h>

#include <iostream>
#include <unordered_map>


using namespace std;

namespace lxc_manager {
	class LXCManager {

		private:
			int numLxcs;
			string startupCmdsDir;
                        bool is_virtual;
                        long num_insns_per_round = 1000000;
			float relative_cpu_speed = 1.0;
			std::unordered_map<int, std::string> lxcNumToStartupCmd;
		public:
		LXCManager(int num_lxcs, string startup_cmds_dir, bool is_virtual,
			   long num_insns_per_round, float relative_cpu_speed) :
			numLxcs(num_lxcs), startupCmdsDir(startup_cmds_dir),
			is_virtual(is_virtual), num_insns_per_round(num_insns_per_round),
			relative_cpu_speed(relative_cpu_speed) {};
		void createConfigFiles();
		void startLXCs();
		void stopLXCs();
		string wrapCmd(string origCmd, bool is_cmd_file);

	};

}

#endif
