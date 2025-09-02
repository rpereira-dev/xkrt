# XKaapi 2

Welcome to the new experimental XKaapi implementation.   
This repository is highly experimental and not yet fully compatible with older XKaapi/XKBlas releases located at http://gitlab.inria.fr/xkblas/versions.
It is a C++ implementation and interfaces of the XKaapi dataflow and task-based programming model.

Please open any issue at https://github.com/rpereira-dev/xkaapi

# Related Projects
This repository hosts the XKaapi runtime system.    
Other repository hosts specialization layers built on top of the runtime:
- XKBlas is a multi-gpu BLAS implementation that allows the tiling and composition of kernels, with asynchronous overlap of computation/transfers. If you want a copy of XKBlas, please contact thierry.gautier@inrialpes.fr or rpereira@anl.gov (https://gitlab.inria.fr/xkblas/dev)
- XKBM is a suite of benchmark for measuring multi-gpu architectures performances, to assist in the design of runtime systems: https://github.com/anlsys/xkbm
- XKOMP is an experimental OpenMP runtime built on top of XKaapi: https://github.com/anlsys/xkomp

# Getting started

## Installation
### Requirements
- A C/C++ compiler with support for C++20 (the only compiler tested is LLVM >=20.x)
- hwloc - https://github.com/open-mpi/hwloc

### Optional
- Cuda, HIP, Level Zero, SYCL, OpenCL
- CUBLAS, HIPBLAS, ONEAPI::MKL
- NVML, RSMI, Level Zero Sysman
- AML - https://github.com/anlsys/aml
- Cairo - https://github.com/msteinert/cairo - for debugging purposes, to visualize memory trees

### Build command example

See the `CMakeLists.txt` file for all available options.

```bash
# with support for only the host driver and debug modes, typically for developing on local machines with no GPUs
CC=clang CXX=clang++ cmake -DUSE_STATS=on -DCMAKE_BUILD_TYPE=Debug ..

# with support for Cuda
CC=clang CXX=clang++ CMAKE_PREFIX_PATH=$CUDA_PATH:$CMAKE_PREFIX_PATH cmake -DUSE_CUDA=on ..

# with support for Cuda and all optimization
CC=clang CXX=clang++ CMAKE_PREFIX_PATH=$CUDA_PATH:$CMAKE_PREFIX_PATH cmake -DUSE_CUDA=on -DUSE_SHUT_UP=on -DENABLE_HEAVY_DEBUG=off -DCMAKE_BUILD_TYPE=Release ..
```

## Available environment variable
- `XKAAPI_HELP=1` - displays available environment variables.

# Directions for improvements
- Add C/Julia bindings
- Rework interface for distributions - so it is an abstract object that may be passed to various constructs - look at what PGAS do
- Add explicit/synchronous driver interfaces, to allow users to bypas event/polling/coherency/tasking. Particularly for OpenMP blocking constructs
- Add a memory coherency controller for 'point' accesses, to retrieve original xkblas/kaapi behavior - that is a decentralized and paged-base coherency protocol
- Tasks are currently only deleted all-at-once on `invalidate` calls.
- Stuff from `xkrt-init` could be moved for lazier initializations
- Add support for blas compact symetric matrices
- Add support for GDRCopy in the Cuda Driver (https://developer.nvidia.com/gdrcopy?utm_source=chatgpt.com) - for low overhead transfer using CPUs instead of GPUs DMAs
- Add support for commutative write, maybe with a priority-heap favoring accesses with different heuristics (the most successors, the most volume of data as successors, etc...)
- Add support for IA/ML devices (most of them only have high-level Python API, only Graphcore seems to have a good C API, but Graphcore seems to be dying)
