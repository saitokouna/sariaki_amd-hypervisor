#include "handlers.hpp"
#include "../util/util.hpp"
#pragma warning (disable : 4293)
#pragma warning (disable : 26451)

namespace svm
{
    namespace handle
    {
        void cpuid(pvcpu_t vcpu, pguest_ctx_t guest_ctx)
        {
            int registers[4]; // eax ebx ecx edx
            int leaf, sub_leaf;
            segment_attributes_t attribute;

            // execute cpuid for request
            leaf = static_cast<int>(guest_ctx->vprocessor_registers->rax);
            sub_leaf = static_cast<int>(guest_ctx->vprocessor_registers->rcx);
            __cpuidex(registers, leaf, sub_leaf);

            switch (leaf)
            {
                //case static_cast<int>(cpuid::processor_feature_id):
                //    // specify hypervisor presence 
                //    registers[2] |= static_cast<int>(cpuid::hypervisor_present_ex);
                //    break;

            case static_cast<int>(cpuid::hypervisor_vendor_id):
                // this is used in util::is_hypervisor_vendor_installed()
                // to check if the hypervisor is running
                registers[0] = static_cast<int>(cpuid::hypervisor_vendor_id); // CPUID_HV_MAX
                registers[1] = ' mvs';
                registers[2] = '    ';
                registers[3] = '    ';
                break;

            case static_cast<int>(cpuid::hypervisor_interface):
                // specify that our hypervisor does not 
                // conform to the Microsoft hypervisor interface
                registers[0] = '0#vH';  // Hv#0
                registers[1] = registers[2] = registers[3] = 0;
                break;

            case CPUID_UNLOAD:
                if (sub_leaf == CPUID_UNLOAD)
                {
                    // unload 
                    attribute.value = vcpu->guest_vmcb.state_save_area.ss.attribute.value;
                    if (attribute.fields.dpl == DPL_SYSTEM)
                        guest_ctx->should_exit = true;
                }
                break;

            default:
                break;
            }

            // update the guest's gpr's used by cpuid
            guest_ctx->vprocessor_registers->rax = registers[0];
            guest_ctx->vprocessor_registers->rbx = registers[1];
            guest_ctx->vprocessor_registers->rcx = registers[2];
            guest_ctx->vprocessor_registers->rdx = registers[3];

            next_instruction;
        }
    }
}