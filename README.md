# XKAAPI V2

Welcome to the new experimental XKaapi implementation.   
This repository is highly experimental and not yet fully compatible with older XKaapi/XKBlas releases located at http://gitlab.inria.fr/xkblas/versions.

# Related Projects
This repository hosts the XKaapi runtime system.    
Other repository hosts specialization layers built on top of the runtime:
- XKBlas is a multi-gpu BLAS implementation that allows the tiling and composition of kernels, with asynchronous overlap of computation/transfers. If you want a copy of XKBlas, please contact thierry.gautier@inrialpes.fr or rpereira@anl.gov (https://gitlab.inria.fr/xkblas/dev)
- XKBM is a suite of benchmark for measuring multi-gpu architectures performances, to assist in the design of runtime systems: https://github.com/anlsys/xkbm
- XKOMP is an experimental OpenMP runtime built on top of XKaapi. Please contact rpereira@anl.gov if you want a copy

# Getting started

## Installation
### Requirements
- A C/C++ compiler with support for C++20 (the only compiler tested is LLVM >=20.x)
- hwloc - https://github.com/open-mpi/hwloc

### Optional
- Cuda, HIP, Level Zero, SYCL, OpenCL
- CUBLAS, HIPBLAS, ONEAPI::MKL
- NVML, RSMI, Level Zero Sysman
- Cairo - https://github.com/msteinert/cairo - to visualize memory trees

### Build command example

See the `CMakeLists.txt` file for all available options.

```bash
# with support for only the host driver and debug modes (useful for developing on local machines)
CC=clang CXX=clang++ cmake -DUSE_STATS=on -DCMAKE_BUILD_TYPE=Debug ..

# with support for Cuda
CC=clang CXX=clang++ CMAKE_PREFIX_PATH=$CUDA_PATH:$CMAKE_PREFIX_PATH cmake -DUSE_CUDA=on ..
```

## Available environment variable
- `XKAAPI_HELP=1` - displays available environment variables.

# Directions for improvements
- Add a memory coherency controller for 'point' accesses, to retrieve original xkblas/kaapi behavior
- If OCR is set on a successor task, when the predecessor writter completes
  - the successor device is known: set it already
  - if a reader predecessor completes and the device is known, transfer can be initiated without waiting for all predecessors to complete
- Tasks descriptor are allocated on the producer thread memory... while it will be heavily accessed and modified by consumers
- Tasks are currently only deleted all-at-once on `invalidate` calls.
- `xkrt-init` could be removed/made noop - so all stuff got initialized lazily
- allow C++ capture that run onto device threads
- add support for blas compact symetric matrices
- add support for commutative write, maybe with a priority-heap favoring accesses with different heuristics (the most successors, the most volume of data as successors, etc...)
- add support for IA/ML devices (most of them only have high-level Python API, only Graphcore seems to have a good C API)