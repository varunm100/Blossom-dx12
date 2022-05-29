struct HitInfo
{
    float4 color;
};

[shader("closesthit")]
void ClosestHit(inout HitInfo payload : SV_RayPayload, in BuiltInTriangleIntersectionAttributes attrib)
{
    payload.color = float4(attrib.barycentrics.x, attrib.barycentrics.y, 0, 1);
}

[shader("miss")]
void Miss(inout HitInfo payload : SV_RayPayload)
{
    payload.color = float4(0.5, 0.5, 0.5, -1);
}

struct DrawConstants
{
    uint tlas;
    uint output_img;
};
ConstantBuffer<DrawConstants> DrawConsts : register(b0, space0);

[shader("raygeneration")]
void RayGen()
{
    RaytracingAccelerationStructure tlas = ResourceDescriptorHeap[DrawConsts.tlas];
    RWTexture2D<float4> output = ResourceDescriptorHeap[DrawConsts.output_img];

    float2 uv = (float2)DispatchRaysIndex() / (float2)DispatchRaysDimensions();
    uv = uv * 2. - 1.;
    uv.y *= -1;

    RayDesc ray;
    ray.Origin = float3(0., 0., -1.5);
    ray.Direction = normalize(float3(uv.x, uv.y, 1.0) - ray.Origin);
    //ray.Direction = float3(0., 0., -1.);
    ray.TMin = 1e-9;
    ray.TMax = 1e9;
    HitInfo payload;
    TraceRay(tlas, RAY_FLAG_NONE, ~0, 0, 1, 0, ray, payload);
    output[DispatchRaysIndex().xy] = float4(payload.color.rgb, 1.f);
}

