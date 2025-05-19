Texture2D<float> tex : register(t0);
SamplerState smp : register(s0);

float main(float4 pos : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
{
    const float c = tex.SampleLevel(smp, texcoord, 0.0);

    // From linear to sRGB.
    return c < 0.0031308 ? 12.92 * c : 1.055 * pow(c, 1.0 / 2.4) - 0.055;
}