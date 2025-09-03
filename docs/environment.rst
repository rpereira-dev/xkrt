Environment Variables
========================

Environment variables available to control the execution can be listed by setting **XKAAPI_HELP=1** and running a program.
Here is a list of environment variables random users might find usefull:

.. csv-table::
   :header: "Environment Variable", "Description"
   :widths: 20, 80

   "XKAAPI_CACHE_LIMIT", "(null)"
   "XKAAPI_D2D_PER_STREAM", "Number of concurrent copies per D2D stream before throttling device-thread"
   "XKAAPI_D2H_PER_STREAM", "Number of concurrent copies per D2H stream before throttling device-thread"
   "XKAAPI_DEFAULT_MATH", "(null)"
   "XKAAPI_DRIVERS", "Exemple: 'cuda,4;hip,2;host,3' - will enable drivers cuda, hip and host respectively with 4, 2, and 3 threads per device."
   "XKAAPI_GPU_MEM_PERCENT", "%% of total memory to allocate initially per GPU (in ]0..100[)"
   "XKAAPI_H2D_PER_STREAM", "Number of concurrent copies per H2D stream before throttling device-thread"
   "XKAAPI_HELP", "Show this helper"
   "XKAAPI_KERN_PER_STREAM", "Number of concurrent kernels per KERN stream before throttling device-thread"
   "XKAAPI_MERGE_TRANSFERS", "Merge memory transfers over continuous virtual memory"
   "XKAAPI_NGPUS", "Number of GPUs to use"
   "XKAAPI_MEMORY_REGISTER_PROTECT_OVERFLOW", "Split memory transfers to avoid overflow over registered/unregistered memory that causes CUDA to crash"
   "XKAAPI_TASK_PREFETCH", "If enabled, after completing a task, initiate data transfers for all its WaR successors that place of execution is already known (else, transfers only start once the successor is ready)"
   "XKAAPI_NSTREAMS_D2D", "Number of D2D streams per GPU"
   "XKAAPI_NSTREAMS_D2H", "Number of D2H streams per GPU"
   "XKAAPI_NSTREAMS_H2D", "Number of H2D streams per GPU"
   "XKAAPI_NSTREAMS_KERN", "Number of KERN streams per GPU"
   "XKAAPI_OFFLOADER_CAPACITY", "Maximum number of pending instructions per stream"
   "XKAAPI_PRECISION", "(null)"
   "XKAAPI_STATS", "Boolean to dump stats on deinit"
   "XKAAPI_USE_P2P", "Boolean to enable/disable the use of P2P transfers"
   "XKAAPI_WARMUP", "Boolean to enable/disable threads/devices warmup on runtime initialization"
   "XKAAPI_VERBOSE", "Verbosity level (the higher the most)"

