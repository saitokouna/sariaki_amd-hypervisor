#include "common-includes.hpp"
#include "svm/svm.hpp"

// not really needed, but good to have for testing
// since the only other way to devirtualize 
// the processor otherwise would be to restart
void driver_unload(PDRIVER_OBJECT driver_object)
{
    svm::devirtualize();
}

NTSTATUS driver_entry(PDRIVER_OBJECT driver_object, PUNICODE_STRING registry_path)
{
    // register driver unload, but only if our driver isn't mapped
    if (driver_object) driver_object->DriverUnload = driver_unload;

    // check to see if processor can be virtualized
    if (!svm::check_support())
    {
        debug_log("[§] your processor does not support virtualization\n");
        return STATUS_HV_FEATURE_UNAVAILABLE;
    }

    // start virtualizing
    HANDLE thread_handle;
    PsCreateSystemThread(&thread_handle, 
        THREAD_ALL_ACCESS,
        NULL, 
        NULL,
        NULL, 
        svm::virtualize,
        NULL);

    return STATUS_SUCCESS;
}