//
// sync_client.cpp
// ~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2012 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <iostream>
#include <istream>
#include <ostream>
#include <string>
#include <ctime>
#include <thread>
#include <chrono> 
#include <fstream>
#include <sys/time.h>
#include <boost/asio.hpp>

using boost::asio::ip::tcp;

void send_request(std::string server_ip, std::string path) {
    boost::asio::io_service io_service;

    // Get a list of endpoints corresponding to the server name.
    tcp::resolver resolver(io_service);
    tcp::resolver::query query(server_ip, "http");
    tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);

    // Try each endpoint until we successfully establish a connection.
    tcp::socket socket(io_service);
    boost::asio::connect(socket, endpoint_iterator);

    // Form the request. We specify the "Connection: close" header so that the
    // server will close the socket after transmitting the response. This will
    // allow us to treat all data up until the EOF as the content.
    boost::asio::streambuf request;
    std::ostream request_stream(&request);
    request_stream << "GET " << path << " HTTP/1.1\r\n";
    request_stream << "Host: " << server_ip << "\r\n";
    request_stream << "Accept: */*\r\n";
    request_stream << "Connection: close\r\n\r\n";

    // Send the request.
    boost::asio::write(socket, request);

    // Read the response status line. The response streambuf will automatically
    // grow to accommodate the entire line. The growth may be limited by passing
    // a maximum size to the streambuf constructor.
    boost::asio::streambuf response;
    boost::asio::read_until(socket, response, "\r\n");

    // Check that response is OK.
    std::istream response_stream(&response);
    std::string http_version;
    response_stream >> http_version;
    unsigned int status_code;
    response_stream >> status_code;
    std::string status_message;
    std::getline(response_stream, status_message);
    if (!response_stream || http_version.substr(0, 5) != "HTTP/")
    {
      std::cout << "Invalid response\n";
      return;
    }
    if (status_code != 200)
    {
      std::cout << "Response returned with status code " << status_code << "\n";
      return;
    }

    // Read the response headers, which are terminated by a blank line.
    boost::asio::read_until(socket, response, "\r\n\r\n");

    // Process the response headers.
    std::string header;
    //while (std::getline(response_stream, header) && header != "\r")
    //  std::cout << header << "\n";
    //std::cout << "\n";

    // Write whatever content we already have to output.
    //if (response.size() > 0)
    //  std::cout << &response;

    // Read until EOF, writing data to output as we go.
    boost::system::error_code error;
    while (boost::asio::read(socket, response,
          boost::asio::transfer_at_least(1), error));
      //std::cout << &response;
    if (error != boost::asio::error::eof)
      throw boost::system::system_error(error);
}

void send_sub_requests(std::string server_ip, int start_book_no, int num_sub_requests) {
	for(int i = start_book_no; i < num_sub_requests + start_book_no; i++) {
		std::string path = "/books/" + std::to_string(i+1) + "_book_details.txt";
		send_request(server_ip, path);
	}

}

int main(int argc, char* argv[])
{
  
	if (argc != 5)
	{
		std::cout << "Usage: sync_client <server> <num_trials> <period> <num_threads>\n";
		std::cout << "Example:\n";
		std::cout << "  client_http11 10.0.0.1 10 100 10\n";
		return 1;
	}

	std::string server_ip = argv[1];
	std::string path;
	int num_sub_requests = 100;
	int num_trials = std::stoi(argv[2]);
	int period_ms = std::stoi(argv[3]);
        int num_threads = std::stoi(argv[4]);
	int per_worker_num_requests = num_sub_requests/num_threads;
	std::vector<double> time_elapsed;
	std::cout << "Waiting for 1 sec for server to start ..." << std::endl;
	std::this_thread::sleep_for (std::chrono::seconds(1));

	for (int j = 0; j < num_trials; j++) {
		struct timeval start, end; 
	  	std::vector<std::thread> workers;
		// start timer. 
		gettimeofday(&start, NULL); 
		for (int i = 0; i < num_threads; i++) {
			std::thread worker(send_sub_requests, server_ip,  i*per_worker_num_requests, per_worker_num_requests);
			workers.push_back(std::move(worker));
	  	}
		for (auto& worker: workers) {
			worker.join();
		}
		// stop timer. 
		gettimeofday(&end, NULL); 
	  
		// Calculating total time taken by the program. 
		double time_taken; 

		time_taken = (end.tv_sec - start.tv_sec) * 1e6; 
		time_taken = (time_taken + (end.tv_usec -  
				      start.tv_usec)) * 1e-6; 
		std::cout << "Time elapsed for request completion: " << time_taken << std::endl;
		time_elapsed.push_back(time_taken);

		std::this_thread::sleep_for(std::chrono::milliseconds(period_ms));
	}
    
  std::ofstream outFile("/tmp/container_log.txt");
// the important part
for (const auto &e : time_elapsed) outFile << e << "\n";
  return 0;
}
