# GPU Coop PoC Benchmark

- Frame: 128x72
- Search Radius: 2
- Iterations: 12

| Mode | Mean Time (ms/frame) |
|---|---:|
| Single GPU Full Frame | 0.167 |
| Coop PoC (2 tiles, sequential dispatch) | 0.638 |
| Coop PoC (2 tiles, async dispatch) | 0.279 |
| Coop PoC (2 tiles, async + single fallback) | 0.496 |

- Relative (CoopSeq/Single): 3.828x
- Relative (CoopAsync/Single): 1.677x
- Relative (CoopAsyncFallback/Single): 2.975x
