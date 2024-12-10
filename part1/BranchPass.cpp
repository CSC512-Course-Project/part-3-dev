#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/Function.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include <fstream>
#include <unordered_map>
#include <vector>
#include <filesystem>
using namespace llvm;

namespace
{

  // Manages CSV output and branch data mapping
  class BranchLogger
  {
  public:
    BranchLogger() : branchID(1) {}

    void logBranch(BranchInst *branchInst, const std::string &filePath)
    {
      if (isConditionalBranch(branchInst))
      {
        int id = getBranchID(branchInst);
        int srcLine = getSourceLine(branchInst);
        int destLine = getDestinationLine(branchInst);
        addBranchToList(filePath, id, srcLine, destLine);
        displayBranchData(filePath, id, srcLine, destLine); // Display branch info on terminal
      }
    }

    void logFunctionPointer(CallInst *callInst)
    {
      if (isIndirectCall(callInst))
      {
        Value *targetFunction = callInst->getCalledOperand();
        if (targetFunction)
        {
          errs() << "*IndirectFuncPtr_" << targetFunction << "\n";
        }
      }
    }

    ~BranchLogger()
    {
      writeCsvFile();
    }

  private:
    std::unordered_map<BranchInst *, int> branchMap;
    std::vector<std::tuple<std::string, int, int, int>> branchDataList; // Holds branch data
    int branchID;

    bool isConditionalBranch(BranchInst *branchInst)
    {
      return branchInst->isConditional();
    }

    int getBranchID(BranchInst *branchInst)
    {
      if (branchMap.find(branchInst) == branchMap.end())
      {
        branchMap[branchInst] = branchID++;
      }
      return branchMap[branchInst];
    }

    int getSourceLine(BranchInst *branchInst)
    {
      return branchInst->getDebugLoc() ? branchInst->getDebugLoc().getLine() : -1;
    }

    int getDestinationLine(BranchInst *branchInst)
    {
      int destLine = -1;
      if (branchInst->getNumSuccessors() > 0)
      {
        BasicBlock *destBlock = branchInst->getSuccessor(0);
        if (!destBlock->empty())
        {
          Instruction &firstInst = *destBlock->begin();
          if (firstInst.getDebugLoc())
          {
            destLine = firstInst.getDebugLoc().getLine();
          }
        }
      }
      return destLine;
    }

    bool isIndirectCall(CallInst *callInst)
    {
      return callInst->isIndirectCall();
    }

    void addBranchToList(const std::string &filePath, int id, int srcLine, int destLine)
    {
      branchDataList.emplace_back(filePath, id, srcLine, destLine);
    }

    void displayBranchData(const std::string &filePath, int id, int srcLine, int destLine) const
    {
      errs() << "br_" << id << ": ";
      errs() << filePath << ", ";
      errs() << srcLine << ", ";
      errs() << destLine << "\n";
    }

    void writeCsvFile()
    {
      // Check if file exists; write header only if it doesn't
      bool fileExists = std::filesystem::exists("branch_data.csv");

      std::ofstream outputFile("branch_data.csv", std::ios::app); // Open in append mode
      if (!fileExists)
      {
        outputFile << "file_path,branch_id,source_line,destination_line\n";
      }

      for (const auto &branchData : branchDataList)
      {
        outputFile << std::get<0>(branchData) << ","
                   << std::get<1>(branchData) << ","
                   << std::get<2>(branchData) << ","
                   << std::get<3>(branchData) << "\n";
      }
      outputFile.close();
      errs() << "CSV data has been written to branch_data.csv\n";
    }
  };

  // Analyzes instructions within a function
  class FunctionAnalyzer
  {
  public:
    FunctionAnalyzer(BranchLogger &logger) : branchLogger(logger) {}

    void analyze(Function &func)
    {
      std::string filePath = getSourceFilePath(func);
      for (auto &instruction : instructions(func))
      {
        analyzeInstruction(&instruction, filePath);
      }
    }

  private:
    BranchLogger &branchLogger;

    std::string getSourceFilePath(Function &func)
    {
      if (DISubprogram *subProg = func.getSubprogram())
      {
        return subProg->getDirectory().str() + "/" + subProg->getFilename().str();
      }
      return "";
    }

    void analyzeInstruction(Instruction *inst, const std::string &filePath)
    {
      if (auto *branchInst = dyn_cast<BranchInst>(inst))
      {
        branchLogger.logBranch(branchInst, filePath);
      }
      else if (auto *callInst = dyn_cast<CallInst>(inst))
      {
        branchLogger.logFunctionPointer(callInst);
      }
    }
  };

  // LLVM pass to analyze branches and function pointers
  struct BranchAnalysisPass : PassInfoMixin<BranchAnalysisPass>
  {
    PreservedAnalyses run(Function &func, FunctionAnalysisManager &)
    {
      BranchLogger branchLogger;
      FunctionAnalyzer analyzer(branchLogger);
      analyzer.analyze(func);
      return PreservedAnalyses::all();
    }

    static bool isRequired() { return true; }
  };

} // namespace

// Plugin registration
llvm::PassPluginLibraryInfo getBranchAnalysisPluginInfo()
{
  return {LLVM_PLUGIN_API_VERSION, "BranchAnalysisPass", LLVM_VERSION_STRING,
          [](PassBuilder &builder)
          {
            builder.registerPipelineParsingCallback(
                [](StringRef passName, FunctionPassManager &fpm, ArrayRef<PassBuilder::PipelineElement>)
                {
                  if (passName == "branch-analysis-pass")
                  {
                    fpm.addPass(BranchAnalysisPass());
                    return true;
                  }
                  return false;
                });
          }};
}

extern "C" ::llvm::PassPluginLibraryInfo llvmGetPassPluginInfo()
{
  return getBranchAnalysisPluginInfo();
}
