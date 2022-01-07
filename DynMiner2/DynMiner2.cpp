// DynMiner2.cpp : This file contains the 'main' function. Program execution begins and ends there.
//



//-mode solo -server http://192.168.1.193:6433/ -user user -pass 123456 -wallet dy1qyc3lkpe8ysns5z65u3t5j0remfpdr49j6h60gg -miner GPU,65536,128,0,0
//-mode solo -server http://52.73.55.126:6433/ -user user -pass 123456 -wallet dy1qyc3lkpe8ysns5z65u3t5j0remfpdr49j6h60gg -miner GPU,65536,128,0,0
//-mode stratum -server web.letshash.it -port 5966 -user dy1qyc3lkpe8ysns5z65u3t5j0remfpdr49j6h60gg.test2 -pass d=2 -miner GPU,16384,128,0,1 -miner GPU,16384,128,0,0
//-mode stratum -server pond.nethervoid.net -port 4234 -user dy1qyc3lkpe8ysns5z65u3t5j0remfpdr49j6h60gg.test2 -pass d=2 -miner GPU,16384,128,0,1

//-mode solo -server http://testnet1.dynamocoin.org:6433/ -user user -pass 123456 -wallet dy1qyc3lkpe8ysns5z65u3t5j0remfpdr49j6h60gg -miner GPU,65536,128,0,0

//-mode stratum -server ayyo.chickenkiller.com -port 8332 -user dy1qyc3lkpe8ysns5z65u3t5j0remfpdr49j6h60gg.test2 -pass d=2 -miner GPU,16384,128,0,0



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
int stratumSocket;      //tcp connected socket for stratum mode

multimap<string, string> commandArgs;
cStatDisplay* statDisplay;
cGetWork* getWork;
cSubmitter* submitter;

struct sRpcConfigParams {
    string server;
    string port;
    string user;
    string pass;
    string wallet;
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
    printf("dynminer2 \n");
    printf("  -mode [solo|stratum]\n");
    printf("  -server <rpc server URL or stratum IP>\n");
    printf("  -port <rpc port>  [only used for stratum]\n");
    printf("  -user <username>\n");
    printf("  -pass <password>\n");
    printf("  -diff <initial difficulty>  [optional]\n");
    printf("  -wallet <wallet address>   [only used for solo]\n");
    printf("  -miner <miner params>\n");
    printf("\n");
    printf("<miner params> format:\n");
    printf("  [CPU|GPU],<cores or compute units>[<work size>,<platform id>,<device id>]\n");
    printf("  <work size>, <platform id> and <device id> are not required for CPU\n");
    printf("  multiple miner params are allowed\n");
    printf("\n");
    printf("Example:\n");
    printf("  dynminer2 -mode solo -server http://127.0.0.1/ -user user -pass 123123 -wallet dy1xxxxxxxxxxxxxxxxxxxxxxxx -miner GPU,1000,64,0,1\n\n");
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
    
    if (commandArgs.find("-mode") == commandArgs.end())
        showUsage("Missing argument: mode");
    else {
        multimap<string, string>::iterator it = commandArgs.find("-mode");
        string mode = it->second;
        transform(mode.begin(), mode.end(), mode.begin(), ::tolower);
        set<string> modeTypes = { "solo", "stratum" };
        if (modeTypes.find(mode) == modeTypes.end())
            showUsage("Invalid MODE argument");
        minerMode = mode;
    }

    if (commandArgs.find("-server") == commandArgs.end())
        showUsage("Missing argument: server");
    else
        rpcConfigParams.server = commandArgs.find("-server")->second;

    if (minerMode == "stratum") {
        if (commandArgs.find("-port") == commandArgs.end())
            showUsage("Missing argument: port");
        else
            rpcConfigParams.port = commandArgs.find("-port")->second;
    }

    if (commandArgs.find("-user") == commandArgs.end())
        showUsage("Missing argument: user");
    else
        rpcConfigParams.user = commandArgs.find("-user")->second;

    if (commandArgs.find("-pass") == commandArgs.end())
        showUsage("Missing argument: pass");
    else
        rpcConfigParams.pass = commandArgs.find("-pass")->second;


    if (minerMode != "stratum") {
        if (commandArgs.find("-wallet") == commandArgs.end())
            showUsage("Missing argument: wallet");
        else
            rpcConfigParams.wallet = commandArgs.find("-wallet")->second;
    }

    if (commandArgs.find("-miner") == commandArgs.end())
        showUsage("Missing argument: miner");



}


void startSubmitter() {
    thread submitThread(&cSubmitter::submitEvalThread, submitter, getWork, statDisplay, minerMode);
    submitThread.detach();
    submitter->stratumSocket = stratumSocket;
    submitter->rpcURL = rpcConfigParams.server;
    submitter->rpcUser = rpcConfigParams.user;
    submitter->rpcPassword = rpcConfigParams.pass;
    submitter->rpcWallet = rpcConfigParams.wallet;
}

void startStatDisplay() {
 
    thread statThread(&cStatDisplay::displayStats, statDisplay, submitter, minerMode);
    statThread.detach();
}

void startGetWork() {
    
    thread workThread(&cGetWork::getWork, getWork, minerMode, stratumSocket, statDisplay);
    workThread.detach();

    getWork->rpcURL = rpcConfigParams.server;
    getWork->rpcUser = rpcConfigParams.user;
    getWork->rpcPassword = rpcConfigParams.pass;
    getWork->rpcWallet = rpcConfigParams.wallet;

}


void startOneMiner(std::string params) {
    printf("Starting miner with params: %s\n", params.c_str());

    cMiner *miner = new cMiner();
    thread minerThread(&cMiner::startMiner, miner, params, getWork, submitter, statDisplay);
    minerThread.detach();
}


void startMiners() {
    pair<multimap<string, string>::iterator, multimap<string, string>::iterator> range = commandArgs.equal_range("-miner");
    multimap<string, string>::iterator it;
    for (it = range.first; it != range.second; ++it)
        startOneMiner(it->second);
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

void connectToStratum() {

    struct hostent* he = gethostbyname(rpcConfigParams.server.c_str());
    if (he == NULL) {
        printf("Cannot resolve host %s\n", rpcConfigParams.server.c_str());
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    struct sockaddr_in addr {};
    stratumSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (stratumSocket < 0) {
        printf("Cannot open socket.\n");
        exit(1);
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(atoi(rpcConfigParams.port.c_str()));
    memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);

    printf("Connecting to %s:%s\n", rpcConfigParams.server.c_str(), rpcConfigParams.port.c_str());
    int err = connect(stratumSocket, (struct sockaddr*)&addr, sizeof(addr));
    if (err != 0) {
        printf("Error connecting to %s:%s\n", rpcConfigParams.server.c_str(), rpcConfigParams.port.c_str());
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }


}

void authorizeStratum() {
    char buf[4096];
    sprintf(buf, "{\"params\": [\"%s\", \"%s\"], \"id\": \"auth\", \"method\": \"mining.authorize\"}", rpcConfigParams.user.c_str(), rpcConfigParams.pass.c_str());
    int numSent = send(stratumSocket, buf, strlen(buf), 0);
    if (numSent < 0)
        printf("Error on authentication\n");        //todo - I'm not sure this is 100% true
}

int main(int argc, char* argv[])
{


    printBanner();

    initOpenCL();

    curl_global_init(CURL_GLOBAL_ALL);

    parseCommandArgs(argc, argv);

    if (minerMode == "stratum") {
        initWinsock();
        connectToStratum();
        authorizeStratum();
    }

    statDisplay = new cStatDisplay();
    getWork = new cGetWork();
    submitter = new cSubmitter();


    startStatDisplay();
    startGetWork();
    startSubmitter();
    startMiners();

    while (true) {
        //todo - need to think about how to do this - reset getwork buffer, etc
        /*
        int error = 0;
        size_t len = sizeof(error);
        int retval = getsockopt(stratumSocket, SOL_SOCKET, SO_ERROR, (char*)&error, (char*)&len);
        if (retval != 0) {
            connectToStratum();
            authorizeStratum();
        }
        */
        std::this_thread::sleep_for(std::chrono::seconds(1));
        //Sleep(1000);
    }

}


