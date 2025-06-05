#include <stdio.h>
#include <stdlib.h>
#include <CL/cl.h>

// Function to read the kernel source from a file
char* readKernelSource(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        perror("Failed to open file");
        exit(EXIT_FAILURE);
    }

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char* source = (char*)malloc(size + 1);
    if (!source) {
        perror("Failed to allocate memory");
        fclose(file);
        exit(EXIT_FAILURE);
    }

    fread(source, 1, size, file);
    source[size] = '\0';
    fclose(file);

    return source;
}

// Function to write SPIR-V binary to a file
void writeSpirvBinary(const char* filename, const unsigned char* binary, size_t size) {
    FILE* file = fopen(filename, "wb");
    if (!file) {
        perror("Failed to open file");
        exit(EXIT_FAILURE);
    }

    fwrite(binary, 1, size, file);
    fclose(file);
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <input_kernel.cl> <output_binary.spv> <options>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char* inputFilename = argv[1];
    const char* outputFilename = argv[2];
    const char* options = argv[3];

    // Read the kernel source from the input file
    char* kernelSource = readKernelSource(inputFilename);

    // Get platform and device information
    cl_platform_id platform_id = NULL;
    cl_device_id device_id = NULL;
    cl_uint ret_num_devices;
    cl_uint ret_num_platforms;
    cl_int ret = clGetPlatformIDs(1, &platform_id, &ret_num_platforms);
    ret = clGetDeviceIDs(platform_id, CL_DEVICE_TYPE_DEFAULT, 1, &device_id, &ret_num_devices);

    // Create an OpenCL context
    cl_context context = clCreateContext(NULL, 1, &device_id, NULL, NULL, &ret);

    // Create a program from the kernel source
    cl_program program = clCreateProgramWithSource(context, 1, (const char**)&kernelSource, NULL, &ret);

    // Build the program with SPIR-V options
    ret = clBuildProgram(program, 1, &device_id, options, NULL, NULL);
    if (ret != CL_SUCCESS) {
        fprintf(stderr, "Failed to build program\n");
        size_t len;
        clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_LOG, 0, NULL, &len);
        char* buffer = (char*)malloc(len);
        clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_LOG, len, buffer, NULL);
        fprintf(stderr, "%s\n", buffer);
        free(buffer);
        clReleaseProgram(program);
        clReleaseContext(context);
        free(kernelSource);
        return EXIT_FAILURE;
    }

    // Get the SPIR-V binary
    size_t binarySize;
    ret = clGetProgramInfo(program, CL_PROGRAM_BINARY_SIZES, sizeof(size_t), &binarySize, NULL);
    unsigned char* binary = (unsigned char*)malloc(binarySize);
    ret = clGetProgramInfo(program, CL_PROGRAM_BINARIES, sizeof(unsigned char*), &binary, NULL);

    // Write the SPIR-V binary to the output file
    writeSpirvBinary(outputFilename, binary, binarySize);

    // Clean up
    free(binary);
    clReleaseProgram(program);
    clReleaseContext(context);
    free(kernelSource);

    return EXIT_SUCCESS;
}
