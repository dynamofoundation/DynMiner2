#pragma once
#include <vector>
#include <string>
#include <mutex>
#include <windows.h>
#include "difficulty.h"
#include "cGetWork.h"
#include "cStatDisplay.h"

using namespace std;


class cHashResult {
public:
	string jobID;
	unsigned char* buffer;
	unsigned int startNonce;
	int size;
};

class cNonceEntry {
public:
	string jobID;
	unsigned int nonce;
};

class cSubmitter
{
public:
	void submitEvalThread(cGetWork *getWork, cStatDisplay *iStatDisplay);
	void submitNonceThread(cGetWork* getWork);
	void submitNonce(unsigned int nonce, cGetWork* getWork);

	void addHashResults(unsigned char* hashBuffer, int buffSize, unsigned int startNonce, string jobID);
	unsigned int countLeadingZeros(unsigned char* hash);

	mutex hashListLock;
	vector<cHashResult*> hashList;

	mutex nonceListLock;
	vector<cNonceEntry*> nonceList;

	int stratumSocket;
	int rpcSequence;

	string user;

	cStatDisplay* statDisplay;

};

