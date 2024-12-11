# part-3-dev
 
# CSC512 Course Project (Part-3-dev)
This project consists of 2 passes. First tool to trace branch-pointer operations. The second tool to extract seminal input features from C code using LLVM.

The second tool has branch pointer tracing built into it. The first pass generates branch info.

#### Running:

##### NOTE: Please only run on VCL (pass relies on debug instructions which may not generate correctly on other platforms (ex: MacOS))

##### Please use the script located in the working directory to run the LLVM passes on a given test. Replace \<file_name\> with file names given under Testing below
`bash clean_build.sh <file_name>`

#### Testing:

2 simple and 3 complex (>200 LOC) test files are located in the tests folder. To test them please use these commands:
1. (Simple) file_copy.c - `bash clean_build.sh file_copy`
2. (Simple) merge_two.c - `bash clean_build.sh merge_two`
3. (Non-trivial) file_manupulation.c - `bash clean_build.sh file_manupulation`
4. (Non-trivial) dmc_unrar.c - `bash clean_build.sh dmc_unrar`
5. (Non-trivial) stb_vorbis.c - `bash clean_build.sh stb_vorbis`

**NOTE:** Some test files may not meet the conditions in the analysis for pass 2 (some simple tests may not populate loop analysis) (try the complex files)

#### Outputs:
The Pass 1 branch pointer trace info should populate in the working directory as `branch_data.csv`
The Pass 2 seminal input features analysis file should populate in the working directory as `analysis_output.txt` by default

**NOTE:** The code by default deletes any previous analysis/branch_data file and rewrites it. So please look/save an output before running the next test.
