#pragma once;

#include "Common.h"
#include "MacroAct.h"
#include "ProductionGoal.h"

namespace UAlbertaBot
{
    class UpgradeCompleteProductionGoal : public ProductionGoal
    {
    private:
        BWAPI::UpgradeType upgrade;
        bool completed;

    public:
        UpgradeCompleteProductionGoal(const MacroAct & macroAct, BWAPI::UpgradeType upgradeType)
            : ProductionGoal(macroAct)
            , upgrade(upgradeType)
            , completed(false) 
        {};

        void update();

        bool done();
    };

};
