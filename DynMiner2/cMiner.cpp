#include "cMiner.h"
#include "cGetWork.h"
#include "cSubmitter.h"
#include "cStatDisplay.h"
#include "cProgramVM.h"

#include <iomanip>
#include <locale>

uint64_t BSWAP64(uint64_t x)
{
	return  ( (x << 56) & 0xff00000000000000UL ) |
		( (x << 40) & 0x00ff000000000000UL ) |
		( (x << 24) & 0x0000ff0000000000UL ) |
		( (x <<  8) & 0x000000ff00000000UL ) |
		( (x >>  8) & 0x00000000ff000000UL ) |
		( (x >> 24) & 0x0000000000ff0000UL ) |
		( (x >> 40) & 0x000000000000ff00UL ) |
		( (x >> 56) & 0x00000000000000ffUL );
}

void cMiner::startMiner(string params, cGetWork *getWork, cSubmitter* submitter, cStatDisplay *statDisplay, uint32_t GPUIndex) {

    pause = false;

    vector<string> vParams = split(params, ",");

    char type = tolower(vParams[0].c_str()[0]);
    if ((type != 'c') && (type != 'g')) {
        printf("Invalid miner type - must be GPU or CPU: %s\n", params.c_str());
        exit(0);
    }

    int numThread = atoi(vParams[1].c_str());
    int platformID;
    int deviceID;

    if (type == 'g') {
        if ((vParams.size() != 5) && (vParams.size() != 6)) {
            printf("GPU miner must specify work items, platform ID and device ID\n");
            exit(0);
        }
        int workSize = atoi(vParams[2].c_str());
        int platformID = atoi(vParams[3].c_str());
        int deviceID = atoi(vParams[4].c_str());
        int gpuLoops = 1;
        if (vParams.size() == 6)
            gpuLoops = atoi(vParams[5].c_str());
        startGPUMiner(numThread, platformID, deviceID, getWork, submitter, statDisplay, workSize, GPUIndex, gpuLoops);
    }
    else {
        uint32_t startNonce = 0xFFFFFFFF / numThread;
        for (unsigned int i = 0; i < numThread; i++) {
            thread minerThread(&cMiner::startCPUMiner, this, getWork, submitter, statDisplay, i, i * startNonce);
            minerThread.detach();
        }
    }


}


static inline uint32_t CLZz(register uint32_t x)
{
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    x -= ((x >> 1) & 0x55555555U);
    x = (x & 0x33333333U) + ((x >> 2) & 0x33333333U);
    return 32U - ((((x + (x >> 4)) & 0x0F0F0F0FU) * 0x01010101U) >> 24);
}


unsigned int countLeadingZeros(unsigned char* hash) {
    int c = CLZz((hash[0] << 24) + (hash[1] << 16) + (hash[2] << 8) + hash[3]);
    if (c == 32) {
        c += CLZz((hash[4] << 24) + (hash[5] << 16) + (hash[6] << 8) + hash[7]);
        if (c == 64) {
            c += CLZz((hash[8] << 24) + (hash[9] << 16) + (hash[10] << 8) + hash[11]);
            if (c == 96) {
                c += CLZz((hash[12] << 24) + (hash[13] << 16) + (hash[14] << 8) + hash[15]);
                if (c == 128) {
                    c += CLZz((hash[16] << 24) + (hash[17] << 16) + (hash[18] << 8) + hash[19]);
                    if (c == 160) {
                        c += CLZz((hash[20] << 24) + (hash[21] << 16) + (hash[22] << 8) + hash[23]);
                        if (c == 192) {
                            c += CLZz((hash[24] << 24) + (hash[25] << 16) + (hash[26] << 8) + hash[27]);
                            if (c == 224)
                                c += CLZz((hash[28] << 24) + (hash[29] << 16) + (hash[30] << 8) + hash[31]);
                        }
                    }
                }
            }
        }
    }
    return c;
}

static void checkReturn(const char* call, int val) {
    if (val != CL_SUCCESS) {
        printf("OPENCL error %d calling %s\n.", val, call);
        exit(0);
    }
}

void cMiner::startCPUMiner(cGetWork* getWork, cSubmitter* submitter, cStatDisplay* statDisplay, int cpuIndex, unsigned int startNonce) {
    bool workReady = false;
    while (!workReady) {
        getWork->lockJob.lock();
        workReady = (getWork->workID != 0);
        getWork->lockJob.unlock();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }


    unsigned char* buffHeader = (unsigned char*) malloc(80);


    char cKey[32];
    sprintf(cKey, "CPU%d", cpuIndex );
    basic_string<char> sKey(cKey);
    statDisplay->addCard(sKey);

    CSHA256 sha256;

    while (true) {
        if (!pause) {

            int workID;

            getWork->lockJob.lock();

            workID = getWork->workID;
            string jobID = getWork->jobID;
            memcpy(buffHeader, getWork->nativeData, 80);


            //int numZero = countLeadingZeros()
            uint64_t target;
            if (getWork->miningMode == "solo")
                target = BSWAP64(((uint64_t*)getWork->nativeTarget)[0]);
            else
                target = share_to_target(getWork->difficultyTarget) * 65536;

            std::vector<unsigned int> program = getWork->programVM->byteCode;

            getWork->lockJob.unlock();

            uint32_t nonce = startNonce;
            unsigned char hash[32];

            while (workID == getWork->workID) {
                memcpy(&buffHeader[76], &nonce, 4);

                runProgram(buffHeader, program, (unsigned int*)hash, sha256);
                uint64_t hash_int{};
                memcpy(&hash_int, hash, 8);
                hash_int = htobe64(hash_int);
                if (hash_int < target)
                    submitter->submitNonce(nonce, getWork, workID);

                nonce++;
                statDisplay->totalStats->nonce_count++;
            }
        }
        else
            std::this_thread::sleep_for(std::chrono::seconds(1));

    }
}


void sha256(unsigned int len, unsigned char* data, unsigned char* output, CSHA256 sha256) {
    sha256.Reset();
    sha256.Write(data, len);
    sha256.Finalize(output);
}

void cMiner::runProgram(unsigned char* myHeader, std::vector<unsigned int> byteCode, unsigned int* myHashResult, CSHA256 _sha256) {

    
    //hex2bin(myHeader, "40000000000002BFAAC1A438664DA0F203BAFF381DD2B32B38DC840C0EA176B125E3B335A40D0F5692310EB526E1855CC6524615EA80A5D6C2E9AA159A7927929F744353394F", 80);

    /*
    for (int i = 0; i < 16; i++) {
        unsigned char swap = myHeader[i+4];
        myHeader[i+4] = myHeader[35 - i];
        myHeader[35 - i] = swap;
    }
    */

    uint32_t myMemGen[512 * 8];
    uint32_t tempStore[8];

    uint32_t prevHashSHA[8];
    sha256(32, &myHeader[4], (unsigned char*)prevHashSHA, _sha256);

    sha256(80, myHeader, (unsigned char*)myHashResult, _sha256);

    uint32_t linePtr = 0;
    uint32_t done = 0;
    uint32_t currentMemSize = 0;
    uint32_t instruction = 0;

    uint32_t loop_opcode_count;
    uint32_t loop_line_ptr;


    while (1) {
       
        /*
        for (int i = 0; i < 8; i++)
            printf("%08X", myHashResult[i]);
        printf("\n");
        */
        

        if (byteCode[linePtr] == HASHOP_ADD) {
            linePtr++;
            for (int i = 0; i < 8; i++)
                myHashResult[i] += byteCode[linePtr + i];
            linePtr += 8;
        }


        else if (byteCode[linePtr] == HASHOP_XOR) {
            linePtr++;
            for (int i = 0; i < 8; i++)
                myHashResult[i] ^= byteCode[linePtr + i];
            linePtr += 8;
        }


        else if (byteCode[linePtr] == HASHOP_SHA_SINGLE) {
            sha256(32, (unsigned char*)myHashResult, (unsigned char*)myHashResult, _sha256);
            linePtr++;
        }


        else if (byteCode[linePtr] == HASHOP_SHA_LOOP) {
            linePtr++;
            uint32_t loopCount = byteCode[linePtr];
            for (int i = 0; i < loopCount; i++) {
                sha256(32, (unsigned char*)myHashResult, (unsigned char*)myHashResult, _sha256);
            }
            linePtr++;
        }


        else if (byteCode[linePtr] == HASHOP_MEMGEN) {
            linePtr++;

            currentMemSize = byteCode[linePtr];

            for (int i = 0; i < currentMemSize; i++) {
                sha256(32, (unsigned char*)myHashResult, (unsigned char*)myHashResult, _sha256);
                for (int j = 0; j < 8; j++)
                    myMemGen[i * 8 + j] = myHashResult[j];
            }


            linePtr++;
        }


        else if (byteCode[linePtr] == HASHOP_MEMADD) {
            linePtr++;

            for (int i = 0; i < currentMemSize; i++)
                for (int j = 0; j < 8; j++)
                    myMemGen[i * 8 + j] += byteCode[linePtr + j];

            linePtr += 8;
        }

        else if (byteCode[linePtr] == HASHOP_MEMADDHASHPREV) {
            linePtr++;

            for (int i = 0; i < currentMemSize; i++)
                for (int j = 0; j < 8; j++) {
                    myMemGen[i * 8 + j] += myHashResult[j] + prevHashSHA[j];
                }

        }


        else if (byteCode[linePtr] == HASHOP_MEMXOR) {
            linePtr++;

            for (int i = 0; i < currentMemSize; i++)
                for (int j = 0; j < 8; j++)
                    myMemGen[i * 8 + j] ^= byteCode[linePtr + j];

            linePtr += 8;
        }

        else if (byteCode[linePtr] == HASHOP_MEMXORHASHPREV) {
            linePtr++;

            for (int i = 0; i < currentMemSize; i++)
                for (int j = 0; j < 8; j++) {
                    myMemGen[i * 8 + j] += myHashResult[j];
                    myMemGen[i * 8 + j] ^= prevHashSHA[j];
                }

        }


        else if (byteCode[linePtr] == HASHOP_MEM_SELECT) {
            linePtr++;
            uint32_t index = byteCode[linePtr] % currentMemSize;
            for (int j = 0; j < 8; j++)
                myHashResult[j] = myMemGen[index * 8 + j];

            linePtr++;
        }

        else if (byteCode[linePtr] == HASHOP_READMEM2) {
            linePtr++;
            if (byteCode[linePtr] == 0) {
                for (int i = 0; i < 8; i++)
                    myHashResult[i] ^= prevHashSHA[i];
            }
            else if (byteCode[linePtr] == 1) {
                for (int i = 0; i < 8; i++)
                    myHashResult[i] += prevHashSHA[i];
            }

            linePtr++;  //this is the source, only supports prev hash currently

            uint32_t index = 0;
            for (int i = 0; i < 8; i++)
                index += myHashResult[i];


            index = index % currentMemSize;

            for (int j = 0; j < 8; j++)
                myHashResult[j] = myMemGen[index * 8 + j];

            linePtr++;

        }

        else if (byteCode[linePtr] == HASHOP_LOOP) {
            loop_opcode_count = 0;
            for (int j = 0; j < 8; j++)
                loop_opcode_count += myHashResult[j];

            linePtr++;
            loop_opcode_count = loop_opcode_count % byteCode[linePtr] + 1;

            //printf("loop %d\n", loop_opcode_count);

            linePtr++;
            loop_line_ptr = linePtr;        //line to return to after endloop
        }

        else if (byteCode[linePtr] == HASHOP_ENDLOOP) {
            linePtr++;
            loop_opcode_count--;
            if (loop_opcode_count > 0)
                linePtr = loop_line_ptr;
        }

        else if (byteCode[linePtr] == HASHOP_IF) {
            linePtr++;
            uint32_t sum = 0;
            for (int j = 0; j < 8; j++)
                sum += myHashResult[j];
            sum = sum % byteCode[linePtr];
            linePtr++;
            uint32_t numToSkip = byteCode[linePtr];
            linePtr++;
            if (sum == 0) {
                linePtr += numToSkip;
            }
        }

        else if (byteCode[linePtr] == HASHOP_STORETEMP) {
            for (int j = 0; j < 8; j++)
                tempStore[j] = myHashResult[j];

            linePtr++;
        }

        else if (byteCode[linePtr] == HASHOP_EXECOP) {
            linePtr++;
            //next byte is source  (hard coded to temp)
            linePtr++;

            uint32_t sum = 0;
            for (int j = 0; j < 8; j++)
                sum += myHashResult[j];

            if (sum % 3 == 0) {
                for (int i = 0; i < 8; i++)
                    myHashResult[i] += tempStore[i];
            }

            else if (sum % 3 == 1) {
                for (int i = 0; i < 8; i++)
                    myHashResult[i] ^= tempStore[i];
            }

            else if (sum % 3 == 2) {
                sha256(32, (unsigned char*)myHashResult, (unsigned char*)myHashResult, _sha256);
            }

        }

        /*
        else if (byteCode[linePtr] == HASHOP_SUMBLOCK) {
            //this calc can be optimized, although the performance gain is minimal
            uint64_t row = (myHashResult[0] + myHashResult[1] + myHashResult[2] + myHashResult[3]) % 3072;
            uint64_t col = (myHashResult[4] + myHashResult[5] + myHashResult[6] + myHashResult[7]) % 32768;
            uint64_t index = row * 32768 + col;
            const uint64_t hashBlockSize = 1024ULL * 1024ULL * 3072ULL;
            for (int i = 0; i < 128; i++) 
                myHashResult[i % 8] += hashBlock[(index + i) % hashBlockSize ];

            linePtr++;
        }
        */



        else if (byteCode[linePtr] == HASHOP_END) {
            break;
        }

    }


}

void cMiner::startGPUMiner(const size_t computeUnits, int platformID, int deviceID, cGetWork *getWork, cSubmitter *submitter, cStatDisplay *statDisplay, size_t gpuWorkSize, uint32_t GPUIndex, int gpuLoops) {

    bool workReady = false;
    while (!workReady) {
        getWork->lockJob.lock();
        workReady = (getWork->workID != 0);
        getWork->lockJob.unlock();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    cl_int returnVal; 
    cl_uint ret_num_platforms;
    cl_uint numOpenCLDevices;

    cl_mem clGPUProgramBuffer;

    uint32_t hashResultSize;
    cl_mem clGPUHashResultBuffer;
    uint32_t* buffHashResult;

    uint32_t headerBuffSize;
    cl_mem clGPUHeaderBuffer;
    unsigned char* buffHeader;

    uint32_t nonceBuffSize;
    cl_mem clNonceBuffer;
    uint32_t* buffNonce;

    cl_mem clProgStartTime;

    cl_mem clMemgenBuffer;
    unsigned char* memGenBuffer;

    unsigned char* seedBlock;

    cl_mem clHashBlock;

    cl_platform_id* platform_id = (cl_platform_id*)malloc(16 * sizeof(cl_platform_id));
    cl_device_id* open_cl_devices = (cl_device_id*)malloc(16 * sizeof(cl_device_id));

    checkReturn("clGetPlatformIDs", clGetPlatformIDs(16, platform_id, &ret_num_platforms));
    checkReturn("clGetDeviceIDs", clGetDeviceIDs(platform_id[platformID], CL_DEVICE_TYPE_GPU, 16, open_cl_devices, &numOpenCLDevices));
    cl_context context = clCreateContext(NULL, 1, &open_cl_devices[deviceID], NULL, NULL, &returnVal);

    cl_ulong maxMemAlloc;
    size_t sizeRet;
    clGetDeviceInfo(open_cl_devices[deviceID], CL_DEVICE_MAX_MEM_ALLOC_SIZE, sizeof(cl_ulong), &maxMemAlloc, &sizeRet);


    cl_program program = loadMiner(context, &open_cl_devices[deviceID], gpuLoops);

    kernel = clCreateKernel(program, "dyn_hash", &returnVal);
    commandQueue = clCreateCommandQueueWithProperties(context, open_cl_devices[deviceID], NULL, &returnVal);

    size_t programBufferSize = 8192;  //getWork->programVM->byteCode.size() * 4
    clGPUProgramBuffer = clCreateBuffer(context, CL_MEM_READ_WRITE, programBufferSize, NULL, &returnVal);
    checkReturn("clSetKernelArg - program", clSetKernelArg(kernel, 0, sizeof(cl_mem), (void*)&clGPUProgramBuffer));

    hashResultSize = computeUnits * 32;
    clGPUHashResultBuffer = clCreateBuffer(context, CL_MEM_READ_WRITE, hashResultSize, NULL, &returnVal);
    checkReturn("clSetKernelArg - hash", clSetKernelArg(kernel, 1, sizeof(cl_mem), (void*)&clGPUHashResultBuffer));
    buffHashResult = (uint32_t*)malloc(hashResultSize);

    headerBuffSize = 80;
    clGPUHeaderBuffer = clCreateBuffer(context, CL_MEM_READ_WRITE, headerBuffSize, NULL, &returnVal);
    checkReturn("clSetKernelArg - header", returnVal = clSetKernelArg(kernel, 2, sizeof(cl_mem), (void*)&clGPUHeaderBuffer));
    buffHeader = (unsigned char*)malloc(headerBuffSize);

    nonceBuffSize = sizeof(cl_uint) * 0x100;
    
    clNonceBuffer = clCreateBuffer(context, CL_MEM_READ_WRITE, nonceBuffSize, NULL, &returnVal);
    checkReturn("clSetKernelArg - nonce", clSetKernelArg(kernel, 3, sizeof(cl_mem), (void*)&clNonceBuffer));
    buffNonce = (uint32_t*)malloc(nonceBuffSize);


    size_t memgenBufferSize = 64 * 8 * computeUnits * sizeof(uint32_t);        //TODO - analyze program to find maximum memgen size
    clMemgenBuffer = clCreateBuffer(context, CL_MEM_READ_WRITE, memgenBufferSize, NULL, &returnVal);
    checkReturn("clSetKernelArg - clMemgenBuffer", clSetKernelArg(kernel, 5, sizeof(cl_mem), (void*)&clMemgenBuffer));

    /*
    size_t hashBockSize = 1024ULL * 1024ULL * 3072ULL;
    clHashBlock = clCreateBuffer(context, CL_MEM_READ_WRITE, hashBockSize, NULL, &returnVal);
    checkReturn("clSetKernelArg - clHashBlock", clSetKernelArg(kernel, 6, sizeof(cl_mem), (void*)&clHashBlock));
    */

    char cKey[32];
    sprintf(cKey, "%02d:%02d.0", platformID, deviceID);
    //string sKey = string::basic_string(cKey);
    basic_string<char> sKey(cKey);
    statDisplay->addCard(sKey);

    /*
    memset(hashBlock, 0, hashBockSize);
    checkReturn("clEnqueueWriteBuffer - hashblock", clEnqueueWriteBuffer(commandQueue, clHashBlock, CL_TRUE, 0, hashBockSize, hashBlock, 0, NULL, NULL));
    */

    while (true) {

        if (!pause) {

            int workID;


            getWork->lockJob.lock();

            //uint32_t openCLprogramLoops = 8;

            workID = getWork->workID;
            string jobID = getWork->jobID;
            memcpy(buffHeader, getWork->nativeData, 80);

            checkReturn("clEnqueueWriteBuffer - program", clEnqueueWriteBuffer(commandQueue, clGPUProgramBuffer, CL_TRUE, 0, getWork->programVM->byteCode.size() * 4, getWork->programVM->byteCode.data(), 0, NULL, NULL));
            
            uint64_t target = 0;

            if (getWork->miningMode == "solo") {
                target = BSWAP64(((uint64_t*)getWork->nativeTarget)[0]);
                //checkReturn("clSetKernelArg - target", clSetKernelArg(kernel, 4, sizeof(cl_ulong), &getWork->targetZeros));
                checkReturn("clSetKernelArg - target", clSetKernelArg(kernel, 4, sizeof(cl_ulong), &target));
            }
            else if ((getWork->miningMode == "stratum") || (getWork->miningMode == "pool")) {
                target = share_to_target(getWork->difficultyTarget) * 65536;
                //checkReturn("clSetKernelArg - target", clSetKernelArg(kernel, 4, sizeof(cl_ulong), &getWork->targetZeros));
                checkReturn("clSetKernelArg - target", clSetKernelArg(kernel, 4, sizeof(cl_ulong), &target));
            }

            getWork->lockJob.unlock();

            // WARNING: what if they have multiple platforms?
            //uint32_t MinNonce = (0xFFFFFFFFULL / numOpenCLDevices) * GPUIndex;
            //uint32_t MaxNonce = MinNonce + (0xFFFFFFFFULL / numOpenCLDevices);
            //uint32_t nonce = MinNonce;



            while (workID == getWork->workID) {
                getWork->lockNonce.lock();
                uint32_t nonce = getWork->nextNonce;
                getWork->nextNonce += computeUnits * gpuLoops;
                getWork->lockNonce.unlock();

                if ((getWork->miningMode == "stratum") || (getWork->miningMode == "pool")) {
                    uint64_t newTarget = share_to_target(getWork->difficultyTarget) * 65536;
                    if (newTarget != target) {
                        target = newTarget;
                        checkReturn("clSetKernelArg - target", clSetKernelArg(kernel, 4, sizeof(cl_ulong), &target));
                        //checkReturn("clSetKernelArg - target", clSetKernelArg(kernel, 4, sizeof(cl_ulong), &getWork->targetZeros));
                    }
                }


                memcpy(&buffHeader[76], &nonce, 4);
                uint32_t zero = 0;
                size_t gOffset = nonce;

                //hex2bin(buffHeader, "400000000000aa91b8738f8c886ea3deb31125566cfaf873875f2548b8d4a1e98a55bf76993be33bb745baa1fadb1ac7c7ddedd2350dc937ea885fb99043d8da8d610b46c1146864ffff001ff1ca0000", 160);


                checkReturn("clEnqueueWriteBuffer - header", clEnqueueWriteBuffer(commandQueue, clGPUHeaderBuffer, CL_TRUE, 0, headerBuffSize, buffHeader, 0, NULL, NULL));
                checkReturn("clEnqueueWriteBuffer - NonceRetBuf", clEnqueueWriteBuffer(commandQueue, clNonceBuffer, CL_TRUE, sizeof(cl_uint) * 0xFF, sizeof(cl_uint), &zero, 0, NULL, NULL));
                size_t localWorkSize = gpuWorkSize;
                checkReturn("clEnqueueNDRangeKernel", clEnqueueNDRangeKernel(commandQueue, kernel, 1, &gOffset, &computeUnits, &localWorkSize, 0, NULL, NULL));
                checkReturn("clFinish", clFinish(commandQueue));
                checkReturn("clEnqueueReadBuffer - nonce", clEnqueueReadBuffer(commandQueue, clNonceBuffer, CL_TRUE, 0, sizeof(cl_uint) * 0x100, buffNonce, 0, NULL, NULL));

                //printf("num nonce %d\n", buffNonce[0xFF]);
                for (int i = 0; i < buffNonce[0xFF]; ++i)
                {
                    submitter->submitNonce(buffNonce[i], getWork, workID);
                }

                //nonce += computeUnits * gpuLoops;
                statDisplay->totalStats->nonce_count += computeUnits * gpuLoops;

                /*
                if (nonce >= MaxNonce)
                {
                    printf("WARNING: GPU ran out of work!\nTODO: Fix me.\n.");
                }
                */
            }
        }
        else
            std::this_thread::sleep_for(std::chrono::seconds(1));

    }
    
}


vector<string> cMiner::split(string str, string token) {
    vector<string>result;
    while (str.size()) {
        int index = str.find(token);
        if (index != string::npos) {
            result.push_back(str.substr(0, index));
            str = str.substr(index + token.size());
            if (str.size() == 0)result.push_back(str);
        }
        else {
            result.push_back(str);
            str = "";
        }
    }
    return result;
}


cl_program cMiner::loadMiner(cl_context context, cl_device_id* deviceID, int gpuLoops) {

    FILE* kernelSourceFile;
    cl_int returnVal;


    kernelSourceFile = fopen("dyn_miner3.cl", "r");
    if (!kernelSourceFile) {
        fprintf(stderr, "Failed to load openCL kernel.\n");
        exit(0);
    }
    fseek(kernelSourceFile, 0, SEEK_END);
    size_t sourceFileLen = ftell(kernelSourceFile) + 1;
    char* kernelSource = (char*)malloc(sourceFileLen);
    memset(kernelSource, 0, sourceFileLen);
    fseek(kernelSourceFile, 0, SEEK_SET);
    size_t numRead = fread(kernelSource, 1, sourceFileLen, kernelSourceFile);
    fclose(kernelSourceFile);

    cl_program program;


    char versionLine[256];
    sprintf(versionLine, "#define VERSION %s", MINER_VERSION);

#ifdef _WIN32
    if (_strnicmp(versionLine, kernelSource, strlen(versionLine)) != 0) {
        printf("Incorrect OpenCL program file.\n");
        exit(0);
    }
#endif // _WIN32

#ifdef __linux__
    if (strncasecmp(versionLine, kernelSource, strlen(versionLine)) != 0) {
        printf("Incorrect OpenCL program file.\n");
        exit(0);
    }
#endif

    // Create kernel program
    program = clCreateProgramWithSource(context, 1, (const char**)&kernelSource, &numRead, &returnVal);
    // Argument #4 here is the arguments to the kernel.

    char loopDefine[64];
    sprintf(loopDefine, "-D GPU_LOOPS=%d", gpuLoops);
    returnVal = clBuildProgram(program, 1, deviceID, loopDefine, NULL, NULL);


    
    if (returnVal != CL_SUCCESS) {
        printf("Error building openCL program:\n");
        size_t log_size;
        clGetProgramBuildInfo(program, *deviceID, CL_PROGRAM_BUILD_LOG, 0, NULL, &log_size);
        char* log = (char*)malloc(log_size);
        clGetProgramBuildInfo(program, *deviceID, CL_PROGRAM_BUILD_LOG, log_size, log, NULL);
        printf("\n\n%s\n", log);
        exit(0);
    }
    

    free(kernelSource);

    return program;

}


void cMiner::startTestCPUMiner(unsigned char* hashBlock) {



}


void cMiner::startTestGPUMiner(unsigned char* hashBlock) {


    cl_int returnVal;
    cl_device_id* device_id = (cl_device_id*)malloc(16 * sizeof(cl_device_id));
    cl_uint ret_num_platforms;

    cl_ulong globalMem;
    cl_uint computeUnits;
    size_t sizeRet;
    uint32_t numOpenCLDevices;

    cl_platform_id*  platform_id = (cl_platform_id*)malloc(16 * sizeof(cl_platform_id));
    returnVal = clGetPlatformIDs(16, platform_id, &ret_num_platforms);

    if (ret_num_platforms <= 0) {
        printf("No OpenCL platforms detected.\n");
        exit(0);
    }

    for (uint32_t i = 0; i < ret_num_platforms; i++) {
        returnVal = clGetDeviceIDs(platform_id[i], CL_DEVICE_TYPE_GPU, 16, device_id, &numOpenCLDevices);
        for (uint32_t j = 0; j < numOpenCLDevices; j++) {

            testOneGPU(i, j, hashBlock);        //note this is overridden from text file - need a more elegant way to test multiple/different cards eventually
        }
    }

}



void cMiner::testOneGPU( int platformID, int deviceID, unsigned char* hashBlock) {

    uint32_t gpuLoops = 1;
    


    string strProgram;
    cProgramVM* programVM = new cProgramVM();

    std::ifstream tff("test_algo.txt");
    std::stringstream buffer;
    buffer << tff.rdbuf();
    strProgram = buffer.str();


	stringstream stream(strProgram);
	string line;
	vector<string> vprogram{};

    getline(stream, line, '\n');        //platform
    platformID = atoi(line.c_str());

    getline(stream, line, '\n');        //device
    deviceID = atoi(line.c_str());

    getline(stream, line, '\n');        //compute unit
    size_t computeUnits = atoi(line.c_str());

    getline(stream, line, '\n');        //worksize
    uint32_t gpuWorkSize = atoi(line.c_str());

	while (getline(stream, line, '\n')) {
		vprogram.push_back(line);
	}
	vprogram.push_back("ENDPROGRAM");

	programVM->byteCode.clear();
    unsigned char junk[32];
	programVM->generateBytecode(vprogram, junk, junk);

    printf("Testing platform %d, device %d\n", platformID, deviceID);


    cl_int returnVal; 
    cl_uint ret_num_platforms;
    cl_uint numOpenCLDevices;

    cl_mem clGPUProgramBuffer;

    uint32_t hashResultSize;
    cl_mem clGPUHashResultBuffer;
    uint32_t* buffHashResult;

    uint32_t headerBuffSize;
    cl_mem clGPUHeaderBuffer;
    unsigned char* buffHeader;

    uint32_t nonceBuffSize;
    cl_mem clNonceBuffer;
    uint32_t* buffNonce;

    cl_mem clProgStartTime;

    cl_mem clMemgenBuffer;
    unsigned char* memGenBuffer;

    unsigned char* seedBlock;

    cl_mem clHashBlock1;
    cl_mem clHashBlock2;
    cl_mem clHashBlock3;

    cl_platform_id* platform_id = (cl_platform_id*)malloc(16 * sizeof(cl_platform_id));
    cl_device_id* open_cl_devices = (cl_device_id*)malloc(16 * sizeof(cl_device_id));

    checkReturn("clGetPlatformIDs", clGetPlatformIDs(16, platform_id, &ret_num_platforms));
    checkReturn("clGetDeviceIDs", clGetDeviceIDs(platform_id[platformID], CL_DEVICE_TYPE_GPU, 16, open_cl_devices, &numOpenCLDevices));
    cl_context context = clCreateContext(NULL, 1, &open_cl_devices[deviceID], NULL, NULL, &returnVal);

    cl_ulong maxMemAlloc;
    size_t sizeRet;
    clGetDeviceInfo(open_cl_devices[deviceID], CL_DEVICE_MAX_MEM_ALLOC_SIZE, sizeof(cl_ulong), &maxMemAlloc, &sizeRet);


    cl_program program = loadMiner(context, &open_cl_devices[deviceID], 1);

    kernel = clCreateKernel(program, "dyn_hash", &returnVal);
    commandQueue = clCreateCommandQueueWithProperties(context, open_cl_devices[deviceID], NULL, &returnVal);

    size_t programBufferSize = 8192;  //getWork->programVM->byteCode.size() * 4
    clGPUProgramBuffer = clCreateBuffer(context, CL_MEM_READ_ONLY, programBufferSize, NULL, &returnVal);
    checkReturn("clSetKernelArg - program", clSetKernelArg(kernel, 0, sizeof(cl_mem), (void*)&clGPUProgramBuffer));

    hashResultSize = computeUnits * 32;
    clGPUHashResultBuffer = clCreateBuffer(context, CL_MEM_READ_WRITE, hashResultSize, NULL, &returnVal);
    checkReturn("clSetKernelArg - hash", clSetKernelArg(kernel, 1, sizeof(cl_mem), (void*)&clGPUHashResultBuffer));
    buffHashResult = (uint32_t*)malloc(hashResultSize);

    headerBuffSize = 80;
    clGPUHeaderBuffer = clCreateBuffer(context, CL_MEM_READ_ONLY, headerBuffSize, NULL, &returnVal);
    checkReturn("clSetKernelArg - header", returnVal = clSetKernelArg(kernel, 2, sizeof(cl_mem), (void*)&clGPUHeaderBuffer));
    buffHeader = (unsigned char*)malloc(headerBuffSize);

    nonceBuffSize = sizeof(cl_uint) * 0x100;
    
    clNonceBuffer = clCreateBuffer(context, CL_MEM_READ_WRITE, nonceBuffSize, NULL, &returnVal);
    checkReturn("clSetKernelArg - nonce", clSetKernelArg(kernel, 3, sizeof(cl_mem), (void*)&clNonceBuffer));
    buffNonce = (uint32_t*)malloc(nonceBuffSize);


    size_t memgenBufferSize = 128 * 8 * computeUnits * sizeof(uint32_t);        //TODO - analyze program to find maximum memgen size
    clMemgenBuffer = clCreateBuffer(context, CL_MEM_READ_WRITE, memgenBufferSize, NULL, &returnVal);
    checkReturn("clSetKernelArg - clMemgenBuffer", clSetKernelArg(kernel, 5, sizeof(cl_mem), (void*)&clMemgenBuffer));

    /*
    size_t hashBockSize = 1024ULL * 1024ULL * 1024ULL;
    clHashBlock1 = clCreateBuffer(context, CL_MEM_READ_ONLY, hashBockSize, NULL, &returnVal);
    checkReturn("clSetKernelArg - clHashBlock", clSetKernelArg(kernel, 6, sizeof(cl_mem), (void*)&clHashBlock1));

    clHashBlock2 = clCreateBuffer(context, CL_MEM_READ_ONLY, hashBockSize, NULL, &returnVal);
    checkReturn("clSetKernelArg - clHashBlock", clSetKernelArg(kernel, 7, sizeof(cl_mem), (void*)&clHashBlock2));

    clHashBlock3 = clCreateBuffer(context, CL_MEM_READ_ONLY, hashBockSize, NULL, &returnVal);
    checkReturn("clSetKernelArg - clHashBlock", clSetKernelArg(kernel, 8, sizeof(cl_mem), (void*)&clHashBlock3));

    memset(hashBlock, 0, hashBockSize);
    checkReturn("clEnqueueWriteBuffer - hashblock", clEnqueueWriteBuffer(commandQueue, clHashBlock1, CL_TRUE, 0, hashBockSize, hashBlock, 0, NULL, NULL));
    checkReturn("clEnqueueWriteBuffer - hashblock", clEnqueueWriteBuffer(commandQueue, clHashBlock2, CL_TRUE, 0, hashBockSize, hashBlock, 0, NULL, NULL));
    checkReturn("clEnqueueWriteBuffer - hashblock", clEnqueueWriteBuffer(commandQueue, clHashBlock3, CL_TRUE, 0, hashBockSize, hashBlock, 0, NULL, NULL));
    */

    while (true) {

        if (!pause) {

            int workID;

            memset(buffHeader, 0, 80);

            checkReturn("clEnqueueWriteBuffer - program", clEnqueueWriteBuffer(commandQueue, clGPUProgramBuffer, CL_TRUE, 0, programVM->byteCode.size() * 4, programVM->byteCode.data(), 0, NULL, NULL));
            
            uint64_t target = 0x00000000FFFFFFFF;
            checkReturn("clSetKernelArg - target", clSetKernelArg(kernel, 4, sizeof(cl_ulong), &target));

/*
            if (getWork->miningMode == "solo") {
                target = BSWAP64(((uint64_t*)getWork->nativeTarget)[0]);
                //checkReturn("clSetKernelArg - target", clSetKernelArg(kernel, 4, sizeof(cl_ulong), &getWork->targetZeros));
                checkReturn("clSetKernelArg - target", clSetKernelArg(kernel, 4, sizeof(cl_ulong), &target));
            }
            else if ((getWork->miningMode == "stratum") || (getWork->miningMode == "pool")) {
                target = share_to_target(getWork->difficultyTarget) * 65536;
                //checkReturn("clSetKernelArg - target", clSetKernelArg(kernel, 4, sizeof(cl_ulong), &getWork->targetZeros));
                checkReturn("clSetKernelArg - target", clSetKernelArg(kernel, 4, sizeof(cl_ulong), &target));
            }
            */

            //getWork->lockJob.unlock();

            // WARNING: what if they have multiple platforms?
            //uint32_t MinNonce = (0xFFFFFFFFULL / numOpenCLDevices) * GPUIndex;
            //uint32_t MaxNonce = MinNonce + (0xFFFFFFFFULL / numOpenCLDevices);

            uint32_t nonce = 0;

            time_t start;
            time(&start);

            while (true) {
                nonce += computeUnits * gpuLoops;

                memcpy(&buffHeader[76], &nonce, 4);
                uint32_t zero = 0;
                size_t gOffset = nonce;

                checkReturn("clEnqueueWriteBuffer - header", clEnqueueWriteBuffer(commandQueue, clGPUHeaderBuffer, CL_TRUE, 0, headerBuffSize, buffHeader, 0, NULL, NULL));
                checkReturn("clEnqueueWriteBuffer - NonceRetBuf", clEnqueueWriteBuffer(commandQueue, clNonceBuffer, CL_TRUE, sizeof(cl_uint) * 0xFF, sizeof(cl_uint), &zero, 0, NULL, NULL));
                size_t localWorkSize = gpuWorkSize;
                checkReturn("clEnqueueNDRangeKernel", clEnqueueNDRangeKernel(commandQueue, kernel, 1, &gOffset, &computeUnits, &localWorkSize, 0, NULL, NULL));
                checkReturn("clFinish", clFinish(commandQueue));
                checkReturn("clEnqueueReadBuffer - nonce", clEnqueueReadBuffer(commandQueue, clNonceBuffer, CL_TRUE, 0, sizeof(cl_uint) * 0x100, buffNonce, 0, NULL, NULL));

                time_t now;
                time(&now);

                //printf("x");

                if (now - start >= 10) {
                    int nn = nonce / 10;
    std::stringstream ss;
    ss.imbue(std::locale(""));
    ss << std::fixed << nn;
    

                    printf("%s\n", ss.str().c_str());
                    nonce = 0;
                    time(&start);
                }

            }
        }
        else
            std::this_thread::sleep_for(std::chrono::seconds(1));

    }
    
}
