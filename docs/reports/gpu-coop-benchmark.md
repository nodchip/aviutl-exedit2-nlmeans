# GPU Coop PoC Benchmark

- Frame: 128x72
- Search Radius: 2
- Iterations: 12

| Mode | Mean Time (ms/frame) |
|---|---:|
| Single GPU Full Frame | 0.176 |
| Coop PoC (2 tiles, sequential dispatch) | 0.512 |
| Coop PoC (2 tiles, async dispatch) | 0.303 |
| Coop PoC (2 tiles, async + single fallback) | 0.501 |

- Relative (CoopSeq/Single): 2.916x
- Relative (CoopAsync/Single): 1.724x
- Relative (CoopAsyncFallback/Single): 2.850x
