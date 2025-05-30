// Cylindrical resampling, Jinc based (not separable).

#define USE_JINC_BASE
#include "kernel_functions.hlsli"

Texture2D<float> tex : register(t0);
SamplerState smp : register(s0);

cbuffer cb0 : register(b0)
{
    int index;
    float radius;
    float blur;
    
    // Free parameters.
    float p1;
    float p2;
    
    // Antiringing strenght.
    float ar;
    
    float scale;
    float bound;
    float2 dims;
    float2 pt;
}

// Expects abs(x).
float get_weight(float x)
{
    if (x < radius) {
        switch (index) {
            case KERNEL_FUNCTION_LANCZOS:
                return base(x, blur) * jinc(x, radius); // EWA Lanczos.
            case KERNEL_FUNCTION_GINSENG:
                return base(x, blur) * sinc(x, radius); // EWA Ginseng.
            case KERNEL_FUNCTION_HAMMING:
                return base(x, blur) * hamming(x, radius);
            case KERNEL_FUNCTION_POW_COSINE:
                return base(x, blur) * power_of_cosine(x, radius, p1);
            case KERNEL_FUNCTION_KAISER:
                return base(x, blur) * kaiser(x, radius, p1);
            case KERNEL_FUNCTION_POW_GARAMOND:
                return base(x, blur) * power_of_garamond(x, radius, p1, p2);
            case KERNEL_FUNCTION_POW_BLACKMAN:
                return base(x, blur) * power_of_blackman(x, radius, p1, p2);
            case KERNEL_FUNCTION_GNW:
                return base(x, blur) * generalized_normal_window(x, p1, p2);
            case KERNEL_FUNCTION_SAID:
                return base(x, blur) * said(x, p1, p2);
            case KERNEL_FUNCTION_BICUBIC:
                return bicubic(x, p1);
            case KERNEL_FUNCTION_FSR:
                return modified_fsr_kernel(x, p1, p2);
            case KERNEL_FUNCTION_BCSPLINE:
                return bc_spline(x, p1, p2);
            default: // Black image.
                return 0.0;
        }
    }

    // x >= radius
    else {
        return 0.0;
    }
}

float main(float4 pos : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
{
    const float2 xy = texcoord * dims;
    const float2 base = floor(xy - 0.5) + 0.5;
    const float2 f = xy - base;
    float csum = 0.0;
    float wsum = 0.0;

    // Antiringing.
    float lo = 1e9;
    float hi = -1e9;
    
    for (float y = 1.0 - bound; y <= bound; ++y) {
        for (float x = 1.0 - bound; x <= bound; ++x) {
            const float color = tex.SampleLevel(smp, (base + float2(x, y)) * pt, 0.0);
            const float weight = get_weight(length(float2(x, y) - f) * scale);
            csum += color * weight;
            wsum += weight;

            // Antiringing.
            if (ar > 0.0f && y >= 0.0 && y <= 1.0 && x >= 0.0 && x <= 1.0) {
                lo = min(lo, color);
                hi = max(hi, color);
            }
        }
    }

    // Normalize weighted color sum.
    csum /= wsum;

    // Antiringing.
    if (ar > 0.0f) {
        return lerp(csum, clamp(csum, lo, hi), ar);
    }

    return csum;
}