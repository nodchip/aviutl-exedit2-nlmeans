# GPU Coop Adoption Decision Report

- adoption_gate: FAIL
- exedit2_e2e_gate: FAIL
- single_gpu_fixed_window: 2026-02-23 to 2026-03-31
- next_gpu_coop_reevaluation: 2026-03-31 (in 35 days)
- policy: keep single-GPU default until adoption_gate becomes PASS.
- adoption_triggers: last 3 history ratios <= 1.500, async/single <= 1.200, no regression check failure.
- recent_ratio(3): 2026-02-24T10:14:35+09:00: ratio=3.503; 2026-02-24T10:17:10+09:00: ratio=3.419; 2026-02-24T10:17:32+09:00: ratio=3.820
- latest_benchmark: single=0.167, seq=0.638, async=0.279, async_fallback=0.496
