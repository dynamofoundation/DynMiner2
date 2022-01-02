#pragma once
#include <vector>
#include <string>
#include <CL/cl.h>
#include <CL/cl_platform.h>
#include "cGetWork.h"
#include "cSubmitter.h"
#include "cStatDisplay.h"

using namespace std;

class cMiner
{
public:
	void startMiner(string params, cGetWork *getWork, cSubmitter* submitter, cStatDisplay* statDisplay);
	void startGPUMiner(const size_t computeUnits, int platformID, int deviceID, cGetWork *getWork, cSubmitter* submitter, cStatDisplay *statDisplay);
	vector<string> split(string str, string token);
	cl_program loadMiner(cl_context context, cl_device_id* deviceID);

	cl_kernel kernel;
	cl_command_queue commandQueue;

};