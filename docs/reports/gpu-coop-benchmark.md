# GPU Coop PoC Benchmark

- Frame: 128x72
- Search Radius: 2
- Iterations: 12

| Mode | Mean Time (ms/frame) |
|---|---:|
| Single GPU Full Frame | 0.126 |
| Coop PoC (2 tiles, sequential dispatch) | 0.515 |

- Relative (Coop/Single): 4.074x
