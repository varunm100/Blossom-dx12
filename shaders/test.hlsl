
struct DrawConstants
{
    uint vbo_index;
    float3 color1;
    float3 color2;
};
ConstantBuffer<DrawConstants> DrawConsts : register(b0, space0);
struct Vert
{
    float3 p;
    float3 n;
};

struct VS_OUT
{
	float4 p: SV_Position;
    float4 n: NORMAL;
};

VS_OUT VSMain(uint vert_id : SV_VertexID) {
    ByteAddressBuffer vbo = ResourceDescriptorHeap[DrawConsts.vbo_index];
    uint stride = sizeof(Vert);
    Vert vert = vbo.Load <Vert> ((0 + vert_id) * stride);

    VS_OUT output;
    output.p = float4(vert.p + float3(0,0,0.5), 1);
    output.n = float4(vert.n * 0.5 + float3(0.5,0.5,0.5), 1);
    return output;
}

float4 PSMain(VS_OUT input) : SV_TARGET {
    //return uv.x/1280. > 0.5 ? float4(DrawConsts.color1, 1.0) : float4(DrawConsts.color2, 1.0);
    return input.n;
}
