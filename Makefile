CXX      = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2

irrigation_server: server.cpp Zone.h MinHeap.h Graph.h IrrigationScheduler.h Json.h HttpServer.h
	$(CXX) $(CXXFLAGS) server.cpp -o irrigation_server

run: irrigation_server
	./irrigation_server

clean:
	rm -f irrigation_server
