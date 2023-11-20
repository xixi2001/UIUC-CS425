CXX = g++
CXXFLAGS = -std=c++17
CXXFLAGS_THREAD = -pthread
CXXFLAGS_FILESYSTEM = -lstdc++fs

all: maple_juice

maple_juice: membership.cpp sdfs.cpp maple_juice.cpp
	$(CXX) $(CXXFLAGS) membership.cpp sdfs.cpp maple_juice.cpp $(CXXFLAGS_THREAD) $(CXXFLAGS_FILESYSTEM) -o maple_juice

clean:
	rm maple_juice *.log
	rm sdfs_files/*

.PHONY: clean