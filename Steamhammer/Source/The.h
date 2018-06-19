#pragma once

#include "MapPartitions.h"

namespace UAlbertaBot
{
	class The
	{
	public:
		The();
		void initialize();

		MapPartitions partitions;

		static The & Root();
	};
}
