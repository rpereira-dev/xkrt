all: clang ocloc

ocloc:
	ocloc compile -file diffusion.cl -device \* -options "-I ../../include"

clang:
	clang -cc1 -triple spir-unknown-unknown -cl-std=CL3.0 -emit-llvm -o diffusion.ll -finclude-default-header diffusion.cl $(CFLAGS) -I ../../include
	llvm-as diffusion.ll -o diffusion.bc
	llvm-spirv diffusion.bc -o diffusion.spv
	cat diffusion.spv | xxd -p -c 12 | sed 's/../0x&, /g' > diffusion.spv.bytes

