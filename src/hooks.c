#include "private.h"

/// Transaction item
typedef struct __gnx_transaction_item_t
{
    uint32_t op_flags; // (add or remove)
        #define GNX_TSXF_ADD 1
        #define GNX_TSXF_DEL 2
    union
    {
        struct
        {
            // Function address to hook (with all jumps skipped)
            void *func_addr;
            // The hook replacement function
            void *hook_addr;
            // Original func_addr without jumps skipped
            userhook_springboard_t *uh;

            void **psrc;
        } add;
        struct
        {
            userhook_springboard_t *uh;
            void **psrc;
        } remove;
    } op;
    GNX_SINGLY_LIST_ITEM_DEFINE;
} gnx_transaction_item_t;


/// Transaction header
typedef struct __gnx_transaction_t
{
    gnx_handle_t gnx;
    gnx_singly_list_item_t items;
} gnx_transaction_t;

//--------------------------------------------------------------------------
#define GET_TRANS \
    gnx_transaction_t *trans = (gnx_transaction_t *)handle

#define GET_VARS \
    GET_TRANS; \
    gnx_workspace_t *ws = (gnx_workspace_t *)trans->gnx;


//--------------------------------------------------------------------------
static gnx_err_t create_function_springboard(
    gnx_workspace_t *ws,
    const void *src_func,
    userhook_springboard_t *uh)
{
    // How big is the destination copy
    ptrdiff_t dest_left = sizeof(uh->springboard);

    // We need to copy just enough bytes to fit the springboard
    ptrdiff_t src_left = GANXO_JUMP_TO_SPRINGBOARD_SIZE;

    uint8_t *reloc_dest = (uint8_t *)uh->springboard;
    const uint8_t *src = src_func;

    gnx_err_t err = GNX_ERR_OK;
    bool small_func = false;
    while (src_left > 0)
    {
        // Make sure we have enough room to copy the biggest possible instruction
        dest_left -= (reloc_dest - (uint8_t *)uh->springboard);
        if (dest_left < GANXO_MAX_INSTR_SIZE)
            return GNX_ERR_BUFFER_TOO_SMALL;

        const uint8_t *psrc = src;

        // Copy and relocate the source instruction
        err = gnx_disasm_copy_instruction(
            ws->dis, 
            (const void **)&src, 
            (void **)&reloc_dest);

        // Bail out on failure
        if (err != GNX_ERR_OK)
            return err;

        // Update the source bytes count to copy
        src_left -= src - psrc;

        // Just copied a return instruction prematurely:
        // - The source function is too short and does not have enough space
        //   so we can write a springboard
        if (src_left > 0 && gnx_disasm_is_ret(ws->dis))
        {
            // Let us see if we have enough alignment bytes we can leverage
            do 
            {
                // Disassemble the function
                size_t aln_size;
                err = gnx_disasm_instruction(
                    ws->dis, 
                    src,
                    &aln_size);
                if (err != GNX_ERR_OK)
                    return err;

                // Check if this is an alignment byte
                if (!gnx_disasm_is_align(ws->dis, &aln_size))
                    return GNX_ERR_FUNCTION_TOO_SMALL;

                // If these are alignment bytes, we can use them freely to make room for the rest of the springboard

                // Copy alignment instruction as-is
                memcpy(reloc_dest, src, aln_size);

                src_left -= aln_size;
                src += aln_size;
                reloc_dest += aln_size;
            } while (src_left > 0);
            // This is a small function:
            // - there is no real springboard because the copied bytes contain the complete function. 
            small_func = true;
        }
    }

    // Last step, if needed, generate the jump to go past our
    // springboard jump in the original function
    if (!small_func)
    {
        if (dest_left < GANXO_JUMP_TO_SPRINGBOARD_SIZE)
            return GNX_ERR_BUFFER_TOO_SMALL;
        
        gnx_asm_gen_relbranch(
            ws->dis,
            false,
            src,
            (void **)&reloc_dest);
    }

    uh->backup_sz = (uint16_t)(src - (uint8_t *)src_func);

    // Let's backup the original bytes
    memcpy(
        uh->backup,
        src_func,
        uh->backup_sz);

    return GNX_ERR_OK;
}

//--------------------------------------------------------------------------
// Create a springboard
static inline gnx_err_t free_user_springboard(
    gnx_workspace_t *ws,
    userhook_springboard_t *uh)
{
    return gnx_block_chunk_free(ws->user_hooks, uh);
}

//--------------------------------------------------------------------------
// Create a springboard
static gnx_err_t make_user_springboard(
    gnx_workspace_t *ws,
    gnx_transaction_item_t *item,
    userhook_springboard_t **uh)
{
    gnx_err_t err;
    userhook_springboard_t *user_hook = NULL;
    do 
    {
        // Allocate memory for the springboard
        user_hook = GNX_ALLOC_CHUNK(
            ws->user_hooks, 
            userhook_springboard_t);
        if (user_hook == NULL)
        {
            err = GNX_ERR_NO_MEM;
            break;
        }

        //
        // Create a springboard
        //
        err = create_function_springboard(
            ws, 
            item->op.add.func_addr,
            user_hook);

        if (err != GNX_ERR_OK)
            break;

        *uh = user_hook;

        return GNX_ERR_OK;
    } while (false);

    // Clean up
    if (user_hook != NULL)
    {
        gnx_block_chunk_free(
            ws->user_hooks, 
            user_hook);
    }
    return err;
}

//--------------------------------------------------------------------------
gnx_err_t GANXO_API gnx_transaction_begin(
    gnx_handle_t gnx,
    gnx_handle_t *handle)
{
    gnx_workspace_t *ws = (gnx_workspace_t *)gnx;

    // Create new transaction
    gnx_transaction_t *trans = GNX_ALLOC(gnx_transaction_t);
    if (trans == NULL)
        return GNX_ERR_NO_MEM;

    // Unlock the blocks when the transaction begins
    if (gnx_block_protect(ws->user_hooks, GNX_MEM_RWX) != GNX_ERR_OK)
    {
        gnx_mfree(trans);
        return GNX_ERR_FAILED;
    }

    // Remember the workspace
    trans->gnx = gnx;

    // Initialize the transaction items linked list
    gnx_singly_list_init(&trans->items);

    *handle = (gnx_handle_t)trans;

    return GNX_ERR_OK;
}

//--------------------------------------------------------------------------
gnx_err_t GANXO_API gnx_transaction_add_hook(
    gnx_handle_t handle,
    void **psrc,
    void *hook)
{
    GET_VARS;

    // Create a transaction item
    gnx_transaction_item_t *item = GNX_ALLOC(gnx_transaction_item_t);
    if (item == NULL)
        return GNX_ERR_NO_MEM;

    item->op_flags = GNX_TSXF_ADD;
    item->op.add.hook_addr = hook;
    item->op.add.psrc = psrc;

    // Get the real function address
    item->op.add.func_addr = (void *)gnx_disasm_skip_jumps(
        ws->dis,
        *psrc);

    userhook_springboard_t *uh;
    gnx_err_t err = make_user_springboard(
        ws, 
        item, 
        &uh);

    if (err != GNX_ERR_OK)
    {
        GNX_FREE(item);
        return err;
    }

    // Remember the original function address for restoration
    uh->func_addr_final = item->op.add.func_addr;
    uh->func_addr = *psrc;

    // Replace the original function address with the springboard address
    *psrc = uh->springboard;

    item->op.add.uh = uh;
    GNX_SINGLY_LIST_ITEM_PUSH(
        &trans->items,
        item);

    return GNX_ERR_OK;
}

//--------------------------------------------------------------------------
gnx_err_t GANXO_API gnx_transaction_remove_hook(
    gnx_handle_t handle,
    void **psrc)
{
    GET_TRANS;

    // Create a transaction item
    gnx_transaction_item_t *item = GNX_ALLOC(gnx_transaction_item_t);
    if (item == NULL)
        return GNX_ERR_NO_MEM;

    item->op_flags = GNX_TSXF_DEL;

    item->op.remove.psrc = psrc;
    // After a successful hook, psrc points to a user springboard structure
    item->op.remove.uh = GNX_CONTAINING_RECORD(
        *psrc, 
        userhook_springboard_t, 
        springboard);

    GNX_SINGLY_LIST_ITEM_PUSH(
        &trans->items,
        item);

    return GNX_ERR_OK;
}

//--------------------------------------------------------------------------
gnx_err_t GANXO_API gnx_transaction_abort(gnx_handle_t handle)
{
    GET_VARS;
    gnx_singly_list_item_t *cur = trans->items.next;
    while (cur != NULL)
    {
        // Get the current item
        gnx_transaction_item_t *item = GNX_SINGLY_LIST_RECORD(
            cur,
            gnx_transaction_item_t);

        if (item->op_flags == GNX_TSXF_ADD)
        {
            // Restore the original function address
            *item->op.add.psrc = item->op.add.uh->func_addr;

            // Free the springboard
            free_user_springboard(ws, item->op.add.uh);
        }
        else if (item->op_flags == GNX_TSXF_DEL)
        {
            // Restore the original function address
            *item->op.remove.psrc = item->op.remove.uh->func_addr;

            // Free the springboard
            free_user_springboard(ws, item->op.remove.uh);
        }
        cur = cur->next;

        GNX_FREE(item);
    }

    // Lock back the blocks
    gnx_block_protect(ws->user_hooks, GNX_MEM_EXEC);

    // The transaction is now empty, free it
    GNX_FREE(trans);

    return GNX_ERR_OK;
}

//--------------------------------------------------------------------------
// Commit the hooks
gnx_err_t GANXO_API gnx_transaction_commit(gnx_handle_t handle)
{
    GET_VARS;

    size_t nb_success = 0;
    gnx_err_t err = GNX_ERR_OK;

    void *func_addr = NULL, *hook_addr = NULL;
    size_t prot_sz;
    gnx_mem_flags_t old_flags;

    gnx_singly_list_item_t *cur = trans->items.next;
    while (cur != NULL)
    {
        // Get the current item
        gnx_transaction_item_t *item = GNX_SINGLY_LIST_RECORD(
            cur,
            gnx_transaction_item_t);

        // Commit / add
        if (item->op_flags == GNX_TSXF_ADD)
        {
            func_addr = item->op.add.func_addr;
            hook_addr = item->op.add.hook_addr;
            prot_sz = item->op.add.uh->backup_sz;
        }
        // Commit / remove
        else if (item->op_flags == GNX_TSXF_DEL)
        {
            func_addr = item->op.remove.uh->func_addr_final;
            prot_sz = item->op.remove.uh->backup_sz;
        }
        else
        {
            // Invalid operation
            prot_sz = 0;
        }

        if (prot_sz != 0) do 
        {
            // Change protection so that we can patch the original function
            err = gnx_vmprotect(
                func_addr,
                prot_sz,
                GNX_MEM_RWX,
                &old_flags);

            // Failed to change the protection?
            if (err != GNX_ERR_OK)
                break;

            if (item->op_flags == GNX_TSXF_ADD)
            {
                // Replace the instruction with a jump to the hook
                gnx_asm_gen_relbranch(
                    ws->dis,
                    false,
                    hook_addr,
                    &func_addr);
            }
            else if (item->op_flags == GNX_TSXF_DEL)
            {
                // Restore the original bytes
                memcpy(
                    func_addr,
                    item->op.remove.uh->backup,
                    prot_sz);

                // Restore the original function address
                *item->op.remove.psrc = item->op.remove.uh->func_addr;

                free_user_springboard(ws, item->op.remove.uh);
            }

            // Restore the protection
            err = gnx_vmprotect(
                func_addr,
                prot_sz,
                old_flags,
                NULL);

            if (err != GNX_ERR_OK)
                break;

            // Flush instruction cache after the patch
            err = gnx_flush_instruction_cache(
                NULL,
                func_addr,
                prot_sz);
            if (err != GNX_ERR_OK)
                break;

            ++nb_success;

        } while (false);

        cur = cur->next;

        GNX_FREE(item);
    }

    // The transaction is now empty, free it
    GNX_FREE(trans);

    if (err != GNX_ERR_OK)
        err = nb_success == 0 ? GNX_ERR_FAILED : GNX_ERR_PARTIAL;

    // Lock back the blocks
    gnx_block_protect(ws->user_hooks, GNX_MEM_EXEC);

    return err;
}
