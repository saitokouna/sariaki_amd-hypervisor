#pragma once
#include "../../common-includes.hpp"
#include "../def/cpu.hpp"
#include "../def/cpu.hpp"
#include "../def/descriptors-info.hpp"
#include "../def/vmexit.hpp"
#include "../def/virtual-memory.hpp"
#include "../def/vprocessor-data.hpp"
#include "../util/util.hpp"

#pragma warning (disable : 4244)
#pragma warning (disable : 4293)
#pragma warning (disable : 26451)

#define next_instruction vcpu->guest_vmcb.state_save_area.rip = vcpu->guest_vmcb.control_area.next_rip

namespace svm
{
	namespace handle
	{
        void cpuid(pvcpu_t vprocessor_data, pguest_ctx_t guest_ctx);

        void msr(pvcpu_t vprocessor_data, pguest_ctx_t guest_ctx);

        void rdtsc(pvcpu_t vcpu, pguest_ctx_t guest_ctx);

        extern "C" bool __stdcall handle_vmexit(pvcpu_t vprocessor_data, pguest_registers_t guest_registers);
	}
}