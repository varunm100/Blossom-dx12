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
    uint camera_buffer;
};
ConstantBuffer<DrawConstants> DrawConsts : register(b0, space0);
struct CameraTransforms
{
    float4x4 view;
    float4x4 proj;
    float4x4 view_inverse;
    float4x4 proj_inverse;
};

[shader("raygeneration")]
void RayGen()
{
    RaytracingAccelerationStructure tlas = ResourceDescriptorHeap[DrawConsts.tlas];
    RWTexture2D<float4> output = ResourceDescriptorHeap[DrawConsts.output_img];

    float2 uv = (float2)DispatchRaysIndex() / (float2)DispatchRaysDimensions();
    uv = uv * 2. - 1.;
    uv.y *= -1;

	ByteAddressBuffer cam_buffer = ResourceDescriptorHeap[DrawConsts.camera_buffer];
    CameraTransforms cam_trans = cam_buffer.Load < CameraTransforms > (0);
    float3 origin = mul(cam_trans.view_inverse, float4(0, 0, 0, 1)).xyz;
    float3 target = normalize(mul(cam_trans.proj_inverse, float4(uv.x, uv.y, 1, 1)).xyz);
    float3 dir = mul(cam_trans.view_inverse, float4(target, 0)).xyz;

    RayDesc ray;
    ray.Origin = origin;
    ray.Direction = dir;
    ray.TMin = 1e-9;
    ray.TMax = 1e9;
    HitInfo payload;
    TraceRay(tlas, RAY_FLAG_NONE, ~0, 0, 1, 0, ray, payload);
    output[DispatchRaysIndex().xy] = float4(payload.color.rgb, 1.f);
}

