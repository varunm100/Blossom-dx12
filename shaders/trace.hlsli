#pragma once

struct CameraTransforms
{
    float4x4 view;
    float4x4 proj;
    float4x4 view_inverse;
    float4x4 proj_inverse;
};

namespace Trace
{

    RayDesc get_camera_ray(in uint camera_buffer_index)
    {
        float2 uv = (float2) DispatchRaysIndex() / (float2) DispatchRaysDimensions();
        uv = uv * 2. - 1.;
        uv.y *= -1;

        ByteAddressBuffer cam_buffer = ResourceDescriptorHeap[camera_buffer_index];
        CameraTransforms cam_trans = cam_buffer.Load < CameraTransforms > (0);
        float3 origin = mul(cam_trans.view_inverse, float4(0, 0, 0, 1)).xyz;
        float3 target = normalize(mul(cam_trans.proj_inverse, float4(uv.x, uv.y, 1, 1)).xyz);
        float3 dir = mul(cam_trans.view_inverse, float4(target, 0)).xyz;

        RayDesc ray;
        ray.Origin = origin;
        ray.Direction = dir;
        ray.TMin = 1e-9;
        ray.TMax = 1e9;
        return ray;
    }
}
