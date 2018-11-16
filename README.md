This branch contains the source for ISAMind, a Locutus fork, taken from the AIIDE 2018 sources. I haven't included the opencv dependencies as the intent here is just to see what the differences are between Locutus and ISAMind.


# ISAMind
An AI for StarCraft: Brood War, designed by Gao Fang and Wu Yuhang
- Race: Protoss
- DLL or Client: DLL
- BWAPI Version: 4.1.2
- File I/O: Require

## Description
ISAMind is a Protoss bot forked from [Locutus](https://github.com/bmnielsen/Locutus). It uses an enemy strategy prediction neural network to recognize enemy plan.

## Build
### Requirement
- Visual Studio 2013
- [BWAPI 4.1.2](https://github.com/bwapi/bwapi/releases/tag/v4.1.2)
- [BWTA 2.2](https://bitbucket.org/auriarte/bwta2/downloads)
- [opencv 2.4.11 windows source code pack](https://sourceforge.net/projects/opencvlibrary/files/opencv-win/2.4.11/opencv-2.4.11.exe/download). (We have already placed necessary cv libraries in [cv](/cv) for ISAMind compiling, no need to build them again)

### Preparatory Work
- Build opencv static library in **/MD** mode. Because the official prebuild binaries were built in MT mode, we have to rebuild them. You can also download the compiled library file from [cvlib](https://github.com/leonfg/leonfg.github.io/tree/master/ISAMind/cv/lib).
- Copy opencv_core2411.lib, opencv_ml2411.lib and zlib.lib to [cv](/cv) folder.
- Follow **Prerequisites** of [ualbertabot Installation Instructions](https://github.com/davechurchill/ualbertabot/wiki/Installation-Instructions#prerequisites).

### Build Operation
1. Open [UAlbertaBot.sln](/Steamhammer/VisualStudio/UAlbertaBot.sln) in Visual Studio 2013.
2. Select **Release** mode
3. Build the UAlbertaBot solution (all projects will be built)
4. Compiled ISAMind.dll will go to the [bin](/Steamhammer/bin) directory
5. Copy ISAMind.dll, [ISAMind.json](/ISAMind.json) and [ISAMind.xml](/ISAMind.xml) to StarCraft/bwapi-data/AI/