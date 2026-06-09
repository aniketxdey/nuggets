# CS50 Nuggets Project (C++ port)
# Aniket Dey
#
# Makefile - player directory

CXX = g++
CXXFLAGS = -Wall -pedantic -std=c++17 -ggdb -I../support

LIBOUT = player.a
OBJS = gridcell.o grid.o player.o

.PHONY: all clean

all: $(LIBOUT)

$(LIBOUT): $(OBJS)
	ar cr $(LIBOUT) $(OBJS)

gridcell.o: gridcell.cpp gridcell.hpp
grid.o: grid.cpp grid.hpp gridcell.hpp
player.o: player.cpp player.hpp grid.hpp gridcell.hpp ../support/message.hpp

clean:
	rm -f *.o *~
	rm -f $(LIBOUT)
