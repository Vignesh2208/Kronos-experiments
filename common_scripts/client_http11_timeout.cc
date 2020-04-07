//
// blocking_tcp_client.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2018 Christopher M. Kohlhoff (chris at kohlhoff dot com)
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

#include <boost/asio/buffer.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/system/system_error.hpp>
#include <boost/asio/write.hpp>
#include <cstdlib>
#include <iostream>
#include <string>
#include <boost/lambda/bind.hpp>
#include <boost/lambda/lambda.hpp>

using boost::asio::ip::tcp;
using boost::lambda::bind;
using boost::lambda::var;
using boost::lambda::_1;
using boost::lambda::_2;

int TIMEOUT = 2;

//----------------------------------------------------------------------

//
// This class manages socket timeouts by running the io_context using the timed
// io_context::run_for() member function. Each asynchronous operation is given
// a timeout within which it must complete. The socket operations themselves
// use boost::lambda function objects as completion handlers. For a given
// socket operation, the client object runs the io_context to block thread
// execution until the operation completes or the timeout is reached. If the
// io_context::run_for() function times out, the socket is closed and the
// outstanding asynchronous operation is cancelled.
//
class client
{
public:
  client()
    : socket_(io_context_)
  {
  }

  void connect(const std::string& host, const std::string& service,
      boost::asio::chrono::steady_clock::duration timeout)
  {
    // Resolve the host name and service to a list of endpoints.
    tcp::resolver::results_type endpoints =
      tcp::resolver(io_context_).resolve(host, service);

    // Start the asynchronous operation itself. The boost::lambda function
    // object is used as a callback and will update the ec variable when the
    // operation completes. The blocking_udp_client.cpp example shows how you
    // can use boost::bind rather than boost::lambda.
    boost::system::error_code ec;
    boost::asio::async_connect(socket_, endpoints, var(ec) = _1);

    // Run the operation until it completes, or until the timeout.
    run(timeout);

    // Determine whether a connection was successfully established.
    if (ec)
      throw boost::system::system_error(ec);
  }

  std::string read_until(boost::asio::chrono::steady_clock::duration timeout, std::string delimiter)
  {
    // Start the asynchronous operation. The boost::lambda function object is
    // used as a callback and will update the ec variable when the operation
    // completes. The blocking_udp_client.cpp example shows how you can use
    // boost::bind rather than boost::lambda.
    boost::system::error_code ec;
    std::size_t n = 0;
    boost::asio::async_read_until(socket_,
        boost::asio::dynamic_buffer(input_buffer_),
        delimiter, (var(ec) = _1, var(n) = _2));

    // Run the operation until it completes, or until the timeout.
    run(timeout);

    // Determine whether the read completed successfully.
    if (ec && ec != boost::asio::error::eof)
      throw boost::system::system_error(ec);

    std::string line(input_buffer_.substr(0, n - 1));
    input_buffer_.erase(0, n);
    return line;
  }

  void write_line(const std::string& line,
      boost::asio::chrono::steady_clock::duration timeout)
  {
    std::string data = line + "\n";

    // Start the asynchronous operation. The boost::lambda function object is
    // used as a callback and will update the ec variable when the operation
    // completes. The blocking_udp_client.cpp example shows how you can use
    // boost::bind rather than boost::lambda.
    boost::system::error_code ec;
    boost::asio::async_write(socket_, boost::asio::buffer(data), var(ec) = _1);

    // Run the operation until it completes, or until the timeout.
    run(timeout);

    // Determine whether the read completed successfully.
    if (ec)
      throw boost::system::system_error(ec);
  }

private:
  void run(boost::asio::chrono::steady_clock::duration timeout)
  {
    // Restart the io_context, as it may have been left in the "stopped" state
    // by a previous operation.
    io_context_.restart();

    // Block until the asynchronous operation has completed, or timed out. If
    // the pending asynchronous operation is a composed operation, the deadline
    // applies to the entire operation, rather than individual operations on
    // the socket.
    io_context_.run_for(timeout);

    // If the asynchronous operation completed successfully then the io_context
    // would have been stopped due to running out of work. If it was not
    // stopped, then the io_context::run_for call must have timed out.
    if (!io_context_.stopped())
    {
      // Close the socket to cancel the outstanding asynchronous operation.
      socket_.close();

      // Run the io_context again until the operation completes.
      io_context_.run();
    }
  }

  boost::asio::io_context io_context_;
  tcp::socket socket_;
  std::string input_buffer_;
};

void send_request(std::string server_ip, std::string path) {
    boost::asio::io_service io_service;

    try {
	    // Get a list of endpoints corresponding to the server name.
    client c;
    c.connect(server_ip, "http", boost::asio::chrono::seconds(TIMEOUT));

    
    boost::asio::streambuf request;
    std::ostream request_stream(&request);
    request_stream << "GET " << path << " HTTP/1.1\r\n";
    request_stream << "Host: " << server_ip << "\r\n";
    request_stream << "Accept: */*\r\n";
    request_stream << "Connection: close\r\n\r\n";

    // Send the request.
    std::string request_str((std::istreambuf_iterator<char>(&request)), std::istreambuf_iterator<char>());
    c.write_line(request_str, boost::asio::chrono::seconds(TIMEOUT));

    // Read the response status line. The response streambuf will automatically
    // grow to accommodate the entire line. The growth may be limited by passing
    // a maximum size to the streambuf constructor.


    boost::asio::streambuf response;
    std::iostream os(&response);
    std::string response_message = c.read_until(boost::asio::chrono::seconds(TIMEOUT), "\r\n");
    os << response_message;

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
    
    response_message = c.read_until(boost::asio::chrono::seconds(TIMEOUT), "\r\n\r\n");
    os << response_message;

    // Process the response headers.
    //std::string header;
    //while (std::getline(response_stream, header) && header != "\r")
    //  std::cout << header << "\n";
    //std::cout << "\n";

    // Write whatever content we already have to output.
    //if (response.size() > 0)
    //   std::cout << &response;

    // Read until EOF, writing data to output as we go.
    response_message = c.read_until(boost::asio::chrono::seconds(TIMEOUT), "\n");
    //std::cout <<  "Response = " << response_message << std::endl;
    }  catch (std::exception &e) {
	    std::cout << "Boost connect error: " << e.what() << std::endl;
    }
  
}

void send_sub_requests(std::string server_ip, int start_book_no, int num_sub_requests) {
        std::vector<std::string> paths;
	for(int i = start_book_no; i < num_sub_requests + start_book_no; i++) {
		std::string path = "/books/" + std::to_string(i+1) + "_book_details.txt";
                paths.push_back(path);
		send_request(server_ip, path);
	}
		

	
}


//----------------------------------------------------------------------

int main(int argc, char* argv[])
{
  if (argc != 5)
	{
		std::cout << "Usage: sync_client <server> <num_trials> <period> <num_threads>\n";
		std::cout << "Example:\n";
		std::cout << "  client_http11_timeout 10.0.0.1 10 100 10\n";
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
