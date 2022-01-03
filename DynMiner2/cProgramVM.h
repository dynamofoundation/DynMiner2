#pragma once

#include <string>
#include <vector>
#include <sstream>
#include <iterator>

#include "hex.h"

using namespace std;

enum eOpcode  {
    ADD = 0,
    XOR = 1,
    SHA_SINGLE = 2,
    SHA_LOOP = 3,
    MEMGEN = 4,
    MEMADD = 5,
    MEMXOR = 6,
    MEM_SELECT = 7,
    END = 8
};


class cProgramVM
{

public:
    void generateBytecode(vector<string> strProgram, unsigned char* merkleRoot, unsigned char* prevBlockHash);
    eOpcode parseOpcode(vector<string> token);
    void append_hex_hash(const std::string& hex);

    vector<uint32_t> byteCode;

};

