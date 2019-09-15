#pragma once

namespace UAlbertaBot
{

class Region
{
private:
	int	id;

public:

	Region();

	void draw() const;
};

// A choke area is a kind of region.
class Choke : Region
{
private:
	int width;
public:
};

}
