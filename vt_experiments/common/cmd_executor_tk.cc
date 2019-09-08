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
#include <iostream>
#include <fstream>
#include <string> 
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <sys/wait.h>

#define GET_PID_ROOT	'X'

const char *FILENAME = "/proc/dilation/status"; //where TimeKeeper LKM is reading commands

/*
Sends a specific command to the TimeKeeper Kernel Module. To communicate with the TLKM, you send messages to the location specified by FILENAME
*/
int send_to_timekeeper(char * cmd) {
    int filedesc = open(FILENAME, O_WRONLY | O_APPEND);
    int ret;
 
    if (filedesc < 0) {
        return -1;
    }
 
    ret = write(filedesc, cmd, strlen(cmd));
    close(filedesc);
    return ret;
}

int file_exists(const char * file_path) {
    /*int filedesc = open(FILENAME, O_RDONLY); 
    if (filedesc < 0) {
        return -1;
    }
    close(filedesc);*/
    if( access( file_path, F_OK ) != -1 ) {
   	return 1;
    } else {
        return -1;
    }
}

int is_root() {
        if (geteuid() == 0)
                return 1;
        printf("Needs to be ran as root\n");
        return 0;

}

/*
Returns 1 if module is loaded, 0 otherwise
*/
int isModuleLoaded() {
        if( access( FILENAME, F_OK ) != -1 ) {
                return 1;
        } else {
                printf("TimeKeeper kernel module is not loaded\n");
                return 0;
        }
}

int get_pid_root(int pid) {
	char command[100];
	memset(command,0,100*sizeof(char));
	
	if (is_root() && isModuleLoaded()) {
		sprintf(command, "%c,%d",GET_PID_ROOT,pid);
		return send_to_timekeeper(command);
	}
	return -1; 
}

using namespace std;

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
    string startup_cmds_file;
    string pid_log_file;
    std::vector <pid_t> pids;

    if (!input.cmdOptionExists("-f") || !input.cmdOptionExists("-l")) {
        std::cout << "ERROR: Missing arguments! " << std::endl;
        std::cout << "Usage: cmd_executor -f <startup cmds file> -l <pid_log_file>"
                << std::endl;
        exit(0);
    }
       
    startup_cmds_file = input.getCmdOption("-f");
    std::ifstream inputFile;
    pid_log_file = input.getCmdOption("-l");
    std::ofstream logFile;

    inputFile.open(startup_cmds_file);
    if (inputFile.is_open()) {
	for( std::string line; getline( inputFile, line );) {
		pid_t pid = fork();
		if (pid > 0) {
		    pids.push_back(pid);
		} else {
		    std::cout << "Waiting for trigger file ... " << std::endl;
		    std::string trigger_file = "/tmp/trigger.txt";
		    while(file_exists(trigger_file.c_str()) < 0) {
    			usleep(100000);
		    }
		    std::cout << "Starting Process: " << line << std::endl;
		    system(line.c_str());
		    exit(-1);
		}
	}
    }
    std::cout << "Logging all child pids ... " << std::endl;
    logFile.open(pid_log_file, std::ofstream::out);
    if (logFile.is_open()) {
        for(pid_t pid : pids) {
		logFile << get_pid_root(pid) << "\n";
	}  
    }
    logFile.close();


    std::cout << "Waiting for all child processes to finish ... " << std::endl;
    for(pid_t pid : pids) {
        int status;
  	waitpid(pid, &status, 0);
    }   
    std::cout << "All child processes exited ! " << std::endl;
    return 0;
}
