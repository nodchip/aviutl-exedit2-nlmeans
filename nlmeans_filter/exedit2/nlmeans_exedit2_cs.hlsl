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
    float TemporalWeightAtT0;
    float TemporalWeightStep;
    float ReservedF0;
    float ReservedF1;
    float ReservedF2;
};

StructuredBuffer<uint> InputPixels : register(t0);
RWStructuredBuffer<uint> OutputPixels : register(u0);

static const float kPatchWeights[9] = {
    0.07f, 0.12f, 0.07f,
    0.12f, 0.20f, 0.12f,
    0.07f, 0.12f, 0.07f
};

int clampi(int v, int low, int high)
{
    return min(high, max(low, v));
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
    const int widthMax = (int)Width - 1;
    const int heightMax = (int)Height - 1;
    const int searchRadius = (int)SearchRadius;
    const int spatialStep = (int)SpatialStep;
    const uint pixelsPerFrame = Width * Height;
    const uint currentFrameBase = CurrentFrameIndex * pixelsPerFrame;
    const uint centerIndex = currentFrameBase + yGlobal * Width + tid.x;
    const uint centerPacked = InputPixels[centerIndex];
    const float3 center = unpack_rgb(centerPacked);
    const uint alpha = unpack_a(centerPacked);
    const int cx0 = clampi(x - 1, 0, widthMax);
    const int cx1 = x;
    const int cx2 = clampi(x + 1, 0, widthMax);
    const int cy0 = clampi(y - 1, 0, heightMax);
    const int cy1 = y;
    const int cy2 = clampi(y + 1, 0, heightMax);
    const uint currentRow0Base = currentFrameBase + (uint)cy0 * Width;
    const uint currentRow1Base = currentFrameBase + (uint)cy1 * Width;
    const uint currentRow2Base = currentFrameBase + (uint)cy2 * Width;
    float3 currentPatch[9];
    currentPatch[0] = unpack_rgb(InputPixels[currentRow0Base + (uint)cx0]);
    currentPatch[1] = unpack_rgb(InputPixels[currentRow0Base + (uint)cx1]);
    currentPatch[2] = unpack_rgb(InputPixels[currentRow0Base + (uint)cx2]);
    currentPatch[3] = unpack_rgb(InputPixels[currentRow1Base + (uint)cx0]);
    currentPatch[4] = unpack_rgb(InputPixels[currentRow1Base + (uint)cx1]);
    currentPatch[5] = unpack_rgb(InputPixels[currentRow1Base + (uint)cx2]);
    currentPatch[6] = unpack_rgb(InputPixels[currentRow2Base + (uint)cx0]);
    currentPatch[7] = unpack_rgb(InputPixels[currentRow2Base + (uint)cx1]);
    currentPatch[8] = unpack_rgb(InputPixels[currentRow2Base + (uint)cx2]);

    float sumW = 0.0f;
    float sumR = 0.0f;
    float sumG = 0.0f;
    float sumB = 0.0f;
    float temporalWeight = TemporalWeightAtT0;

    for (uint t = 0; t < FrameCount; ++t)
    {
        const uint frameBase = t * pixelsPerFrame;
        for (int dy = -searchRadius; dy <= searchRadius; dy += spatialStep)
        {
            const int sy = clampi(y + dy, 0, heightMax);
            for (int dx = -searchRadius; dx <= searchRadius; dx += spatialStep)
            {
                const int sx = clampi(x + dx, 0, widthMax);
                const int x0 = clampi(sx - 1, 0, widthMax);
                const int x1 = sx;
                const int x2 = clampi(sx + 1, 0, widthMax);
                const int y0 = clampi(sy - 1, 0, heightMax);
                const int y1 = sy;
                const int y2 = clampi(sy + 1, 0, heightMax);
                const int patchX[3] = { x0, x1, x2 };
                const int patchY[3] = { y0, y1, y2 };
                float patchDistance = 0.0f;
                // パッチ中心は候補画素そのものなので、先に読み出して加算色へ使う。
                const float3 sample = unpack_rgb(InputPixels[frameBase + (uint)sy * Width + (uint)sx]);

                int patchIndex = 0;
                [unroll]
                for (int py = 0; py < 3; ++py)
                {
                    const uint rowBase = frameBase + (uint)patchY[py] * Width;
                    [unroll]
                    for (int px = 0; px < 3; ++px)
                    {
                        const float3 samplePatch = unpack_rgb(InputPixels[rowBase + (uint)patchX[px]]);
                        const float3 diff = currentPatch[patchIndex] - samplePatch;
                        patchDistance += dot(diff, diff) * kPatchWeights[patchIndex];
                        ++patchIndex;
                    }
                }

                const float w = exp(-sqrt(max(patchDistance, 0.0f)) * InvSigma) * temporalWeight;
                sumW += w;
                sumR += w * sample.r;
                sumG += w * sample.g;
                sumB += w * sample.b;
            }
        }
        temporalWeight *= TemporalWeightStep;
    }

    float3 outRgb = center;
    if (sumW > 0.0f)
    {
        outRgb = float3(sumR / sumW, sumG / sumW, sumB / sumW);
    }
    OutputPixels[yGlobal * Width + tid.x] = pack_rgba(outRgb, alpha);
}
