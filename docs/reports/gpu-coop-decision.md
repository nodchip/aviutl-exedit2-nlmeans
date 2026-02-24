# GPU Coop Adoption Decision Report

- decision: NOT_ADOPTED
- policy: Keep single-GPU as the default indefinitely. Multi-GPU coop remains non-adopted.
- adoption_gate: FAIL
- exedit2_e2e_gate: PASS
- benchmark_profile: fhd
- single_gpu_fixed_window: 2026-02-23 to indefinite
- next_gpu_coop_reevaluation: none (not scheduled (policy disabled))
- adoption_triggers: explicit project decision is required to reopen GPU coop adoption review
- recent_ratio(3): 2026-02-24T18:29:35+09:00: ratio=2.047; 2026-02-24T18:29:49+09:00: ratio=1.894; 2026-02-24T20:23:59+09:00: ratio=1.939
- latest_benchmark: frame=1920x1080, single=6.120, seq=11.869, async=7.958, async_fallback=11.755
