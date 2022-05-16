
struct DrawConstants
{
    uint vbo_index;
    float3 color1;
    float3 color2;
};
ConstantBuffer<DrawConstants> DrawConsts : register(b0, space0);

float4 VSMain(uint vert_id : SV_VertexID) : SV_Position {
    ByteAddressBuffer vbo = ResourceDescriptorHeap[DrawConsts.vbo_index];
    uint stride = sizeof(float3);
    float3 vert = vbo.Load <float3> ((0 + vert_id) * stride);

    return float4(vert, 1.);
}

float4 PSMain(float4 uv: SV_Position) : SV_TARGET {
    return uv.x/1280. > 0.5 ? float4(DrawConsts.color1, 1.0) : float4(DrawConsts.color2, 1.0);
}
