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
#include <vector>
#include <unordered_map>

extern "C"
{
#include "utility_functions.h"
#include "TimeKeeper_functions.h"   
}

using namespace std;

namespace lxc_manager {

	class TkLXCManager {

		private:
			int numLxcs;
			string startupCmdsDir;
                        bool is_virtual;
                        long timeslice = 100000;
			float tdf = 1.0;
			std::unordered_map<int, std::string> lxcNumToStartupCmd;
		public:
                std::vector<std::string> lxcIPs;
		TkLXCManager(int num_lxcs, string startup_cmds_dir, bool is_virtual,
			   long timeslice, float tdf) :
			numLxcs(num_lxcs), startupCmdsDir(startup_cmds_dir),
			is_virtual(is_virtual), timeslice(timeslice),
			tdf(tdf) {};
		void createConfigFiles();
		void startLXCs();
		void stopLXCs();
		string wrapCmd(string origCmd, string logFile);

	};

}

#endif
