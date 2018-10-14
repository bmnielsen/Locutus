#pragma once

#include "MapPartitions.h"
#include "Micro.h"
#include "OpsBoss.h"

namespace UAlbertaBot
{
	class The
	{
	public:
		The();
		void initialize();

		OpsBoss ops;

		MapPartitions partitions;

		Micro micro;

		static The & Root();
	};
}
