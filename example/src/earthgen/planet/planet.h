#ifndef planet_h
#define planet_h

#include "grid/grid.h"
#include "terrain/terrain.h"
#include "climate/climate.h"

namespace earthgen {
	class Planet {
	public:
		Planet();
		~Planet();

		Grid* grid;
		Terrain* terrain;
		Climate* climate;
	};

	void clear(Planet&);
}
#endif
