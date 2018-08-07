#pragma once

#include "Common.h"
#include "UnitData.h"
#include "LocutusMapGrid.h"

namespace UAlbertaBot
{
class UpgradeTracker 
{
	BWAPI::Player	_player;

    std::map<BWAPI::WeaponType, int>    weaponDamage;
    std::map<BWAPI::WeaponType, int>    weaponRange;
    std::map<BWAPI::UnitType, int>      unitCooldown;
    std::map<BWAPI::UnitType, double>   unitTopSpeed;
    std::map<BWAPI::UnitType, int>      unitArmor;

public:

    UpgradeTracker(BWAPI::Player player) : _player(player) {};

    void    update(const UIMap & units, LocutusMapGrid & grid);

    int     getWeaponDamage(BWAPI::WeaponType wpn);
    int     getWeaponRange(BWAPI::WeaponType wpn);
    int     getUnitCooldown(BWAPI::UnitType type);
    double  getUnitTopSpeed(BWAPI::UnitType type);
    int     getUnitArmor(BWAPI::UnitType type);
};
}
