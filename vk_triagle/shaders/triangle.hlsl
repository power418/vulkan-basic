struct VSInput {
    float2 position : POSITION;
    float3 color    : COLOR;
};

struct VSOutput {
    float4 position : SV_Position;
    float3 color    : COLOR;
};

VSOutput vs_main(VSInput input) {
    VSOutput output;
    output.position = float4(input.position, 0.0, 1.0);
    output.color    = input.color;
    return output;
}

float4 ps_main(VSOutput input) : SV_Target {
    return float4(input.color, 1.0);
}

void main() {
    
}