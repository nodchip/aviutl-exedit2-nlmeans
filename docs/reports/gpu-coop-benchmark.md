# GPU Coop PoC Benchmark

- Frame: 128x72
- Search Radius: 2
- Iterations: 12

| Mode | Mean Time (ms/frame) |
|---|---:|
| Single GPU Full Frame | 0.159 |
| Coop PoC (2 tiles, sequential dispatch) | 0.561 |
| Coop PoC (2 tiles, async dispatch) | 0.269 |
| Coop PoC (2 tiles, async + single fallback) | 0.485 |

- Relative (CoopSeq/Single): 3.522x
- Relative (CoopAsync/Single): 1.688x
- Relative (CoopAsyncFallback/Single): 3.043x
