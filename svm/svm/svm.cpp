#include "svm.hpp"

namespace svm
{
    inline bool is_amd()
    {
        int registers[4]; // eax ebx ecx edx

        __cpuid(registers, static_cast<int>(cpuid::cpu_vendor_string));

        // an AMD processor should always return AuthenticAMD
        return ((registers[1] == 'htuA') &&
            (registers[3] == 'itne') &&
            (registers[2] == 'DMAc'));
    }

    inline bool is_svm_supported()
    {
        int registers[4]; // eax ebx ecx edx

        __cpuid(registers, static_cast<int>(cpuid::svm_features_ex));
        return (registers[2] & static_cast<int>(cpuid::cpu_ecx_svm));
    }

    inline bool is_npt_supported()
    {
        int registers[4]; // eax ebx ecx edx

        __cpuid(registers, static_cast<int>(cpuid::svm_features));
        return registers[3] & static_cast<int>(cpuid::cpu_edx_np);
    }

    inline bool can_svm_be_enabled()
    {
        uint64_t vmcr;

        vmcr = __readmsr(static_cast<int>(msr::svm_vm_cr));

        // if VMCR.SVMDIS is enabled SVM cannot be enabled
        // "The VMRUN, VMLOAD, VMSAVE, CLGI, VMMCALL, and INVLPGA instructions can be used
        // when the EFER.SVME is set to 1; otherwise, these instructions generate a #UD exception"
        return !(vmcr & static_cast<int>(msr::svm_vm_cr_svmdis));
    }

    inline bool is_svm_disabled_at_boot()
    {
        int registers[4]; // eax ebx ecx edx

        __cpuid(registers, static_cast<int>(cpuid::svm_features));
        return (registers[3] & static_cast<int>(cpuid::cpu_edx_np));
    }

    bool check_support()
    {
        return is_amd() &&
            is_svm_supported() &&
            is_npt_supported() &&
            can_svm_be_enabled() &&
            is_svm_disabled_at_boot();
    }

    void setup_vmcb(pvcpu_t vcpu, pshared_vcpu_t shared_vcpu, const PCONTEXT ctx)
    {
        descriptor_table_register_t gdtr, idtr;
        PHYSICAL_ADDRESS guest_vmcb_pa, host_vmcb_pa, host_state_area_pa, pml4_base_pa, msrpm_pa;

        // capture current gdtr (global descriptor table register) 
        // & idtr (interrupt descriptor table) to use in the guest
        _sgdt(&gdtr);
        __sidt(&idtr);

        guest_vmcb_pa = MmGetPhysicalAddress(&vcpu->guest_vmcb);
        host_vmcb_pa = MmGetPhysicalAddress(&vcpu->host_vmcb);
        host_state_area_pa = MmGetPhysicalAddress(&vcpu->host_state_area);
        pml4_base_pa = MmGetPhysicalAddress(&shared_vcpu->pml4_entries);
        msrpm_pa = MmGetPhysicalAddress(shared_vcpu->msr_permissions_map);

        // trigger vmexit with all the shit i wanna intercept
        vcpu->guest_vmcb.control_area.intercept_misc1 |= misc1_intercept::intercept_cpuid;
        vcpu->guest_vmcb.control_area.intercept_misc1 |= misc1_intercept::intercept_msr_prot;
        vcpu->guest_vmcb.control_area.intercept_misc1 |= misc1_intercept::intercept_rdtsc;
        vcpu->guest_vmcb.control_area.intercept_misc2 |= misc2_intercept::intercept_vmrun;
        vcpu->guest_vmcb.control_area.intercept_misc2 |= misc2_intercept::intercept_vmload;
        vcpu->guest_vmcb.control_area.intercept_misc2 |= misc2_intercept::intercept_vmsave;
        vcpu->guest_vmcb.control_area.msrpm_base_pa = msrpm_pa.QuadPart;

        // guest address space id (ASID)
        vcpu->guest_vmcb.control_area.guest_asid = 1;

        // enable guest NPT (nested page tables)
        vcpu->guest_vmcb.control_area.np_enable |= SVM_NP_ENABLE_NP_ENABLE;
        vcpu->guest_vmcb.control_area.ncr3 = pml4_base_pa.QuadPart;

        // load initial guest state based on current state
        vcpu->guest_vmcb.state_save_area.gdtr.base = gdtr.base;
        vcpu->guest_vmcb.state_save_area.gdtr.limit = gdtr.limit;
        vcpu->guest_vmcb.state_save_area.idtr.base = idtr.base;
        vcpu->guest_vmcb.state_save_area.idtr.limit = idtr.limit;

        // setup all segments we saved earlier
        vcpu->guest_vmcb.state_save_area.cs.limit = __segmentlimit(ctx->SegCs);
        vcpu->guest_vmcb.state_save_area.ds.limit = __segmentlimit(ctx->SegDs);
        vcpu->guest_vmcb.state_save_area.es.limit = __segmentlimit(ctx->SegEs);
        vcpu->guest_vmcb.state_save_area.ss.limit = __segmentlimit(ctx->SegSs);
        vcpu->guest_vmcb.state_save_area.cs.selector = ctx->SegCs;
        vcpu->guest_vmcb.state_save_area.ds.selector = ctx->SegDs;
        vcpu->guest_vmcb.state_save_area.es.selector = ctx->SegEs;
        vcpu->guest_vmcb.state_save_area.ss.selector = ctx->SegSs;
        vcpu->guest_vmcb.state_save_area.cs.attribute.value = util::get_segment_access_rights(ctx->SegCs, gdtr.base);
        vcpu->guest_vmcb.state_save_area.ds.attribute.value = util::get_segment_access_rights(ctx->SegDs, gdtr.base);
        vcpu->guest_vmcb.state_save_area.es.attribute.value = util::get_segment_access_rights(ctx->SegEs, gdtr.base);
        vcpu->guest_vmcb.state_save_area.ss.attribute.value = util::get_segment_access_rights(ctx->SegSs, gdtr.base);

        vcpu->guest_vmcb.state_save_area.efer = __readmsr(IA32_MSR_EFER);
        vcpu->guest_vmcb.state_save_area.cr0 = __readcr0();
        vcpu->guest_vmcb.state_save_area.cr2 = __readcr2();
        vcpu->guest_vmcb.state_save_area.cr3 = __readcr3();
        vcpu->guest_vmcb.state_save_area.cr4 = __readcr4();
        vcpu->guest_vmcb.state_save_area.rflags = ctx->EFlags;
        vcpu->guest_vmcb.state_save_area.rsp = ctx->Rsp;
        vcpu->guest_vmcb.state_save_area.rip = ctx->Rip;
        vcpu->guest_vmcb.state_save_area.gpat = __readmsr(IA32_MSR_PAT);

        // save current state on vmcb such as:
        // FS GS TR LDTR
        // kernel GS base
        // STAR LSTAR CSTAR FSMASK
        // SYSENTER_CS SYSENTER_ESP SYSENTER_EIP

        // these are restored to the processor right before vmexit using vmload, 
        // so that the guest can continue it's execution with the saved state
        __svm_vmsave(guest_vmcb_pa.QuadPart);

        // store data for the host to use
        vcpu->host_stack_layout.reserved1 = MAXUINT64;
        vcpu->host_stack_layout.shared_vcpu = shared_vcpu;
        vcpu->host_stack_layout.self = vcpu;
        vcpu->host_stack_layout.host_vmcb_pa = host_vmcb_pa.QuadPart;
        vcpu->host_stack_layout.guest_vmcb_pa = guest_vmcb_pa.QuadPart;

        // set an address of the host state area to VM_HSAVE:PA MSR. 
        // the processor saves some of the current state on vmrun and loads them on vmexit
        // see "VM_HSAVE_PA MSR (C001_0117h)".
        __writemsr(SVM_MSR_VM_HSAVE_PA, host_state_area_pa.QuadPart);

        // save the current state to the vmcb for the host to be loaded after vmexit 
        __svm_vmsave(host_vmcb_pa.QuadPart);
    }

    bool devirtualize_processor(PVOID ctx)
    {
        int registers[4]; // eax ebx ecx edx
        uint64_t high, low;
        pvcpu_t vcpu = nullptr;
        pshared_vcpu_t* shared_vcpu = nullptr;

        // ask the hypervisor to unload 
        __cpuidex(registers, 
            CPUID_UNLOAD, CPUID_UNLOAD);

        // check if our vmexit label modified ecx, 
        // thereby indicating that our hypervisor has been uninstalled
        if (registers[2] != 'svm')
            return false;

        debug_log("[§] the processor has been de-virtualized.\n");

        // get an address of per processor data as indicated by EDX:EAX.
        high = registers[3];
        low = registers[0] & MAXUINT32;
        vcpu = reinterpret_cast<pvcpu_t>(high << 32 | low);
        NT_ASSERT(vcpu->host_stack_layout.reserved1 == MAXUINT64);

        // save an address of shared data, then free per processor data.
        shared_vcpu = static_cast<pshared_vcpu_t*>(ctx);
        *shared_vcpu = vcpu->host_stack_layout.shared_vcpu;
        util::free_nonpaged_pool(vcpu);

        return true;
    }

    void devirtualize()
    {
        pshared_vcpu_t shared_vcpu = nullptr;

        util::execute_on_each_processor(devirtualize_processor,
            &shared_vcpu);

        // remove everything 
        if (shared_vcpu)
        {
            util::free_contiguous_memory(shared_vcpu->msr_permissions_map);
            util::free_nonpaged_pool(shared_vcpu);
        }
    }

    bool virtualize_processor(PVOID ctx)
    {
        pshared_vcpu_t shared_vcpu = nullptr;
        pvcpu_t vcpu = nullptr;
        PCONTEXT current_ctx = nullptr;

        current_ctx = static_cast<PCONTEXT>(
            util::alloc_nonpaged_pool(sizeof(*current_ctx)));

        if (current_ctx == nullptr) [[unlikely]]
        {
            debug_log("[§] failed allocating memory for current ctx\n");
            return false;
        }

        vcpu = static_cast<pvcpu_t>(
            util::alloc_nonpaged_pool(sizeof(vcpu_t)));

        if (!vcpu) [[unlikely]]
        {
            debug_log("[§] failed allocating memory for vcpu\n");
            return false;
        }

        RtlCaptureContext(current_ctx);

        if (!util::is_hypervisor_vendor_installed())
        {
            shared_vcpu = static_cast<pshared_vcpu_t>(ctx);

            // enable EFER.SVME
            __writemsr(IA32_MSR_EFER, 
                __readmsr(IA32_MSR_EFER) | EFER_SVME);

            // setup virtual machine control block
            setup_vmcb(vcpu, shared_vcpu, current_ctx);

            // enter vm
            vmenter(&vcpu->host_stack_layout.guest_vmcb_pa);

            // we should never get to this part
            KeBugCheck(0xB16B00B5UL);
        }

        debug_log("[§] successfully virtualized processor\n");

        return true;
    }

    void virtualize(PVOID)
    {
        pshared_vcpu_t shared_vcpu = nullptr;

        // alloc shared struct between processors
        shared_vcpu = static_cast<pshared_vcpu_t>(
            util::alloc_nonpaged_pool(sizeof(shared_vcpu_t)));

        if (!shared_vcpu) [[unlikely]]
        {
            debug_log("[§] failed allocating memory for shared processor data\n");
            PsTerminateSystemThread(0);
        }

        // allocate MSRPM (MSR permissions map) 
        shared_vcpu->msr_permissions_map = util::alloc_contiguous_memory(
            SVM_MSR_PERMISSIONS_MAP_SIZE);

        if (!shared_vcpu->msr_permissions_map) [[unlikely]]
        {
            debug_log("[§] failed allocating memory for MSR permissions map\n");
            PsTerminateSystemThread(0);
        }

        // build NPT and setup MSR permissions bitmap
        util::setup_npt(shared_vcpu);
        util::setup_msr_permissions_bitmap(shared_vcpu->msr_permissions_map);

        if (!util::execute_on_each_processor(virtualize_processor, shared_vcpu))
        {
            debug_log("[§] failed executing on each processor\n");
            PsTerminateSystemThread(0);
        }

        // exit gracefully
        PsTerminateSystemThread(0);
    }
}