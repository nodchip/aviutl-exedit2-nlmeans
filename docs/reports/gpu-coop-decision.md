# GPU Coop Adoption Decision Report

- adoption_gate: FAIL
- exedit2_e2e_gate: PASS
- benchmark_profile: fhd
- single_gpu_fixed_window: 2026-02-23 to 2026-03-31
- next_gpu_coop_reevaluation: 2026-03-31 (in 35 days)
- policy: keep single-GPU default until adoption_gate becomes PASS.
- adoption_triggers: last 3 history ratios <= 1.500, async/single <= 1.200, no regression check failure (target profile only).
- recent_ratio(3): 2026-02-24T17:38:51+09:00: ratio=1.982; 2026-02-24T18:29:35+09:00: ratio=2.047; 2026-02-24T18:29:49+09:00: ratio=1.894
- latest_benchmark: frame=1920x1080, single=6.335, seq=12.001, async=7.768, async_fallback=10.710
