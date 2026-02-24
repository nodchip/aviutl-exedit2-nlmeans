# GPU Variants Benchmark

- Profile: smoke
- Frame: 128x72
- Search Radius: 2
- Time Radius: 1
- Iterations: 12

- Warmup: 1 run per variant

| Variant | Spatial Step | Temporal Decay | Mean Time (ms/frame) | Relative to Baseline |
|---|---:|---:|---:|---:|
| Baseline | 1 | 0.000 | 0.152 | 1.000x |
| Fast(step=2) | 2 | 0.000 | 0.160 | 1.057x |
| Fast(step=3) | 3 | 0.000 | 0.136 | 0.898x |
| Temporal(decay=1.0) | 1 | 1.000 | 0.192 | 1.264x |
| Fast(step=2)+Temporal(decay=1.0) | 2 | 1.000 | 0.119 | 0.784x |
