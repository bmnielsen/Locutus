# BWEB
## BWEB is currently v1.04
Broodwar Easy Builder or BWEB for short, is a BWEM based building placement addon. The purpose of this addon is to provide easily accesible building management. BWEB started as a decision to create a standard and simple method for bots to optimize their building space and placement.

## What's New?
Wall placement is faster and better, Hannes Bredburg helped me implement his BFS/A* algorithm. Placement of Pylons and defenses is much better and placement of the door is done based on adjacent tiles touching a wall, rather than being a part of the chokepoints geometry. Added a new Block to Protoss that is 8 gates and 1 pylon for the extra greedy production players like myself.

## What does BWEB do?
BWEB has 3 classes of information, Walls, Blocks and Stations.

Walls are made by permutating through a vector of UnitTypes in as many combinations as possible and measuring an A* path length to find a wall that is either wall tight or minimizes enemy movement. Walls can be created using any UnitTypes and have optional parameters to pass in; what UnitType you want to be wall tight against, a vector of UnitTypes for defenses, or a reserve path can be created to ensure your units can leave the BWEM::Area that the Wall is being created in.

Blocks are used for all building types and are modifiable for any race and build. Blocks should be generated in your onStart and take only a few milliseconds to create. Blocks are useful for maximizing your space for production buildings without trapping units, while providing the ability to create specific areas to hide tech from your opponents scout.

Stations are placed on every BWEM::Base and include defense positions that provide coverage for all your workers.

## Why use BWEB?
Building placement is a very important aspect of Broodwar. Decisions such as hiding tech, walling a choke or finding more optimal use of your space are possible using BWEB. Most Broodwar bots suffer from many issues stemming from building placement, such as; timeouts, building where it's not safe, trapping units, and lack of fast expand options due to poor wall placement.

## How do I install BWEB?
1) Clone the repository or download a copy.
2) In your source code, create a BWEB folder and store the files in there.
3) In Visual Studio, add the files to your project and edit the properties of your projects include directory to include BWEBs folder.

## How do I use BWEB?

In your main header file, you will need to include the BWEB header file.

```
#include "BWEB.h"
```

There is a singleton instance accessor function that allows you to access BWEB easily, this will also go in your main header file.

```
namespace { auto & mapBWEB = BWEB::Map::Instance(); }

```

You will need to put the onStart function into your onStart event after BWEM initialization. It is very important to note that finding blocks is not done within BWEBs onStart function anymore, this was a design choice as you may have other logic in your code to decide a type of Wall you want to choose. By generating the Blocks after you generate a Wall, there is no need for erasing overlapping Blocks and no issues generating the Wall. This also allows people to take advantage of BWEBs walling functionality if they don't want to use the Blocks.

``` 
void McRaveModule::onStart()
  mapBWEB.onStart();
  mapBWEB.findBlocks();
```
You will need to put onDiscover, onMorph and onDestroy from BWEB into their respective events in your code as well if you wish to take advantage of how BWEB handles TilePositions that are used. If you use BWEBs getBuildLocation function, it is necessary to include these.

``` 
void McRaveModule::onUnitDiscover(Unit unit)
  mapBWEB.onUnitDiscover(unit);

void McRaveModule::onUnitDestroy(Unit unit)
  mapBWEB.onUnitDestroy(unit);

void McRaveModule::onUnitMorph(Unit unit)
  mapBWEB.onUnitMorph(unit);

```

(Optional) To draw the Blocks, Walls and Stations, you will need to put the draw function into your onFrame event.

```
void McRaveModule::onFrame()
  mapBWEB.draw();
```

All other BWEB functions have full comments describing their use and what parameters are required or optional. GL HF!
If you have any questions, feel free to ask on BWAPI Discord.
