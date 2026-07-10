//=========================================================
// triangle.hlsl – Vertex & Fragment shader untuk piramida 3D
// Dengan texture mapping + vertex color
//=========================================================

struct VSInput {
    float3 position : POSITION;   // posisi 3D (x, y, z)
    float3 color    : COLOR;      // warna per vertex (r, g, b)
    float2 texcoord : TEXCOORD;   // UV texture coordinates
};

struct VSOutput {
    float4 position : SV_Position;
    float3 color    : COLOR;
    float2 texcoord : TEXCOORD;
};

// Uniform buffer: diisi CPU tiap frame dengan matriks MVP
[[vk::binding(0, 0)]] cbuffer UniformBuffer : register(b0) {
    row_major float4x4 mvp;
};

// Texture dan sampler digabung ke binding 1 (Combined Image Sampler)
[[vk::binding(1, 0)]] Texture2D    tex  : register(t1);
[[vk::binding(1, 0)]] SamplerState samp : register(s1);

VSOutput vs_main(VSInput input) {
    VSOutput output;
    // Transformasi posisi lokal -> clip-space via matriks MVP
    output.position = mul(float4(input.position, 1.0), mvp);
    output.color    = input.color;
    output.texcoord = input.texcoord;
    return output;
}

float4 ps_main(VSOutput input) : SV_Target {
    float4 texColor = tex.Sample(samp, input.texcoord);
    // Kembalikan warna texture murni tanpa tinting (tanpa transparansi)
    return float4(texColor.rgb, 1.0);
}
