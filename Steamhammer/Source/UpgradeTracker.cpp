#include "UpgradeTracker.h"

using namespace UAlbertaBot;

void UpgradeTracker::update(const UIMap & units, LocutusMapGrid & grid)
{
    for (auto& weaponDamageItem : weaponDamage)
    {
        int current = _player->damage(weaponDamageItem.first);
        if (current > weaponDamageItem.second)
        {
            // Update the grid for all known units with this weapon type
            for (auto& ui : units)
                if (ui.second.lastPosition.isValid() &&
                    !ui.second.goneFromLastPosition &&
                    (ui.second.type.groundWeapon() == weaponDamageItem.first ||
                        ui.second.type.airWeapon() == weaponDamageItem.first))
                {
                    grid.unitWeaponDamageUpgraded(ui.second.type, ui.second.lastPosition, weaponDamageItem.first, weaponDamageItem.second, current);
                }

            weaponDamageItem.second = current;
        }
    }

    for (auto& weaponRangeItem : weaponRange)
    {
        int current = _player->weaponMaxRange(weaponRangeItem.first);
        if (current > weaponRangeItem.second)
        {
            // Update the grid for all known units with this weapon type
            for (auto& ui : units)
                if (ui.second.lastPosition.isValid() &&
                    !ui.second.goneFromLastPosition &&
                    (ui.second.type.groundWeapon() == weaponRangeItem.first ||
                        ui.second.type.airWeapon() == weaponRangeItem.first))
                {
                    grid.unitWeaponRangeUpgraded(ui.second.type, ui.second.lastPosition, weaponRangeItem.first, weaponRangeItem.second, current);
                }

            weaponRangeItem.second = current;
        }
    }

    for (auto& unitCooldownItem : unitCooldown)
    {
        int current = _player->weaponDamageCooldown(unitCooldownItem.first);
        if (current > unitCooldownItem.second)
        {
            unitCooldownItem.second = current;
        }
    }

    for (auto& unitTopSpeedItem : unitTopSpeed)
    {
        int current = _player->topSpeed(unitTopSpeedItem.first);
        if (current > unitTopSpeedItem.second)
        {
            unitTopSpeedItem.second = current;
        }
    }

    for (auto& unitArmorItem : unitArmor)
    {
        int current = _player->armor(unitArmorItem.first);
        if (current > unitArmorItem.second)
        {
            unitArmorItem.second = current;
        }
    }
}

int UpgradeTracker::getWeaponDamage(BWAPI::WeaponType wpn)
{
    if (weaponDamage.find(wpn) == weaponDamage.end())
        weaponDamage[wpn] = _player->damage(wpn);

    return weaponDamage[wpn];
}

int UpgradeTracker::getWeaponRange(BWAPI::WeaponType wpn)
{
    if (weaponRange.find(wpn) == weaponRange.end())
        weaponRange[wpn] = _player->weaponMaxRange(wpn);

    return weaponRange[wpn];
}

int UpgradeTracker::getUnitCooldown(BWAPI::UnitType type)
{
    if (unitCooldown.find(type) == unitCooldown.end())
        unitCooldown[type] = _player->weaponDamageCooldown(type);

    return unitCooldown[type];
}

double UpgradeTracker::getUnitTopSpeed(BWAPI::UnitType type)
{
    if (unitTopSpeed.find(type) == unitTopSpeed.end())
        unitTopSpeed[type] = _player->topSpeed(type);

    return unitTopSpeed[type];
}

int UpgradeTracker::getUnitArmor(BWAPI::UnitType type)
{
    if (unitArmor.find(type) == unitArmor.end())
        unitArmor[type] = _player->armor(type);

    return unitArmor[type];
}
