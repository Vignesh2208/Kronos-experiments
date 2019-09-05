#include <iostream>
#include <ctime>
#include <thread>
#include <chrono>
#include <string> 
#include <sys/time.h>
#include <fstream>
#include <nghttp2/asio_http2_client.h>

using boost::asio::ip::tcp;
using namespace std;
using namespace nghttp2::asio_http2;
using namespace nghttp2::asio_http2::client;


void trigger_nxt_requests(string dest_ip, int num_requests) {
  boost::system::error_code ec;
  boost::asio::io_service io_service;
  session sess(io_service, dest_ip, "80");

  sess.on_connect([&sess, dest_ip, num_requests](tcp::resolver::iterator endpoint_it) {
    boost::system::error_code ec;

    auto printer = [](const response &res) {
      res.on_data([](const uint8_t *data, std::size_t len) {
        //std::cerr.write(reinterpret_cast<const char *>(data), len);
        //std::cerr << std::endl;
      });
    };

    auto count = std::make_shared<int>(num_requests);

    for (std::size_t i = 0; i < num_requests; ++i) {
      auto req = sess.submit(ec, "GET",
                             "http://" + dest_ip + ":80/books/" + std::to_string(i + 1) + "_book_details.txt");

      req->on_response(printer);
      req->on_close([&sess, count](uint32_t error_code) {
        if (--*count == 0) {
          std::cout << "Finishing session after |num| requests are done !" << std::endl;
          sess.shutdown();
          std::cout << "Exiting !" << std::endl;
        }
      });
    }
  });

  sess.on_error([](const boost::system::error_code &ec) {
    std::cerr << "error: " << ec.message() << std::endl;
  });

  io_service.run();
}

void initiate_connection(string dest_ip) {

  boost::system::error_code ec;
  boost::asio::io_service io_service;

  std::cout << "Connecting to server ..." << std::endl;
  session sess(io_service, dest_ip, "80");

  sess.on_connect([&sess, dest_ip](tcp::resolver::iterator endpoint_it) {
    boost::system::error_code ec;
    auto req = sess.submit(ec, "GET", "http://" + dest_ip + "/pruned_books.json");

    req->on_response([](const response &res) {
      // print status code and response header fields.
      //std::cerr << "HTTP/2 " << res.status_code() << std::endl;
      //for (auto &kv : res.header()) {
      //  std::cerr << kv.first << ": " << kv.second.value << "\n";
      //}
      //std::cerr << std::endl;

      res.on_data([](const uint8_t *data, std::size_t len) {
        //std::cout << "Received Data of length: " << len << std::endl;
        //std::cout.write(reinterpret_cast<const char *>(data), len);
        //std::cout << std::endl;
        //std::cout << "Trigerring next set of requests " << std::endl;
      });
    });
    
    
    req->on_close([&sess](uint32_t error_code) {
        // shutdown session after first request was done.
        std::cout << "Finishing Main request !" << std::endl;
	sess.shutdown();
	std::cout << "Exiting ! " << std::endl;
    });
  });

  sess.on_error([](const boost::system::error_code &ec) {
    std::cerr << "error: " << ec.message() << std::endl;
  });
  

  io_service.run();

}

void initiate_trial(string dest_ip, int num_sub_requests) {
	initiate_connection(dest_ip);
        trigger_nxt_requests(dest_ip, num_sub_requests);
}

int main(int argc, char *argv[]) {


if (argc != 4)
{
	std::cout << "Usage: client <server_ip> <num_trials> <period>\n";
	std::cout << "Example:\n";
	std::cout << "  client 10.0.0.1\n";
	return 1;
}

string dest_ip = argv[1];
int num_sub_requests = 100;
int num_trials = std::stoi(argv[2]);
int period_ms = std::stoi(argv[3]);
std::vector<double> time_elapsed;
std::cout << "Waiting for 1 sec for server to start ..." << std::endl;
std::this_thread::sleep_for (std::chrono::seconds(1));

for(int i = 0; i < num_trials; i++) {

	struct timeval start, end; 
  
	// start timer. 
	gettimeofday(&start, NULL); 
  
	initiate_trial(dest_ip, num_sub_requests);
  
	// stop timer. 
	gettimeofday(&end, NULL); 
  
	// Calculating total time taken by the program. 
	double time_taken; 

	time_taken = (end.tv_sec - start.tv_sec) * 1e6; 
	time_taken = (time_taken + (end.tv_usec -  
		              start.tv_usec)) * 1e-6; 
        std::cout << "Time elapsed for request completion: " << time_taken << std::endl;
	time_elapsed.push_back(time_taken);

	std::this_thread::sleep_for (std::chrono::milliseconds(period_ms));
}

std::ofstream outFile("/tmp/container_log.txt");
// the important part
for (const auto &e : time_elapsed) outFile << e << "\n";

return 0;
}
