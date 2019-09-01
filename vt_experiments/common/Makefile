build:
	g++ client_http2.cc -o client_http2 -lnghttp2_asio -lboost_system -lssl -lcrypto
	g++ client_http11.cc -o client_http11 -lboost_system -lboost_thread -pthread
	g++ cmd_executor.cc -o cmd_executor -std=c++11
clean:
	rm client_http2
	rm client_http11
	rm cmd_executor
