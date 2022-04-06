#include "handlers.hpp"
#include "../util/util.hpp"
#pragma warning (disable : 4293)
#pragma warning (disable : 26451)

namespace svm
{
    namespace handle
    {
        void msr(pvcpu_t vcpu, pguest_ctx_t guest_ctx)
        {
            ULARGE_INTEGER value;
            uint32_t msr;
            bool write_access;

            msr = guest_ctx->vprocessor_registers->rcx & MAXUINT;
            write_access = (vcpu->guest_vmcb.control_area.exit_info1 != 0);

            if (msr == IA32_MSR_EFER)
            {
                // vmexit on EFER access should only occur on write access.
                NT_ASSERT(write_access);

                value.LowPart = guest_ctx->vprocessor_registers->rax & MAXUINT;
                value.HighPart = guest_ctx->vprocessor_registers->rdx & MAXUINT;

                if ((value.QuadPart & EFER_SVME) == 0)
                    util::invalid_opcode_exception(vcpu); // protect EFER.SVME bit by causing exeption

                // otherwise return the physical address of the value
                vcpu->guest_vmcb.state_save_area.efer = value.QuadPart;
            }
            else
            {
                if (write_access)
                {
                    value.LowPart = guest_ctx->vprocessor_registers->rax & MAXUINT;
                    value.HighPart = guest_ctx->vprocessor_registers->rdx & MAXUINT;
                    __writemsr(msr, value.QuadPart);
                }
                else
                {
                    value.QuadPart = __readmsr(msr);
                    guest_ctx->vprocessor_registers->rax = value.LowPart;
                    guest_ctx->vprocessor_registers->rdx = value.HighPart;
                }
            }

            next_instruction;
        }
    }
}