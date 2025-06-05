all: ze cu

ze:
	clang -cc1 -triple spir-unknown-unknown -cl-std=CL3.0 -emit-llvm -o empty-kernel.ll -finclude-default-header empty-kernel.cl
	llvm-as empty-kernel.ll -o empty-kernel.bc
	llvm-spirv empty-kernel.bc -o empty-kernel.spv
	cat empty-kernel.spv | xxd -p -c 12 | sed 's/../0x&, /g' > empty-kernel.spv.bytes

cu:
	nvcc -ptx empty-kernel.cu -o empty-kernel.ptx --generate-code arch=compute_50,code=sm_50
	cat empty-kernel.ptx | xxd -p -c 12 | sed 's/../0x&, /g' > empty-kernel.ptx.bytes
	ptxas empty-kernel.ptx -o empty-kernel.cubin
	cat empty-kernel.cubin | xxd -p -c 12 | sed 's/../0x&, /g' > empty-kernel.cubin.bytes
