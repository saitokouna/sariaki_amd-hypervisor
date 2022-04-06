#include <iostream>
#include <intrin.h>
#include <windows.h>

std::string get_vendor_id()
{
    int registers[4];   // eax ebx ecx edx
    char vedor_id[13];

    // when hypervisor is active, CPUID leaf 0x40000000
    // should return our own hypervisor vendor
    __cpuid(registers, 0x40000000);
    RtlCopyMemory(vedor_id + 0, &registers[1], sizeof(registers[1]));
    RtlCopyMemory(vedor_id + 4, &registers[2], sizeof(registers[2]));
    RtlCopyMemory(vedor_id + 8, &registers[3], sizeof(registers[3]));
    vedor_id[12] = ANSI_NULL;

    return vedor_id;
}

int main()
{
	std::printf("[§] initializing\n");
    std::printf("[§] hypervisor vendor -> %s\n", get_vendor_id());
    std::getchar();
	return 0;
}