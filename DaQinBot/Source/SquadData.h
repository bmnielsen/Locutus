#pragma once

#include "Squad.h"

namespace DaQinBot
{
class SquadData
{
	std::map<std::string, Squad> _squads;

    void    updateAllSquads();
    void    verifySquadUniqueMembership();

public:

	SquadData();

    void            clearSquadData();

    bool            canAssignUnitToSquad(BWAPI::Unit unit, const Squad & squad) const;
    void            assignUnitToSquad(BWAPI::Unit unit, Squad & squad);
    void            addSquad(const Squad & squad);
    void            removeSquad(const std::string & squadName);
    void            clearSquad(const std::string & squadName);
	void            drawSquadInformation(int x, int y);

    void            update();
    void            setRegroup();

    bool            squadExists(const std::string & squadName) const;
    bool            unitIsInSquad(BWAPI::Unit unit) const;
    const Squad *   getUnitSquad(BWAPI::Unit unit) const;
    Squad *         getUnitSquad(BWAPI::Unit unit);

    Squad &         getSquad(const std::string & squadName);
    const Squad &   getSquad(const std::string & squadName) const;
    const std::map<std::string, Squad> & getSquads() const;
    Squad &         getSquad(const MicroManager * microManager);
};
}