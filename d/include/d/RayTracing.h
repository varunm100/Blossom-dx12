#pragma once

#include "d/stdafx.h"
#include "d/Types.h"
#include "d/Resource.h"

namespace d {
	struct BlasBuilderStream {
		std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> geometries;

		auto add_triangles() -> BlasBuilderStream;
		auto build()->Resource<AccelStructure>;
	};
}

