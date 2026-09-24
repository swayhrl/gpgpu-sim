# ORACLE_ELASTIC_QWEIGHT_RESIDENCY_V1 sidecar schema

The functional identity is `ORACLE_SOFTWARE_REGION_TAG`. The sidecar is a canonical UTF-8 TSV file and its complete byte stream is authenticated before any interval is admitted.

Header (exact):

```text
ORACLE_ELASTIC_QWEIGHT_RESIDENCY_V1<TAB>1<TAB>MODELED_L2_GET_ADDR
```

Each following nonempty row has exactly four tab-separated fields:

1. inclusive interval start, accepted by `strtoull(..., base=0)`;
2. exclusive interval end;
3. nonempty allocation identity;
4. unsigned 32-bit diagnostic target-class ID.

Rows must be sorted by start address, nonempty, and nonoverlapping. The configured lowercase SHA-256 must match the file exactly. Lookup is half-open: `begin <= mem_fetch::get_addr() < end`.

Only non-write `GLOBAL_ACC_R` requests may match. PTE, instruction, local, constant, texture, synthetic VM, allocation/write and writeback requests are excluded by access type before interval lookup. Preserved SimVA is not assumed; admission of a future producer sidecar remains gated on independent `TRACE_ADDRESS_NAMESPACE -> L2 get_addr()` qualification.

Changing the sidecar cannot retroactively retag resident lines. V1 has no hit promotion.
