[GLOBAL _cpuid]
type _cpuid function
_cpuid:
    mov eax, [esp + 4]
    cpuid
    ret