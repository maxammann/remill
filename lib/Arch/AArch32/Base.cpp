#include <glog/logging.h>
#include <llvm/ADT/Triple.h>
#include <llvm/IR/Attributes.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>
#include <remill/Arch/AArch32/AArch32Base.h>
#include <remill/Arch/Name.h>
#include <remill/BC/ABI.h>
#include <remill/BC/Util.h>
#include <remill/OS/OS.h>

namespace remill {


// TODO(pag): Eventually handle Thumb2 and unaligned addresses.
uint64_t AArch32ArchBase::MinInstructionAlign(void) const {
  return 4;
}

uint64_t AArch32ArchBase::MinInstructionSize(void) const {
  return 4;
}

// Maximum number of bytes in an instruction for this particular architecture.
uint64_t AArch32ArchBase::MaxInstructionSize(bool) const {
  return 4;
}

// Default calling convention for this architecture.
llvm::CallingConv::ID AArch32ArchBase::DefaultCallingConv(void) const {
  return llvm::CallingConv::C;  // cdecl.
}

// Get the LLVM triple for this architecture.
llvm::Triple AArch32ArchBase::Triple(void) const {
  auto triple = BasicTriple();
  switch (arch_name) {
    case kArchAArch32LittleEndian: triple.setArch(llvm::Triple::arm); break;
    default:
      LOG(FATAL) << "Cannot get triple for non-aarch32 architecture "
                 << GetArchName(arch_name);
  }

  return triple;
}

// Get the LLVM DataLayout for a module.
llvm::DataLayout AArch32ArchBase::DataLayout(void) const {
  std::string dl;
  switch (os_name) {
    case kOSInvalid:
      LOG(FATAL) << "Cannot convert module for an unrecognized OS.";
      break;

    case kOSLinux:
    case kOSSolaris:
    case kOSmacOS:
    case kOSWindows:
      dl = "e-m:e-p:32:32-Fi8-i64:64-v128:64:128-a:0:32-n32-S64";
      break;
  }

  return llvm::DataLayout(dl);
}

// Returns the name of the stack pointer register.
std::string_view AArch32ArchBase::StackPointerRegisterName(void) const {
  return "SP";
}

// Returns the name of the program counter register.
std::string_view AArch32ArchBase::ProgramCounterRegisterName(void) const {
  return "PC";
}

// Populate the table of register information.
void AArch32ArchBase::PopulateRegisterTable(void) const {
  CHECK_NOTNULL(context);

  reg_by_offset.resize(sizeof(AArch32State));

  auto u8 = llvm::Type::getInt8Ty(*context);

  auto u32 = llvm::Type::getInt32Ty(*context);

#define OFFSET_OF(type, access) \
  (reinterpret_cast<uintptr_t>(&reinterpret_cast<const volatile char &>( \
      static_cast<type *>(nullptr)->access)))

#define REG(name, access, type) \
  AddRegister(#name, type, OFFSET_OF(AArch32State, access), nullptr)

#define SUB_REG(name, access, type, parent_reg_name) \
  AddRegister(#name, type, OFFSET_OF(AArch32State, access), #parent_reg_name)

  REG(R0, gpr.r0.dword, u32);
  REG(R1, gpr.r1.dword, u32);
  REG(R2, gpr.r2.dword, u32);
  REG(R3, gpr.r3.dword, u32);
  REG(R4, gpr.r4.dword, u32);
  REG(R5, gpr.r5.dword, u32);
  REG(R6, gpr.r6.dword, u32);
  REG(R7, gpr.r7.dword, u32);
  REG(R8, gpr.r8.dword, u32);
  REG(R9, gpr.r9.dword, u32);
  REG(R10, gpr.r10.dword, u32);
  REG(R11, gpr.r11.dword, u32);
  REG(R12, gpr.r12.dword, u32);
  REG(R13, gpr.r13.dword, u32);
  REG(R14, gpr.r14.dword, u32);
  REG(R15, gpr.r15.dword, u32);

  SUB_REG(SP, gpr.r13.dword, u32, R13);
  SUB_REG(LR, gpr.r14.dword, u32, R14);
  SUB_REG(PC, gpr.r15.dword, u32, R15);

  REG(N, sr.n, u8);
  REG(C, sr.c, u8);
  REG(Z, sr.z, u8);
  REG(V, sr.v, u8);
}


// Populate a just-initialized lifted function function with architecture-
// specific variables.
void AArch32ArchBase::FinishLiftedFunctionInitialization(
    llvm::Module *module, llvm::Function *bb_func) const {
  const auto &dl = module->getDataLayout();
  CHECK_EQ(sizeof(State), dl.getTypeAllocSize(StateStructType()))
      << "Mismatch between size of State type for x86/amd64 and what is in "
      << "the bitcode module";

  auto &context = module->getContext();
  auto u8 = llvm::Type::getInt8Ty(context);

  //  auto u16 = llvm::Type::getInt16Ty(context);
  auto u32 = llvm::Type::getInt32Ty(context);
  auto addr = llvm::Type::getIntNTy(context, address_size);

  const auto entry_block = &bb_func->getEntryBlock();
  llvm::IRBuilder<> ir(entry_block);

  const auto pc_arg = NthArgument(bb_func, kPCArgNum);
  const auto state_ptr_arg = NthArgument(bb_func, kStatePointerArgNum);
  ir.CreateStore(pc_arg,
                 ir.CreateAlloca(addr, nullptr, kNextPCVariableName.data()));
  ir.CreateStore(
      pc_arg, ir.CreateAlloca(addr, nullptr, kIgnoreNextPCVariableName.data()));

  auto zero_c = ir.CreateAlloca(u8, nullptr, "ZERO_C");
  ir.CreateStore(llvm::Constant::getNullValue(u8), zero_c);
  ir.CreateAlloca(u32, nullptr, "SUPPRESS_WRITEBACK");
  (void) this->RegisterByName("PC")->AddressOf(state_ptr_arg, ir);
}
}  // namespace remill