#include  "shaders/trace.hlsli"

struct HitInfo
{
    float4 color;
};

struct VoxelVolumeAttr
{
    float3 color;
};

[shader("closesthit")]
void ClosestHitTriangle(inout HitInfo payload : SV_RayPayload, in BuiltInTriangleIntersectionAttributes attrib)
{
    payload.color = float4(attrib.barycentrics.x, attrib.barycentrics.y, 0, 1);
}

[shader("closesthit")]
void ClosestHitVoxelVolume(inout HitInfo payload : SV_RayPayload, in VoxelVolumeAttr attr) {
    payload.color = float4(attr.color, 1.);
}

[shader("miss")]
void Miss(inout HitInfo payload : SV_RayPayload)
{
    payload.color = float4(0.5, 0.5, 0.5, -1);
}

[shader("intersection")]
void IntersectVoxelVolume() {
    VoxelVolumeAttr attr;
    attr.color = float3(0,1,0);
    ReportHit(1., 1, attr);
}

struct DrawConstants
{
    uint tlas;
    uint output_img;
    uint camera_buffer;
};
ConstantBuffer<DrawConstants> DrawConsts : register(b0, space0);

[shader("raygeneration")]
void RayGen()
{
    RaytracingAccelerationStructure tlas = ResourceDescriptorHeap[DrawConsts.tlas];
    RWTexture2D<float4> output = ResourceDescriptorHeap[DrawConsts.output_img];

    RayDesc ray = Trace::get_camera_ray(DrawConsts.camera_buffer);
    HitInfo payload;
    TraceRay(tlas, RAY_FLAG_NONE, ~0, 0, 1, 0, ray, payload);
    output[DispatchRaysIndex().xy] = float4(payload.color.rgb, 1.f);
}

