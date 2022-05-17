#include "llvm/IRReader/IRReader.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/ScalarEvolutionExpressions.h"
#include "llvm/Transforms/Utils/Mem2Reg.h"

using namespace llvm;

//===----------------------------------------------------------------------===//
// Command line options
//===----------------------------------------------------------------------===//
static cl::OptionCategory CallCounterCategory{"call counter options"};

static cl::opt<std::string> InputModule{cl::Positional,
                                        cl::desc{"<Module to analyze>"},
                                        cl::value_desc{"bitcode filename"},
                                        cl::init(""),
                                        cl::Required,
                                        cl::cat{CallCounterCategory}};

//===----------------------------------------------------------------------===//
// static - implementation
//===----------------------------------------------------------------------===//


static void countStaticCalls(Module &M) {

  const llvm::Function* test_func = nullptr;
  llvm::Module* test_mod = nullptr;
  for (auto it = M.functions().begin(); it != M.functions().end(); ++it) {
    test_func = &it->getFunction();
    break;
  }
  llvm::Function* test_func2 = const_cast<llvm::Function*>(test_func);

  FunctionPassManager MPM;
  MPM.addPass(PromotePass());
  MPM.addPass(ScalarEvolutionPrinterPass(llvm::errs()));
  FunctionAnalysisManager MAM;
  //MAM.registerPass([&] { return StaticCallCounter(); });
  MAM.registerPass([&] {return ScalarEvolutionAnalysis();});
  // Register all available module analysis passes defined in PassRegisty.def.
  // We only really need PassInstrumentationAnalysis (which is pulled by
  // default by PassBuilder), but to keep this concise, let PassBuilder do all
  // the _heavy-lifting_.
  PassBuilder PB;
  PB.registerFunctionAnalyses(MAM);
  // Finally, run the passes registered with MPM
  MPM.run(*test_func2, MAM);

  auto se = &MAM.getResult<ScalarEvolutionAnalysis>(*test_func2);
  auto loopInfo = &MAM.getResult<LoopAnalysis>(*test_func2);

  for(LoopInfo::iterator i=loopInfo->begin();i!=loopInfo->end();++i) {
    Loop *L = *i;
    errs() << "Loop ";
    L->getHeader()->printAsOperand(errs(), /*PrintType=*/false);
    errs() << ": ";
    SmallVector<BasicBlock *, 8> ExitingBlocks;
    L->getExitingBlocks(ExitingBlocks);
    if (ExitingBlocks.size() != 1)
      errs() << "<multiple exits> ";
    
    if (se->hasLoopInvariantBackedgeTakenCount(L)) {
      errs() << "backedge-taken count is " << *se->getBackedgeTakenCount(L)
             << "\n";
      auto back_cnt = se->getBackedgeTakenCount(L);
      auto scev_con = cast<llvm::SCEVConstant>(back_cnt)->getValue()->getSExtValue();
      errs()<< "\n";
    }
    else
      errs() << "Unpredictable backedge-taken count.\n";

    if (ExitingBlocks.size() > 1)
      for (BasicBlock *ExitingBlock : ExitingBlocks) {
        errs() << "  exit count for " << ExitingBlock->getName() << ": "
           << *se->getExitCount(L, ExitingBlock) << "\n";
      }
    
    errs() << "Loop ";
    L->getHeader()->printAsOperand(errs(), /*PrintType=*/false);
    errs() << ": ";
    
    if (!isa<SCEVCouldNotCompute>(se->getConstantMaxBackedgeTakenCount(L))) {
      errs() << "max backedge-taken count is " << *se->getConstantMaxBackedgeTakenCount(L);
      if (se->isBackedgeTakenCountMaxOrZero(L))
        errs() << ", actual taken count either this or zero.";
    } else {
      errs() << "Unpredictable max backedge-taken count. ";
    }
  }

}

//===----------------------------------------------------------------------===//
// Main driver code.
//===----------------------------------------------------------------------===//
int main(int Argc, char **Argv) {
  // Hide all options apart from the ones specific to this tool
  cl::HideUnrelatedOptions(CallCounterCategory);

  cl::ParseCommandLineOptions(Argc, Argv,
                              "Counts the number of static function "
                              "calls in the input IR file\n");

  // Makes sure llvm_shutdown() is called (which cleans up LLVM objects)
  //  http://llvm.org/docs/ProgrammersManual.html#ending-execution-with-llvm-shutdown
  llvm_shutdown_obj SDO;

  // Parse the IR file passed on the command line.
  SMDiagnostic Err;
  LLVMContext Ctx;
  std::unique_ptr<Module> M = parseIRFile(InputModule.getValue(), Err, Ctx);

  if (!M) {
    errs() << "Error reading bitcode file: " << InputModule << "\n";
    Err.print(Argv[0], errs());
    return -1;
  }

  // Run the analysis and print the results
  countStaticCalls(*M);

  return 0;
}
