# GPU Variants Benchmark

- Profile: smoke
- Frame: 128x72
- Search Radius: 2
- Time Radius: 1
- Iterations: 12

- Warmup: 1 run per variant

| Variant | Spatial Step | Temporal Decay | Mean Time (ms/frame) | Relative to Baseline |
|---|---:|---:|---:|---:|
| Baseline | 1 | 0.000 | 0.172 | 1.000x |
| Fast(step=2) | 2 | 0.000 | 0.196 | 1.142x |
| Fast(step=3) | 3 | 0.000 | 0.189 | 1.104x |
| Temporal(decay=1.0) | 1 | 1.000 | 0.166 | 0.970x |
| Fast(step=2)+Temporal(decay=1.0) | 2 | 1.000 | 0.149 | 0.870x |
