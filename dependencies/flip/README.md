# NVIDIA FLIP

This directory vendors the single-header API from NVLabs FLIP v1.7,
commit `b475eb4bf394ab877c42166c9eb0a84a02cc5b14`.

Source: https://github.com/NVlabs/flip

tev builds the normal CPU/OpenMP implementation and can additionally compile the
official `FLIP_ENABLE_CUDA` implementation directly into the Windows executable.

`FLIP.h` has three CUDA-only integration changes while retaining NVLabs' constants
and algorithm:

- `DeviceFLIPConstants` uses aggregate initialization to avoid dynamic CUDA
  startup code before tev selects a backend.
- The magma and viridis GPU images are constructed lazily so process startup
  does not allocate device memory before tev selects the CUDA backend.
- Fatal CUDA paths are routed through an overridable macro. The CUDA translation
  unit turns them into exceptions so tev can fall back to CPU instead of terminating.

The CPU code path is unchanged from the vendored revision.
