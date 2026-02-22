cbuffer Constants : register(b0)
{
    uint Width;
    uint Height;
    uint SearchRadius;
    uint Reserved0;
    float InvSigma2;
    float Reserved1;
    float Reserved2;
    float Reserved3;
};

struct PixelData
{
    uint r;
    uint g;
    uint b;
    uint a;
};

StructuredBuffer<PixelData> InputPixels : register(t0);
RWStructuredBuffer<PixelData> OutputPixels : register(u0);

int clampi(int v, int low, int high)
{
    return min(high, max(low, v));
}

[numthreads(16,16,1)]
void main(uint3 tid : SV_DispatchThreadID)
{
    if (tid.x >= Width || tid.y >= Height)
        return;

    const int x = (int)tid.x;
    const int y = (int)tid.y;
    const uint centerIndex = tid.y * Width + tid.x;
    const PixelData center = InputPixels[centerIndex];

    float sumW = 0.0f;
    float sumR = 0.0f;
    float sumG = 0.0f;
    float sumB = 0.0f;

    for (int dy = -((int)SearchRadius); dy <= (int)SearchRadius; ++dy)
    {
        const int sy = clampi(y + dy, 0, (int)Height - 1);
        for (int dx = -((int)SearchRadius); dx <= (int)SearchRadius; ++dx)
        {
            const int sx = clampi(x + dx, 0, (int)Width - 1);
            const PixelData sample = InputPixels[(uint)(sy * (int)Width + sx)];
            const float dr = (float)sample.r - (float)center.r;
            const float dg = (float)sample.g - (float)center.g;
            const float db = (float)sample.b - (float)center.b;
            const float dist2 = dr * dr + dg * dg + db * db;
            const float w = exp(-dist2 * InvSigma2);
            sumW += w;
            sumR += w * (float)sample.r;
            sumG += w * (float)sample.g;
            sumB += w * (float)sample.b;
        }
    }

    PixelData outPixel = center;
    if (sumW > 0.0f)
    {
        outPixel.r = (uint)clamp(sumR / sumW, 0.0f, 255.0f);
        outPixel.g = (uint)clamp(sumG / sumW, 0.0f, 255.0f);
        outPixel.b = (uint)clamp(sumB / sumW, 0.0f, 255.0f);
    }
    OutputPixels[centerIndex] = outPixel;
}
