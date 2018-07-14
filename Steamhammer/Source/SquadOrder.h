#pragma once

#include "Common.h"

namespace UAlbertaBot
{
namespace SquadOrderTypes
{
    enum {
		None,
		Idle,			// workers, overlords with no other job
		Attack,			// go attack
		Defend,			// defend a base (automatically disbanded when enemy is gone)
		Hold,			// hold ground, stand ready to defend until needed
		Load,			// load into a transport (Drop squad)
		Drop,			// go drop on the enemy (Drop squad)
		DestroyNeutral,	// destroy neutral units by attack (e.g. destroy blocking buildings)
	};
}

class SquadOrder
{
    size_t              _type;
	BWAPI::Position     _position;
	int                 _radius;
    std::string         _status;

public:

	SquadOrder() 
		: _type(SquadOrderTypes::None)
        , _radius(0)
	{
	}

	SquadOrder(int type, BWAPI::Position position, int radius, std::string status = "Default") 
		: _type(type)
		, _position(position)
		, _radius(radius)
		, _status(status)
	{
	}

	const std::string & getStatus() const 
	{
		return _status;
	}

    const BWAPI::Position & getPosition() const
    {
        return _position;
    }

    const int & getRadius() const
    {
        return _radius;
    }

    const size_t & getType() const
    {
        return _type;
    }

	const char getCharCode() const
	{
		switch (_type)
		{
			case SquadOrderTypes::None:				return '-';
			case SquadOrderTypes::Idle:				return 'I';
			case SquadOrderTypes::Attack:			return 'a';
			case SquadOrderTypes::Defend:			return 'd';
			case SquadOrderTypes::Hold:				return 'H';
			case SquadOrderTypes::Load:				return 'L';
			case SquadOrderTypes::Drop:				return 'D';
			case SquadOrderTypes::DestroyNeutral:	return 'N';
		}
		return '?';
	}

	// These orders are considered combat orders and are linked to combat-related micro.
	bool isCombatOrder() const
	{
		return
			_type == SquadOrderTypes::Attack ||
			_type == SquadOrderTypes::Defend ||
			_type == SquadOrderTypes::Hold ||
			_type == SquadOrderTypes::Drop ||
			_type == SquadOrderTypes::DestroyNeutral;
	}

	// These orders use the regrouping mechanism to retreat when facing superior enemies.
	// Combat orders not in this group fight on against any odds.
	bool isRegroupableOrder() const
	{
		return
			_type == SquadOrderTypes::Attack ||
			_type == SquadOrderTypes::Defend ||
			_type == SquadOrderTypes::DestroyNeutral;
	}

};
}