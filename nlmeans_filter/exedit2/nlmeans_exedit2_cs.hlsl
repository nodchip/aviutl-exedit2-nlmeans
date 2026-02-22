cbuffer Constants : register(b0)
{
    uint Width;
    uint Height;
    uint SearchRadius;
    uint FrameCount;
    uint CurrentFrameIndex;
    uint SpatialStep;
    float InvSigma2;
    float TemporalDecay;
    float Reserved0;
    float Reserved1;
    float Reserved2;
    float Reserved3;
};

StructuredBuffer<uint> InputPixels : register(t0);
RWStructuredBuffer<uint> OutputPixels : register(u0);

int clampi(int v, int low, int high)
{
    return min(high, max(low, v));
}

uint frameIndex(uint t, uint x, uint y)
{
    return (t * Height + y) * Width + x;
}

float3 unpack_rgb(uint packed)
{
    return float3(
        (float)(packed & 0xffu),
        (float)((packed >> 8) & 0xffu),
        (float)((packed >> 16) & 0xffu));
}

uint unpack_a(uint packed)
{
    return (packed >> 24) & 0xffu;
}

uint pack_rgba(float3 rgb, uint a)
{
    const uint r = (uint)clamp(rgb.x, 0.0f, 255.0f);
    const uint g = (uint)clamp(rgb.y, 0.0f, 255.0f);
    const uint b = (uint)clamp(rgb.z, 0.0f, 255.0f);
    return (a << 24) | (b << 16) | (g << 8) | r;
}

[numthreads(16,16,1)]
void main(uint3 tid : SV_DispatchThreadID)
{
    if (tid.x >= Width || tid.y >= Height)
        return;

    const int x = (int)tid.x;
    const int y = (int)tid.y;
    const uint centerIndex = frameIndex(CurrentFrameIndex, tid.x, tid.y);
    const uint centerPacked = InputPixels[centerIndex];
    const float3 center = unpack_rgb(centerPacked);
    const uint alpha = unpack_a(centerPacked);

    float sumW = 0.0f;
    float sumR = 0.0f;
    float sumG = 0.0f;
    float sumB = 0.0f;

    for (uint t = 0; t < FrameCount; ++t)
    {
        const int dt = abs((int)t - (int)CurrentFrameIndex);
        const float temporalWeight = exp(-TemporalDecay * (float)dt);
        for (int dy = -((int)SearchRadius); dy <= (int)SearchRadius; dy += (int)SpatialStep)
        {
            const int sy = clampi(y + dy, 0, (int)Height - 1);
            for (int dx = -((int)SearchRadius); dx <= (int)SearchRadius; dx += (int)SpatialStep)
            {
                const int sx = clampi(x + dx, 0, (int)Width - 1);
                const float3 sample = unpack_rgb(InputPixels[frameIndex(t, (uint)sx, (uint)sy)]);
                const float dr = sample.r - center.r;
                const float dg = sample.g - center.g;
                const float db = sample.b - center.b;
                const float dist2 = dr * dr + dg * dg + db * db;
                const float w = exp(-dist2 * InvSigma2) * temporalWeight;
                sumW += w;
                sumR += w * sample.r;
                sumG += w * sample.g;
                sumB += w * sample.b;
            }
        }
    }

    float3 outRgb = center;
    if (sumW > 0.0f)
    {
        outRgb = float3(sumR / sumW, sumG / sumW, sumB / sumW);
    }
    OutputPixels[tid.y * Width + tid.x] = pack_rgba(outRgb, alpha);
}
