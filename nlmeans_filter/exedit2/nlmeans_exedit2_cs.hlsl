cbuffer Constants : register(b0)
{
    uint Width;
    uint Height;
    uint SearchRadius;
    uint FrameCount;
    uint CurrentFrameIndex;
    uint SpatialStep;
    uint ProcessYBegin;
    uint ProcessYEnd;
    uint ReservedU0;
    uint ReservedU1;
    float InvSigma;
    float TemporalDecay;
    float ReservedF0;
    float ReservedF1;
    float ReservedF2;
    float ReservedF3;
};

StructuredBuffer<uint> InputPixels : register(t0);
RWStructuredBuffer<uint> OutputPixels : register(u0);

static const int2 kPatchOffsets[9] = {
    int2(-1, -1), int2(0, -1), int2(1, -1),
    int2(-1,  0), int2(0,  0), int2(1,  0),
    int2(-1,  1), int2(0,  1), int2(1,  1)
};

static const float kPatchWeights[9] = {
    0.07f, 0.12f, 0.07f,
    0.12f, 0.20f, 0.12f,
    0.07f, 0.12f, 0.07f
};

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
    const uint yGlobal = tid.y + ProcessYBegin;
    if (tid.x >= Width || yGlobal >= Height || yGlobal >= ProcessYEnd)
        return;

    const int x = (int)tid.x;
    const int y = (int)yGlobal;
    const uint centerIndex = frameIndex(CurrentFrameIndex, tid.x, yGlobal);
    const uint centerPacked = InputPixels[centerIndex];
    const float3 center = unpack_rgb(centerPacked);
    const uint alpha = unpack_a(centerPacked);
    float3 currentPatch[9];

    [unroll]
    for (int i = 0; i < 9; ++i)
    {
        const int cx = clampi(x + kPatchOffsets[i].x, 0, (int)Width - 1);
        const int cy = clampi(y + kPatchOffsets[i].y, 0, (int)Height - 1);
        currentPatch[i] = unpack_rgb(InputPixels[frameIndex(CurrentFrameIndex, (uint)cx, (uint)cy)]);
    }

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
                float patchDistance = 0.0f;

                [unroll]
                for (int i = 0; i < 9; ++i)
                {
                    const int tx = clampi(sx + kPatchOffsets[i].x, 0, (int)Width - 1);
                    const int ty = clampi(sy + kPatchOffsets[i].y, 0, (int)Height - 1);
                    const float3 samplePatch = unpack_rgb(InputPixels[frameIndex(t, (uint)tx, (uint)ty)]);
                    const float3 diff = currentPatch[i] - samplePatch;
                    patchDistance += dot(diff, diff) * kPatchWeights[i];
                }

                const float3 sample = unpack_rgb(InputPixels[frameIndex(t, (uint)sx, (uint)sy)]);
                const float w = exp(-sqrt(max(patchDistance, 0.0f)) * InvSigma) * temporalWeight;
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
    OutputPixels[yGlobal * Width + tid.x] = pack_rgba(outRgb, alpha);
}
