#pragma once

#include <stdio.h>
#include <windows.h>
#include <string>
#include <vector>
#include "json.hpp"
#include <mutex>
#include <vector>
#include <string>
#include "hex.h"
#include "common.h"
#include "sha256.h"
#include "cProgramVM.h"
#include "cStatDisplay.h"


using namespace std;
using json = nlohmann::json;


class cGetWork
{
public:
	cGetWork();
	void getWork(string mode, int stratumSocket, cStatDisplay *statDisplay);
	void setJobDetails(json msg);

	atomic<uint32_t> difficultyTarget = 0;
	atomic<uint32_t> workID;
	mutex lockJob;

	string jobID;
	string prevBlockHashHex;
	unsigned char prevBlockHashBin[32];
	unsigned char merkleRoot[32];
	string coinBase1;
	string coinBase2;
	string timeHex;
	string nbits;
	string strProgram;
	unsigned char nativeData[80];
	vector<string> program;
	vector<uint32_t> byteCode;
	cProgramVM* programVM;

};

