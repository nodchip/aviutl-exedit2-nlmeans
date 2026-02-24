# GPU Coop PoC Benchmark

- Search Radius: 2
- Tile Count: 2
- Notes: adoption gate should use real-size profile (`fhd`) instead of smoke size.

| Profile | Frame | Iterations | Single GPU Full Frame | Coop PoC (2 tiles, sequential dispatch) | Coop PoC (2 tiles, async dispatch) | Coop PoC (2 tiles, async + single fallback) | Relative (CoopSeq/Single) | Relative (CoopAsync/Single) | Relative (CoopAsyncFallback/Single) |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|
| smoke | 128x72 | 12 | 0.234 | 0.504 | 0.272 | 0.563 | 2.156x | 1.166x | 2.413x |
| hd | 1280x720 | 6 | 2.998 | 5.174 | 3.359 | 5.874 | 1.726x | 1.120x | 1.959x |
| fhd | 1920x1080 | 4 | 6.335 | 12.001 | 7.768 | 10.710 | 1.894x | 1.226x | 1.690x |
