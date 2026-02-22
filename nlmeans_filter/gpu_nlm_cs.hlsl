cbuffer Constants : register(b0)
{
    uint Width;
    uint Height;
    uint SearchRadius;
    uint FrameCount;
    uint CurrentFrameIndex;
    uint Reserved0;
    uint Reserved1;
    uint Reserved2;
    float H2;
    float Reserved3;
    float Reserved4;
    float Reserved5;
};

struct PixelData
{
    int y;
    int cb;
    int cr;
    int reserved;
};

StructuredBuffer<PixelData> InputPixels : register(t0);
RWStructuredBuffer<PixelData> OutputPixels : register(u0);

int clampi(int v, int low, int high)
{
    return min(high, max(low, v));
}

int getChannel(PixelData p, int channel)
{
    if (channel == 0) return p.y;
    if (channel == 1) return p.cb;
    return p.cr;
}

int readChannel(int x, int y, int t, int channel)
{
    const int cx = clampi(x, 0, (int)Width - 1);
    const int cy = clampi(y, 0, (int)Height - 1);
    const int ct = clampi(t, 0, (int)FrameCount - 1);
    const uint frameOffset = (uint)ct * Width * Height;
    return getChannel(InputPixels[frameOffset + cy * Width + cx], channel);
}

[numthreads(16,16,1)]
void main(uint3 tid : SV_DispatchThreadID)
{
    if (tid.x >= Width || tid.y >= Height) return;

    const int x = (int)tid.x;
    const int y = (int)tid.y;
    const uint index = tid.y * Width + tid.x;
    PixelData outPixel;
    outPixel.reserved = 0;

    for (int channel = 0; channel < 3; ++channel)
    {
        float sum = 0.0f;
        float value = 0.0f;
        for (int dy = -((int)SearchRadius); dy <= (int)SearchRadius; ++dy)
        {
            for (int dx = -((int)SearchRadius); dx <= (int)SearchRadius; ++dx)
            {
                for (int dt = 0; dt < (int)FrameCount; ++dt)
                {
                    float sum2 = 0.0f;
                    for (int ny = -1; ny <= 1; ++ny)
                    {
                        for (int nx = -1; nx <= 1; ++nx)
                        {
                            const float a = (float)readChannel(x + nx, y + ny, (int)CurrentFrameIndex, channel);
                            const float b = (float)readChannel(x + dx + nx, y + dy + ny, dt, channel);
                            const float diff = a - b;
                            sum2 += diff * diff;
                        }
                    }
                    const float w = exp(-sum2 * H2);
                    sum += w;
                    value += (float)readChannel(x + dx, y + dy, dt, channel) * w;
                }
            }
        }

        const float filtered = (sum > 0.0f) ? (value / sum) : (float)readChannel(x, y, (int)CurrentFrameIndex, channel);
        const int v = (int)filtered;
        if (channel == 0) outPixel.y = v;
        if (channel == 1) outPixel.cb = v;
        if (channel == 2) outPixel.cr = v;
    }

    OutputPixels[index] = outPixel;
}
