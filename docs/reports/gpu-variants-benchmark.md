# GPU Variants Benchmark

- Frame: 128x72
- Search Radius: 2
- Time Radius: 1
- Iterations: 12

- Warmup: 1 run per variant

| Variant | Spatial Step | Temporal Decay | Mean Time (ms/frame) | Relative to Baseline |
|---|---:|---:|---:|---:|
| Baseline | 1 | 0.000 | 0.190 | 1.000x |
| Fast(step=2) | 2 | 0.000 | 0.127 | 0.670x |
| Fast(step=3) | 3 | 0.000 | 0.121 | 0.638x |
| Temporal(decay=1.0) | 1 | 1.000 | 0.162 | 0.854x |
| Fast(step=2)+Temporal(decay=1.0) | 2 | 1.000 | 0.120 | 0.633x |
