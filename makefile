all: init client server

CXX=gcc
BIN=bin
CLN=$(BIN)/client
SRV=$(BIN)/server
PORT=51511
IP=::1
VERSION=v6

init:
	@ test -d $(BIN) && true || mkdir $(BIN)

client: client.c common.h common.c
	$(CXX) client.c common.c -o $(CLN) -Wall -g

server: server.c common.h common.c
	$(CXX) server.c common.c -o $(SRV) -Wall -g

clean:
	test ! -d $(BIN) && true || rm -rf $(BIN) 

run_server: init server
	./$(SRV) $(VERSION) $(PORT)

run_client: init client
	./$(CLN) $(IP) $(PORT)

.PHONY: all init client server clean run_server run_client
