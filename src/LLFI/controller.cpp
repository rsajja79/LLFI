#include <string>

#include "llvm/Module.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#include "controller.h"
#include "fiinstselector.h"
#include "insttypefiinstselector.h"
#include "firegselector.h"
#include "indexbasedfiregselector.h"

using namespace llvm;

namespace llfi {
/**
 * Inject Instruction
 */
// TODO: adding default values
static cl::opt< FIInstSelSrc > fiinstselsrc(
    cl::desc("Choose where the fault injection targets are specified"),
    cl::values(
      clEnumVal(insttype, "Specified through selecting instruction type"),
      clEnumVal(sourcecode, "Specified through source code"),
      clEnumVal(custominst, "Specified through custom function"),
      clEnumValEnd));

// inst type
static cl::list< std::string > includeinst("includeinst", 
    cl::desc("The type of instruction to be included into fault injection"), 
    cl::ZeroOrMore);
static cl::list< std::string > excludeinst("excludeinst", 
    cl::desc("The type of instruction to be excluded into fault injection"), 
    cl::ZeroOrMore);

// custom function
static cl::opt < std::string > fiinstselfunc("fiinstselfunc",
    cl::desc("Fault injection instruction selection function name (for custom)"));

// backtrace or forwardtrace included
static cl::opt< bool > includebackwardtrace("includebackwardtrace", 
    cl::desc("Include backtrace of the selected instructions into fault injection"));
static cl::opt< bool > includeforwardtrace("includeforwardtrace", 
    cl::desc("Include forwardtrace of the selected instructions into fault injection"));

/**
 * Inject Register
 */
static cl::opt< FIRegSelSrc > firegselsrc(
    cl::desc("Choose where the source variable targets are spedified (for source case)"),
    cl::values(
      clEnumVal(index, "Selecting source variable by source index (all, 0, 1, etc.)"),
      clEnumVal(customreg, "Selecting source variable by custom function"),
      clEnumValEnd));

static cl::opt< FIRegLoc > fireglocation(
    cl::desc("Choose fault injection location:"),
    cl::values(
      clEnumVal(destreg, "Inject into destination register"),
      clEnumVal(allsrcreg, "Inject into all source registers"),
      clEnumVal(srcreg1, "Inject into 1st source register"),
      clEnumVal(srcreg2, "Inject into 2nd source register"),
      clEnumVal(srcreg3, "Inject into 3rd source register"),
      clEnumVal(srcreg4, "Inject into 4th source register"),
      clEnumValEnd));

static cl::opt < std::string > firegselfunc("firegselfunc",
    cl::desc("Source variable selection function name (for custom)"));


void Controller::genFullNameOpcodeMap(NameOpcodeMap &opcodenamemap) {
#define HANDLE_INST(N, OPC, CLASS) opcodenamemap[std::string(Instruction::getOpcodeName(N))] = N;
#include "llvm/Instruction.def"
}

void Controller::getOpcodeListofFIInsts(std::set<unsigned> *fi_opcode_list) {
  NameOpcodeMap fullnameopcodemap;
  genFullNameOpcodeMap(fullnameopcodemap);

  // include
  for (unsigned i = 0; i != includeinst.size(); ++i) {
    // TODO: make "all" a static string
    if (includeinst[i] == "all") {
      for (NameOpcodeMap::const_iterator it = fullnameopcodemap.begin();
          it != fullnameopcodemap.end(); ++it) {
        fi_opcode_list->insert(it->second);  
      }
      break;
    } else {
      NameOpcodeMap::iterator loc = fullnameopcodemap.find(includeinst[i]);
      if (loc != fullnameopcodemap.end()) {
        fi_opcode_list->insert(loc->second);
      } else {
        errs() << "Invalid included instruction type: " << includeinst[i] << "\n";
        exit(1);
      }
    }
  }

  // exclude
  for (unsigned i = 0; i != excludeinst.size(); ++i) {
    NameOpcodeMap::iterator loc = fullnameopcodemap.find(excludeinst[i]);
    if (loc != fullnameopcodemap.end()) {
      fi_opcode_list->erase(loc->second);
    } else {
      errs() << "Invalid excluded instruction type: " << excludeinst[i] << "\n";
      exit(1);
    }
  }
}

void Controller::processInstSelArgs() {
  fiinstselector = NULL;
  if (fiinstselsrc == insttype) {
    std::set<unsigned> *fi_opcode_list = new std::set<unsigned>;
    getOpcodeListofFIInsts(fi_opcode_list);
    fiinstselector = new InstTypeFIInstSelector(fi_opcode_list,
                                                includebackwardtrace,
                                                includeforwardtrace);
  } else if (fiinstselsrc == custominst) {
    // TODO: convert the function name to function 
  }   
}

void Controller::processRegSelArgs() {
  firegselector = NULL;
  if (firegselsrc == index) {
    firegselector = new IndexBasedFIRegSelector(fireglocation);
  } else {
    // TODO: handle the custom case
  }
}

void Controller::processCmdArgs() {
  processInstSelArgs();

  processRegSelArgs();
}


void Controller::init(Module *M) {
  processCmdArgs();
  
  // select fault injection instructions
  std::set<Instruction*> fiinstset;
  fiinstselector->getFIInsts(M, &fiinstset);
  
  // select fault injection registers
  firegselector->getFIInstRegMap(&fiinstset, &fi_inst_regs_map);
}

Controller::~Controller() {
  delete fiinstselector;
  delete firegselector;
}

void Controller::dump() const {
  for (std::map<Instruction*, std::list< Value* > *>::const_iterator inst_it =
       fi_inst_regs_map.begin(); inst_it != fi_inst_regs_map.end(); ++inst_it) {
    errs() << "Selected instruction " << *(inst_it->first) << "\nRegs:\n";
    for (std::list<Value*>::const_iterator reg_it = inst_it->second->begin();
         reg_it != inst_it->second->end(); ++reg_it) {
      errs() << "\t" << **reg_it << "\n";
    }
    errs() << "\n";
  }
}

}
