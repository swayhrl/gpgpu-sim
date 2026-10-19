# A TVD observer / local data diagnostic delta — SOURCE ONLY

Parent/C adapter preimage remains eb6739b03883fecee155ba6b12a3a9f92c00d444.
C should finish its one exact-preimage patch there; do not rebase/chase this
A-owned follow-up. A will merge both on the same public branch. Frozen R1
17e7e224 and CLEAN94996ec5 sources remain unchanged. No C++ compilation,
configure, syntax compiler, native execution, app or new binary was started.

The actual TVD frontend returns before the original l2cache ms_observe branch.
Simply permitting observer1 would miss query/backpressure waits and the real
response-enqueue admission. This delta records actual TVD query, WB, pin,
response-space and bank-denial phases plus actual commit. A native-path miss
closes the completed query before unrelated native admission. Later masked
predicates remain UNKNOWN rather than a measured unblocked resource. The
observer is never read by the controller and all existing cycle/port/pin/data
predicates remain. The new row is EPL2MSV2 WAIT reason15 (VICTIM_LOOKUP).
Published EPL2MSV1 rows0..13 and aggregate reason14 keep their exact identities;
reason14 is reserved rather than silently repurposed. E must account for the
new V2 row in its full observer intake after a real independently qualified
binary. No output change means accepted runtime evidence here.

TVD + observer / L2_DATA_FREE is now a source-supported WV subset. The actual
paid payload API already executes the real data/ownership transition and
exempts only bank service in DATA_FREE; this does not remove query latency,
queues, lower writes, fixed pipelines, copy values, capacity or completion.
COPY_FREE with TVD remains explicitly rejected. RO is absent from this common
preimage and its combination guards remain: this is NOT implemented/qualified
full F_DATA_FREE. C must retain the same constraints in its RO adapter.

Four finite source invocations are specified in
A_TVD_OBSERVER_DATA_FREE_DIRECTED_r1.json: normal-P1/observer real victim and
dirty-WB traffic, same stimulus DATA_FREE, COPY_FREE rejection, and bounded
observer output-schema unit check. Existing native fixture1024Tags/1168payload
and independent sector-version checks are reused, not a new miniature cache.
Normal/data-free runs require a real victim hit without a lower read, immutable
actual dirty WB data, wait UID closure and real drain; DATA_FREE checks no
physical bank grants/retry tokens while still counting fill/read/frontend/
lower-WB logical services. These are proposed assertions, ALL UNRUN. They do
not establish old-red/new-green, nativeDRAM timing, RO or application results.

Next real validation requires E's exact source/API/host/output resource lease.
No independent F build, implicit resource exception or old14 full queue follows
from this source publication. Complete F/RO/common native qualification and
E independent acceptance still precede final same-binary main application pairs.
