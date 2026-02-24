# GPU Coop PoC Benchmark

- Frame: 128x72
- Search Radius: 2
- Iterations: 12

| Mode | Mean Time (ms/frame) |
|---|---:|
| Single GPU Full Frame | 0.171 |
| Coop PoC (2 tiles, sequential dispatch) | 0.559 |
| Coop PoC (2 tiles, async dispatch) | 0.304 |
| Coop PoC (2 tiles, async + single fallback) | 0.520 |

- Relative (CoopSeq/Single): 3.267x
- Relative (CoopAsync/Single): 1.776x
- Relative (CoopAsyncFallback/Single): 3.036x
