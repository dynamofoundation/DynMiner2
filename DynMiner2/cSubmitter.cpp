#include "cSubmitter.h"

void cSubmitter::submitEvalThread(cGetWork *getWork, cStatDisplay *iStatDisplay) {

    statDisplay = iStatDisplay;

    while (getWork->workID == 0)
        Sleep(10);

    thread submitThread(&cSubmitter::submitNonceThread, this, getWork);
    submitThread.detach();

    rpcSequence = 0;

	while (true) {

        uint64_t target = share_to_target(getWork->difficultyTarget) * 65536;

		hashListLock.lock();
		if (!hashList.empty()) {
        	cHashResult *hashResult = hashList[0];
			hashList.erase(hashList.begin());
			hashListLock.unlock();

			// find a hash with difficulty higher than share diff
			for (uint32_t k = 0; k < hashResult->size; k += 32 ) {
                
                /*
				int numZeros = countLeadingZeros(&pHashResult->buffer[k]);
                if (numZeros > getWork->difficultyTarget + 8) {               //diff multiplier is 65536 = 16 leading zeros?
                */
                uint64_t hash_int{};
                memcpy(&hash_int, &hashResult->buffer[k], 8);
                hash_int = htobe64(hash_int);
                if (hash_int < target) {                
                    nonceListLock.lock();
                    unsigned int nonce = hashResult->startNonce + k / 32;
                    cNonceEntry *entry = new cNonceEntry();
                    entry->nonce = nonce;
                    entry->jobID = hashResult->jobID;

                    nonceList.push_back(entry);
                    nonceListLock.unlock();
                }

                
			}

            free(hashResult->buffer);
            delete hashResult;

		}
        else {
            hashListLock.unlock();
            Sleep(10);
        }
		
	}

}




void cSubmitter::addHashResults(unsigned char* hashBuffer, int buffSize, unsigned int startNonce, string jobID) {
	hashListLock.lock();
    cHashResult* hashResult = new cHashResult();
    hashResult->buffer = (unsigned char*)malloc(buffSize);
    memcpy(hashResult->buffer, hashBuffer, buffSize);
    hashResult->size = buffSize;
    hashResult->startNonce = startNonce;
    hashResult->jobID = jobID;
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
            Sleep(10);
        }
    }

}


void cSubmitter::submitNonce(unsigned int nonce, cGetWork *getWork) {
    char buf[4096];
    unsigned int* pNonce = &nonce;
    sprintf_s(buf, "{\"params\": [\"%s\", \"%s\", \"\", \"%s\", \"%s\"], \"id\": \"%d\", \"method\": \"mining.submit\"}",
        user.c_str(), getWork->jobID.c_str(), getWork->timeHex.c_str(),makeHex((unsigned char*)pNonce, 4).c_str(), rpcSequence);

    int numSent = send(stratumSocket, buf, strlen(buf), 0);
    if (numSent < 0)
        printf("Socket error on submit block\n");

    rpcSequence++;

    statDisplay->share_count++;
    
}
