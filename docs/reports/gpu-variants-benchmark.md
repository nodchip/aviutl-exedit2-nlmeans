# GPU Variants Benchmark

- Profile: smoke
- Frame: 128x72
- Search Radius: 2
- Time Radius: 1
- Iterations: 12

- Warmup: 1 run per variant

| Variant | Spatial Step | Temporal Decay | Mean Time (ms/frame) | Relative to Baseline |
|---|---:|---:|---:|---:|
| Baseline | 1 | 0.000 | 0.156 | 1.000x |
| Fast(step=2) | 2 | 0.000 | 0.138 | 0.882x |
| Fast(step=3) | 3 | 0.000 | 0.176 | 1.130x |
| Temporal(decay=1.0) | 1 | 1.000 | 0.198 | 1.268x |
| Fast(step=2)+Temporal(decay=1.0) | 2 | 1.000 | 0.147 | 0.939x |
