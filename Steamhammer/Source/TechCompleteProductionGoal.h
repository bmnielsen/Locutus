#pragma once;

#include "Common.h"
#include "MacroAct.h"
#include "ProductionGoal.h"

namespace UAlbertaBot
{
    class TechCompleteProductionGoal : public ProductionGoal
    {
    private:
        BWAPI::TechType tech;
        bool completed;

    public:
        TechCompleteProductionGoal(const MacroAct & macroAct, BWAPI::TechType techType) 
            : ProductionGoal(macroAct)
            , tech(techType) 
            , completed(false) 
        {};

        void update();

        bool done();
    };

};
