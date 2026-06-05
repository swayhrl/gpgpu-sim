# Experiments Common Infrastructure

Common experiment infrastructure lives here. The GPGPU-Sim matrix runner and collector are reused by paper-specific and suite-level workflows.

The W20 suite framework intentionally shells out to the common collector so Mascar M1-M4 counters, W15 `paper_mascar_m3diag_*` counters, and W16 power/energy fields remain available.
