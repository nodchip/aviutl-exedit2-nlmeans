# GPU Coop PoC Benchmark

- Frame: 128x72
- Search Radius: 2
- Iterations: 12

| Mode | Mean Time (ms/frame) |
|---|---:|
| Single GPU Full Frame | 0.155 |
| Coop PoC (2 tiles, sequential dispatch) | 0.543 |
| Coop PoC (2 tiles, async dispatch) | 0.310 |
| Coop PoC (2 tiles, async + single fallback) | 0.475 |

- Relative (CoopSeq/Single): 3.510x
- Relative (CoopAsync/Single): 2.002x
- Relative (CoopAsyncFallback/Single): 3.067x
