# GPU Coop PoC Benchmark

- Frame: 128x72
- Search Radius: 2
- Iterations: 12

| Mode | Mean Time (ms/frame) |
|---|---:|
| Single GPU Full Frame | 0.210 |
| Coop PoC (2 tiles, sequential dispatch) | 0.527 |
| Coop PoC (2 tiles, async dispatch) | 0.292 |
| Coop PoC (2 tiles, async + single fallback) | 0.472 |

- Relative (CoopSeq/Single): 2.511x
- Relative (CoopAsync/Single): 1.392x
- Relative (CoopAsyncFallback/Single): 2.250x
