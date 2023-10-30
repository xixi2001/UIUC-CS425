CXX = g++
CXXFLAGS = -std=c++17
CXXFLAGS_THREAD = -pthread
CXXFLAGS_FILESYSTEM = -lstdc++fs

all: sdfs

sdfs: membership.cpp sdfs.cpp
	$(CXX) $(CXXFLAGS) membership.cpp sdfs.cpp $(CXXFLAGS_THREAD) $(CXXFLAGS_FILESYSTEM) -o sdfs

clean:
	rm sdfs *.log
	rm sdfs_files/*

.PHONY: clean