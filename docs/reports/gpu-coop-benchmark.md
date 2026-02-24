# GPU Coop PoC Benchmark

- Search Radius: 2
- Tile Count: 2
- Notes: adoption gate should use real-size profile (`fhd`) instead of smoke size.

| Profile | Frame | Iterations | Single GPU Full Frame | Coop PoC (2 tiles, sequential dispatch) | Coop PoC (2 tiles, async dispatch) | Coop PoC (2 tiles, async + single fallback) | Relative (CoopSeq/Single) | Relative (CoopAsync/Single) | Relative (CoopAsyncFallback/Single) |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|
| smoke | 128x72 | 12 | 0.202 | 0.547 | 0.296 | 0.621 | 2.707x | 1.464x | 3.074x |
| hd | 1280x720 | 6 | 2.761 | 5.142 | 3.390 | 4.897 | 1.863x | 1.228x | 1.774x |
| fhd | 1920x1080 | 4 | 6.120 | 11.869 | 7.958 | 11.755 | 1.939x | 1.300x | 1.921x |
