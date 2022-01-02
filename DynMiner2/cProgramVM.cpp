#include "cProgramVM.h"



void cProgramVM::generateBytecode(vector<string> strProgram, unsigned char* merkleRoot, unsigned char* prevBlockHash ) {

    for (uint32_t line = 0; line < strProgram.size(); line++) {
        std::istringstream iss(strProgram[line]);
        std::vector<std::string> tokens{
          std::istream_iterator<std::string>{iss}, std::istream_iterator<std::string>{} }; // split line into tokens

        eOpcode op = parseOpcode(tokens);
        byteCode.push_back(op);
        switch (op) {
        case eOpcode::ADD:
            append_hex_hash(tokens[1]);
            break;

        case eOpcode::XOR:
            append_hex_hash(tokens[1]);
            break;

        case eOpcode::SHA_LOOP:
            byteCode.push_back(atoi(tokens[1].c_str()));
            break;

        case eOpcode::SHA_SINGLE:
            break;

        case eOpcode::MEMGEN:
            byteCode.push_back(atoi(tokens[2].c_str()));
            break;

        case eOpcode::MEMADD:
            append_hex_hash(tokens[1]);
            break;

        case eOpcode::MEMXOR:
            append_hex_hash(tokens[1]);
            break;

        case eOpcode::MEM_SELECT:
            if (tokens[1] == "MERKLE") {
                uint32_t v0 = *(uint32_t*)merkleRoot;
                byteCode.push_back(v0);
            }

            else if (tokens[1] == "HASHPREV") {
                uint32_t v0 = *(uint32_t*)prevBlockHash;
                byteCode.push_back(v0);
            }
            break;

        case eOpcode::END:
            break;
        }
    }
}


eOpcode cProgramVM::parseOpcode(vector<string> token) {
    const std::string& op = token[0];
    if (op == "ADD") {
        return eOpcode::ADD;
    }
    if (op == "XOR") {
        return eOpcode::XOR;
    }
    if (op == "SHA2") {
        if (token.size() == 2) {
            return eOpcode::SHA_LOOP;
        }
        return eOpcode::SHA_SINGLE;
    }
    if (op == "MEMGEN") {
        return eOpcode::MEMGEN;
    }
    if (op == "MEMADD") {
        return eOpcode::MEMADD;
    }
    if (op == "MEMXOR") {
        return eOpcode::MEMXOR;
    }
    if (op == "READMEM") {
        return eOpcode::MEM_SELECT;
    }
    if (op == "ENDPROGRAM") {
        return eOpcode::END;
    }
    printf("Unrecognized opcode %s in program\n", token[0].c_str());
}


void cProgramVM::append_hex_hash(const std::string& hex) {
    uint32_t arg1[8];
    parseHex(hex, (unsigned char*)arg1);
    for (uint32_t i = 0; i < 8; i++) {
        byteCode.push_back(arg1[i]);
    }
}