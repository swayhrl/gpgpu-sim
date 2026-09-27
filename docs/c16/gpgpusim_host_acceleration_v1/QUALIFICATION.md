# C16 GPGPU-Sim host acceleration qualification V1

Status: `HOST_ACCEL_NO_SAFE_MATERIAL_GAIN`.

Authority base: `0271de82432db004beed43280ed01057246a0f2c` (scientific parent `a2322069b9701597db7019080b5b54d29518e3a2`).

The only source candidate, `5bf79a3635ae4e84e4c0f5b8e7bbc46c48f7778e`, added disabled-mode guards around PTX/source-line statistics. It preserved exact normalized simulator output on the accepted 16-kernel real prefix, but five committed-binary repetitions showed no repeatable host gain: config=1 median 7.055922120 s versus config=0 median 7.161037644 s (-1.49%). The combined candidate was also slower than the pristine same-build median (6.528739090 s) and current authority binary median (6.121070993 s).

The candidate was therefore reverted by `c583a6f36d232bfc69f92a73e6b17a5080ebfaf3`. Production simulator source after the revert is tree-equivalent to the authority base for the candidate paths. The exact rejected binary SHA-256 is `610c1a5a4d0ba8e11c7d80b7ed8882d79bb66a85fe7bdaca701dec7683cc14c4`; it is evidence only and must not be used for future runs.

The full profile, equivalence matrices, raw-receipt closure, runner/config recommendation, and restart assessment live in the Framework review pack `C16_GPGPUSIM_HOST_ACCELERATION_QUALIFICATION_V1`.
