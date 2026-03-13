all :client3
client1:client1.cpp
	g++ -std=c++11 $^ -o $@
client2:client2.cpp
	g++ -std=c++11 $^ -o $@
client3:client3.cpp
	g++ -std=c++11 $^ -o $@

server:server.cc
	g++ -g -std=c++11 $^ -o $@
client:tcp_cli.cc
	g++ -std=c++11 $^ -o $@
tcp_srv:tcp_srv.cc
	g++ -g -std=c++11 $^ -o $@