#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <chrono>
#include <vector>
#include <math.h>
#include <stdio.h>
#include <sys/time.h>
#include <unistd.h> 

using namespace std;

#include <openssl/sha.h>
double sha256(const string str)
{
    unsigned char hash[SHA256_DIGEST_LENGTH];
    
    
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, str.c_str(), str.size());
    SHA256_Final(hash, &sha256);
    stringstream ss;
    for(int i = 0; i < SHA256_DIGEST_LENGTH; i++)
    {
        ss << hex << setw(2) << setfill('0') << (int)hash[i];
    }
    
    //for(int i = 0; i < 1000000; i++);
    

    return 0;
}

int main() {
    double mu_elapsed_time = 0.0;
    double std_elapsed_time = 0.0;
    for(int i = 0; i < 1; i++) {
	struct timeval t1, t2;
        double elapsedTime;
    	gettimeofday(&t1, NULL);
	double duration_ns;
        for (int j= 0; j < 1000; j++) {
           sha256("1234567890_1" + std::to_string(j));
        }
	gettimeofday(&t2, NULL);
        elapsedTime = (t2.tv_sec - t1.tv_sec) * 1000.0;      // sec to ms
        elapsedTime += (t2.tv_usec - t1.tv_usec) / 1000.0;   // us to ms
        duration_ns = elapsedTime*1000000.0;
        mu_elapsed_time += duration_ns;
        std_elapsed_time += (duration_ns*duration_ns);
    }
    mu_elapsed_time = mu_elapsed_time;
    std_elapsed_time = sqrt((std_elapsed_time) - (mu_elapsed_time*mu_elapsed_time));  
    std::cout << "Mu Elapsed time (us): " << mu_elapsed_time/1000.0 << std::endl;
    std::cout << "Std Elapsed time (us): " << std_elapsed_time/1000.0 << std::endl;
    fflush(stdout);
    while(1);
    return 0;
}
