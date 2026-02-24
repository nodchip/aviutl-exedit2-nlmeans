# GPU Variants Benchmark

- Frame: 128x72
- Search Radius: 2
- Time Radius: 1
- Iterations: 12

| Variant | Spatial Step | Temporal Decay | Mean Time (ms/frame) | Relative to Baseline |
|---|---:|---:|---:|---:|
| Baseline | 1 | 0.000 | 1.261 | 1.000x |
| Fast(step=2) | 2 | 0.000 | 0.194 | 0.154x |
| Fast(step=3) | 3 | 0.000 | 0.200 | 0.159x |
| Temporal(decay=1.0) | 1 | 1.000 | 0.209 | 0.165x |
| Fast(step=2)+Temporal(decay=1.0) | 2 | 1.000 | 0.261 | 0.207x |
