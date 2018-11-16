#include "FAP.h"
#include "BWAPI.h"
#include "InformationManager.h"
#include "MathUtil.h"
#include "Logger.h"
#include "Random.h"

UAlbertaBot::FastAPproximation fap;

// NOTE FAP does not use UnitInfo.goneFromLastPosition. The flag is always set false
// on a UnitInfo value which is passed in (CombatSimulation makes sure of it).

namespace UAlbertaBot {

    FastAPproximation::FastAPproximation() {
#ifdef FAP_DEBUG
        std::ostringstream filename;
        filename << "bwapi-data/write/combatsim-" << Random::Instance().index(10000) << ".csv";
        debug.open(filename.str());
        debug << "bwapi frame;sim frame;self;unit type;unit id;score;x;y;health;shields;cooldown;target type;target id;target x;target y;target dist;action;new x;new y";
#endif
    }

    void FastAPproximation::addUnitPlayer1(FAPUnit fu) { player1.push_back(fu); }

    void FastAPproximation::addIfCombatUnitPlayer1(FAPUnit fu) {
        if (fu.unitType == BWAPI::UnitTypes::Protoss_Interceptor)
            return;
        if (fu.groundDamage || fu.airDamage ||
            fu.unitType == BWAPI::UnitTypes::Terran_Medic)
            addUnitPlayer1(fu);
    }

    void FastAPproximation::addUnitPlayer2(FAPUnit fu) { player2.push_back(fu); }

    void FastAPproximation::addIfCombatUnitPlayer2(FAPUnit fu) {
        if (fu.groundDamage || fu.airDamage ||
            fu.unitType == BWAPI::UnitTypes::Terran_Medic)
            addUnitPlayer2(fu);
    }

    void FastAPproximation::simulate(int nFrames) {
        while (nFrames--) {
            if (!player1.size() || !player2.size())
                break;

            didSomething = false;

            isimulate();
            frame++;

            if (!didSomething)
                break;
        }
    }

    const auto score = [](const FastAPproximation::FAPUnit &fu) {
        if (fu.health && fu.maxHealth)
            return ((fu.score * (fu.health * 3 + fu.shields) + fu.score) / (fu.maxHealth * 3 + fu.maxShields)) +
            (fu.unitType == BWAPI::UnitTypes::Terran_Bunker) *
            BWAPI::UnitTypes::Terran_Marine.destroyScore() * 4;
        return 0;
    };

    std::pair<int, int> FastAPproximation::playerScores() const {
        std::pair<int, int> res;

        for (const auto &u : player1)
            res.first += score(u);

        for (const auto &u : player2)
            res.second += score(u);

        return res;
    }

    std::pair<int, int> FastAPproximation::playerScoresUnits() const {
        std::pair<int, int> res;

        for (const auto &u : player1)
            if (!u.unitType.isBuilding())
                res.first += score(u);

        for (const auto &u : player2)
            if (!u.unitType.isBuilding())
                res.second += score(u);

        return res;
    }

    std::pair<int, int> FastAPproximation::playerScoresBuildings() const {
        std::pair<int, int> res;

        for (const auto &u : player1)
            if (u.unitType.isBuilding())
                res.first += score(u);

        for (const auto &u : player2)
            if (u.unitType.isBuilding())
                res.second += score(u);

        return res;
    }

    std::pair<std::vector<FastAPproximation::FAPUnit> *,
        std::vector<FastAPproximation::FAPUnit> *>
        FastAPproximation::getState() {
        return { &player1, &player2 };
    }

    void FastAPproximation::clearState() {
        player1.clear(), player2.clear(), frame = 0;
#ifdef FAP_DEBUG
        debug.flush();
#endif
    }

    void FastAPproximation::dealDamage(const FastAPproximation::FAPUnit &fu,
        int damage,
        BWAPI::DamageType damageType) const {
        damage <<= 8;
        int remainingShields = fu.shields - damage + (fu.shieldArmor << 8);
        if (remainingShields > 0) {
            fu.shields = remainingShields;
            return;
        }
        else if (fu.shields) {
            damage -= fu.shields + (fu.shieldArmor << 8);
            fu.shields = 0;
        }

        if (!damage)
            return;

        damage -= fu.armor << 8;

        if (damageType == BWAPI::DamageTypes::Concussive) {
            if (fu.unitSize == BWAPI::UnitSizeTypes::Large)
                damage = damage / 4;
            else if (fu.unitSize == BWAPI::UnitSizeTypes::Medium)
                damage = damage / 2;
        }
        else if (damageType == BWAPI::DamageTypes::Explosive) {
            if (fu.unitSize == BWAPI::UnitSizeTypes::Small)
                damage = damage / 2;
            else if (fu.unitSize == BWAPI::UnitSizeTypes::Medium)
                damage = (damage * 3) / 4;
        }

        fu.health -= std::max(128, damage);
    }

    int inline FastAPproximation::distance(
        const FastAPproximation::FAPUnit &u1,
        const FastAPproximation::FAPUnit &u2) const {
        return MathUtil::EdgeToEdgeDistance(u1.unitType, BWAPI::Position(u1.x, u1.y), u2.unitType, BWAPI::Position(u2.x, u2.y));
    }

    bool FastAPproximation::isSuicideUnit(BWAPI::UnitType ut) {
        return (ut == BWAPI::UnitTypes::Zerg_Scourge ||
            ut == BWAPI::UnitTypes::Terran_Vulture_Spider_Mine ||
            ut == BWAPI::UnitTypes::Zerg_Infested_Terran ||
            ut == BWAPI::UnitTypes::Protoss_Scarab);
    }

    void FastAPproximation::unitsim(
        const FastAPproximation::FAPUnit &fu,
        std::vector<FastAPproximation::FAPUnit> &enemyUnits) {

#ifdef FAP_DEBUG
        debug << "\n" << BWAPI::Broodwar->getFrameCount() << ";" << frame << ";" << (fu.player==BWAPI::Broodwar->self()) << ";" << fu.unitType << ";" << fu.id << ";" << score(fu) << ";" << fu.x << ";" << fu.y << ";" << fu.health << ";" << fu.shields << ";" << fu.attackCooldownRemaining;
#endif

        bool kite = false;
        if (fu.attackCooldownRemaining) {
            if (fu.unitType == BWAPI::UnitTypes::Terran_Vulture ||
                (fu.unitType == BWAPI::UnitTypes::Protoss_Dragoon &&
                fu.attackCooldownRemaining <= BWAPI::UnitTypes::Protoss_Dragoon.groundWeapon().damageCooldown() - 9))
            {
                kite = true;
            }

            if (!kite)
            {
#ifdef FAP_DEBUG
                debug << ";;;;;;;;";
#endif
                didSomething = true;
                return;
            }
        }

        auto closestEnemy = enemyUnits.end();
        int closestDist;

        for (auto enemyIt = enemyUnits.begin(); enemyIt != enemyUnits.end();
            ++enemyIt) {
            if (enemyIt->flying) {
                if (fu.airDamage) {
                    int d = distance(fu, *enemyIt);
                    if ((closestEnemy == enemyUnits.end() || d < closestDist) &&
                        d >= fu.airMinRange) {
                        closestDist = d;
                        closestEnemy = enemyIt;
                    }
                }
            }
            else {
                if (fu.groundDamage) {
                    int d = distance(fu, *enemyIt);
                    if ((closestEnemy == enemyUnits.end() || d < closestDist) &&
                        d >= fu.groundMinRange) {
                        closestDist = d;
                        closestEnemy = enemyIt;
                    }
                }
            }
        }

#ifdef FAP_DEBUG
        if (closestEnemy != enemyUnits.end())
            debug << ";" << closestEnemy->unitType << ";" << closestEnemy->id << ";" << closestEnemy->x << ";" << closestEnemy->y << ";" << closestDist;
        else
            debug << ";;;;;";
#endif

        if (kite)
        {
            if (closestEnemy != enemyUnits.end() &&
                closestEnemy->groundMaxRange < fu.groundMaxRange &&
                closestDist <= (fu.groundMaxRange + fu.speed))
            {
                int dx = closestEnemy->x - fu.x, dy = closestEnemy->y - fu.y;

                fu.x -= (int)(dx * (fu.speed / sqrt(dx * dx + dy * dy)));
                fu.y -= (int)(dy * (fu.speed / sqrt(dx * dx + dy * dy)));

#ifdef FAP_DEBUG
                debug << ";kite;" << fu.x << ";" << fu.y;
#endif
            }
#ifdef FAP_DEBUG
            else
                debug << ";idle;" << fu.x << ";" << fu.y;
#endif

            didSomething = true;
            return;
        }

        if (closestEnemy != enemyUnits.end() && closestDist <= fu.speed &&
            !(fu.x == closestEnemy->x && fu.y == closestEnemy->y)) {
            fu.x = closestEnemy->x;
            fu.y = closestEnemy->y;
            closestDist = 0;

            didSomething = true;
        }

        if (closestEnemy != enemyUnits.end() &&
            closestDist <=
            (closestEnemy->flying ? fu.airMaxRange : fu.groundMaxRange)) {
            if (closestEnemy->flying)
                dealDamage(*closestEnemy, fu.airDamage, fu.airDamageType),
                fu.attackCooldownRemaining = fu.airCooldown;
            else {
                dealDamage(*closestEnemy, fu.groundDamage, fu.groundDamageType);
                fu.attackCooldownRemaining = fu.groundCooldown;
                if (fu.elevation != -1 && closestEnemy->elevation != -1)
                    if (closestEnemy->elevation > fu.elevation)
                        fu.attackCooldownRemaining += fu.groundCooldown;
            }

            if (closestEnemy->health < 1) {
                auto temp = *closestEnemy;
                *closestEnemy = enemyUnits.back();
                enemyUnits.pop_back();
                unitDeath(temp, enemyUnits);
            }

#ifdef FAP_DEBUG
            debug << ";attack;" << fu.x << ";" << fu.y;
#endif

            didSomething = true;
            return;
        }
        else if (closestEnemy != enemyUnits.end() && closestDist > fu.speed) {
            int dx = closestEnemy->x - fu.x, dy = closestEnemy->y - fu.y;

            fu.x += (int)(dx * (fu.speed / sqrt(dx * dx + dy * dy)));
            fu.y += (int)(dy * (fu.speed / sqrt(dx * dx + dy * dy)));

#ifdef FAP_DEBUG
            debug << ";move;" << fu.x << ";" << fu.y;
#endif

            didSomething = true;
            return;
        }

#ifdef FAP_DEBUG
        debug << ";idle;" << fu.x << ";" << fu.y;
#endif
    }

    void FastAPproximation::medicsim(const FAPUnit &fu,
        std::vector<FAPUnit> &friendlyUnits) {
        auto closestHealable = friendlyUnits.end();
        int closestDist;

        for (auto it = friendlyUnits.begin(); it != friendlyUnits.end(); ++it) {
            if (it->isOrganic && it->health < it->maxHealth && !it->didHealThisFrame) {
                int d = distance(fu, *it);
                if (closestHealable == friendlyUnits.end() || d < closestDist) {
                    closestHealable = it;
                    closestDist = d;
                }
            }
        }

        if (closestHealable != friendlyUnits.end()) {
            fu.x = closestHealable->x;
            fu.y = closestHealable->y;

            closestHealable->health += 150;

            if (closestHealable->health > closestHealable->maxHealth)
                closestHealable->health = closestHealable->maxHealth;

            closestHealable->didHealThisFrame = true;
        }
    }

    bool FastAPproximation::suicideSim(const FAPUnit &fu,
        std::vector<FAPUnit> &enemyUnits) {
        auto closestEnemy = enemyUnits.end();
        int closestDist;

        for (auto enemyIt = enemyUnits.begin(); enemyIt != enemyUnits.end();
            ++enemyIt) {
            if (enemyIt->flying) {
                if (fu.airDamage) {
                    int d = distance(fu, *enemyIt);
                    if ((closestEnemy == enemyUnits.end() || d < closestDist) &&
                        d >= fu.airMinRange) {
                        closestDist = d;
                        closestEnemy = enemyIt;
                    }
                }
            }
            else {
                if (fu.groundDamage) {
                    int d = distance(fu, *enemyIt);
                    if ((closestEnemy == enemyUnits.end() || d < closestDist) &&
                        d >= fu.groundMinRange) {
                        closestDist = d;
                        closestEnemy = enemyIt;
                    }
                }
            }
        }

        if (closestEnemy != enemyUnits.end() && closestDist <= fu.speed) {
            if (closestEnemy->flying)
                dealDamage(*closestEnemy, fu.airDamage, fu.airDamageType);
            else
                dealDamage(*closestEnemy, fu.groundDamage, fu.groundDamageType);

            if (closestEnemy->health < 1) {
                auto temp = *closestEnemy;
                *closestEnemy = enemyUnits.back();
                enemyUnits.pop_back();
                unitDeath(temp, enemyUnits);
            }

            didSomething = true;
            return true;
        }
        else if (closestEnemy != enemyUnits.end() && closestDist > fu.speed) {
            int dx = closestEnemy->x - fu.x, dy = closestEnemy->y - fu.y;

            fu.x += (int)(dx * (fu.speed / sqrt(dx * dx + dy * dy)));
            fu.y += (int)(dy * (fu.speed / sqrt(dx * dx + dy * dy)));

            didSomething = true;
        }

        return false;
    }

    void FastAPproximation::isimulate() {
        for (auto fu = player1.begin(); fu != player1.end();) {
            if (isSuicideUnit(fu->unitType)) {
                bool result = suicideSim(*fu, player2);
                if (result)
                    fu = player1.erase(fu);
                else
                    ++fu;
            }
            else {
                if (fu->unitType == BWAPI::UnitTypes::Terran_Medic)
                    medicsim(*fu, player1);
                else
                    unitsim(*fu, player2);
                ++fu;
            }
        }

        for (auto fu = player2.begin(); fu != player2.end();) {
            if (isSuicideUnit(fu->unitType)) {
                bool result = suicideSim(*fu, player1);
                if (result)
                    fu = player2.erase(fu);
                else
                    ++fu;
            }
            else {
                if (fu->unitType == BWAPI::UnitTypes::Terran_Medic)
                    medicsim(*fu, player2);
                else
                    unitsim(*fu, player1);
                ++fu;
            }
        }

        for (auto &fu : player1) {
            if (fu.attackCooldownRemaining)
                --fu.attackCooldownRemaining;
            if (fu.didHealThisFrame)
                fu.didHealThisFrame = false;
            if (fu.unitType.getRace() == BWAPI::Races::Zerg) {
                if (fu.health < fu.maxHealth)
                    fu.health += 4;
                if (fu.health > fu.maxHealth)
                    fu.health = fu.maxHealth;
            }
            else if (fu.unitType.getRace() == BWAPI::Races::Protoss) {
                if (fu.shields < fu.maxShields)
                    fu.shields += 7;
                if (fu.shields > fu.maxShields)
                    fu.shields = fu.maxShields;
            }
        }

        for (auto &fu : player2) {
            if (fu.attackCooldownRemaining)
                --fu.attackCooldownRemaining;
            if (fu.didHealThisFrame)
                fu.didHealThisFrame = false;
            if (fu.unitType.getRace() == BWAPI::Races::Zerg) {
                if (fu.health < fu.maxHealth)
                    fu.health += 4;
                if (fu.health > fu.maxHealth)
                    fu.health = fu.maxHealth;
            }
            else if (fu.unitType.getRace() == BWAPI::Races::Protoss) {
                if (fu.shields < fu.maxShields)
                    fu.shields += 7;
                if (fu.shields > fu.maxShields)
                    fu.shields = fu.maxShields;
            }
        }
    }

    void FastAPproximation::unitDeath(const FAPUnit &fu,
        std::vector<FAPUnit> &itsFriendlies) {
        if (fu.unitType == BWAPI::UnitTypes::Terran_Bunker) {
            convertToUnitType(fu, BWAPI::UnitTypes::Terran_Marine);

            for (unsigned i = 0; i < 4; ++i)
                itsFriendlies.push_back(fu);
        }
    }

    void FastAPproximation::convertToUnitType(const FAPUnit &fu,
        BWAPI::UnitType ut) {
        UAlbertaBot::UnitInfo ui;
        ui.lastPosition = BWAPI::Position(fu.x, fu.y);
        ui.player = fu.player;
        ui.type = ut;

        FAPUnit funew(ui);
        funew.attackCooldownRemaining = fu.attackCooldownRemaining;
        funew.elevation = fu.elevation;

        fu.operator=(funew);
    }

    FastAPproximation::FAPUnit::FAPUnit(BWAPI::Unit u) : FAPUnit(UnitInfo(u)) {}

    FastAPproximation::FAPUnit::FAPUnit(UnitInfo ui)
        : x(ui.lastPosition.x), y(ui.lastPosition.y),

        speed(InformationManager::Instance().getUnitTopSpeed(ui.player, ui.type)),

        health(ui.lastHealth),
        maxHealth(ui.type.maxHitPoints()),

        shields(ui.lastShields),
        shieldArmor(ui.player->getUpgradeLevel(BWAPI::UpgradeTypes::Protoss_Plasma_Shields)),
        maxShields(ui.type.maxShields()),
        armor(InformationManager::Instance().getUnitArmor(ui.player, ui.type)),
        flying(ui.type.isFlyer()),

        groundDamage(InformationManager::Instance().getWeaponDamage(ui.player, ui.type.groundWeapon())),
        groundCooldown(ui.type.groundWeapon().damageFactor() && ui.type.maxGroundHits() ? InformationManager::Instance().getUnitCooldown(ui.player, ui.type) / (ui.type.groundWeapon().damageFactor() * ui.type.maxGroundHits()) : 0),
        groundMaxRange(InformationManager::Instance().getWeaponRange(ui.player, ui.type.groundWeapon())),
        groundMinRange(ui.type.groundWeapon().minRange()),
        groundDamageType(ui.type.groundWeapon().damageType()),

        airDamage(InformationManager::Instance().getWeaponDamage(ui.player, ui.type.airWeapon())),
        airCooldown(ui.type.airWeapon().damageFactor() && ui.type.maxAirHits() ? ui.type.airWeapon().damageCooldown() / (ui.type.airWeapon().damageFactor() * ui.type.maxAirHits()) : 0),
        airMaxRange(InformationManager::Instance().getWeaponRange(ui.player, ui.type.airWeapon())),
        airMinRange(ui.type.airWeapon().minRange()),
        airDamageType(ui.type.airWeapon().damageType()),

        attackCooldownRemaining(std::max(0, ui.groundWeaponCooldownFrame - BWAPI::Broodwar->getFrameCount())),

        unitType(ui.type),
        isOrganic(ui.type.isOrganic()),
        score(ui.type.destroyScore()),
        player(ui.player)
    {

        static int nextId = 0;
        id = nextId++;

        if (ui.type == BWAPI::UnitTypes::Protoss_Carrier)
        {
            groundDamage = InformationManager::Instance().getWeaponDamage(ui.player, 
                BWAPI::UnitTypes::Protoss_Interceptor.groundWeapon());

            if (ui.unit && ui.unit->isVisible()) {
                auto interceptorCount = ui.unit->getInterceptorCount();
                if (interceptorCount) {
                    groundCooldown = (int)round(37.0f / interceptorCount);
                }
                else {
                    groundDamage = 0;
                    groundCooldown = 5;
                }
            }
            else {
                if (ui.player) {
                    groundCooldown =
                        (int)round(37.0f / (ui.player->getUpgradeLevel(
                            BWAPI::UpgradeTypes::Carrier_Capacity)
                            ? 8
                            : 4));
                }
                else {
                    groundCooldown = (int)round(37.0f / 8);
                }
            }

            groundDamageType =
                BWAPI::UnitTypes::Protoss_Interceptor.groundWeapon().damageType();
            groundMaxRange = 32 * 8;

            airDamage = groundDamage;
            airDamageType = groundDamageType;
            airCooldown = groundCooldown;
            airMaxRange = groundMaxRange;
        } 
        else if (ui.type == BWAPI::UnitTypes::Terran_Bunker)
        {
            groundDamage = InformationManager::Instance().getWeaponDamage(ui.player, BWAPI::WeaponTypes::Gauss_Rifle);
            groundCooldown =
                BWAPI::UnitTypes::Terran_Marine.groundWeapon().damageCooldown() / 4;
            groundMaxRange = InformationManager::Instance().getWeaponRange(ui.player, 
                BWAPI::UnitTypes::Terran_Marine.groundWeapon()) +
                32;

            airDamage = groundDamage;
            airCooldown = groundCooldown;
            airMaxRange = groundMaxRange;

            // Good enemies repair their bunkers, so fudge this a bit by giving the bunker more health
            // TODO: Actually simulate the repair
            health *= 2;
            maxHealth *= 2;
        }
        else if (ui.type == BWAPI::UnitTypes::Protoss_Reaver)
        {
            groundDamage = InformationManager::Instance().getWeaponDamage(ui.player, BWAPI::WeaponTypes::Scarab);
        }
        // Destroy score is not a good value measurement for static ground defense, so set them manually
        else if (ui.type == BWAPI::UnitTypes::Protoss_Photon_Cannon)
        {
            score = 750; // approximate at 1.5 dragoons
        }
        else if (ui.type == BWAPI::UnitTypes::Zerg_Sunken_Colony)
        {
            score = 1000; // approximate at 2 dragoons
        }

        if (ui.unit && ui.unit->isStimmed()) {
            groundCooldown /= 2;
            airCooldown /= 2;
        }

        elevation = BWAPI::Broodwar->getGroundHeight(BWAPI::TilePosition(ui.lastPosition));

        //groundMaxRange *= groundMaxRange;
        //groundMinRange *= groundMinRange;
        //airMaxRange *= airMaxRange;
        //airMinRange *= airMinRange;

        health <<= 8;
        maxHealth <<= 8;
        shields <<= 8;
        maxShields <<= 8;
    }

    const FastAPproximation::FAPUnit &FastAPproximation::FAPUnit::
        operator=(const FAPUnit &other) const {
        x = other.x, y = other.y;
        health = other.health, maxHealth = other.maxHealth;
        shields = other.shields, maxShields = other.maxShields;
        speed = other.speed, armor = other.armor, flying = other.flying,
            unitSize = other.unitSize;
        groundDamage = other.groundDamage, groundCooldown = other.groundCooldown,
            groundMaxRange = other.groundMaxRange, groundMinRange = other.groundMinRange,
            groundDamageType = other.groundDamageType;
        airDamage = other.airDamage, airCooldown = other.airCooldown,
            airMaxRange = other.airMaxRange, airMinRange = other.airMinRange,
            airDamageType = other.airDamageType;
        score = other.score;
        attackCooldownRemaining = other.attackCooldownRemaining;
        unitType = other.unitType;
        isOrganic = other.isOrganic;
        didHealThisFrame = other.didHealThisFrame;
        elevation = other.elevation;
        player = other.player;

        return *this;
    }

    bool FastAPproximation::FAPUnit::operator<(const FAPUnit &other) const {
        return id < other.id;
    }

}
