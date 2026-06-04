# Mascar M8C Guidance: Mechanism Activation Trend Analysis

## Stage

This is M8C.

Input: W8 actual results, W8 run plan

Goal:
- Summarize activation of M2/M3/M4
- Generate preliminary trend report
- Identify workloads still not triggering mechanisms
- Produce W8 trend CSV/summary

Tasks:
1. Parse results CSV and collect per-workload counters:
   - M2: MP cycles, owner acquire
   - M3: hit-only attempt/hit/NACK
   - M4: enqueue/retry/hit/NACK
2. Compute summary:
   - number of workloads triggering M2/M3/M4
   - memory vs compute
   - rows needing further activation input
3. Generate:
   - w8_activation_summary.csv
   - w8_trend_summary.md
   - docs/papers/mascar_w8_activation_trend_report.md
4. Update postcheck with runtime, counts, warnings

Validation:
- Ensure all 3 ready workloads in W7 subset appear
- Stats match W8 run manifest
- Report clearly separates triggered vs non-triggered counters
