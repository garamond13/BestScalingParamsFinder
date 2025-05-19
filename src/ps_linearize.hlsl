Texture2D<float> tex : register(t0);
SamplerState smp : register(s0);

float main(float4 pos : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
{
    const float c = tex.SampleLevel(smp, texcoord, 0.0);

    // From sRGB to linear.
    return c < 0.04045 ? c / 12.92 : pow((c + 0.055) / 1.055, 2.4);
}
