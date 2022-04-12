#pragma once
#include <vector>
#include <string>
#include <mutex>
#include <fstream>
#include <CL/cl.h>
#include <CL/cl_platform.h>

#include "version.h"

#include "sha256.h"

class cGetWork;
class cSubmitter;
class cStatDisplay;
class cProgramVM;

using namespace std;


#define HASHOP_ADD 0
#define HASHOP_XOR 1
#define HASHOP_SHA_SINGLE 2
#define HASHOP_SHA_LOOP 3
#define HASHOP_MEMGEN 4
#define HASHOP_MEMADD 5
#define HASHOP_MEMXOR 6
#define HASHOP_MEM_SELECT 7
#define HASHOP_END 8
#define HASHOP_READMEM2 9
#define HASHOP_LOOP 10
#define HASHOP_ENDLOOP 11
#define HASHOP_IF 12
#define HASHOP_STORETEMP 13
#define HASHOP_EXECOP 14
#define HASHOP_MEMADDHASHPREV 15
#define HASHOP_MEMXORHASHPREV 16
#define HASHOP_SUMBLOCK 17

class cMiner
{
public:
	void startMiner(string params, cGetWork *getWork, cSubmitter* submitter, cStatDisplay* statDisplay, uint32_t GPUIndex);
	void startGPUMiner(const size_t computeUnits, int platformID, int deviceID, cGetWork *getWork, cSubmitter* submitter, cStatDisplay *statDisplay, size_t gpuWorkSize, uint32_t GPUIndex, int gpuLoops);
	void startCPUMiner(cGetWork* getWork, cSubmitter* submitter, cStatDisplay* statDisplay, int cpuIndex, unsigned int startNonce);
	void runProgram(unsigned char* header, std::vector<unsigned int> program, unsigned int* hash, CSHA256 _sha256);
	vector<string> split(string str, string token);
	cl_program loadMiner(cl_context context, cl_device_id* deviceID, int gpuLoops);

	void startTestCPUMiner(unsigned char* hashBlock);
	void startTestGPUMiner(unsigned char* hashBlock);
	void testOneGPU( int platformID, int deviceID, unsigned char* hashBlock);


	cl_kernel kernel;
	cl_command_queue commandQueue;

	bool pause;





};
