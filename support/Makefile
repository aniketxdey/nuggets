# Makefile - support directory (C++ port)
# Aniket Dey

CXX = g++
CXXFLAGS = -Wall -pedantic -std=c++17 -ggdb

LIB = support.a
OBJS = message.o log.o

.PHONY: all clean

all: $(LIB)

$(LIB): $(OBJS)
	ar cr $(LIB) $(OBJS)

message.o: message.cpp message.hpp log.hpp
log.o: log.cpp log.hpp

clean:
	rm -f *.o *.a *~
