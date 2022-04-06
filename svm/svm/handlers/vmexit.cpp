#pragma once
#include "handlers.hpp"

namespace svm
{
    namespace handle
    {
        extern "C" bool __stdcall handle_vmexit(pvcpu_t vcpu, pguest_registers_t guest_registers)
        {
            guest_ctx_t guest_ctx;
            KIRQL old_irql;

            guest_ctx.vprocessor_registers = guest_registers;
            guest_ctx.should_exit = false;

            // load some of the host state which isn't loaded on vmexit
            __svm_vmload(vcpu->host_stack_layout.host_vmcb_pa);

            NT_ASSERT(vcpu->host_stack_layout.reserved1 == MAXUINT64);

            // not needed
            old_irql = KeGetCurrentIrql();
            if (old_irql < DISPATCH_LEVEL)
                KeRaiseIrqlToDpcLevel();

            // the guest's rax is overwritten by the hosts on vmexit 
            // and saved in the vmcb instead
            guest_registers->rax = vcpu->guest_vmcb.state_save_area.rax;

            // update the _KTRAP_FRAME struct values in hypervisor stack, 
            // so that windbg can reconstruct the guests stack frame
            vcpu->host_stack_layout.trap_frame.Rsp = vcpu->guest_vmcb.state_save_area.rsp;
            vcpu->host_stack_layout.trap_frame.Rip = vcpu->guest_vmcb.control_area.next_rip;

            // vmexit handling
            switch (vcpu->guest_vmcb.control_area.exit_code)
            {
            case vmexit::cpuid:
                handle::cpuid(vcpu, &guest_ctx);
                break;

            case vmexit::msr:
                handle::msr(vcpu, &guest_ctx);
                break;

            case vmexit::vmrun:
                util::general_protection_exeption(vcpu);
                break;

            case vmexit::vmload:
                util::general_protection_exeption(vcpu);
                break;

            case vmexit::vmsave:
                util::general_protection_exeption(vcpu);
                break;

            case vmexit::rdtsc:
                handle::rdtsc(vcpu, &guest_ctx);
                break;

            default:
                KeBugCheck(0xB16B00B5UL);
            }

            if (old_irql < DISPATCH_LEVEL)
                KeLowerIrql(old_irql);

            // terminate hypervisor
            if (guest_ctx.should_exit)
            {
                //  RBX     : address to return
                //  RCX     : stack pointer to restore
                //  EDX:EAX : address of per processor data to be freed by the caller
                guest_ctx.vprocessor_registers->rax = reinterpret_cast<uint64_t>(vcpu) & MAXUINT;
                guest_ctx.vprocessor_registers->rbx = vcpu->guest_vmcb.control_area.next_rip;
                guest_ctx.vprocessor_registers->rcx = vcpu->guest_vmcb.state_save_area.rsp;
                guest_ctx.vprocessor_registers->rdx = reinterpret_cast<uint64_t>(vcpu) >> 32;

                // load guest state
                __svm_vmload(MmGetPhysicalAddress(&vcpu->guest_vmcb).QuadPart);

                // set the global interrupt flag (GIF), but still disable interrupts by
                // clearing IF. GIF must be set to return to the normal execution, but
                // interruptions are unwanted untill SVM is disabled as it would
                // execute random kernel-code in the host context.
                _disable();
                __svm_stgi();


                // disable svm and restore the guest rflags
                // this may enable interrupts
                __writemsr(IA32_MSR_EFER, 
                    __readmsr(IA32_MSR_EFER) & ~EFER_SVME);

                __writeeflags(vcpu->guest_vmcb.state_save_area.rflags);
                NT_ASSERT(vcpu->host_stack_layout.reserved1 == MAXUINT64);
                return guest_ctx.should_exit;
            }

            // update rax
            vcpu->guest_vmcb.state_save_area.rax = guest_ctx.vprocessor_registers->rax;
            NT_ASSERT(vcpu->host_stack_layout.reserved1 == MAXUINT64);
            return guest_ctx.should_exit;
        }
    }
}