/*
 * afterburn-wedge/include/ia32/vmiCalls.h
 *
 *   List of 32-bit VMI interface calls and parameters
 *
 *   Copyright (C) 2005, VMware, Inc.
 *   Copyright (C) 2006, University of Karlsruhe
 *
 */

#define VMI_CALLS \
   VDEF(RESERVED0) \
   VDEF(RESERVED1) \
   VDEF(RESERVED2) \
   VDEF(RESERVED3) \
   VDEF(Init) \
   VDEF(CPUID) \
   VDEF(WRMSR) \
   VDEF(RDMSR) \
   VDEF(SetGDT) \
   VDEF(SetLDT) \
   VDEF(SetIDT) \
   VDEF(SetTR) \
   VDEF(GetGDT) \
   VDEF(GetLDT) \
   VDEF(GetIDT) \
   VDEF(GetTR) \
   VDEF(WriteGDTEntry) \
   VDEF(WriteLDTEntry) \
   VDEF(WriteIDTEntry) \
   VDEF(UpdateKernelStack) \
   VDEF(SetCR0) \
   VDEF(SetCR2) \
   VDEF(SetCR3) \
   VDEF(SetCR4) \
   VDEF(GetCR0) \
   VDEF(GetCR2) \
   VDEF(GetCR3) \
   VDEF(GetCR4) \
   VDEF(INVD) \
   VDEF(WBINVD) \
   VDEF(SetDR) \
   VDEF(GetDR) \
   VDEF(RDPMC) \
   VDEF(RDTSC) \
   VDEF(CLTS) \
   VDEF(EnableInterrupts) \
   VDEF(DisableInterrupts) \
   VDEF(GetInterruptMask) \
   VDEF(SetInterruptMask) \
   VDEF(IRET) \
   VDEF(SYSEXIT) \
   VDEF(Pause) \
   VDEF(Halt) \
   VDEF(Reboot) \
   VDEF(Shutdown) \
   VDEF(SetPxE) \
   VDEF(GetPxE) \
   VDEF(SwapPxE) \
   /* Reserved for PAE / long mode */ \
   VDEF(SetPxELong) \
   VDEF(GetPxELong) \
   VDEF(SwapPxELongAtomic) \
   VDEF(TestAndSetPxEBit) \
   VDEF(TestAndClearPxEBit) \
   /* Notify the hypervisor how a page should be shadowed */ \
   VDEF(AllocatePage) \
   /* Release shadowed parts of a page */ \
   VDEF(ReleasePage) \
   VDEF(InvalPage) \
   VDEF(FlushTLB) \
   VDEF(FlushDeferredCalls) \
   VDEF(SetLinearMapping) \
   VDEF(IN) \
   VDEF(INB) \
   VDEF(INW) \
   VDEF(INS) \
   VDEF(INSB) \
   VDEF(INSW) \
   VDEF(OUT) \
   VDEF(OUTB) \
   VDEF(OUTW) \
   VDEF(OUTS) \
   VDEF(OUTSB) \
   VDEF(OUTSW) \
   VDEF(SetIOPLMask) \
   VDEF(DeactivatePxELongAtomic) \
   VDEF(TestAndSetPxELongBit) \
   VDEF(TestAndClearPxELongBit) \
   VDEF(SetInitialAPState) \
   VDEF(APICWrite) \
   VDEF(APICRead) \
   VDEF(IODelay) \
   VDEF(GetCycleFrequency) \
   VDEF(GetCycleCounter) \
   VDEF(SetAlarm) \
   VDEF(CancelAlarm) \
   VDEF(GetWallclockTime) \
   VDEF(WallclockUpdated) \
   VDEF(PhysToMachine) \
   VDEF(MachineToPhys) \
   VDEF(PhysIsCoherent)

