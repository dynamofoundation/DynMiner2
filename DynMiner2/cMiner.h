#pragma once
#include <vector>
#include <string>
#include <mutex>
#include <CL/cl.h>
#include <CL/cl_platform.h>

#include "version.h"

class cGetWork;
class cSubmitter;
class cStatDisplay;
class cProgramVM;

using namespace std;

class cMiner
{
public:
	void startMiner(string params, cGetWork *getWork, cSubmitter* submitter, cStatDisplay* statDisplay, uint32_t GPUIndex);
	void startGPUMiner(const size_t computeUnits, int platformID, int deviceID, cGetWork *getWork, cSubmitter* submitter, cStatDisplay *statDisplay, size_t gpuWorkSize, uint32_t GPUIndex);
	vector<string> split(string str, string token);
	cl_program loadMiner(cl_context context, cl_device_id* deviceID);

	cl_kernel kernel;
	cl_command_queue commandQueue;

};
