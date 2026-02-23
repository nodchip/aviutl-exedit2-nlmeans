# GPU Coop Adoption Decision Report

- adoption_gate: FAIL
- policy: keep single-GPU default until adoption_gate becomes PASS.
- adoption_triggers: last 3 history ratios <= 1.500, async/single <= 1.200, no regression check failure.
- recent_ratio(3): 2026-02-23T00:06:23+09:00: ratio=3.928; 2026-02-23T09:19:53+09:00: ratio=3.684; 2026-02-23T09:20:46+09:00: ratio=4.087
- latest_benchmark: single=0.159, seq=0.561, async=0.269, async_fallback=0.485
