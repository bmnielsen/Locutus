#pragma once

#include "Common.h"

namespace UAlbertaBot
{
	// Struct used when generating and scoring all of the forge + gateway options
	struct ForgeGatewayWallOption
	{
		BWAPI::TilePosition forge;
		BWAPI::TilePosition gateway;

		int gapSize;
		BWAPI::Position gapCenter;
		BWAPI::Position gapEnd1;
		BWAPI::Position gapEnd2;

		// Constructor for a valid option
		ForgeGatewayWallOption(BWAPI::TilePosition _forge, BWAPI::TilePosition _gateway, int _gapSize, BWAPI::Position _gapCenter, BWAPI::Position _gapEnd1, BWAPI::Position _gapEnd2)
			: forge(_forge)
			, gateway(_gateway)
			, gapSize(_gapSize)
			, gapCenter(_gapCenter)
			, gapEnd1(_gapEnd1)
			, gapEnd2(_gapEnd2)
		{};

		// Constructor for an invalid option
		ForgeGatewayWallOption(BWAPI::TilePosition _forge, BWAPI::TilePosition _gateway)
			: forge(_forge)
			, gateway(_gateway)
			, gapSize(INT_MAX)
			, gapCenter(BWAPI::Positions::Invalid)
			, gapEnd1(BWAPI::Positions::Invalid)
			, gapEnd2(BWAPI::Positions::Invalid)
		{};

		// Default constructor
		ForgeGatewayWallOption()
			: forge(BWAPI::TilePositions::Invalid)
			, gateway(BWAPI::TilePositions::Invalid)
			, gapSize(INT_MAX)
			, gapCenter(BWAPI::Positions::Invalid)
			, gapEnd1(BWAPI::Positions::Invalid)
			, gapEnd2(BWAPI::Positions::Invalid)
		{};
	};

	struct LocutusWall
	{
		BWAPI::TilePosition forge;
		BWAPI::TilePosition gateway;
		BWAPI::TilePosition pylon;
		std::vector<BWAPI::TilePosition> cannons;

		int gapSize;
		BWAPI::Position gapCenter;
		BWAPI::Position gapEnd1;
		BWAPI::Position gapEnd2;

		std::set<BWAPI::TilePosition> tilesInsideWall;
		std::set<BWAPI::TilePosition> tilesOutsideWall;
		std::set<BWAPI::TilePosition> tilesOutsideButCloseToWall;

		LocutusWall()
			: forge(BWAPI::TilePositions::Invalid)
			, gateway(BWAPI::TilePositions::Invalid)
			, pylon(BWAPI::TilePositions::Invalid)
			, gapSize(INT_MAX)
			, gapCenter(BWAPI::Positions::Invalid)
			, gapEnd1(BWAPI::Positions::Invalid)
			, gapEnd2(BWAPI::Positions::Invalid)
		{};

		LocutusWall(ForgeGatewayWallOption wall)
			: forge(wall.forge)
			, gateway(wall.gateway)
			, pylon(BWAPI::TilePositions::Invalid)
			, gapSize(wall.gapSize)
			, gapCenter(wall.gapCenter)
			, gapEnd1(wall.gapEnd1)
			, gapEnd2(wall.gapEnd2)
		{};

		bool isValid() const
		{
			return pylon.isValid() && forge.isValid() && gateway.isValid();
		}

        bool exists() const
        {
            return isValid() && (
                BWEB::Map::Instance().usedTiles.find(pylon) != BWEB::Map::Instance().usedTiles.end() ||
                BWEB::Map::Instance().usedTiles.find(forge) != BWEB::Map::Instance().usedTiles.end() ||
                BWEB::Map::Instance().usedTiles.find(gateway) != BWEB::Map::Instance().usedTiles.end());
        }

        bool containsBuildingAt(BWAPI::TilePosition tile) const
        {
            if (!exists()) return false;
            if (pylon == tile || forge == tile || gateway == tile) return true;
            for (auto const & cannon : cannons)
                if (cannon == tile) return true;
            
            return false;
        }

		std::vector<std::pair<BWAPI::UnitType, BWAPI::TilePosition>> placements() const
		{
			std::vector<std::pair<BWAPI::UnitType, BWAPI::TilePosition>> result;
			if (!isValid()) return result;

			result.push_back(std::make_pair(BWAPI::UnitTypes::Protoss_Pylon, pylon));
			result.push_back(std::make_pair(BWAPI::UnitTypes::Protoss_Forge, forge));
			result.push_back(std::make_pair(BWAPI::UnitTypes::Protoss_Gateway, gateway));
			for (auto const & tile : cannons)
				result.push_back(std::make_pair(BWAPI::UnitTypes::Protoss_Photon_Cannon, tile));

			return result;
		}

		friend std::ostream& operator << (std::ostream& out, const LocutusWall& wall)
		{
			if (!wall.isValid())
				out << "invalid";
			else
			{
				out << "pylon@" << wall.pylon << ";forge@" << wall.forge << ";gate@" << wall.gateway << "cannons@";
				for (auto const & tile : wall.cannons)
					out << tile;
			}
			return out;
		};

		static LocutusWall CreateForgeGatewayWall(bool tight = true);
	};
}