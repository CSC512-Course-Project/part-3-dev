# take 1 command line argument - the name of file to be tested

rm -r build
rm -r IR_Outputs
mkdir build
mkdir IR_Outputs
cd build
cmake ..
make
cd ..

echo "file_path,branch_id,source_line,destination_line" > branch_data.csv

clang -g -o0 -S -emit-llvm -c tests/$1.c -o IR_Outputs/$1-1.ll

clang -g -o0 -fpass-plugin=build/libSeminalInputFeaturesAnalysis.so -emit-llvm -c tests/$1.c -o IR_Outputs/$1-2.ll
opt -load-pass-plugin build/libBranchPass.so -passes=branch-analysis-pass -disable-output IR_Outputs/$1-1.ll