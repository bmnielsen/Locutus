#pragma once

#include "UnitData.h"

//#define FAP_DEBUG 1

namespace DaQinBot {

    struct FastAPproximation {
        struct FAPUnit {
            FAPUnit(BWAPI::Unit u);
            FAPUnit(UnitInfo ui);
            const FAPUnit &operator=(const FAPUnit &other) const;

            int id = 0;

            mutable int x = 0, y = 0;

            mutable int health = 0;
            mutable int maxHealth = 0;
            mutable int armor = 0;

            mutable int shields = 0;
            mutable int shieldArmor = 0;
            mutable int maxShields = 0;

            mutable double speed = 0;
            mutable bool flying = 0;
            mutable int elevation = -1;

            mutable bool undetected = false;

            mutable BWAPI::UnitSizeType unitSize;

            mutable int groundDamage = 0;
            mutable int groundCooldown = 0;
            mutable int groundMaxRange = 0;
            mutable int groundMinRange = 0;
            mutable BWAPI::DamageType groundDamageType;

            mutable int airDamage = 0;
            mutable int airCooldown = 0;
            mutable int airMaxRange = 0;
            mutable int airMinRange = 0;
            mutable BWAPI::DamageType airDamageType;

            mutable BWAPI::UnitType unitType;
            mutable BWAPI::Player player = nullptr;
            mutable bool isOrganic = false;
            mutable bool didHealThisFrame = false;
            mutable int score = 0;

            mutable int attackCooldownRemaining = 0;

            bool operator<(const FAPUnit &other) const;
        };

        FastAPproximation();

        void addUnitPlayer1(FAPUnit fu);
        void addIfCombatUnitPlayer1(FAPUnit fu);
        void addUnitPlayer2(FAPUnit fu);
        void addIfCombatUnitPlayer2(FAPUnit fu);

        void simulate(int nFrames = 96); // = 24*4, 4 seconds on fastest

        std::pair<int, int> playerScores() const;
        std::pair<int, int> playerScoresUnits() const;
        std::pair<int, int> playerScoresBuildings() const;
        std::pair<std::vector<FAPUnit> *, std::vector<FAPUnit> *> getState();
        void clearState();

    private:
#ifdef FAP_DEBUG
        std::ofstream debug;
#endif

        std::vector<FAPUnit> player1, player2;

        // Current approach to collisions: allow two units to share the same grid cell, using half-tile resolution
        // This seems to strike a reasonable balance between improving how large melee armies are simmed and avoiding
        // expensive collision-based pathing calculations
        unsigned short collision[512][512] = {};

        int frame;
        bool didSomething;
        void dealDamage(const FastAPproximation::FAPUnit &fu, int damage,
            BWAPI::DamageType damageType) const;
        int distance(const FastAPproximation::FAPUnit &u1,
            const FastAPproximation::FAPUnit &u2) const;
        void updatePosition(const FAPUnit &fu, int x, int y);
        bool isSuicideUnit(BWAPI::UnitType ut);
        void unitsim(const FAPUnit &fu, std::vector<FAPUnit> &enemyUnits);
        void medicsim(const FAPUnit &fu, std::vector<FAPUnit> &friendlyUnits);
        bool suicideSim(const FAPUnit &fu, std::vector<FAPUnit> &enemyUnits);
        void isimulate();
        void unitDeath(const FAPUnit &fu, std::vector<FAPUnit> &itsFriendlies);
        void convertToUnitType(const FAPUnit &fu, BWAPI::UnitType ut);
        };

}

extern DaQinBot::FastAPproximation fap;