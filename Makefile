# Makefile for 'Nuggets' (C++ port)
#
# Aniket Dey, May 2023

S = support
P = player
LLIBS = $P/player.a $S/support.a

CXX = g++
CXXFLAGS = -Wall -pedantic -std=c++17 -ggdb -I$S -I$P
# for memory-leak tests
VALGRIND = valgrind --leak-check=full --show-leak-kinds=all

.PHONY: all clean

all:
	make -C support
	make -C player
	make server
	make client
	make latencytest
	make rendertest

# server executable
server: server.o $(LLIBS)
	$(CXX) $(CXXFLAGS) $^ -o $@

# client executable (needs ncurses)
client: client.o $(LLIBS)
	$(CXX) $(CXXFLAGS) $^ -lncurses -o $@

# latency benchmark (standalone, proves sub-1ms message latency)
latencytest: latencytest.o
	$(CXX) $(CXXFLAGS) $^ -o $@

# render benchmark (proves <2ms visibility + DISPLAY rendering)
rendertest: rendertest.o $(LLIBS)
	$(CXX) $(CXXFLAGS) $^ -o $@

# object files depend on include files
server.o: server.cpp $S/message.hpp $P/player.hpp $P/grid.hpp $P/gridcell.hpp
client.o: client.cpp $S/message.hpp
latencytest.o: latencytest.cpp
rendertest.o: rendertest.cpp $P/player.hpp $P/grid.hpp $P/gridcell.hpp $S/message.hpp

valgrind:
	$(VALGRIND) ./server maps/visdemo.txt 5

clean:
	rm -f core
	rm -rf *~ *.o *.dSYM
	rm -f client
	rm -f server
	rm -f latencytest
	rm -f rendertest
	make -C support clean
	make -C player clean
