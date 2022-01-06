#pragma once

#include <stdio.h>
#include <string>
#include <vector>
#include "json.hpp"
#include <mutex>
#include <vector>
#include <string>
#include <atomic>
#include <thread>
#include <iterator>
#include "hex.h"
#include "common.h"
#include "sha256.h"
#include "struct.h"

#ifdef __linux__
#include "curl/curl.h"
#endif

#ifdef _WIN32
#include <windows.h>
#include <curl\curl.h>
#endif

class cStatDisplay;
class cProgramVM;

using namespace std;
using json = nlohmann::json;

#define STRATUM_BUFFER_SIZE 1024 * 1024 * 16

typedef unsigned char tree_entry[32];

size_t address_to_script(unsigned char* out, size_t outsz, const char* addr);
void memrev(unsigned char* p, size_t len);
int varint_encode(unsigned char* p, uint64_t n);
static bool bech32_decode(char* hrp, uint8_t* data, size_t* data_len, const char* input);
static bool convert_bits(uint8_t* out, size_t* outlen, int outbits, const uint8_t* in, size_t inlen, int inbits, int pad);
static int b58check(unsigned char* bin, size_t binsz, const char* b58);

class cGetWork
{
public:
	cGetWork();
	void getWork(string mode, int stratumSocket, cStatDisplay *statDisplay);
	void setJobDetailsStratum(json msg);
	void setJobDetailsSolo(json msg);
	void startStratumGetWork(int stratumSocket, cStatDisplay* statDisplay);
	void startSoloGetWork(cStatDisplay* statDisplay);
	json execRPC(string data);
	static size_t WriteMemoryCallback(void* contents, size_t size, size_t nmemb, void* userp);

	cStatDisplay* stats;

	atomic<uint32_t> difficultyTarget{ 0 };
	atomic<uint32_t> workID;
	mutex lockJob;

	string jobID;
	uint32_t chainHeight;
	string prevBlockHashHex;
	unsigned char prevBlockHashBin[32];
	unsigned char merkleRoot[32];
	string coinBase1;
	string coinBase2;
	string timeHex;
	string nbits;
	string strProgram;
	uint32_t programStartTime;
	unsigned char nativeData[80];
	vector<string> program;
	vector<uint32_t> byteCode;
	cProgramVM* programVM;

	std::string strNativeTarget;
	uint32_t iNativeTarget[8];
	unsigned char nativeTarget[32];
	char strMerkleRoot[128];
	string strBlock;
	char* transactionString;

	CURL* curl;
	CURLcode res;
	struct MemoryStruct chunk;
	string rpcURL;
	string rpcUser;
	string rpcPassword;
	string rpcWallet;

	//used by all miners
	mutex nonceLock;
	uint32_t masterNonce;

	bool reqNewBlockFlag;		//for solo mining - to get new block on either block height change or submission of good block

};

