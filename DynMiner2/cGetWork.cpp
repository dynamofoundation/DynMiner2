#include "cGetWork.h"


cGetWork::cGetWork() {
	workID = 0;
}


bool readLine(char* buffer, char* line, int* head, int* tail) {

	bool found = false;
	int i = *tail;
	int j = 0;
	char temp[2048];
	while ((!found) && (i != *head)) {
		line[j] = buffer[i];
		j++;
		if (buffer[i] == '\n')
			found = true;
		else {
			i++;
			if (i == 65536)
				i = 0;
		}
	}
	if (found) {
		line[j - 1] = 0;
		*tail = i + 1;
	}

	return found;
}


void cGetWork::getWork(string mode, int stratumSocket, cStatDisplay* statDisplay) {

	programVM = new cProgramVM();

	if (mode == "stratum") {
		char* buffer = (char*)malloc(65536);
		int head = 0, tail = 0;

		while (true) {
			const int tmpBuffLen = 4096;
			char tmpBuff[tmpBuffLen];
			memset(tmpBuff, 0, tmpBuffLen);
			int numRecv = recv(stratumSocket, tmpBuff, tmpBuffLen, 0);
			if (numRecv > 0) {
				for (int i = 0; i < numRecv; i++) {			//todo can optimize with memcpy and calc delta - no big deal really
					buffer[head] = tmpBuff[i];
					head++;
					if (head == 65536)
						head = 0;
				}
			}

			if (head != tail) {
				char line[2048];
				while (readLine(buffer, line, &head, &tail)) {
					json msg = json::parse(line);
					const json& id = msg["id"];
					if (id.is_null()) {
						const std::string& method = msg["method"];
						if (method == "mining.notify") {
							setJobDetails(msg);
						}
						else if (method == "mining.set_difficulty") {
							const std::vector<uint32_t>& params = msg["params"];
							difficultyTarget = params[0];
							statDisplay->latest_diff.store (difficultyTarget);
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
								statDisplay->rejected_share_count++;
								////printf("Error (%s): %s (code: %d)\n", resp.c_str(), message.c_str(), code);
								////////////miner.shares.stats.rejected_share_count++;
							}
							else {
								statDisplay->accepted_share_count++;
								///printf("accepted\n");
								////////////miner.shares.stats.accepted_share_count++;
							}
						}
					}

				}
			}

			Sleep(1);

		}
	}
}

void cGetWork::setJobDetails(json msg) {

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

	workID++;

	lockJob.unlock();
}