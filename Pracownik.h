#pragma once

#include <algorithm>
#include <park_wspolne.h>
#include <cstdio>
#include <unistd.h>
struct czasy {
    SimTime czasJazdy;
    std::vector<pid_t> pids;
    bool zajete;
};
