/**
 * @file SeminalInputFeaturesAnalysis.cpp
 * @brief This file contains the implementation of the SeminalInputFeaturesAnalysis pass
 * @author Srinath Srinivasan (ssrini27@ncsu.edu)
 * @bug No known bugs
 * @attention Please make sure FILE_NAME is set to the correct file path before running the pass
 *
 * This file contains the implementation of the SeminalInputFeaturesAnalysis pass.
 * The pass performs the following tasks:
 * 1. Find conditional branching and store them
 * 2. Map the variables to their names (using debug information)
 * 3. Find the ways in which the mapped variables are used
 * 4. Perform the analysis and print the outputs to a file
 */

#include <string>
#include <vector>
#include <map>
#include <set>
#include <sstream>
#include <fstream>

#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/LoopAnalysisManager.h"

using namespace llvm;

namespace
{
    class SeminalInputFeaturesAnalysis : public PassInfoMixin<SeminalInputFeaturesAnalysis>
    {
    public:
        /**
         * @brief Serves as the entry point for the analysis (main function of the pass)
         * @param M The module to be analyzed
         * @param MAM The module analysis manager
         * @return PreservedAnalyses
         *
         * This function is the main function of the pass. It is called by the pass manager to run the analysis.
         * The function performs the following tasks:
         * 1. Find conditional branching and store them
         * 2. Map the variables to their names (using debug information)
         * 3. Find the ways in which the mapped variables are used
         * 4. Perform the analysis and print the outputs to a file
         */
        PreservedAnalyses run(Module &M, ModuleAnalysisManager &MAM)
        {
            writeToFile("\n=====================================\n");
            writeToFile("Seminal Input Features Analysis Pass:\n");
            writeToFile("=====================================\n");

            // OBJECTIVE: Find conditional branching

            for (Function &F : M)
            {
                for (BasicBlock &BB : F)
                {
                    for (Instruction &I : BB)
                    {
                        if (auto *BI = dyn_cast<BranchInst>(&I))
                        {
                            if (BI->isConditional())
                            {
                                branches.push_back(BI);
                            }
                        }
                        else if (auto *DI = dyn_cast<DbgDeclareInst>(&I))
                        {
                            std::string varName = DI->getVariable()->getName().str();

                            // Map the address of the variable to its name.
                            variables[DI->getAddress()] = varName;

                            // Collect all instructions that use this variable.
                            std::set<Instruction *> dependents;
                            defUseAnalysis(DI->getAddress(), dependents);

                            // Iterate over all users of the variable.
                            for (auto dep : dependents)
                            {
                                // if load instruction uses this var in an operand, store the variable name with its address as key
                                if (auto LI = dyn_cast<LoadInst>(dep))
                                {
                                    if (LI->getPointerOperand() == DI->getAddress())
                                    {
                                        variables[LI] = varName;
                                    }
                                }
                                else if (auto UI = dyn_cast<UnaryInstruction>(dep))
                                {
                                    // unary only has 1 operand
                                    variables[UI] = varName;
                                }
                                else if (auto SI = dyn_cast<StoreInst>(dep))
                                {
                                    // If store instruction uses this var in an operand, store the variable name with its address as key
                                    if (SI->getValueOperand() == DI->getAddress())
                                    {
                                        variables[SI] = varName;
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // OBJECTIVE: Go through the call instructions and find the ways in which our mapped variables are used

            for (Function &F : M)
            {
                for (BasicBlock &BB : F)
                {
                    for (Instruction &I : BB)
                    {
                        if (auto CI = dyn_cast<CallInst>(&I))
                        {
                            if (CI->getCalledFunction())
                            {
                                checkCallInst(CI);
                            }
                        }
                    }
                }
            }

            // OBJECTIVE: Perform the analysis and print the outputs to a file

            performAnalysis();

            writeToFile("\n==================================\n");
            writeToFile("Analysis of Loops:");
            writeToFile("\n==================================\n");

            // OBJECTIVE: Perform Loop Analysis

            auto &FAM = MAM.getResult<FunctionAnalysisManagerModuleProxy>(M).getManager();

            for (Function &F : M)
            {
                if (F.isDeclaration())
                {
                    continue;
                }

                auto &LI = FAM.getResult<LoopAnalysis>(F);

                for (Loop *L : LI)
                {
                    analyzeLoop(L);
                }
            }

            writeToFile("\n====================================\n");
            writeToFile("End of Seminal Input Features Analysis Pass\n");

            return PreservedAnalyses::all();
        }

    private:
        /**
         * @brief Define the variables that will be used throughout the analysis.
         *
         * The variables are:
         * 1. flag_for_file: A flag to check if the file has been created or not.
         * 2. variables: Stores the variables mapped to their names
         * 3. dependentInstructions: Mapped instructions to their dependent instructions.
         * 4. branches: All conditional branches are stored here
         * 5. calledFunc: Any function that may be called by user input functions
         * 6. libraryFunctions: A list of library functions that are considered for analysis
         * 7. FILE_NAME: The name of the file where the output will be written
         */

        bool flag_for_file = false;

        std::map<Value *, std::string> variables;
        std::map<Value *, std::set<Instruction *>> dependentInstructions;
        std::vector<BranchInst *> branches;
        std::map<Value *, std::string> calledFunc;
        std::vector<std::string> libraryFunctions = {"getc", "fopen", "scanf", "fclose", "fread", "fwrite"};

        std::string FILE_NAME = "analysis_output.txt";

        /**
         * @brief Perform def-use analysis on the given value(instruction) and find its dependent instructions
         * @param v The instruction to be analyzed
         * @param dependents The set of dependent instructions
         *
         * This function performs def-use analysis on the given instruction and finds its dependent instructions.
         * It recursively calls itself on the dependent instructions to find their dependent instructions.
         * Once we have the dependent instructions mapped, we are able to use to find the ways in which the variables are used.
         */
        void defUseAnalysis(Value *v, std::set<Instruction *> &dependents)
        {
            // Check for value
            if (!v)
            {
                return;
            }

            for (User *U : v->users())
            {
                if (Instruction *I = dyn_cast<Instruction>(U))
                {
                    // dont redo work
                    if (dependents.find(I) != dependents.end())
                    {
                        return;
                    }

                    // add this instruction to set of dependents
                    dependents.insert(I);

                    /* we choose to only perform recursive analysis on store and return
                    instructions as they are the only ones that can change the value of a variable

                    If we choose to explore other instructions, our analyzed variables may be incorrect
                    as we reutrn when we find a dependent instruction that is already analyzed
                    */

                    if (StoreInst *SI = dyn_cast<StoreInst>(I))
                    {
                        defUseAnalysis(SI->getPointerOperand(), dependents);
                    }
                    // recursively call the defUseAnalysis function on the return value of the return instruction
                    else if (ReturnInst *RI = dyn_cast<ReturnInst>(I))
                    {
                        if (RI->getReturnValue())
                        {
                            defUseAnalysis(RI->getFunction(), dependents);
                        }
                    }
                    else
                    {
                        // recursively call defUseAnalysis
                        defUseAnalysis(I, dependents);
                    }
                }
            }
        }

        /**
         * @brief For call instructions that are user input functions, find the ways in which the mapped variables are used
         * @param CI The call instruction to be analyzed
         *
         * This function performs the following tasks:
         * 1. Check if the function is among the library functions
         * 2. Find the dependent instructions of the call instruction
         * 3. Perform def-use analysis on the arguments of the function that is being called (if call by reference)
         * 4. Trace the instruction for variable name mapping
         */
        void checkCallInst(CallInst *CI)
        {
            std::string FuncName = CI->getCalledFunction()->getName().str();

            // check if function among library functions. If not then we are not interested
            if (std::find(libraryFunctions.begin(), libraryFunctions.end(), FuncName) == libraryFunctions.end())
            {
                return;
            }

            // check if the function is in the dependentInstructions, if its not, add it
            if (dependentInstructions.find(CI) == dependentInstructions.end())
            {
                dependentInstructions[CI] = std::set<Instruction *>();
                calledFunc[CI] = FuncName;
            }

            // Perform def-use analysis to find the dependent instructions
            defUseAnalysis(CI, dependentInstructions[CI]);

            // Perform def-use analysis on the arguments of the function
            for (auto arg = CI->arg_begin(); arg != CI->arg_end(); ++arg)
            {
                // only perform analysis on pointer arguments (others maybe call by value)
                if (arg->get()->getType()->getTypeID() == Type::TypeID::PointerTyID)
                {
                    auto argVal = arg->get();
                    defUseAnalysis(argVal, dependentInstructions[CI]);
                }
            }

            Instruction *nextInst = CI;

            /* Go through previously mapped variables and check if they are instructions
            that are dependent on the current call instruction. if so, update the variables map
            to reflect that the current call instuction is responsible for the use of the variable
            */

            for (auto var : variables)
            {
                if (auto SI = dyn_cast<StoreInst>(var.first))
                {
                    if (SI->getOperand(0) == nextInst)
                    {
                        variables[CI] = variables[var.first];
                        break;
                    }
                }
                else if (auto CAI = dyn_cast<CastInst>(var.first))
                {
                    if (CAI->getOperand(0) == CI)
                    {
                        nextInst = CAI;
                    }
                }
            }
        }

        /**
         * @brief Perform the analysis and print the outputs to a file
         *
         * This function performs the following tasks:
         * 1. Iterate over all the conditional branches
         * 2. For each branch, check if the condition is a comparison instruction
         * 3. If it is, find the operands of the comparison instruction
         * 4. Find the dependent instructions of the operands
         * 5. If the dependent instructions are found, print the output to a file
         * 6. The output includes the line number of the branch, the seminal input detected, and the function used
         */
        void performAnalysis()
        {
            for (auto branch : branches)
            {
                auto condition = branch->getCondition();

                if (isa<ICmpInst>(condition))
                {
                    std::map<Value *, std::vector<std::string>> branch_call_mapping;
                    auto cmp = dyn_cast<ICmpInst>(condition);
                    auto op1 = cmp->getOperand(0);
                    auto op2 = cmp->getOperand(1);

                    for (auto inst : dependentInstructions)
                    {
                        for (auto dep : inst.second)
                        {
                            if (dep == op1 || dep == op2 || dep == cmp)
                            {
                                if (variables.find(dep) != variables.end())
                                {
                                    branch_call_mapping[inst.first].push_back(variables.at(dep));
                                }
                            }
                        }
                    }

                    if (branch_call_mapping.size() > 0)
                    {
                        writeToFile("\nAt Line # " + std::to_string(branch->getDebugLoc()->getLine()) + ": ");
                        writeToFile("\nSeminal Input variable: ");
                        bool first = true;

                        for (auto call : branch_call_mapping)
                        {
                            if (first)
                            {
                                for (auto var : call.second)
                                {
                                    writeToFile(var + ", ");
                                }
                                writeToFile("\n");
                                first = false;
                            }

                            std::string funcName = calledFunc.find(call.first) != calledFunc.end() ? calledFunc.at(call.first) : "unkown";

                            if (auto CI = dyn_cast<CallInst>(call.first))
                            {
                                writeToFile("used at Function \"" + funcName + "\" on Line #" + std::to_string(CI->getDebugLoc()->getLine()) + "\n");
                            }
                        }
                    }
                }
            }
        }

        /**
         * @brief Analyze a loop and find the variables that are influential in the loop condition
         * @param L The loop to be analyzed
         *
         * This function analyzes the loop and finds the variables that are influential in the loop condition.
         * It performs the following tasks:
         * 1. Find the exit blocks of the loop
         * 2. Analyze the exit conditions to see the variables that are influential in the loop condition
         * 3. Record the variables that are influential in the loop condition
         */
        void analyzeLoop(Loop *L)
        {
            SmallVector<BasicBlock *, 8> ExitBlocks;
            L->getExitBlocks(ExitBlocks);

            std::map<std::string, std::set<std::string>> variablesInLine;

            // Analyze exit conditions to see the variables that are influential in the loop condition
            for (BasicBlock *ExitBlock : ExitBlocks)
            {
                for (auto &I : *ExitBlock)
                {
                    if (auto *Cmp = dyn_cast<ICmpInst>(&I))
                    {
                        for (unsigned i = 0; i < Cmp->getNumOperands(); ++i)
                        {
                            if (i == 1 && variables.find(Cmp->getOperand(i)) != variables.end())
                            {
                                Value *Operand = Cmp->getOperand(i);

                                std::string operandName = variables[Operand];

                                variablesInLine[operandName].insert(std::to_string(Cmp->getDebugLoc()->getLine()));
                            }
                        }
                    }
                }
            }

            // record the variables that are influential in the loop condition

            for (auto var : variablesInLine)
            {
                writeToFile("\nInfluential loop operand " + var.first + " used in Lines:");
                bool first = true;
                for (auto line : var.second)
                {
                    writeToFile(" #" + line + "");
                }
            }
        }

        /**
         * @brief Write the content to a file
         * @param content The content to be written to the file
         *
         * This function writes the content to a file. If the file does not exist, it creates a new file.
         * If the file exists,it deletes the file and creates a new file. (to avoid appending to existing file)
         */
        void writeToFile(std::string content)
        {
            std::ofstream file;
            // Delete file if it already exists, otherwise create a new file.
            if (!flag_for_file)
            {
                file.open(FILE_NAME, std::fstream::out | std::fstream::trunc);
                flag_for_file = true;
            }
            // Open the file in append mode (after start)
            else
            {
                file.open(FILE_NAME, std::fstream::app);
            }
            file << content;
            file.close();
        }
    };
}

extern "C" LLVM_ATTRIBUTE_WEAK PassPluginLibraryInfo llvmGetPassPluginInfo()
{
    return {
        .APIVersion = LLVM_PLUGIN_API_VERSION,
        .PluginName = "SeminalInputFeaturesAnalysis",
        .PluginVersion = "v0.1",
        .RegisterPassBuilderCallbacks = [](PassBuilder &PB)
        {
            PB.registerPipelineStartEPCallback(
                [](ModulePassManager &MPM, OptimizationLevel Level)
                {
                    MPM.addPass(SeminalInputFeaturesAnalysis());
                });
        }};
}