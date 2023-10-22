CXX = g++
CXXFLAGS = -std=c++17
CXXFLAGS_THREAD = -pthread

all: sdfs

sdfs: membership.cpp sdfs.cpp
	$(CXX) $(CXXFLAGS) membership.cpp sdfs.cpp $(CXXFLAGS_THREAD) -o sdfs

clean:
	rm sdfs *.log

.PHONY: clean