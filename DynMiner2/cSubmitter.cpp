#include "cSubmitter.h"
#include "cGetWork.h"
#include "cStatDisplay.h"


void cSubmitter::submitEvalThread(cGetWork *getWork, cStatDisplay *iStatDisplay, string mode) {

    statDisplay = iStatDisplay;
    minerMode = mode;

    return;

    while (getWork->workID == 0)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

    thread submitThread(&cSubmitter::submitNonceThread, this, getWork);
    submitThread.detach();

    rpcSequence = 0;

    curl = curl_easy_init();

	while (true) {

		hashListLock.lock();
		if (!hashList.empty()) {
        	cHashResult *hashResult = hashList[0];
			hashList.erase(hashList.begin());
			hashListLock.unlock();

            if (mode == "stratum") {
                uint64_t target = share_to_target(getWork->difficultyTarget) * 65536;

                // find a hash with difficulty higher than share diff
                for (uint32_t k = 0; k < hashResult->hashCount; k++) {
                    uint64_t hash_int{};
                    memcpy(&hash_int, &hashResult->buffer[k*32], 8);
                    hash_int = htobe64(hash_int);
                    if (hash_int < target) {
                        nonceListLock.lock();
                        uint32_t nonce = hashResult->nonceIndex[k];
                        cNonceEntry* entry = new cNonceEntry();
                        entry->nonce = nonce;
                        entry->jobID = hashResult->jobID;
                        nonceList.push_back(entry);
                        nonceListLock.unlock();
                    }
                }
            }
            else if (mode == "solo") {
                unsigned int target = countLeadingZeros((unsigned char*)(getWork->iNativeTarget));
                unsigned char* ptr = hashResult->buffer;
                for (uint32_t k = 0; k < hashResult->hashCount; k++) {
                    unsigned int numZeros = countLeadingZeros(ptr);
                    if (numZeros > target) {
                        nonceListLock.lock();
                        uint32_t nonce = hashResult->nonceIndex[k];
                        cNonceEntry* entry = new cNonceEntry();
                        entry->nonce = nonce;
                        entry->jobID = hashResult->jobID;
                        nonceList.push_back(entry);
                        nonceListLock.unlock();
                    }
                    ptr += 32;
                }

            }

            free(hashResult->nonceIndex);
            free(hashResult->buffer);
            delete hashResult;

		}
        else {
            hashListLock.unlock();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));

        }
		
	}

    //we will never get here, but anyway...
    curl_easy_cleanup(curl);

}




void cSubmitter::addHashResults(unsigned char* hashBuffer, int hashCount, string jobID, int deviceID, uint32_t* nonceIndex) {
	hashListLock.lock();
    cHashResult* hashResult = new cHashResult();

    hashResult->buffer = (unsigned char*)malloc(hashCount * 32);
    memcpy(hashResult->buffer, hashBuffer, hashCount * 32);

    hashResult->nonceIndex = (uint32_t*)malloc(hashCount * 4);
    memcpy(hashResult->nonceIndex, nonceIndex, hashCount * 4);

    hashResult->hashCount = hashCount;
    //hashResult->startNonce = startNonce;
    hashResult->jobID = jobID;
    hashResult->deviceID = deviceID;
	hashList.push_back(hashResult);
	hashListLock.unlock();
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


//todo - maybe a better way to do this, but might have only marginal CPU enhancement
unsigned int cSubmitter::countLeadingZeros(unsigned char* hash) {
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


void cSubmitter::submitNonceThread(cGetWork* getWork) {
    
    /*
    while (true) {
        nonceListLock.lock();
        if (!nonceList.empty()) {
            cNonceEntry* entry = nonceList[0];
            nonceList.erase(nonceList.begin());
            nonceListLock.unlock();

            if (getWork->jobID == entry->jobID)
                submitNonce(entry->nonce, getWork);

            delete entry;
        }
        else {
            nonceListLock.unlock();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));

        }
    }
    */

}


void cSubmitter::submitNonce(unsigned int nonce, cGetWork *getWork, int workID ) {

    submitLock.lock();

    if (minerMode == "stratum") {
        char buf[4096];
        unsigned int* pNonce = &nonce;
        sprintf(buf, "{\"params\": [\"%s\", \"%s\", \"\", \"%s\", \"%s\"], \"id\": \"%d\", \"method\": \"mining.submit\"}",
            rpcUser.c_str(), getWork->jobID.c_str(), getWork->timeHex.c_str(), makeHex((unsigned char*)pNonce, 4).c_str(), rpcSequence);

        //printf("%s\n", buf);

        int numSent = send(*stratumSocket, buf, strlen(buf), 0);
        if (numSent < 0) {
            printf("Socket error on submit block\n");
            *socketError = true;
            submitLock.unlock();
            return;
        }
        

        rpcSequence++;

        statDisplay->totalStats->share_count++;
    }

    else if (minerMode == "solo") {
        getWork->lockJob.lock();

        unsigned char header[80];
        memcpy(header, getWork->nativeData, 80);

        memcpy(header + 76, &nonce, 4);

        
        for (int i = 0; i < 16; i++) {
            unsigned char swap = header[4 + i];
            header[4 + i] = header[35 - i];
            header[35 - i] = swap;
        }
        
        

        std::string strBlock;

        char hexHeader[256];
        bin2hex(hexHeader, header, 80);
        strBlock += std::string(hexHeader);
        strBlock += getWork->transactionString;

        getWork->lockJob.unlock();

        if (getWork->workID != workID) {
            printf("Stale nonce, skipping\n");
            submitLock.unlock();
            return;
        }

        //printf("submit header: %s\n\n", hexHeader);

        json jResult = execRPC("{ \"id\": 0, \"method\" : \"submitblock\", \"params\" : [\"" + strBlock + "\"] }");

        printf("submit result: %s\n", jResult.dump().c_str());

        if (jResult["error"].is_null()) {
            //printf(" **** SUBMITTED BLOCK SOLUTION FOR APPROVAL!!! ****\n");
            getWork->reqNewBlockFlag = true;
            statDisplay->totalStats->share_count++;
            if (jResult["result"].is_null()) {
                statDisplay->totalStats->accepted_share_count++;
            }
            else {
                if (jResult["result"] == "high-hash")
                    statDisplay->totalStats->rejected_share_count++;
            }
        }
        else {
            //printf("Submit block failed: %s.\n", jResult["error"]);
            statDisplay->totalStats->rejected_share_count++;
        }

    }

    else if (minerMode == "pool") {
        getWork->lockJob.lock();

        unsigned char header[80];
        memcpy(header, getWork->nativeData, 80);

        memcpy(header + 76, &nonce, 4);


        for (int i = 0; i < 16; i++) {
            unsigned char swap = header[4 + i];
            header[4 + i] = header[35 - i];
            header[35 - i] = swap;
        }

        std::string strBlock;

        char hexHeader[256];
        bin2hex(hexHeader, header, 80);
        strBlock += std::string(hexHeader);
        strBlock += getWork->transactionString;

        getWork->lockJob.unlock();

        string data = "{ \"command\" : \"submit\", \"data\" : \"" + strBlock + "\" }\n";        

        int len = data.length();
        int sent = 0;
        while ((sent < len) && (!(*socketError))) {
            int numSent = send(*stratumSocket, data.c_str() + sent, len, 0);
            if (numSent <= 0) {
                *socketError = true;
            }
            else
                sent += numSent;
        }

        statDisplay->totalStats->share_count++;
    }

    submitLock.unlock();
    
}


json cSubmitter::execRPC(string data) {

    chunk.size = 0;

    curl = curl_easy_init();

    curl_easy_setopt(curl, CURLOPT_URL, rpcURL.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPAUTH, (long)CURLAUTH_BASIC);
    curl_easy_setopt(curl, CURLOPT_USERNAME, rpcUser.c_str());
    curl_easy_setopt(curl, CURLOPT_PASSWORD, rpcPassword.c_str());

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&chunk);

    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());

    res = curl_easy_perform(curl);

    if (res != CURLE_OK)
        fprintf(stderr, "execRPC(%s): curl_easy_perform() failed: %s\n", data.c_str(), curl_easy_strerror(res));

    json result = json::parse(chunk.memory);

    curl_easy_cleanup(curl);

    return result;
}

size_t cSubmitter::WriteMemoryCallback(void* contents, size_t size, size_t nmemb, void* userp)
{
    size_t realsize = size * nmemb;
    struct MemoryStruct* mem = (struct MemoryStruct*)userp;

    char* ptr = (char*)realloc(mem->memory, mem->size + realsize + 1);
    if (ptr == NULL) {
        /* out of memory! */
        printf("not enough memory (realloc returned NULL)\n");
        return 0;
    }

    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}
