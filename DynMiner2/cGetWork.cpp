#include "cGetWork.h"
#include "cProgramVM.h"
#include "cStatDisplay.h"


cGetWork::cGetWork() {
	workID = 0;
}


bool readLine(vector<char>& buffer, string& line) {

	bool found = false;
    int i = 0;
	while ((!found) && (i != buffer.size())) 
		if (buffer[i] == '\n')
			found = true;
		else 
			i++;

	if (found) {
        line = string(buffer.data(), i );
        buffer.erase(buffer.begin(), buffer.begin() + i + 1);
	}

	return found;
}


void cGetWork::getWork(string mode, int stratumSocket, cStatDisplay* statDisplay) {

    stats = statDisplay;
    miningMode = mode;

    curl = curl_easy_init();

	programVM = new cProgramVM();

	if (mode == "stratum")
		startStratumGetWork(stratumSocket, statDisplay);
    else if (mode == "solo")
        startSoloGetWork(statDisplay);
    else if (mode == "pool")
        startPoolGetWork(stratumSocket, statDisplay);
}

void cGetWork::startSoloGetWork( cStatDisplay* statDisplay) {

	json jResult;


	chunk.memory = (char*)malloc(1);
	chunk.size = 0;

    transactionString = NULL;

	while (true) {
		jResult = execRPC("{ \"id\": 0, \"method\" : \"gethashfunction\", \"params\" : [] }");
        strProgram = jResult["result"][0]["program"];
        int start_time = jResult["result"][0]["start_time"];
        printf("got program %d\n", start_time);
        programStartTime = start_time;


        auto nonceNow = std::chrono::high_resolution_clock::now();
        auto tick = nonceNow.time_since_epoch();
        uint64_t lTick = tick.count();
        uint32_t extra_nonce = lTick % 0xFFFFFFFF;

		jResult = execRPC("{ \"id\": 0, \"method\" : \"getblocktemplate\", \"params\" : [{ \"rules\": [\"segwit\"] }] }");
        setJobDetailsSolo(jResult, extra_nonce, rpcWallet);
        statDisplay->totalStats->blockHeight = jResult["result"]["height"];

        reqNewBlockFlag = false;
        bool newBlock = false;
        time_t start, now;
        time(&start);
        time(&now);
        while ((!newBlock) && (!reqNewBlockFlag) && (now - start < 3)) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            jResult = execRPC("{ \"id\": 0, \"method\" : \"getblocktemplate\", \"params\" : [{ \"rules\": [\"segwit\"] }] }");
            if (jResult["result"]["height"] != chainHeight) {
                newBlock = true;
                //printf("Requesting new block");
            }
            time(&now);
        }
	}
}


void cGetWork::startStratumGetWork(int stratumSocket, cStatDisplay* statDisplay) {
    std::vector<char> buffer;

	while (true) {
		const int tmpBuffLen = 4096;
		char tmpBuff[tmpBuffLen];
		memset(tmpBuff, 0, tmpBuffLen);
		int numRecv = recv(stratumSocket, tmpBuff, tmpBuffLen, 0);
        if (numRecv > 0)
            for (int i = 0; i < numRecv; i++)
                buffer.push_back(tmpBuff[i]);


        //TODO - evaluate memory leaks due to return - might need a ~cGetWork
        if (numRecv < 0) {
            *socketError = true;
            return;
        }

        if (*socketError)        //if submitter flags error on send
            return;

        if (buffer.size() > 0) {
            string line;
			while (readLine(buffer, line)) {
				json msg = json::parse(line.c_str());
				const json& id = msg["id"];
				if (id.is_null()) {
					const std::string& method = msg["method"];
					if (method == "mining.notify") {
						setJobDetailsStratum(msg);
					}
					else if (method == "mining.set_difficulty") {
						const std::vector<uint32_t>& params = msg["params"];
						difficultyTarget = params[0];
 						statDisplay->totalStats->latest_diff.store(difficultyTarget);
					}
					else {
						printf("Unknown stratum method %s\n", method.data());
					}
				}
				else {
					const std::string& resp = id;
					if (resp == "auth") {
						const bool result = msg["result"];
						if (!result) {
							printf("Failed authentication\n");
						}
					}
					else {
						const bool result = msg["result"];
						if (!result) {
							const std::vector<json>& error = msg["error"];
							const int code = error[0];
							const std::string& message = error[1];
                            printf("%s\n", message.c_str());
							statDisplay->totalStats->rejected_share_count++;
							///////printf("Error (%s): %s (code: %d)\n", resp.c_str(), message.c_str(), code);
							////////////miner.shares.stats.rejected_share_count++;
						}
						else {
							statDisplay->totalStats->accepted_share_count++;
							///printf("accepted\n");
							////////////miner.shares.stats.accepted_share_count++;
						}
					}
				}

			}
		}

        std::this_thread::sleep_for(std::chrono::milliseconds(1));

	}
}

void cGetWork::setJobDetailsStratum(json msg) {

	lockJob.lock();

	const std::vector<json>& params = msg["params"];

	jobID = params[0];         
	prevBlockHashHex = params[1]; 

	hex2bin(prevBlockHashBin, prevBlockHashHex.c_str(), 32);

	strProgram = params[8];

	coinBase1 = params[2]; 
	coinBase2 = params[3]; 
	nbits = params[6];  
	timeHex = params[7];

	
	unsigned char coinbase[4096];
	memset(coinbase, 0, 4096);
	hex2bin(coinbase, coinBase1.c_str(), coinBase1.size());
	hex2bin(coinbase + (coinBase1.size() / 2), coinBase2.c_str(), coinBase2.size());
	size_t coinbase_size = (coinBase1.size() + coinBase2.size()) / 2;
	

	uint32_t ntime{};
	if (8 >= timeHex.size()) {
		hex2bin((unsigned char*)(&ntime), timeHex.c_str(), 4);
		ntime = swab32(ntime);
	}
	else {
		printf("Expected `ntime` with size 8 got size %lu\n", timeHex.size());
	}

	// Version

	nativeData[0] = 0x40;
	nativeData[1] = 0x00;
	nativeData[2] = 0x00;
	nativeData[3] = 0x00;

	memcpy(nativeData + 4, prevBlockHashBin, 32);

	sha256d(merkleRoot, coinbase, coinbase_size);
	memcpy(nativeData + 36, merkleRoot, 32);

	// reverse merkle root...why?  because bitcoin
	for (int i = 0; i < 16; i++) {
		unsigned char tmp = merkleRoot[i];
		merkleRoot[i] = merkleRoot[31 - i];
		merkleRoot[31 - i] = tmp;
	}

	memcpy(nativeData + 68, &ntime, 4);

	unsigned char bits[8];
	hex2bin(bits, nbits.data(), nbits.size());
	memcpy(nativeData + 72, &bits[3], 1);
	memcpy(nativeData + 73, &bits[2], 1);
	memcpy(nativeData + 74, &bits[1], 1);
	memcpy(nativeData + 75, &bits[0], 1);

	stringstream stream(strProgram);
	string line;
	vector<string> program{};
	while (getline(stream, line, '$')) {
		program.push_back(line);
	}
	program.push_back("ENDPROGRAM");

	programVM->byteCode.clear();
	programVM->generateBytecode(program, merkleRoot, prevBlockHashBin);

    if (programVM->byteCode.size() == 77)
        programStartTime = 1;
    else
        programStartTime = 2;

	workID++;

	lockJob.unlock();
}


void cGetWork::startPoolGetWork(int stratumSocket, cStatDisplay* statDisplay) {
    std::vector<char> buffer;
    uint32_t extraNonce = 0;

    while (true) {
        const int tmpBuffLen = 4096;
        char tmpBuff[tmpBuffLen];
        memset(tmpBuff, 0, tmpBuffLen);
        int numRecv = recv(stratumSocket, tmpBuff, tmpBuffLen, 0);
        if (numRecv > 0)
            for (int i = 0; i < numRecv; i++)
                buffer.push_back(tmpBuff[i]);


        //TODO - evaluate memory leaks due to return - might need a ~cGetWork
        if (numRecv < 0) {
            *socketError = true;
            return;
        }

        if (*socketError)        //if submitter flags error on send
            return;

        if (buffer.size() > 0) {
            string line;
            while (readLine(buffer, line)) {
                json msg = json::parse(line.c_str());
                const json& id = msg["id"];
                if (id.is_null()) {
                    const std::string& method = msg["command"];
                    if (method == "block_data") {
                        strProgram = msg["data"]["program"];
                        setJobDetailsSolo(msg["data"], extraNonce, miningWallet);
                    }
                    else if (method == "set_difficulty") {
                        difficultyTarget = msg["data"];
                        statDisplay->totalStats->latest_diff.store(difficultyTarget);
                    }
                    else if (method == "set_extranonce") {
                        extraNonce = msg["data"];
                    }
                    else if (method == "set_mining_wallet") {
                        miningWallet = msg["data"];
                    }
                    else {
                        printf("Unknown pool method %s\n", method.data());
                    }
                }
                else {
                    const std::string& resp = id;
                    if (resp == "auth") {
                        const bool result = msg["result"];
                        if (!result) {
                            printf("Failed authentication\n");
                        }
                    }
                    else {
                        const bool result = msg["result"];
                        if (!result) {
                            const std::vector<json>& error = msg["error"];
                            const int code = error[0];
                            const std::string& message = error[1];
                            printf("%s\n", message.c_str());
                            statDisplay->totalStats->rejected_share_count++;
                            ///////printf("Error (%s): %s (code: %d)\n", resp.c_str(), message.c_str(), code);
                            ////////////miner.shares.stats.rejected_share_count++;
                        }
                        else {
                            statDisplay->totalStats->accepted_share_count++;
                            ///printf("accepted\n");
                            ////////////miner.shares.stats.accepted_share_count++;
                        }
                    }
                }

            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));

    }
}


json cGetWork::execRPC(string data) {

	chunk.size = 0;


	curl_easy_setopt(curl, CURLOPT_URL, rpcURL.c_str());
	curl_easy_setopt(curl, CURLOPT_HTTPAUTH, (long)CURLAUTH_BASIC);
	curl_easy_setopt(curl, CURLOPT_USERNAME, rpcUser.c_str());
	curl_easy_setopt(curl, CURLOPT_PASSWORD, rpcPassword.c_str());
	
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&chunk);

	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());

	res = curl_easy_perform(curl);

	if (res != CURLE_OK)
		fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));

	json result = json::parse(chunk.memory);

	return result;
}

size_t cGetWork::WriteMemoryCallback(void* contents, size_t size, size_t nmemb, void* userp)
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
static unsigned int countLeadingZeros(unsigned char* hash) {
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

void cGetWork::setJobDetailsSolo(json result, uint32_t extranonce, string coinbaseAddress) {


    lockJob.lock();

    chainHeight = result["result"]["height"];
    uint32_t version = result["result"]["version"];
    string prevBlockHash = result["result"]["previousblockhash"];
    int64_t coinbaseVal = result["result"]["coinbasevalue"];
    uint32_t curtime = result["result"]["curtime"];
    std::string difficultyBits = result["result"]["bits"];
    json jtransactions = result["result"]["transactions"];
    string strNativeTarget = result["result"]["target"];

    jobID = to_string(chainHeight);


    int tx_size = 0;
    for (int i = 0; i < jtransactions.size(); i++) {
        std::string strTransaction = jtransactions[i]["data"];
        tx_size += strlen(strTransaction.c_str()) / 2;
    }

    int tx_count = jtransactions.size();

    //decode pay to address for miner
    static unsigned char pk_script[25] = { 0 };
    std::string payToAddress(coinbaseAddress);
    int pk_script_size = address_to_script(pk_script, sizeof(pk_script), payToAddress.c_str());

    //decode pay to address for developer
    static unsigned char pk_script_dev[25] = { 0 };
    std::string payToAddressDev("dy1qzvx3yfrucqa2ntsw8e7dyzv6u6dl2c2wjvx5jy");
    int pk_script_size_dev = address_to_script(pk_script_dev, sizeof(pk_script_dev), payToAddressDev.c_str());

    static unsigned char pk_script_charity[25] = { 0 };
    std::string payToAddressCharity("dy1qnt3gjkefzez7my4zmwx9w0xs3c2jcxks6kxrgp");
    int pk_script_size_charity = address_to_script(pk_script_charity, sizeof(pk_script_charity), payToAddressCharity.c_str());


    //create coinbase transaction

    unsigned char cbtx[512];
    memset(cbtx, 0, 512);
    le32enc((uint32_t*)cbtx, 1);    //version
    cbtx[4] = 1;                    //txin count
    memset(cbtx + 5, 0x00, 32);     //prev txn hash out
    le32enc((uint32_t*)(cbtx + 37), 0xffffffff);    //prev txn index out
    int cbtx_size = 43;

    for (int n = chainHeight; n; n >>= 8) {
        cbtx[cbtx_size++] = n & 0xff;
        if (n < 0x100 && n >= 0x80)
            cbtx[cbtx_size++] = 0;
    }
    
    //memcpy(cbtx + cbtx_size, cbmsg, strlen(cbmsg));
    //cbtx_size += strlen(cbmsg);
    
    //memcpy(cbtx + cbtx_size, cbmsg3, strlen(cbmsg3));
    //cbtx_size += strlen(cbmsg3);
    
    cbtx[42] = cbtx_size - 43;

    cbtx[41] = cbtx_size - 42;      //script signature length
    le32enc((uint32_t*)(cbtx + cbtx_size), 0xffffffff);         //out sequence
    cbtx_size += 4;

    cbtx[cbtx_size++] = 4;             //out count - one txout to devfee, one txout to miner, one txout for charity, one for witness sighash

    //coinbase to miner
    le32enc((uint32_t*)(cbtx + cbtx_size), (uint32_t)coinbaseVal);          //tx out amount
    le32enc((uint32_t*)(cbtx + cbtx_size + 4), coinbaseVal >> 32);
    cbtx_size += 8;
    cbtx[cbtx_size++] = pk_script_size;         //tx out script len
    memcpy(cbtx + cbtx_size, pk_script, pk_script_size);
    cbtx_size += pk_script_size;

    //coinbase to developer
    int64_t devFee = 5000000;
    le32enc((uint32_t*)(cbtx + cbtx_size), (uint32_t)devFee);          //tx out amount
    le32enc((uint32_t*)(cbtx + cbtx_size + 4), devFee >> 32);
    cbtx_size += 8;
    cbtx[cbtx_size++] = pk_script_size_dev;         //tx out script len
    memcpy(cbtx + cbtx_size, pk_script_dev, pk_script_size_dev);
    cbtx_size += pk_script_size_dev;

    //coinbase to charity
    int64_t charityFee = 5000000;
    le32enc((uint32_t*)(cbtx + cbtx_size), (uint32_t)charityFee);          //tx out amount
    le32enc((uint32_t*)(cbtx + cbtx_size + 4), charityFee >> 32);
    cbtx_size += 8;
    cbtx[cbtx_size++] = pk_script_size_charity;         //tx out script len
    memcpy(cbtx + cbtx_size, pk_script_charity, pk_script_size_charity);
    cbtx_size += pk_script_size_charity;



    tree_entry* wtree = (tree_entry*)malloc((tx_count + 3) * 32);
    memset(wtree, 0, (tx_count + 3) * 32);

    memset(cbtx + cbtx_size, 0, 8);                     //value of segwit txout
    cbtx_size += 8;
    cbtx[cbtx_size++] = 38;                 //tx out script length
    cbtx[cbtx_size++] = 0x6a;               //txout script
    cbtx[cbtx_size++] = 0x24;
    cbtx[cbtx_size++] = 0xaa;
    cbtx[cbtx_size++] = 0x21;
    cbtx[cbtx_size++] = 0xa9;
    cbtx[cbtx_size++] = 0xed;

    for (int i = 0; i < jtransactions.size(); i++) {
        std::string strTransactionHash = jtransactions[i]["hash"];
        hex2bin(wtree[i + 1], strTransactionHash.c_str(), 32);
        memrev(wtree[1 + i], 32);
    }
    /*
    for (int i = 0; i < tx_count; i++) {
        const json_t* tx = json_array_get(txa, i);
        const json_t* hash = json_object_get(tx, "hash");
        if (!hash || !hex2bin(wtree[1 + i], json_string_value(hash), 32)) {
            applog(LOG_ERR, "JSON invalid transaction hash");
            free(wtree);
            goto out;
        }
        memrev(wtree[1 + i], 32);
    }
    */


    int n = tx_count + 1;
    while (n > 1) {
        if (n % 2)
            memcpy(wtree[n], wtree[n - 1], 32);
        n = (n + 1) / 2;
        for (int i = 0; i < n; i++)
            sha256d(wtree[i], wtree[2 * i], 64);
    }


    memset(wtree[1], 0, 32);  // witness reserved value = 0
    sha256d(cbtx + cbtx_size, wtree[0], 64);

    cbtx_size += 32;
    free(wtree);



    le32enc((uint32_t*)(cbtx + cbtx_size), 0);      //  tx out lock time
    cbtx_size += 4;

    unsigned char txc_vi[9];


    n = varint_encode(txc_vi, 1 + tx_count);
    if (transactionString != NULL)
        free(transactionString);
    transactionString = (char*)malloc(2 * (n + cbtx_size + tx_size) + 2);
    memset(transactionString, 0, 2 * (n + cbtx_size + tx_size) + 2);
    bin2hex(transactionString, txc_vi, n);
    bin2hex(transactionString + 2 * n, cbtx, cbtx_size);
    char* txs_end = transactionString + strlen(transactionString);


    //create merkle root

    tree_entry* merkle_tree = (tree_entry*)malloc(32 * ((1 + tx_count + 1) & ~1));
    //size_t tx_buf_size = 32 * 1024;
    //unsigned char *tx = (unsigned char*)malloc(tx_buf_size);
    sha256d(merkle_tree[0], cbtx, cbtx_size);

    for (int i = 0; i < tx_count; i++) {
        std::string tx_hex = jtransactions[i]["data"];
        const size_t tx_hex_len = tx_hex.length();
        const int tx_size = tx_hex_len / 2;
        std::string txid = jtransactions[i]["txid"];
        hex2bin(merkle_tree[1 + i], txid.c_str(), 32);
        memrev(merkle_tree[1 + i], 32);
        memcpy(txs_end, tx_hex.c_str(), tx_hex.length());
        txs_end += tx_hex_len;
    }

    //free(tx); 
    //tx = NULL;

    n = 1 + tx_count;
    while (n > 1) {
        if (n % 2) {
            memcpy(merkle_tree[n], merkle_tree[n - 1], 32);
            ++n;
        }
        n /= 2;
        for (int i = 0; i < n; i++)
            sha256d(merkle_tree[i], merkle_tree[2 * i], 64);
    }


    //assemble header

    uint32_t headerData[32];
    version = 0x04000000;
    headerData[0] = swab32(version);

    uint32_t prevhash[8];
    hex2bin((unsigned char*)&prevhash, prevBlockHash.c_str(), 32);
    for (int i = 0; i < 8; i++)
        headerData[8 - i] = le32dec(prevhash + i);

    for (int i = 0; i < 8; i++)
        headerData[9 + i] = be32dec((uint32_t*)merkle_tree[0] + i);

    headerData[17] = swab32(curtime);

    uint32_t bits;
    hex2bin((unsigned char*)&bits, difficultyBits.c_str(), 4);
    headerData[18] = le32dec(&bits);

    memset(headerData + 19, 0x00, 52);

    headerData[20] = 0x80000000;
    headerData[31] = 0x00000280;


    //set up variables for the miner

    unsigned char cVersion[4];
    memcpy(cVersion, &version, 4);
    for (int i = 0; i < 4; i++)
        cVersion[i] = ((cVersion[i] & 0x0F) << 4) + (cVersion[i] >> 4);

    memcpy(nativeData, &cVersion[3], 1);
    memcpy(nativeData + 1, &cVersion[2], 1);
    memcpy(nativeData + 2, &cVersion[1], 1);
    memcpy(nativeData + 3, &cVersion[0], 1);

    memcpy(nativeData + 4, prevhash, 32);

    memcpy(nativeData + 36, merkle_tree[0], 32);

    memcpy(nativeData + 68, &curtime, 4);

    unsigned char cBits[4];
    memcpy(cBits, &bits, 4);
    memcpy(nativeData + 72, &cBits[3], 1);
    memcpy(nativeData + 73, &cBits[2], 1);
    memcpy(nativeData + 74, &cBits[1], 1);
    memcpy(nativeData + 75, &cBits[0], 1);

    hex2bin((unsigned char*)&iNativeTarget, strNativeTarget.c_str(), 32);

    memcpy(&nativeTarget, &iNativeTarget, 32);


    //reverse merkle root...why?  because bitcoin
    unsigned char revMerkleRoot[32];
    memcpy(revMerkleRoot, merkle_tree[0], 32);
    for (int i = 0; i < 16; i++) {
        unsigned char tmp = revMerkleRoot[i];
        revMerkleRoot[i] = revMerkleRoot[31 - i];
        revMerkleRoot[31 - i] = tmp;
    }
    bin2hex(strMerkleRoot, revMerkleRoot, 32);


    stringstream stream(strProgram);
    string line;
    vector<string> program{};
    while (getline(stream, line, '\n')) {
        program.push_back(line);
    }
    program.push_back("ENDPROGRAM");

    programVM->byteCode.clear();
    programVM->generateBytecode(program, merkleRoot, prevBlockHashBin);

    if (stats != NULL)
        stats->totalStats->latest_diff = countLeadingZeros((unsigned char*)iNativeTarget);

    workID++;

    lockJob.unlock();


}

static const char b58digits[] = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";


static bool b58dec(unsigned char* bin, size_t binsz, const char* b58)
{
    size_t i, j;
    uint64_t t;
    uint32_t c;
    uint32_t* outi;
    size_t outisz = (binsz + 3) / 4;
    int rem = binsz % 4;
    uint32_t remmask = 0xffffffff << (8 * rem);
    size_t b58sz = strlen(b58);
    bool rc = false;

    outi = (uint32_t*)calloc(outisz, sizeof(*outi));

    for (i = 0; i < b58sz; ++i) {
        for (c = 0; b58digits[c] != b58[i]; c++)
            if (!b58digits[c])
                goto out;
        for (j = outisz; j--; ) {
            t = (uint64_t)outi[j] * 58 + c;
            c = t >> 32;
            outi[j] = t & 0xffffffff;
        }
        if (c || outi[0] & remmask)
            goto out;
    }

    j = 0;
    switch (rem) {
    case 3:
        *(bin++) = (outi[0] >> 16) & 0xff;
    case 2:
        *(bin++) = (outi[0] >> 8) & 0xff;
    case 1:
        *(bin++) = outi[0] & 0xff;
        ++j;
    default:
        break;
    }
    for (; j < outisz; ++j) {
        be32enc((uint32_t*)bin, outi[j]);
        bin += sizeof(uint32_t);
    }

    rc = true;
out:
    free(outi);
    return rc;
}


static bool segwit_addr_decode(int* witver, uint8_t* witdata, size_t* witdata_len, const char* addr) {
    uint8_t data[84];
    char hrp_actual[84];
    size_t data_len;
    if (!bech32_decode(hrp_actual, data, &data_len, addr)) return false;
    if (data_len == 0 || data_len > 65) return false;
    if (data[0] > 16) return false;
    *witdata_len = 0;
    if (!convert_bits(witdata, witdata_len, 8, data + 1, data_len - 1, 5, 0)) return false;
    if (*witdata_len < 2 || *witdata_len > 40) return false;
    if (data[0] == 0 && *witdata_len != 20 && *witdata_len != 32) return false;
    *witver = data[0];
    return true;
}

static size_t bech32_to_script(uint8_t* out, size_t outsz, const char* addr) {
    uint8_t witprog[40];
    size_t witprog_len;
    int witver;

    if (!segwit_addr_decode(&witver, witprog, &witprog_len, addr))
        return 0;
    if (outsz < witprog_len + 2)
        return 0;
    out[0] = witver ? (0x50 + witver) : 0;
    out[1] = witprog_len;
    memcpy(out + 2, witprog, witprog_len);
    return witprog_len + 2;
}

size_t address_to_script(unsigned char* out, size_t outsz, const char* addr)
{
    unsigned char addrbin[25];
    int addrver;
    size_t rv;

    if (!b58dec(addrbin, sizeof(addrbin), addr))
        return bech32_to_script(out, outsz, addr);
    addrver = b58check(addrbin, sizeof(addrbin), addr);
    if (addrver < 0)
        return 0;
    switch (addrver) {
    case 5:    /* Bitcoin script hash */
    case 196:  /* Testnet script hash */
        if (outsz < (rv = 23))
            return rv;
        out[0] = 0xa9;  /* OP_HASH160 */
        out[1] = 0x14;  /* push 20 bytes */
        memcpy(&out[2], &addrbin[1], 20);
        out[22] = 0x87;  /* OP_EQUAL */
        return rv;
    default:
        if (outsz < (rv = 25))
            return rv;
        out[0] = 0x76;  /* OP_DUP */
        out[1] = 0xa9;  /* OP_HASH160 */
        out[2] = 0x14;  /* push 20 bytes */
        memcpy(&out[3], &addrbin[1], 20);
        out[23] = 0x88;  /* OP_EQUALVERIFY */
        out[24] = 0xac;  /* OP_CHECKSIG */
        return rv;
    }
}

void memrev(unsigned char* p, size_t len)
{
    unsigned char c, * q;
    for (q = p + len - 1; p < q; p++, q--) {
        c = *p;
        *p = *q;
        *q = c;
    }
}

int varint_encode(unsigned char* p, uint64_t n)
{
    int i;
    if (n < 0xfd) {
        p[0] = n;
        return 1;
    }
    if (n <= 0xffff) {
        p[0] = 0xfd;
        p[1] = n & 0xff;
        p[2] = n >> 8;
        return 3;
    }
    if (n <= 0xffffffff) {
        p[0] = 0xfe;
        for (i = 1; i < 5; i++) {
            p[i] = n & 0xff;
            n >>= 8;
        }
        return 5;
    }
    p[0] = 0xff;
    for (i = 1; i < 9; i++) {
        p[i] = n & 0xff;
        n >>= 8;
    }
    return 9;
}






static int b58check(unsigned char* bin, size_t binsz, const char* b58)
{
    unsigned char buf[32];
    int i;

    sha256d(buf, bin, binsz - 4);
    if (memcmp(&bin[binsz - 4], buf, 4))
        return -1;

    /* Check number of zeros is correct AFTER verifying checksum
     * (to avoid possibility of accessing the string beyond the end) */
    for (i = 0; bin[i] == '\0' && b58[i] == '1'; ++i);
    if (bin[i] == '\0' || b58[i] == '1')
        return -3;

    return bin[0];
}


static uint32_t bech32_polymod_step(uint32_t pre) {
    uint8_t b = pre >> 25;
    return ((pre & 0x1FFFFFF) << 5) ^
        (-((b >> 0) & 1) & 0x3b6a57b2UL) ^
        (-((b >> 1) & 1) & 0x26508e6dUL) ^
        (-((b >> 2) & 1) & 0x1ea119faUL) ^
        (-((b >> 3) & 1) & 0x3d4233ddUL) ^
        (-((b >> 4) & 1) & 0x2a1462b3UL);
}

static const int8_t bech32_charset_rev[128] = {
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    15, -1, 10, 17, 21, 20, 26, 30,  7,  5, -1, -1, -1, -1, -1, -1,
    -1, 29, -1, 24, 13, 25,  9,  8, 23, -1, 18, 22, 31, 27, 19, -1,
     1,  0,  3, 16, 11, 28, 12, 14,  6,  4,  2, -1, -1, -1, -1, -1,
    -1, 29, -1, 24, 13, 25,  9,  8, 23, -1, 18, 22, 31, 27, 19, -1,
     1,  0,  3, 16, 11, 28, 12, 14,  6,  4,  2, -1, -1, -1, -1, -1
};


static bool convert_bits(uint8_t* out, size_t* outlen, int outbits, const uint8_t* in, size_t inlen, int inbits, int pad) {
    uint32_t val = 0;
    int bits = 0;
    uint32_t maxv = (((uint32_t)1) << outbits) - 1;
    while (inlen--) {
        val = (val << inbits) | *(in++);
        bits += inbits;
        while (bits >= outbits) {
            bits -= outbits;
            out[(*outlen)++] = (val >> bits) & maxv;
        }
    }
    if (pad) {
        if (bits) {
            out[(*outlen)++] = (val << (outbits - bits)) & maxv;
        }
    }
    else if (((val << (outbits - bits)) & maxv) || bits >= inbits) {
        return false;
    }
    return true;
}


static bool bech32_decode(char* hrp, uint8_t* data, size_t* data_len, const char* input) {
    uint32_t chk = 1;
    size_t i;
    size_t input_len = strlen(input);
    size_t hrp_len;
    int have_lower = 0, have_upper = 0;
    if (input_len < 8 || input_len > 90) {
        return false;
    }
    *data_len = 0;
    while (*data_len < input_len && input[(input_len - 1) - *data_len] != '1') {
        ++(*data_len);
    }
    hrp_len = input_len - (1 + *data_len);
    if (1 + *data_len >= input_len || *data_len < 6) {
        return false;
    }
    *(data_len) -= 6;
    for (i = 0; i < hrp_len; ++i) {
        int ch = input[i];
        if (ch < 33 || ch > 126) {
            return false;
        }
        if (ch >= 'a' && ch <= 'z') {
            have_lower = 1;
        }
        else if (ch >= 'A' && ch <= 'Z') {
            have_upper = 1;
            ch = (ch - 'A') + 'a';
        }
        hrp[i] = ch;
        chk = bech32_polymod_step(chk) ^ (ch >> 5);
    }
    hrp[i] = 0;
    chk = bech32_polymod_step(chk);
    for (i = 0; i < hrp_len; ++i) {
        chk = bech32_polymod_step(chk) ^ (input[i] & 0x1f);
    }
    ++i;
    while (i < input_len) {
        int v = (input[i] & 0x80) ? -1 : bech32_charset_rev[(int)input[i]];
        if (input[i] >= 'a' && input[i] <= 'z') have_lower = 1;
        if (input[i] >= 'A' && input[i] <= 'Z') have_upper = 1;
        if (v == -1) {
            return false;
        }
        chk = bech32_polymod_step(chk) ^ v;
        if (i + 6 < input_len) {
            data[i - (1 + hrp_len)] = v;
        }
        ++i;
    }
    if (have_lower && have_upper) {
        return false;
    }
    return chk == 1;
}





