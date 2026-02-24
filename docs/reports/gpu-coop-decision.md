# GPU Coop Adoption Decision Report

- adoption_gate: FAIL
- exedit2_e2e_gate: FAIL
- single_gpu_fixed_window: 2026-02-23 to 2026-03-31
- next_gpu_coop_reevaluation: 2026-03-31 (in 35 days)
- policy: keep single-GPU default until adoption_gate becomes PASS.
- adoption_triggers: last 3 history ratios <= 1.500, async/single <= 1.200, no regression check failure.
- recent_ratio(3): 2026-02-24T10:17:32+09:00: ratio=3.820; 2026-02-24T15:57:18+09:00: ratio=2.510; 2026-02-24T16:08:06+09:00: ratio=3.269
- latest_benchmark: single=0.171, seq=0.559, async=0.304, async_fallback=0.520
