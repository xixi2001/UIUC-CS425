CXX = g++
CXXFLAGS = -std=c++17
CXXFLAGS_THREAD = -pthread
CXXFLAGS_FILESYSTEM = -lstdc++fs

all: maple_juice

maple_juice: membership.cpp sdfs.cpp maple_juice.cpp
	$(CXX) $(CXXFLAGS) membership.cpp sdfs.cpp maple_juice.cpp $(CXXFLAGS_THREAD) $(CXXFLAGS_FILESYSTEM) -o mj

clean:
	rm -rf sdfs_files
	rm maple_juice
	rm  *.log

.PHONY: clean