# L2CHARV1 implementation map

| Metric | Production object | Observation point | Capacity | Allocation / release |
|---|---|---|---:|---|
| Reserved way | `tag_array` | pre-frontend sample | sets×ways | tag reserve / fill or invalidate |
| MSHR entry/target | `m_mshrs` | pre-frontend sample | config | `add` / final `next_access` |
| MissQ | `m_miss_queue` | sample and actual drain | config | lower-request creation / `cycle` send |
| L2→DRAM queue | `m_L2_dram_queue` | actual `L2interface::push`/pop plus sample | queue config | lower push / partition issue |
| DRAM→L2 / L2→ICNT / ICNT→L2 | FIFO pipelines | pre-frontend sample | queue config | production FIFO operations |
| Fill/data port | `bandwidth_management` | fill head / preview | cache config | production use / replenish |
| WB path | MissQ and L2→DRAM classes | actual committed cache events | shared | dirty victim event / lower issue |

The collector only reads existing state and updates host-side counters. It has
no inputs to cache admission, queue operations, ports, arbitration, or timing.
