# BWTA2 #

BWTA2 is a fork of [BWTA](https://code.google.com/p/bwta), an add-on for [BWAPI](https://github.com/bwapi/bwapi) that analyzes the map and computes the regions, chokepoints, and base locations. 
The original BWTA is only aimed to analyze "normal" maps, i.e. ICCup maps or other competitive starcraft maps. So don't be surprised if it crashes or produces strange results for maps like [Crystallis](http://classic.battle.net/images/battle/scc/lp/bw02/cy.jpg) or other [money maps](http://starcraft.wikia.com/wiki/Money_maps).

BWTA2 offers more functionalities (see **[Release Notes](https://bitbucket.org/auriarte/bwta2/wiki/Release%20Notes)**) and compatibility with BWAPI 4.

**[Download](https://bitbucket.org/auriarte/bwta2/downloads)** the last version and follow the **[starting guide](https://bitbucket.org/auriarte/bwta2/wiki/Getting%20Started)**

## Information for developers ##

CGAL library is used to create the [Segment Delaunay Graph](http://doc.cgal.org/latest/Segment_Delaunay_graph_2/index.html). [How to install](http://www.cgal.org/windows_installation.html) (only if you need to compile the project).

There are two different branches:

* **master**: It has the project in VS2013 and it is under maintenance.
* **vs2008**: It is the old BWTA (VS2008) with some extra features (not planning to update anymore). 

## Dependencies ##
* **master**
    * BWAPI 4.1.0
    * Boost 1.56
    * CGAL 4.4
    * Qt 5.3.0 (only for debugging)
    * StormLib (only for off-line map analysis)
* **vs2008**
    * BWAPI 3.7.4
    * Boost 1.40
    * CGAL 3.5
    * Qt 4.6.3 (only for debugging)