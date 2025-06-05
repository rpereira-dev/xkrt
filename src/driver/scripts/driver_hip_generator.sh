#!/bin/sh

# https://stackoverflow.com/questions/59895/how-do-i-get-the-directory-where-a-bash-script-is-located-from-within-the-script
SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
HIPIFY=$SCRIPT_DIR/hipify-perl
DRIVER_CUDA=$SCRIPT_DIR/../driver_cu.cc
DRIVER_HIP=$SCRIPT_DIR/../driver_hip.cc

#$HIPIFY $DRIVER_CUDA > $DRIVER_HIP
#sed -i 's/cuda/hip/g' $DRIVER_HIP
#sed -i 's/CUDA/HIP/g' $DRIVER_HIP
#sed -i 's/cu\./hip\./g' $DRIVER_HIP
#sed -i 's/CU_/HIP_/g' $DRIVER_HIP
#sed -i 's/cublas/hipblas/g' $DRIVER_HIP
#sed -i 's/CUBLAS/HIPBLAS/g' $DRIVER_HIP
#sed -i 's/<hipblas.h/<hipblas\/hipblas.h/g' $DRIVER_HIP
sed -i 's/hwloc\/hip\/hip_runtime.h/hwloc\/rsmi.h/g' $DRIVER_HIP
