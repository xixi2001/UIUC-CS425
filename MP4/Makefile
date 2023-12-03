CXX = g++
CXXFLAGS = -std=c++17
CXXFLAGS_THREAD = -pthread
CXXFLAGS_FILESYSTEM = -lstdc++fs

all: maple_juice

maple_juice: membership.cpp sdfs.cpp maple_juice.cpp
	$(CXX) $(CXXFLAGS) membership.cpp sdfs.cpp maple_juice.cpp $(CXXFLAGS_THREAD) $(CXXFLAGS_FILESYSTEM) -o mj

clean:
	rm -rf sdfs_files
	rm mj
	rm  *.log
	rm -rf local_input_maple
	rm -rf local_result_maple
	rm -rf local_input_juice
	rm -rf local_result_juice

.PHONY: clean