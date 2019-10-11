#include "MathUtil.h"

using namespace DaQinBot;

//��Ե����Ե����
int MathUtil::EdgeToEdgeDistance(BWAPI::UnitType firstType, BWAPI::Position firstCenter, BWAPI::UnitType secondType, BWAPI::Position secondCenter)
{
    // Compute bounding boxes
    BWAPI::Position firstTopLeft = firstCenter + BWAPI::Position(-firstType.dimensionLeft(), -firstType.dimensionUp());
    BWAPI::Position firstBottomRight = firstCenter + BWAPI::Position(firstType.dimensionRight(), firstType.dimensionDown());
    BWAPI::Position secondTopLeft = secondCenter + BWAPI::Position(-secondType.dimensionLeft(), -secondType.dimensionUp());
    BWAPI::Position secondBottomRight = secondCenter + BWAPI::Position(secondType.dimensionRight(), secondType.dimensionDown());

    // Compute offsets
    int xDist = (std::max)({ firstTopLeft.x - secondBottomRight.x - 1, secondTopLeft.x - firstBottomRight.x - 1, 0 });
    int yDist = (std::max)({ firstTopLeft.y - secondBottomRight.y - 1, secondTopLeft.y - firstBottomRight.y - 1, 0 });

    // Compute distance
    return BWAPI::Positions::Origin.getApproxDistance(BWAPI::Position(xDist, yDist));
}

int MathUtil::EdgeToPointDistance(BWAPI::UnitType type, BWAPI::Position center, BWAPI::Position point)
{
    // Compute bounding box
    BWAPI::Position topLeft = center + BWAPI::Position(-type.dimensionLeft(), -type.dimensionUp());
    BWAPI::Position bottomRight = center + BWAPI::Position(type.dimensionRight(), type.dimensionDown());

    // Compute offsets
    int xDist = (std::max)({ topLeft.x - point.x - 1, point.x - bottomRight.x - 1, 0 });
    int yDist = (std::max)({ topLeft.y - point.y - 1, point.y - bottomRight.y - 1, 0 });

    // Compute distance
    return BWAPI::Positions::Origin.getApproxDistance(BWAPI::Position(xDist, yDist));
}

bool MathUtil::Overlaps(BWAPI::UnitType firstType, BWAPI::Position firstCenter, BWAPI::UnitType secondType, BWAPI::Position secondCenter)
{
	// Compute bounding boxes
	BWAPI::Position firstTopLeft = firstCenter + BWAPI::Position(-firstType.dimensionLeft(), -firstType.dimensionUp());
	BWAPI::Position firstBottomRight = firstCenter + BWAPI::Position(firstType.dimensionRight(), firstType.dimensionDown());
	BWAPI::Position secondTopLeft = secondCenter + BWAPI::Position(-secondType.dimensionLeft(), -secondType.dimensionUp());
	BWAPI::Position secondBottomRight = secondCenter + BWAPI::Position(secondType.dimensionRight(), secondType.dimensionDown());

	return firstBottomRight.x >= secondTopLeft.x && secondBottomRight.x >= firstTopLeft.x &&
		firstBottomRight.y >= secondTopLeft.y && secondBottomRight.y >= firstTopLeft.y;
}

bool MathUtil::Overlaps(BWAPI::UnitType type, BWAPI::Position center, BWAPI::Position point)
{
	// Compute bounding box
	BWAPI::Position topLeft = center + BWAPI::Position(-type.dimensionLeft(), -type.dimensionUp());
	BWAPI::Position bottomRight = center + BWAPI::Position(type.dimensionRight(), type.dimensionDown());

	return bottomRight.x >= point.x && point.x >= topLeft.x &&
		bottomRight.y >= point.y && point.y >= topLeft.y;
}

bool MathUtil::Walkable(BWAPI::UnitType type, BWAPI::Position center)
{
	for (int x = center.x - type.dimensionLeft(); x <= center.x + type.dimensionRight(); x++)
		for (int y = center.y - type.dimensionUp(); y <= center.y + type.dimensionDown(); y++)
			if (!BWAPI::Broodwar->isWalkable(x / 8, y / 8))
				return false;
	return true;
}

int MathUtil::DistanceFromPointToLine(BWAPI::Position linepoint1, BWAPI::Position linepoint2, BWAPI::Position targetpoint)
{
	int distance = 0;
	if (linepoint1.getDistance(linepoint2) == 0) distance = linepoint1.getDistance(targetpoint);
	else
	{
		int A = linepoint2.y - linepoint1.y;
		int B = linepoint1.x - linepoint2.x;
		int C = linepoint2.x * linepoint1.y - linepoint1.x * linepoint2.y;
		double qx = (double)(B*B*targetpoint.x - A*B*targetpoint.y - A*C) / (double)(A*A + B*B);
		bool qout = (linepoint1.x < linepoint2.x) ? (qx < linepoint1.x || qx > linepoint2.x) : (qx < linepoint2.x || qx > linepoint1.x);
		if (qout)
		{
			return linepoint2.getDistance(targetpoint);
		}
		distance = (int)(round(std::abs((A * targetpoint.x + B * targetpoint.y + C) / std::sqrt(A*A + B*B))));
	}
	return distance;
}