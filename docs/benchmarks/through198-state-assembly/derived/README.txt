Raw evidence is copied byte-for-byte. Compact runtime/source JSON stays compact. The emitted
runtime manifest retains the complete linked-library paths, bytes, SHA-256 digests and ldd output.
The scene generator is deterministic and nonrandom: no random seed applies. UI scale is not
applicable to these headless calls. CPU affinity and hardware concurrency are retained as emitted;
no unrecorded thread-pool cap or portable timing guarantee is inferred.
The executable/library blobs are not copied into Git; preserve an optional exact binary archive
outside the repository if needed. Raw failures and deadlines remain failures; unknown counts remain
unknown. Smoke evidence is a separate diagnostic and is never pooled with these18publication
processes. Cold is the first measured batch in a fresh process without an OS cache flush. State and
Assembly times are batch/call totals, not native UI activation or per-leaf latency. The three cold
samples and30warm samples are pooled from raw observations per size/metric; per-process percentiles
are not averaged. Nearest-rank p95 for30warm observations is the29th ordered sample. RSS is a
process-lifetime observation before artifact serialization, with its original measurement scope.

Before each commit, inspect staged content and `git diff --cached --numstat`; the generated plan
counts new-file insertions and does not include unrelated staged changes. Retain every commit at
or below1500additions. No portable budget or numbered acceptance follows from this baseline.
