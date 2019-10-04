#pragma once
#include <BWAPI.h>
namespace BWTA
{
	class Region;
	class BaseLocation
	{
	public:
		virtual ~BaseLocation(){};
		virtual BWAPI::Position getPosition() const = 0;
		virtual BWAPI::TilePosition getTilePosition() const = 0;

		virtual Region* getRegion() const = 0;

		virtual int minerals() const = 0;
		virtual int gas() const = 0;

		virtual const BWAPI::Unitset &getMinerals() = 0;
		virtual const BWAPI::Unitset &getStaticMinerals() const = 0;
		virtual const BWAPI::Unitset &getGeysers() const = 0;

		virtual double getGroundDistance(BaseLocation* other) const = 0;
		virtual double getAirDistance(BaseLocation* other) const = 0;

		virtual bool isIsland() const = 0;
		virtual bool isMineralOnly() const = 0;
		virtual bool isStartLocation() const = 0;
	};
}