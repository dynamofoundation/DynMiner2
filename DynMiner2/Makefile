CXX = g++
LIBS = -lpthread -L/opt/cuda/lib64 -lOpenCL -lcurl
CXXFLAGS = -I. -std=gnu++11
all: DynMiner2

DynMiner2:
	g++ $(CXXFLAGS) *.cpp $(LIBS) -o dyn_miner2
