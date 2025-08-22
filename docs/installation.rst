Installation
============

This guide will help you install **XKRT** and its dependencies.

Requirements
------------

- A C/C++ compiler with support for C++20 (the only compiler tested is LLVM >=20.x)
  hwloc - https://github.com/open-mpi/hwloc

Optional
--------------------

- Cuda, HIP, Level Zero, SYCL, OpenCL
- CUBLAS, HIPBLAS, ONEAPI::MKL
- NVML, RSMI, Level Zero Sysman
- Cairo - https://github.com/msteinert/cairo - for debugging purposes, to visualize memory trees

Installing from Source
----------------------

If you’re installing directly from the source code:

.. code-block:: bash

    # with support for Cuda and all optimization
    CC=clang CXX=clang++ CMAKE_PREFIX_PATH=$CUDA_PATH:$CMAKE_PREFIX_PATH cmake -DUSE_CUDA=on -DUSE_SHUT_UP=on -DENABLE_HEAVY_DEBUG=off -DCMAKE_BUILD_TYPE=Release ..
    make install

Development Installation
------------------------

For contributors,

.. code-block:: bash

    # with support for only the host driver and debug modes, typically for developing on local machines with no GPUs
    CC=clang CXX=clang++ cmake -DUSE_STATS=on -DCMAKE_BUILD_TYPE=Debug ..
    make -j

After building, verify it works:

.. code-block:: bash

   ./tests/init
