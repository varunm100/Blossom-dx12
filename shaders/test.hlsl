
float4 VSMain(uint vert_id : SV_VertexID) : SV_Position {
    ByteAddressBuffer vbo = ResourceDescriptorHeap[0];
    uint stride = sizeof(float3);
    float3 vert = vbo.Load <float3> ((0 + vert_id) * stride);

    return float4(vert, 1.);
}

float4 PSMain() : SV_TARGET {
    return float4(1., 1., 1., 1.);
}
