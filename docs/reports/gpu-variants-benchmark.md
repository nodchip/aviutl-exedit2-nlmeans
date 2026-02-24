# GPU Variants Benchmark

- Frame: 128x72
- Search Radius: 2
- Time Radius: 1
- Iterations: 12

| Variant | Spatial Step | Temporal Decay | Mean Time (ms/frame) | Relative to Baseline |
|---|---:|---:|---:|---:|
| Baseline | 1 | 0.000 | 1.305 | 1.000x |
| Fast(step=2) | 2 | 0.000 | 0.192 | 0.147x |
| Fast(step=3) | 3 | 0.000 | 0.213 | 0.163x |
| Temporal(decay=1.0) | 1 | 1.000 | 0.209 | 0.160x |
| Fast(step=2)+Temporal(decay=1.0) | 2 | 1.000 | 0.220 | 0.169x |
