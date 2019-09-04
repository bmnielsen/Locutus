#pragma once

#include "Common.h"

namespace WorkerOrderTimer
{
    void initialize();
    void write();

    void update();

    // Resends gather orders to optimize the worker order timer. Returns whether an order was sent to the worker.
    bool optimizeMineralWorker(BWAPI::Unit worker, BWAPI::Unit resource);
}
