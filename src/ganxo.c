#include "private.h"

//--------------------------------------------------------------------------
// Platform APIs
//--------------------------------------------------------------------------

static gnx_platform_apis_t papis = { 0 };

#if defined(GANXO_PLATFORM_WINDOWS)
	#include "win-papi-impl.c"
#endif

// malloc()
void *GANXO_API gnx_malloc(size_t size)
{
	return papis.malloc(size);
}

// free()
void GANXO_API gnx_mfree(void *block)
{
	papis.mfree(block);
}

// vmalloc()
void *GANXO_API gnx_vmalloc(
	size_t size,
	gnx_mem_flags_t flags)
{
	return papis.vmalloc(size, flags);
}

// vmprotect()
gnx_err_t GANXO_API gnx_vmprotect(
    const void *block,
    size_t size,
    gnx_mem_flags_t flags,
    gnx_mem_flags_t *old_flags)
{
    return papis.vmprotect(
        block, 
        size, 
        flags, 
        old_flags);
}

// vmfree()
gnx_err_t GANXO_API gnx_vmfree(void *block)
{
	return papis.vmfree(block);
}

gnx_err_t GANXO_API gnx_flush_instruction_cache(
    void *proc,
    const void *address,
    size_t size)
{
    return papis.flush_instruction_cache(proc, address, size);
}

//--------------------------------------------------------------------------
gnx_err_t GANXO_API gnx_set_platform_apis(gnx_platform_apis_t *apis)
{
	if (apis == NULL || apis->cb != sizeof(gnx_platform_apis_t))
		return GNX_ERR_INVALID_ARGS;

	// Overwrite assigned callbacks
	if (apis->malloc != NULL)
		papis.malloc = apis->malloc;

	if (apis->mfree != NULL)
		papis.mfree = apis->mfree;

	if (apis->vmalloc != NULL)
		papis.vmalloc = apis->vmalloc;

	if (apis->vmfree != NULL)
		papis.vmfree = apis->vmfree;

    if (apis->vmprotect != NULL)
        papis.vmprotect = apis->vmprotect;

    if (apis->flush_instruction_cache != NULL)
        papis.flush_instruction_cache = apis->flush_instruction_cache;

	// TODO: pass and update CS_OPT_MEM accordingly
	return GNX_ERR_OK;
}

//--------------------------------------------------------------------------
// Initialize Ganxo
gnx_err_t GANXO_API gnx_init(void)
{
    // Set the default helper APIs for the current platform
	set_default_platform_apis();

    // Check if Capstone is compiled with support for the same architecture as Ganxo
	return cs_support(GNX_CS_ARCH) ? GNX_ERR_OK : GNX_ERR_NOT_SUPPORTED;
}

//--------------------------------------------------------------------------
gnx_err_t GANXO_API gnx_open(gnx_handle_t *handle)
{
    gnx_err_t err;
    gnx_workspace_t *ws;
    do
    {
        // Create a new workspace
        ws = GNX_ALLOC(gnx_workspace_t);
        if (ws == NULL)
        {
            err = GNX_ERR_NO_MEM;
            break;
        }

        // Create disassembler for the workspace
        ws->dis = gnx_disasm_create();
        if (ws->dis == GNX_INVALID_HANDLE)
        {
            err = GNX_ERR_DISASM;
            break;
        }

        //
        // Create user hooks blocks
        // (it will be used for springboard allocations)
        //
        gnx_block_options_t bo;
        bo.block_size           = 4096 * 10; // 10 pages block size
        bo.chunk_align          = 0x10; // Align the first chunk (springboards in this case) to 16 bytes
        
        // User hooks require a plain springboard with no special dispatcher
        bo.chunk_size = sizeof(userhook_springboard_t);
        // Default block allocation
        bo.vmflags = GNX_MEM_RWX;

        // Attempt to create the block
        ws->user_hooks = gnx_block_create(&bo);
        if (ws->user_hooks == GNX_INVALID_HANDLE)
        {
            err = GNX_ERR_NO_MEM;
            break;
        }

        // Return the workspace handle
        *handle = (gnx_handle_t)ws;

        // Success
        return GNX_ERR_OK;
    } while (false);

    //
    // Failure cleanup from here on
    //
    if (ws->dis != GNX_INVALID_HANDLE)
        gnx_disasm_free(ws->dis);

    if (ws->user_hooks != GNX_INVALID_HANDLE)
        gnx_block_free(ws->user_hooks);

    if (ws != NULL)
        gnx_mfree(ws);

    *handle = GNX_INVALID_HANDLE;
	return err;
}

//--------------------------------------------------------------------------
void GANXO_API gnx_close(gnx_handle_t handle)
{
	GET_WORKSPACE;

    gnx_disasm_free(ws->dis);
    gnx_block_free(ws->user_hooks);
	gnx_mfree(ws);
}