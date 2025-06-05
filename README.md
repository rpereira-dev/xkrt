# XKAAPI V2

Welcome to the new experimental XKaapi implementation.
This repository is highly experimental and not yet fully compatible with older XKaapi/XKBlas releases located at http://gitlab.inria.fr/xkblas/versions.

## ENVIRONMENT VARIABLES
- `XKAAPI_HELP=1` - displays available environment variables
Not that alternative environement name could start by XKRT_

## BUILD EXAMPLE
Must have hwloc installed and be sure your `CMAKE_PREFIX_PATH` holds libs/include locations
```bash
mkdir build-debug
cd build-debug
CC=clang CXX=clang++ CMAKE_PREFIX_PATH=$ONEAPI_ROOT:$CUDA_PATH:/usr:$CMAKE_PREFIX_PATH cmake -DCMAKE_INSTALL_PREFIX=$HOME/install/xkrt/debug-dgx -DCMAKE_BUILD_TYPE=Debug -DSTRICT=on -DUSE_STATS=on -DUSE_CUDA=on -DUSE_ZE=off -DUSE_SYCL=off -DUSE_ZE_SYCL_INTEROP=off -DUSE_CL=off -DUSE_HIP=off -DENABLE_HEAVY_DEBUG=off -DUSE_CAIRO=off -DUSE_NVML=off ..
cmake -DCMAKE_INSTALL_PREFIX=$HOME/install/xkrt/debug -DCMAKE_BUILD_TYPE=Debug -DUSE_STATS=on -DUSE_CUDA=on ..
```

See the `CMakeLists.txt` file for all available options.

## To improve
- If OCR is set on a successor task, when the predecessor writter completes
  - the successor device is known: set it already
  - if reader predecessor completes and the device is known, transfer can be initiated without waiting for all predecessors to complete
- Tasks descriptor is allocated in the producer thread memory... while it will be heavily accessed and modified by consumers
- Tasks are currently never deleted
- Merge continuous memory block to a single transfer - it is unclear if we win on this or not

## Future Directions
- remove/(make useless) xkrt-init - so all stuff got initialized lazily
- allow C++ capture that run onto device threads
- implement other access type (interval 1D, blas compact symetric)
- sycl backend to finally get the shit running on Aurora
