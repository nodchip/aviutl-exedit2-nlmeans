# GPU Coop Adoption Decision Report

- adoption_gate: FAIL
- exedit2_e2e_gate: FAIL
- single_gpu_fixed_window: 2026-02-23 to 2026-03-31
- next_gpu_coop_reevaluation: 2026-03-31 (in 35 days)
- policy: keep single-GPU default until adoption_gate becomes PASS.
- adoption_triggers: last 3 history ratios <= 1.500, async/single <= 1.200, no regression check failure.
- recent_ratio(3): 2026-02-23T23:42:51+09:00: ratio=2.796; 2026-02-23T23:55:06+09:00: ratio=2.581; 2026-02-24T09:32:02+09:00: ratio=2.909
- latest_benchmark: single=0.176, seq=0.512, async=0.303, async_fallback=0.501
