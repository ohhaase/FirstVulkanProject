struct VSInput
{
    float2 inPosition;
    float3 inColor;
    float2 inTexCoord;
}

struct VSOutput
{
    float4 pos : SV_Position;
    float3 color;
    float2 texCoord;
}

struct UniformBuffer
{
    float4x4 model;
    float4x4 view;
    float4x4 proj;
};
ConstantBuffer<UniformBuffer> uniforms;

[shader("vertex")]
VSOutput vertMain(VSInput input)
{
    VSOutput output;
    output.pos = mul(uniforms.proj, mul(uniforms.view, mul(uniforms.model, float4(input.inPosition, 0.0, 1.0))));
    output.color = input.inColor;
    output.texCoord = input.inTexCoord;
    return output;
}

Sampler2D texture;

[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_Target
{
    // return float4(vertIn.color, 1.0);
    return texture.Sample(vertIn.texCoord);
}