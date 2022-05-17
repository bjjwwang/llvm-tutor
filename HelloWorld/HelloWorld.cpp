//=============================================================================
// FILE:
//    HelloWorld.cpp
//
// DESCRIPTION:
//    Visits all functions in a module, prints their names and the number of
//    arguments via stderr. Strictly speaking, this is an analysis pass (i.e.
//    the functions are not modified). However, in order to keep things simple
//    there's no 'print' method here (every analysis pass should implement it).
//
// USAGE:
//    1. Legacy PM
//      opt -load libHelloWorld.dylib -legacy-hello-world -disable-output `\`
//        <input-llvm-file>
//    2. New PM
//      opt -load-pass-plugin=libHelloWorld.dylib -passes="hello-world" `\`
//        -disable-output <input-llvm-file>
//
//
// License: MIT
//=============================================================================
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

//-----------------------------------------------------------------------------
// HelloWorld implementation
//-----------------------------------------------------------------------------
// No need to expose the internals of the pass to the outside world - keep
// everything in an anonymous namespace.
namespace {

void printBlock(BasicBlock* B) {
  errs() << "(llvm-tutor)   block name: " << B->getName() <<"\n\n";
  for(auto iit = B->begin(); iit != B->end(); ) {
    ilist_iterator<
        ilist_detail::node_options<Instruction, false, false, void>,
        false, false>
        Ins = iit++;
    Instruction* ins = Ins->clone();
    errs() << "(llvm-tutor)   loop block ins: " << *ins <<"\n";
    if (BranchInst* br = dyn_cast<BranchInst>(ins)) {
      for (int bi = 0; bi < br->getNumOperands(); ++bi) {
        errs() << "(llvm-tutor)  br inst:" << br->getOperand(bi)->getValueName()->getKey()<< "\n";
      }
    }
  }
  errs() << "\n";
}
// This method implements what the pass does
void visitor(Function &F) {
    errs() << "(llvm-tutor) Hello from: "<< F.getName() << "\n";
    errs() << "(llvm-tutor)   number of arguments: " << F.arg_size() << "\n";

    llvm::DominatorTree DT = llvm::DominatorTree();
    DT.recalculate(F);
    auto loopInfo = new llvm::LoopInfoBase<llvm::BasicBlock, llvm::Loop>();
    loopInfo->releaseMemory();
    loopInfo->analyze(DT);
    for(LoopInfo::iterator i=loopInfo->begin();i!=loopInfo->end();++i){
        Loop *L=*i;
        auto loop_name = L->getName();
        errs() << "(llvm-tutor)   loop name: " << loop_name <<"\n";

//        BasicBlock* loop_header = L->getHeader();
//        printBlock(loop_header);
//        BasicBlock* loop_latch = L->getLoopLatch();
//        printBlock(loop_latch);

        for(llvm::Loop::block_iterator bit = L->block_begin(); bit != L->block_end(); bit++){
            llvm::BasicBlock * B = * bit;
            printBlock(B);
        }

        auto loop_vec = L->getSubLoops();
        errs() << "\n\n";
        for(size_t lni = 0; lni < loop_vec.size(); ++lni) {
          errs() << "(llvm-tutor)  sub loop name: " << loop_vec[lni]->getName() <<"\n";
          for(llvm::Loop::block_iterator bit = loop_vec[lni]->block_begin(); bit != loop_vec[lni]->block_end(); bit++){
            llvm::BasicBlock * B = * bit;
            printBlock(B);
          }
        }
    }
}

// New PM implementation
struct HelloWorld : PassInfoMixin<HelloWorld> {
  // Main entry point, takes IR unit to run the pass on (&F) and the
  // corresponding pass manager (to be queried if need be)
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &) {
    visitor(F);
    return PreservedAnalyses::all();
  }

  // Without isRequired returning true, this pass will be skipped for functions
  // decorated with the optnone LLVM attribute. Note that clang -O0 decorates
  // all functions with optnone.
  static bool isRequired() { return true; }
};

// Legacy PM implementation
struct LegacyHelloWorld : public FunctionPass {
  static char ID;
  LegacyHelloWorld() : FunctionPass(ID) {}
  // Main entry point - the name conveys what unit of IR this is to be run on.
  bool runOnFunction(Function &F) override {
    visitor(F);
    // Doesn't modify the input unit of IR, hence 'false'
    return false;
  }
};
} // namespace

//-----------------------------------------------------------------------------
// New PM Registration
//-----------------------------------------------------------------------------
llvm::PassPluginLibraryInfo getHelloWorldPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "HelloWorld", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "hello-world") {
                    FPM.addPass(HelloWorld());
                    return true;
                  }
                  return false;
                });
          }};
}

// This is the core interface for pass plugins. It guarantees that 'opt' will
// be able to recognize HelloWorld when added to the pass pipeline on the
// command line, i.e. via '-passes=hello-world'
extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getHelloWorldPluginInfo();
}

//-----------------------------------------------------------------------------
// Legacy PM Registration
//-----------------------------------------------------------------------------
// The address of this variable is used to uniquely identify the pass. The
// actual value doesn't matter.
char LegacyHelloWorld::ID = 0;

// This is the core interface for pass plugins. It guarantees that 'opt' will
// recognize LegacyHelloWorld when added to the pass pipeline on the command
// line, i.e.  via '--legacy-hello-world'
static RegisterPass<LegacyHelloWorld>
    X("legacy-hello-world", "Hello World Pass",
      true, // This pass doesn't modify the CFG => true
      false // This pass is not a pure analysis pass => false
    );
