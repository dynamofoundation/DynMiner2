#pragma once

#include <stdio.h>
#include <cstdint>
#include <atomic>
#include <string>
#include <thread>
#include <map>

#include "version.h"

class cSubmitter;

using namespace std;

#ifdef _WIN32
#include <windows.h>
#define SET_COLOR(color) SetConsoleTextAttribute(hConsole, color);
#include "Windows.h"
#else
#define SET_COLOR(color)
#endif


class cStats  {
public:
    atomic<uint64_t> nonce_count{};
    atomic<uint64_t> share_count{};
    atomic<uint32_t> accepted_share_count{};
    atomic<uint32_t> rejected_share_count{};
    atomic<uint32_t> latest_diff{};
    uint32_t blockHeight;
};

class cStatDisplay
{

public:
	void displayStats(cSubmitter* submitter, string mode, int hiveos);
    void addCard(string key);

    string seconds_to_uptime(int n);

    cStats* totalStats;
    std::map<string, cStats*> perCardStats;
    
    const double tb = 1099511627776;
    const double gb = 1073741824;
    const double mb = 1048576;
    const double kb = 1024;

    const int BLACK = 0;
    const int BLUE = 1;
    const int GREEN = 2;
    const int CYAN = 3;
    const int RED = 4;
    const int MAGENTA = 5;
    const int BROWN = 6;
    const int LIGHTGRAY = 7;
    const int DARKGRAY = 8;
    const int LIGHTBLUE = 9;
    const int LIGHTGREEN = 10;
    const int LIGHTCYAN = 11;
    const int LIGHTRED = 12;
    const int LIGHTMAGENTA = 13;
    const int YELLOW = 14;
    const int WHITE = 15;
};

