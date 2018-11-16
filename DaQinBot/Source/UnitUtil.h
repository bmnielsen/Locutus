#pragma once

#include <Common.h>
#include <BWAPI.h>

namespace UAlbertaBot
{
namespace UnitUtil
{      
	bool IsMorphedBuildingType(BWAPI::UnitType type);
	bool IsMorphedUnitType(BWAPI::UnitType type);

	bool NeedsPylonPower(BWAPI::UnitType type);

	bool IsStaticDefense(BWAPI::UnitType type);
	bool IsComingStaticDefense(BWAPI::UnitType type);

	bool IsCombatSimUnit(BWAPI::Unit unit);
	bool IsCombatSimUnit(BWAPI::UnitType type);
	bool IsCombatUnit(BWAPI::UnitType type);
	bool IsCombatUnit(BWAPI::Unit unit);
    bool IsValidUnit(BWAPI::Unit unit);
    bool IsTierOneCombatUnit(BWAPI::UnitType type);
    
	bool CanAttack(BWAPI::Unit attacker, BWAPI::Unit target);
	bool CanAttack(BWAPI::UnitType attacker, BWAPI::UnitType target);
	bool CanAttackAir(BWAPI::Unit attacker);
	bool TypeCanAttackAir(BWAPI::UnitType attacker);
	bool CanAttackGround(BWAPI::Unit attacker);
	bool TypeCanAttackGround(BWAPI::UnitType attacker);
	double CalculateLTD(BWAPI::Unit attacker, BWAPI::Unit target);
	BWAPI::WeaponType GetWeapon(BWAPI::Unit attacker, BWAPI::Unit target);
	BWAPI::WeaponType GetWeapon(BWAPI::UnitType attacker, BWAPI::Unit target);
	BWAPI::WeaponType GetWeapon(BWAPI::UnitType attacker, BWAPI::UnitType target);
	int GetAttackRange(BWAPI::Unit attacker, BWAPI::Unit target);
	int GetAttackRangeAssumingUpgrades(BWAPI::UnitType attacker, BWAPI::UnitType target);
	int GetMaxAttackRange(BWAPI::UnitType attacker);    // air or ground
	int GetWeaponDamageToWorker(BWAPI::Unit attacker);

	bool GoodUnderDarkSwarm(BWAPI::Unit attacker);
	bool GoodUnderDarkSwarm(BWAPI::UnitType attacker);

	int GetAllUnitCount(BWAPI::UnitType type);
	int GetCompletedUnitCount(BWAPI::UnitType type);
	int GetUncompletedUnitCount(BWAPI::UnitType type);

	BWAPI::Unit GetNextCompletedBuildingOfType(BWAPI::UnitType type);
};
}