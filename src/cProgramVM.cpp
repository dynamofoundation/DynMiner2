#include "cProgramVM.h"



int opSize(string op) {
    //todo - all opcodes

    if (op.rfind("ADD",2) != string::npos) {
        return 9;
    }
    if (op.rfind("XOR",2) != string::npos) {
        return 9;
    }
    if (op.rfind("SHA2",3) != string::npos) {
        return 2;
    }
    if (op.rfind("READMEM2",7) != string::npos) {
        return 3;
    }
    if (op.rfind("STORETEMP",9) != string::npos) {
        return 1;
    }
    if (op.rfind("MEMADDHASHPREV", 14) != string::npos) {
        return 1;
    }
    if (op.rfind("MEMXORHASHPREV", 14) != string::npos) {
        return 1;
    }

    printf("unsupported opcode after IF: %s\n", op.c_str());
    exit(0);

}

void cProgramVM::generateBytecode(vector<string> strProgram, unsigned char* merkleRoot, unsigned char* prevBlockHash ) {

    bool saveIFpointer = false;
    int ifPointer = 0;

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

        case eOpcode::MEMADDHASHPREV:
            break;

        case eOpcode::MEMXORHASHPREV:
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

        case eOpcode::READMEM2:
            if (tokens[1] == "XOR") {
                byteCode.push_back(0);
            }

            else if (tokens[1] == "ADD") {
                byteCode.push_back(1);
            }
            byteCode.push_back(0);  //only support prev hash currently
            break;

        case eOpcode::LOOP:
            byteCode.push_back(atoi(tokens[1].c_str()));
            break;

        case eOpcode::ENDLOOP:
            break;

        case eOpcode::IF:
            byteCode.push_back(atoi(tokens[1].c_str()));
            byteCode.push_back(opSize(strProgram[line+1]));
            break;

        case eOpcode::STORETEMP:
            break;

        case eOpcode::EXECOP:
            if (tokens[1] == "TEMP") {
                byteCode.push_back(0);
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
    if (op == "READMEM2") {
        return eOpcode::READMEM2;
    }
    if (op == "LOOP") {
        return eOpcode::LOOP;
    }
    if (op == "ENDLOOP") {
        return eOpcode::ENDLOOP;
    }
    if (op == "IF") {
        return eOpcode::IF;
    }
    if (op == "STORETEMP") {
        return eOpcode::STORETEMP;
    }
    if (op == "EXECOP") {
        return eOpcode::EXECOP;
    }
    if (op == "MEMADDHASHPREV") {
        return eOpcode::MEMADDHASHPREV;
    }
    if (op == "MEMXORHASHPREV") {
        return eOpcode::MEMXORHASHPREV;
    }

    printf("Unrecognized opcode %s in program\n", token[0].c_str());
    exit(0);
}


void cProgramVM::append_hex_hash(const std::string& hex) {
    uint32_t arg1[8];
    parseHex(hex, (unsigned char*)arg1);
    for (uint32_t i = 0; i < 8; i++) {
        byteCode.push_back(arg1[i]);
    }
}