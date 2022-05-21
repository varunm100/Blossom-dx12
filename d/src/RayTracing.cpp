#include "d/RayTracing.h"	


auto d::BlasBuilderStream::add_triangles() -> BlasBuilderStream {
	auto geometry_desc = D3D12_RAYTRACING_GEOMETRY_DESC{
		.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES,
		.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE,
		.Triangles = D3D12_RAYTRACING_GEOMETRY_TRIANGLES_DESC {
			// fill in shit lmao
		},
	};
	return *this;
}

//auto d::BlasBuilderStream::build() -> Resource<AccelStructure> {
//	return Resource<AccelStructure
//}
