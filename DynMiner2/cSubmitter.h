#pragma once
#include <vector>
#include <string>
#include <mutex>
#include "json.hpp"
#include "difficulty.h"
#include "struct.h"

#ifdef __linux__
#include "curl/curl.h"
#endif

#ifdef _WIN32
#include <windows.h>
#include <curl\curl.h>
#endif

class cGetWork;
class cStatDisplay;

using namespace std;
using json = nlohmann::json;



class cHashResult {
public:
	string jobID;
	unsigned char* buffer;
	uint32_t* nonceIndex;
	//unsigned int startNonce;
	int hashCount;
	int deviceID;
};

class cNonceEntry {
public:
	string jobID;
	unsigned int nonce;
};

class cSubmitter
{
public:
	void submitEvalThread(cGetWork *getWork, cStatDisplay *iStatDisplay, string mode);
	void submitNonceThread(cGetWork* getWork);
	void submitNonce(unsigned int nonce, cGetWork* getWork, int workID);
	json execRPC(string data);
	static size_t WriteMemoryCallback(void* contents, size_t size, size_t nmemb, void* userp);

	void addHashResults(unsigned char* hashBuffer, int hashCount, string jobID, int deviceID, uint32_t* nonceIndex);
	unsigned int countLeadingZeros(unsigned char* hash);

	mutex submitLock;

	mutex hashListLock;
	vector<cHashResult*> hashList;

	mutex nonceListLock;
	vector<cNonceEntry*> nonceList;

	int* stratumSocket;
	int rpcSequence;
	int* socketError;

	cStatDisplay* statDisplay;

	string minerMode;
	string statURL;
	string minerName;

	CURL* curl;
	CURLcode res;
	struct MemoryStruct chunk;
	string rpcURL;
	string rpcUser;
	string rpcPassword;
	string rpcWallet;

};

