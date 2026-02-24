# GPU Variants Benchmark

- Frame: 128x72
- Search Radius: 2
- Time Radius: 1
- Iterations: 12

| Variant | Spatial Step | Temporal Decay | Mean Time (ms/frame) | Relative to Baseline |
|---|---:|---:|---:|---:|
| Baseline | 1 | 0.000 | 1.263 | 1.000x |
| Fast(step=2) | 2 | 0.000 | 0.206 | 0.163x |
| Fast(step=3) | 3 | 0.000 | 0.222 | 0.176x |
| Temporal(decay=1.0) | 1 | 1.000 | 0.194 | 0.153x |
| Fast(step=2)+Temporal(decay=1.0) | 2 | 1.000 | 0.242 | 0.191x |
