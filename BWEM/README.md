# BWEM-community

[![Build status](https://ci.appveyor.com/api/projects/status/6do734d9cdi31vvl/branch/master?svg=true)](https://ci.appveyor.com/project/N00byEdge/bwem-community/branch/master)

Brood War Easy Map is a C++ library that analyses Brood War's maps and provides relevant information such as areas, choke points and bases. It is built on top of the BWAPI library (http://bwapi.github.io/index.html). It first aims at simplifying the development of bots for Brood War, but can be used for any task requiring high level map information.

BWEM-community is based on BWEM by Igor Dimitrijevic. It is a community maintained version of BWEM, containing fixes and changes by the community. Original BWEM can be found [here](http://bwem.sourceforge.net/).

## Prerequisites

On Windows

- Visual Studio 2017
	- C++ workload installed
	- Windows XP support for C++ (from individual components tab)
	- Microsoft Test Adapter for Google Test (for running tests inside VS)
	- CMake support (by preference)

- Starcraft installed or just 3 MPQ files from original Starcraft `StarDat.mpq`, `BrooDat.mpq`, `Patch_rt.mpq`

## Development

Set environment variable `OPENBW_MPQ_PATH` to point to location of Starcraft, or folder where Starcraft files `StarDat.mpq`, `BrooDat.mpq`, `Patch_rt.mpq` are located.

### Using command line

	git clone https://github.com/N00byEdge/BWEM-community BWEM --recursive
	cd BWEM
	mkdir build
	cd build
	cmake ..

### Using Visual Studio

	git clone https://github.com/N00byEdge/BWEM-community BWEM --recursive
	./BWEM/BWEM.sln

or alternatively, if you want use CMake

	git clone https://github.com/N00byEdge/BWEM-community BWEM --recursive

then use `File -> Open -> CMake` to open BWEM folder where project is cloned.
