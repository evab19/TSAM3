linux: 
	g++ -Wall -std=c++11 server.cpp -o tsamgroup82
	g++ -pthread -Wall -std=c++11 client.cpp -o client

osx:
	g++ -std=c++11 server.cpp -o server
	g++ -pthread -std=c++11 client.cpp -o client
