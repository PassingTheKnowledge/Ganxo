#pragma warning(push)
#pragma warning(disable: 4820 4255)
#include <windows.h>
#pragma warning(pop)

//--------------------------------------------------------------------------
static inline void *win_papi_malloc(size_t size)
{
    return malloc(size);
}

//--------------------------------------------------------------------------
static inline void GANXO_API win_papi_mfree(void *block)
{
    free(block);
}

//--------------------------------------------------------------------------
// Convert Ganxo memory protection flags to win32's
static bool gnx_memprot_to_win_memprot(
    gnx_mem_flags_t flags, 
    DWORD *flProtect)
{
    bool has_write = GNX_HAS_FLAG(flags, GNX_MEM_WRITE);
    bool has_exec = GNX_HAS_FLAG(flags, GNX_MEM_EXEC);

    if (has_write && !has_exec)
        *flProtect = PAGE_READWRITE;
    else if (has_write && has_exec)
        *flProtect = PAGE_EXECUTE_READWRITE;
    else if (!has_write && !has_exec)
        *flProtect = PAGE_READONLY;
    else if (!has_write && has_exec)
        *flProtect = PAGE_EXECUTE_READ;
    else
        return false;

    return true;
}

//--------------------------------------------------------------------------
// Convert win32's memory protection flags to Ganxo's
static inline void win_memprot_to_gnx_memprot(
    DWORD flProtect,
    gnx_mem_flags_t *flags)
{
    gnx_mem_flags_t fl = 0;
    if (GNX_HAS_FLAG(flProtect, PAGE_READONLY))
        fl |= GNX_MEM_READ;

    if (GNX_HAS_FLAG(flProtect, PAGE_READWRITE))
        fl |= GNX_MEM_READ | GNX_MEM_WRITE;

    if (GNX_HAS_FLAG(flProtect, PAGE_EXECUTE))
        fl |= GNX_MEM_EXEC;

    if (GNX_HAS_FLAG(flProtect, PAGE_EXECUTE_READ))
        fl |= GNX_MEM_EXEC | GNX_MEM_READ;

    if (GNX_HAS_FLAG(flProtect, PAGE_EXECUTE_READWRITE))
        fl |= GNX_MEM_READ | GNX_MEM_WRITE | GNX_MEM_EXEC;

    *flags = fl;
}

//--------------------------------------------------------------------------
static gnx_err_t GANXO_API win_papi_flush_instruction_cache(
    void *proc,
    const void *addr,
    size_t size)
{
    if (proc != NULL)
        return GNX_ERR_INVALID_ARGS;

    return FlushInstructionCache(
        GetCurrentProcess(),
        addr,
        size) == FALSE ? GNX_ERR_FAILED : GNX_ERR_OK;
}

//--------------------------------------------------------------------------
static gnx_err_t GANXO_API win_papi_vmprotect(
    const void *block,
    size_t size,
    gnx_mem_flags_t flags,
    gnx_mem_flags_t *old_flags)
{
    // Convert to win32 memory protection flags
    DWORD flProtect;
    if (!gnx_memprot_to_win_memprot(flags, &flProtect))
        return GNX_ERR_INVALID_ARGS;

    DWORD flOldProtect;
    if (!VirtualProtect(
            (void *)block, 
            size, 
            flProtect, 
            &flOldProtect))
    {
        return GNX_ERR_FAILED;
    }

    if (old_flags != NULL)
        win_memprot_to_gnx_memprot(flOldProtect, old_flags);

    return GNX_ERR_OK;
}

//--------------------------------------------------------------------------
static void *GANXO_API win_papi_vmalloc(
    size_t size,
    gnx_mem_flags_t flags)
{
    DWORD flProtect;
    if (!gnx_memprot_to_win_memprot(flags, &flProtect))
        return NULL;

    return VirtualAlloc(NULL, size, MEM_COMMIT, flProtect);
}

//--------------------------------------------------------------------------
static gnx_err_t GANXO_API win_papi_vmfree(void *block)
{
    return VirtualFree(block, 0, MEM_RELEASE) ? GNX_ERR_OK : GNX_ERR_FAILED;
}

//--------------------------------------------------------------------------
// Set the default/built-in helper APIs
static void set_default_platform_apis(void)
{
    papis.malloc                    = win_papi_malloc;
    papis.mfree                     = win_papi_mfree;
    papis.vmalloc                   = win_papi_vmalloc;
    papis.vmfree                    = win_papi_vmfree;
    papis.vmprotect                 = win_papi_vmprotect;
    papis.flush_instruction_cache   = win_papi_flush_instruction_cache;
}
