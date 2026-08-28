# Blocking semantics

All V1 ratios are `blocked eligible cycles / eligible cycles`, never divided
by kernel cycles.  Frontend eligibility comes from the corrected production
preview: allocation, new-MSHR, merge-target, MissQ entries, data port, and
immediate-response slot.  A request held for consecutive retry cycles is one
blocked request and one episode, while every retry contributes a blocked cycle.

Fill eligibility is a DRAM→L2 queue head for which `waiting_for_fill()` is
true; it is blocked only when the real fill port is unavailable. ROP input,
MSHR response drain, MissQ lower drain, DRAM return, and DRAM issue records
observe their existing production queues/arbitration decisions. Multiple
simultaneous DRAM issue conditions are recorded independently.
