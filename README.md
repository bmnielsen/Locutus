# Locutus
AI for StarCraft: Brood War

## About
Locutus is a Protoss bot forked from [Steamhammer](http://satirist.org/ai/starcraft/steamhammer/). It uses [BWEB](https://github.com/Cmccrave/BWEB) for building placement, which depends on [BWEM](http://bwem.sourceforge.net/).

## BWTA
Locutus inherited the BWTA map analysis library from Steamhammer and UAlbertaBot. It is an older library with some difficult-to-build dependencies, so it has typically blocked bot authors from upgrading to newer versions of BWAPI and Visual Studio.

Until such time as the bot is refactored to no longer use BWTA, I have therefore replaced the BWTA dependency with a stripped-down version lacking all of the map analysis features (and therefore not requiring the awkward dependencies). This allowed me to upgrade the bot to BWAPI 4.4 and the Visual Studio 2017 toolchain, but with the side-effect that the bot no longer works if the BWTA cache files are not available for the map being loaded.

Cache files for all maps used in AI tournaments are generally available for download from the tournament websites. Another option is to run a previous version of Locutus to get the map analysis file, which the new version will be able to load.

## License

Versions of Locutus up to and including the version submitted to the AIIDE StarCraft tournament in 2018 were licensed under the MIT license.

In said AIIDE tournament, four relatively "shallow" forks of Locutus participated (of which two were eventually disqualified for being too similar to Locutus), which caused me to reconsider using such a permissive license. It doesn't do anyone any good to have multiple virtually-identical copies of a bot participating in a tournament, and I don't think it should only be the burden of the tournament organizers to make the judgement of what is "different enough".

As a result of this, Locutus is now licensed under a modified version of the MIT license that adds an additional restriction: you may not submit bots based on Locutus to public StarCraft tournaments without my permission.

How exactly this will work in practice is not entirely known (let's cross that bridge if we get there), but if you are interested in forking Locutus and using it in a tournament, create an issue here or message me on Discord and we'll talk.