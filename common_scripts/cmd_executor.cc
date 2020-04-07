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
#include <sys/wait.h>

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
    std::vector <pid_t> pids;

    if (!input.cmdOptionExists("-f")) {
        std::cout << "ERROR: Missing arguments! " << std::endl;
        std::cout << "Usage: cmd_executor -f <startup cmds file>"
                << std::endl;
        exit(0);
    }
       
    startup_cmds_file = input.getCmdOption("-f");
    std::ifstream inputFile;
    inputFile.open(startup_cmds_file);
    if (inputFile.is_open()) {
	for( std::string line; getline( inputFile, line );) {
		pid_t pid = fork();
		if (pid > 0) {
		    pids.push_back(pid);
		} else {
		    std::cout << "Starting Process: " << line << std::endl;
		    system(line.c_str());
		    exit(-1);
		}
	}
    }

    std::cout << "Waiting for all child processes to finish ... " << std::endl;
    for(pid_t pid : pids) {
        int status;
  	waitpid(pid, &status, 0);
    }   
    std::cout << "All child processes exited ! " << std::endl;
    return 0;
}
