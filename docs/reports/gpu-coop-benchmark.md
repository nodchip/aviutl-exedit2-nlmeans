# GPU Coop PoC Benchmark

- Frame: 128x72
- Search Radius: 2
- Iterations: 12

| Mode | Mean Time (ms/frame) |
|---|---:|
| Single GPU Full Frame | 0.216 |
| Coop PoC (2 tiles, sequential dispatch) | 0.604 |
| Coop PoC (2 tiles, async dispatch) | 0.355 |
| Coop PoC (2 tiles, async + single fallback) | 0.524 |

- Relative (CoopSeq/Single): 2.798x
- Relative (CoopAsync/Single): 1.645x
- Relative (CoopAsyncFallback/Single): 2.428x
