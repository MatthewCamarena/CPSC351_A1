CXX = g++ -std=c++11 -Wall

all: sender recv

sender: msg.h sender.cpp
	${CXX} sender.cpp -o sender

recv: msg.h recv.cpp
	${CXX} recv.cpp -o recv

clean:
	rm -f sender recv recvfile
