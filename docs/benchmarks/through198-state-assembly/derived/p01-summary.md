# P01 state / nested Assembly baseline

Overall: **passed**; publication ready: **true**.

Timing cells: p50 / nearest-rank p95 / observed max in ms (actual count). Cold expects 3; warm expects 30. Missing observations are shown as —.

| Mode | Size | Status | Processes /3 | Samples verified / obtained | Metric | Cold | Warm | Peak RSS KiB min / p50 / max (process count) |
| --- | ---: | --- | ---: | ---: | --- | --- | --- | --- |
| states | 2 | passed | 3 | 33 / 33 | `effective_evaluation_batch_ms` | 7.63578 / 8.37242 / 8.37242 (3) | 7.81457 / 8.64152 / 9.27082 (30) | 523748 / 524040 / 524092 (3) |
| states | 2 | passed | 3 | 33 / 33 | `geometry_batch_ms` | 6.35752 / 6.47102 / 6.47102 (3) | 10.5766 / 11.9051 / 11.9076 (30) |  |
| states | 10 | passed | 3 | 33 / 33 | `effective_evaluation_batch_ms` | 41.09 / 42.8666 / 42.8666 (3) | 39.337 / 43.6288 / 44.2223 (30) | 523412 / 524028 / 524212 (3) |
| states | 10 | passed | 3 | 33 / 33 | `geometry_batch_ms` | 32.4354 / 34.6233 / 34.6233 (3) | 52.6959 / 58.8407 / 59.7583 (30) |  |
| states | 50 | passed | 3 | 33 / 33 | `effective_evaluation_batch_ms` | 200.762 / 208.107 / 208.107 (3) | 202.818 / 212.366 / 287.671 (30) | 523288 / 523732 / 523868 (3) |
| states | 50 | passed | 3 | 33 / 33 | `geometry_batch_ms` | 161.719 / 166.512 / 166.512 (3) | 266.715 / 281.061 / 287.702 (30) |  |
| assembly | 10 | passed | 3 | 33 / 33 | `physical_properties_ms` | 15.3654 / 15.4176 / 15.4176 (3) | 13.2873 / 15.3519 / 16.9637 (30) | 523740 / 524024 / 524184 (3) |
| assembly | 10 | passed | 3 | 33 / 33 | `resolver_ms` | 0.254291 / 0.286165 / 0.286165 (3) | 0.219313 / 0.279666 / 0.314358 (30) |  |
| assembly | 100 | passed | 3 | 33 / 33 | `physical_properties_ms` | 26.4482 / 27.4534 / 27.4534 (3) | 24.8514 / 27.4317 / 27.6785 (30) | 523820 / 523840 / 524048 (3) |
| assembly | 100 | passed | 3 | 33 / 33 | `resolver_ms` | 1.84007 / 2.36676 / 2.36676 (3) | 1.69604 / 1.9356 / 1.9362 (30) |  |
| assembly | 1000 | passed | 3 | 33 / 33 | `physical_properties_ms` | 175.273 / 175.349 / 175.349 (3) | 170.32 / 181.308 / 197.575 (30) | 523560 / 523860 / 524064 (3) |
| assembly | 1000 | passed | 3 | 33 / 33 | `resolver_ms` | 38.7117 / 39.9048 / 39.9048 (3) | 38.9581 / 41.0754 / 46.1362 (30) |  |

Cold is the first measured batch in each new process, without an OS cache flush. RSS is process-lifetime peak resident memory, not a per-call allocation or baseline-subtracted delta.

states 2 RSS measurement point: after workload generation, all timed batches and independent checks; before source and result artifact serialization/publication.

states 10 RSS measurement point: after workload generation, all timed batches and independent checks; before source and result artifact serialization/publication.

states 50 RSS measurement point: after workload generation, all timed batches and independent checks; before source and result artifact serialization/publication.

assembly 10 RSS measurement point: after workload generation, all timed batches and independent checks; before source and result artifact serialization/publication.

assembly 100 RSS measurement point: after workload generation, all timed batches and independent checks; before source and result artifact serialization/publication.

assembly 1000 RSS measurement point: after workload generation, all timed batches and independent checks; before source and result artifact serialization/publication.

Runtime manifest SHA-256: `396d70c6988508f81ceec7f76b5f8a642dc67454d5bd7d5c09b7c056781224c6`.
Series SHA-256: `393d6ee27bac79f2966f11c042dd0320b6e111b5b3881b3d469743509dbaa844`.

This synchronous headless baseline does not establish native UI latency, unmeasured solve/export performance, a portable budget, or numbered acceptance.
