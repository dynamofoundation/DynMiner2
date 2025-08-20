// DynMiner2.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include "cStatDisplay.h"
#include "cGetWork.h"
#include "cMiner.h"
#include "cSubmitter.h"
#include <CL/cl.h>
#include <CL/cl_platform.h>

#ifdef __linux__
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h> 
#include "json.hpp"
#include "curl/curl.h"
#endif

#ifdef _WIN32
#include "json.hpp"
#include <curl\curl.h>
#endif
#include <set>
#include <thread>


#include "version.h"


using namespace std;

string minerMode;       //solo or stratum
string statURL;
string minerName;
int stratumSocket;      //tcp connected socket for stratum mode
int socketError;        //global var to detect socket errors


multimap<string, string> commandArgs;
cStatDisplay* statDisplay;
cGetWork* getWork;
cSubmitter* submitter;

//unsigned char* hashBlock;

vector<cMiner*> miners;

struct sRpcConfigParams {
    string server;
    string port;
    string user;
    string pass;
    string wallet;
    int hiveos;
} rpcConfigParams;



void printBanner() {

    printf("Dynamo miner %s\n\n", MINER_VERSION);

}

void initOpenCL() {

    cl_int returnVal;
    cl_device_id* device_id = (cl_device_id*)malloc(16 * sizeof(cl_device_id));
    cl_uint ret_num_platforms;

    cl_ulong globalMem;
    cl_uint computeUnits;
    size_t sizeRet;
    uint32_t numOpenCLDevices;

    cl_platform_id*  platform_id = (cl_platform_id*)malloc(16 * sizeof(cl_platform_id));
    returnVal = clGetPlatformIDs(16, platform_id, &ret_num_platforms);

    if (ret_num_platforms > 0) {
        printf("OpenCL GPUs detected:\n");
    }
    else {
        printf("No OpenCL platforms detected.\n");
    }

    for (uint32_t i = 0; i < ret_num_platforms; i++) {
        returnVal = clGetDeviceIDs(platform_id[i], CL_DEVICE_TYPE_GPU, 16, device_id, &numOpenCLDevices);
        for (uint32_t j = 0; j < numOpenCLDevices; j++) {
            returnVal = clGetDeviceInfo(
                device_id[j], CL_DEVICE_GLOBAL_MEM_SIZE, sizeof(globalMem), &globalMem, &sizeRet);
            returnVal = clGetDeviceInfo(
                device_id[j], CL_DEVICE_MAX_COMPUTE_UNITS, sizeof(computeUnits), &computeUnits, &sizeRet);
            printf("platform %d, device %d [memory %lu, compute units %d]\n", i, j, globalMem, computeUnits);
        }
    }

    printf("\n");

}


void showUsage(const char* message) {
    printf("Error parsing command line arguments\n%s\n\n", message);

    printf("USAGE\n");
    printf("dynminer \n");
    printf("  -mode [solo|stratum|pool|testcpu|testgpu]\n");
    printf("  -server <rpc server URL or stratum/pool IP>\n");
    printf("  -port <rpc port>  [only used for stratum and pool]\n");
    printf("  -user <username>\n");
    printf("  -pass <password>  [used only for solo and stratum]\n");
    printf("  -wallet <wallet address>   [only used for solo]\n");
    printf("  -miner <miner params>\n");
    printf("  -hiveos [0|1]   [optional, if 1 will format output for hiveos]\n");
    printf("  -statrpcurl <URL to send stats to> [optional]\n");
    printf("  -minername <display name of miner> [required with statrpcurl]\n");
    printf("\n");
    printf("<miner params> format:\n");
    printf("  [CPU|GPU],<cores or compute units>[<work size>,<platform id>,<device id>[,<loops>]]\n");
    printf("  <work size>, <platform id> and <device id> are not required for CPU\n");
    printf("  <loops> is an optional GPU tuning param - default is 1, optimal range can be 2 to 10 for high end cards");
    printf("  multiple miner params are allowed\n");
    printf("\n");
    printf("Example:\n");
    printf("  dynminer -mode solo -server http://127.0.0.1:6433/ -user user -pass 123123 -wallet dy1xxxxxxxxxxxxxxxxxxxxxxxx -miner GPU,1000,64,0,1\n\n");
    exit(0);
}

void parseCommandArgs(int argc, char* argv[]) {

    if ((argc - 1) % 2 != 0)
        showUsage("Wrong number of arguments");

    if (argc == 1)
        showUsage("Missing arguments");

    for (int i = 1; i < argc; i += 2) {
        for (int j = 0; j < strlen(argv[i]); j++)
            argv[i][j] = tolower(argv[i][j]);
        commandArgs.emplace(argv[i], argv[i + 1]);
    }

    rpcConfigParams.hiveos = 0;

    if (commandArgs.find("-mode") == commandArgs.end())
        showUsage("Missing argument: mode");
    else {
        multimap<string, string>::iterator it = commandArgs.find("-mode");
        string mode = it->second;
        transform(mode.begin(), mode.end(), mode.begin(), ::tolower);
        set<string> modeTypes = { "solo", "stratum", "pool", "testcpu", "testgpu"};
        if (modeTypes.find(mode) == modeTypes.end())
            showUsage("Invalid MODE argument");
        minerMode = mode;
    }

    if ((minerMode == "testcpu") || (minerMode == "testgpu"))
        return;

    if (commandArgs.find("-statrpcurl") != commandArgs.end()) {
        multimap<string, string>::iterator it = commandArgs.find("-statrpcurl");
        statURL = it->second;
        if (commandArgs.find("-minername") == commandArgs.end())
            showUsage("-minername is required when using -statrpcurl");

        if (commandArgs.find("-minername") != commandArgs.end()) {
            multimap<string, string>::iterator it = commandArgs.find("-minername");
            minerName = it->second;
        }

        printf("Sending stats to URL: %s using miner name %s\n", statURL.c_str(), minerName.c_str());
    }


    if (commandArgs.find("-server") == commandArgs.end())
        showUsage("Missing argument: server");
    else
        rpcConfigParams.server = commandArgs.find("-server")->second;

    if ((minerMode == "stratum") || (minerMode == "pool")) {
        if (commandArgs.find("-port") == commandArgs.end())
            showUsage("Missing argument: port");
        else
            rpcConfigParams.port = commandArgs.find("-port")->second;
    }

    if (commandArgs.find("-user") == commandArgs.end())
        showUsage("Missing argument: user");
    else
        rpcConfigParams.user = commandArgs.find("-user")->second;

    if (minerMode != "pool") {
        if (commandArgs.find("-pass") == commandArgs.end())
            showUsage("Missing argument: pass");
        else
            rpcConfigParams.pass = commandArgs.find("-pass")->second;
    }


    if ((minerMode != "stratum") && (minerMode != "pool")) {
        if (commandArgs.find("-wallet") == commandArgs.end())
            showUsage("Missing argument: wallet");
        else
            rpcConfigParams.wallet = commandArgs.find("-wallet")->second;
    }

    if (commandArgs.find("-miner") == commandArgs.end())
        showUsage("Missing argument: miner");


    if (commandArgs.find("-hiveos") != commandArgs.end()) {
        string num = commandArgs.find("-hiveos")->second;
        rpcConfigParams.hiveos = atoi(num.c_str());
    }


}


void startSubmitter() {
    submitter->stratumSocket = &stratumSocket;
    submitter->rpcURL = rpcConfigParams.server;
    submitter->rpcUser = rpcConfigParams.user;
    submitter->rpcPassword = rpcConfigParams.pass;
    submitter->rpcWallet = rpcConfigParams.wallet;

    socketError = false;
    submitter->socketError = &socketError;

    thread submitThread(&cSubmitter::submitEvalThread, submitter, getWork, statDisplay, minerMode);
    submitThread.detach();
}

void startStatDisplay() {
 
    thread statThread(&cStatDisplay::displayStats, statDisplay, submitter, minerMode, rpcConfigParams.hiveos, statURL, minerName);
    statThread.detach();
}

void startGetWork() {

    getWork->rpcURL = rpcConfigParams.server;
    getWork->rpcUser = rpcConfigParams.user;
    getWork->rpcPassword = rpcConfigParams.pass;
    getWork->rpcWallet = rpcConfigParams.wallet;

    socketError = false;
    getWork->socketError = &socketError;

    thread workThread(&cGetWork::getWork, getWork, minerMode, stratumSocket, statDisplay);
    workThread.detach();


}


void startTestGPUMiner() {
    /*
    cMiner *miner = new cMiner();
    thread minerThread(&cMiner::startTestGPUMiner, miner);
    minerThread.detach();
    */
}

void startTestCPUMiner() {
    /*
    cMiner *miner = new cMiner();
    thread minerThread(&cMiner::startTestCPUMiner, miner);
    minerThread.detach();
    */
}


void startOneMiner(std::string params, uint32_t GPUIndex) {
    printf("Starting miner with params: %s\n", params.c_str());

    cMiner *miner = new cMiner();
    thread minerThread(&cMiner::startMiner, miner, params, getWork, submitter, statDisplay, GPUIndex);
    minerThread.detach();

    miners.push_back(miner);
}


void startMiners() {
    miners.clear();

	uint32_t GPUIndex = 0;
    pair<multimap<string, string>::iterator, multimap<string, string>::iterator> range = commandArgs.equal_range("-miner");
    multimap<string, string>::iterator it;
    for (it = range.first; it != range.second; ++it, ++GPUIndex)
        startOneMiner(it->second, GPUIndex);
}


void initWinsock() {
#ifdef _WIN32
    WSADATA wsa;
    int res = WSAStartup(MAKEWORD(2, 2), &wsa);
    if (res != NO_ERROR) {
        printf("WSAStartup failed with error: %d\n", res);
        exit(0);
    }
#endif


}

bool connectToStratum() {

    struct hostent* he = gethostbyname(rpcConfigParams.server.c_str());
    if (he == NULL) {
        printf("Cannot resolve host %s\n", rpcConfigParams.server.c_str());
        std::this_thread::sleep_for(std::chrono::seconds(1));
        return false;
    }

    struct sockaddr_in addr {};
    stratumSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (stratumSocket < 0) {
        printf("Cannot open socket.\n");
        return false;
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(atoi(rpcConfigParams.port.c_str()));
    memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);

    printf("Connecting to %s:%s\n", rpcConfigParams.server.c_str(), rpcConfigParams.port.c_str());
    int err = connect(stratumSocket, (struct sockaddr*)&addr, sizeof(addr));
    if (err != 0) {
        printf("Error connecting to %s:%s\n", rpcConfigParams.server.c_str(), rpcConfigParams.port.c_str());
        std::this_thread::sleep_for(std::chrono::seconds(1));
        return false;
    }

    socketError = false;

    return true;
}

void authorizeStratum() {
    char buf[4096];
    sprintf(buf, "{\"params\": [\"%s\", \"%s\"], \"id\": \"auth\", \"method\": \"mining.authorize\"}", rpcConfigParams.user.c_str(), rpcConfigParams.pass.c_str());
    int numSent = send(stratumSocket, buf, strlen(buf), 0);
    if (numSent < 0) {
        printf("Error on authentication\n");        //todo - I'm not sure this is 100% true
    }
}




void authorizePool() {
    char buf[4096];
    sprintf(buf, "{\"command\": \"auth\", \"data\": \"%s\"}\n", rpcConfigParams.user.c_str());
    int numSent = send(stratumSocket, buf, strlen(buf), 0);
    if (numSent < 0) {
        printf("Error on authentication\n");        //todo - I'm not sure this is 100% true
    }
}

int main(int argc, char* argv[])
{


    printBanner();

    initOpenCL();

    curl_global_init(CURL_GLOBAL_ALL);

    parseCommandArgs(argc, argv);

    if (minerMode == "stratum") {
        initWinsock();
        if (!connectToStratum())
            exit(0);
        authorizeStratum();
    }

    if (minerMode == "pool") {
        initWinsock();
        if (!connectToStratum())    //same logic as stratum to connect to pool
            exit(0);
        authorizePool();
    }

    /*
    const uint64_t hashBlockSize = 1024ULL * 1024ULL * 3072ULL;
    hashBlock = (unsigned char*)malloc(hashBlockSize);
    if (hashBlock == NULL) {
        printf("Unable to allocate 3GB hash block, aborting.\n");
        exit(0);
    }

    memset(hashBlock, 0, hashBlockSize);
    */

    if (minerMode == "testgpu") {
        startTestGPUMiner();
        while (true)
            std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    else if (minerMode == "testcpu") {
        startTestCPUMiner();
        while (true)
            std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    else {
        statDisplay = new cStatDisplay();
        getWork = new cGetWork();
        submitter = new cSubmitter();

        startStatDisplay();
        startGetWork();
        startSubmitter();
        startMiners();

        while (true) {
            if ((minerMode == "stratum") || (minerMode == "pool")) {
                if (socketError) {
                    printf("Socket error\nInitiating restart\n\n");

                    printf("Pausing miners...\n");

                    for (int i = 0; i < miners.size(); i++)
                        miners[i]->pause = true;
                    std::this_thread::sleep_for(std::chrono::seconds(3));

                    printf("Reconnecting to stratum/pool...\n");
                    while (!connectToStratum())
                        std::this_thread::sleep_for(std::chrono::seconds(1));

                    if (minerMode == "stratum") 
                        authorizeStratum();
                    else 
                        authorizePool();

                    startGetWork();
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                    printf("Starting miners...\n");
                    for (int i = 0; i < miners.size(); i++)
                        miners[i]->pause = false;
                    std::this_thread::sleep_for(std::chrono::seconds(3));
                    printf("Restart complete\n\n");
                }
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

}


