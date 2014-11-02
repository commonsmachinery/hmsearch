
CC = gcc
CXX = g++
CFLAGS = -g -Wall -D_FILE_OFFSET_BITS=64
CXXFLAGS = $(CFLAGS)
LDFLAGS = -g
LIBS = -lm -lleveldb

bin-objs = hm_initdb.o hm_dump.o hm_insert.o hm_lookup.o
common-objs = hmsearch.o

all: $(bin-objs:%.o=%)

%: %.o $(common-objs)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LIBS)

clean:
	rm -f $(bin-targets) *.o

$(bin-objs) $(common-objs): hmsearch.h
