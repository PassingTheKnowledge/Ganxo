#include "private.h"

//--------------------------------------------------------------------------
/// Branch information
#pragma pack(push, 1)
typedef struct branch_info_t
{
	uint64_t target;
	uint32_t info; ///< \ref GNX_DIS_BI
	/// \defgroup GNX_DIS_BI
	/// Used by branch_info_t::info
	//@{
#define GNX_DIS_BI_IS_REL8		0x00000001
#define GNX_DIS_BI_IS_REL32		0x00000002
#define GNX_DIS_BI_IS_CALL      0x00000004
#define GNX_DIS_BI_IS_JMP       0x00000008
#define GNX_DIS_BI_IS_COND      0x00000010
#define GNX_DIS_BI_IS_RET       0x00000020
#define GNX_DIS_BI_IS_X86_JCX   0x20000000 ///< Checks for CX
#define GNX_DIS_BI_IS_X86_JECX  0x40000000 ///< Checks for ECX
#define GNX_DIS_BI_IS_X86_JRCX  0x80000000 ///< Checks for RCX
	//@}
	uint8_t index;
} branch_info_t;
#pragma pack(pop)

//--------------------------------------------------------------------------
static inline bool gnx_disasm_is_call_(gnx_disasm_t *dis)
{
	register uint8_t *g = dis->insn->detail->groups;
	for (register uint8_t i = 0, c = dis->insn->detail->groups_count; i < c; i++)
	{
		register uint8_t v = g[i];
		if (v == CS_GRP_CALL || v == CS_GRP_INT)
			return true;
	}
	return false;
}

//--------------------------------------------------------------------------
// Check if instruction is a return instruction
static inline bool gnx_disasm_is_ret_(gnx_disasm_t *dis)
{
	register uint8_t *g = dis->insn->detail->groups;
	for (register uint8_t i = 0, c = dis->insn->detail->groups_count; i < c; i++)
	{
		register uint8_t v = g[i];
		if (v == CS_GRP_RET || v == CS_GRP_IRET)
			return true;
	}
	return false;
}

//--------------------------------------------------------------------------
// Include architecture specific implementations
#if defined(GANXO_ARCH_X86)
	#include "disasm-x86-impl.c"
#endif

//--------------------------------------------------------------------------
// Create and initialize the underlying disassembler library
gnx_handle_t GANXO_API gnx_disasm_create(void)
{
    csh cs_handle = 0;
    do
    {
        // Initialize Capstone
        if (cs_open(GNX_CS_ARCH, GNX_CS_MODE, &cs_handle) != CS_ERR_OK)
            break;

        // Enable instruction details
        if (cs_option(cs_handle, CS_OPT_DETAIL, CS_OPT_ON) != CS_ERR_OK)
            break;

        // Create an internal handle
        gnx_disasm_t *dis = GNX_ALLOC(gnx_disasm_t);

        // Remember the Capstone handle and a single working instruction variable
        dis->cs = cs_handle;
        dis->insn = cs_malloc(cs_handle);

        // Success
        if (dis->insn != NULL)
            return (gnx_handle_t)dis;

        // fall-through to cleanup
    } while (false);

    if (cs_handle != 0)
        cs_close(&cs_handle);

    return GNX_INVALID_HANDLE;
}

//--------------------------------------------------------------------------
gnx_handle_t GANXO_API gnx_disasm_from_workspace(gnx_handle_t handle)
{
    GET_WORKSPACE;
    return ws->dis;
}

//--------------------------------------------------------------------------
// Free the disassembler instance
void GANXO_API gnx_disasm_free(gnx_handle_t handle)
{
	GET_DISASM;
    cs_free(dis->insn, 1);
    cs_close(&dis->cs);

    gnx_mfree(dis);
}

//--------------------------------------------------------------------------
// Disassemble a single instruction
static inline gnx_err_t gnx_disasm_instruction_(
	gnx_disasm_t *dis,
	const void *src,
	size_t *size)
{
	size_t max_inst_sz = GANXO_MAX_INSTR_SIZE;

	// The 'code' and 'addr' will advance by instruction size
	// while the 'size' will decrease by it. We discard those values anyway.
	const uint8_t *code = (const uint8_t *)src;
	uint64_t addr = (uint64_t)*(uint64_t *)&code;
	if (!cs_disasm_iter(
		dis->cs,
		&code,
		&max_inst_sz,
		&addr,
		dis->insn))
	{
		return GNX_ERR_DISASM;
	}

    // Copy the instruction size to the caller
	if (size != NULL)
		*size = dis->insn->size;

	return GNX_ERR_OK;
}

//--------------------------------------------------------------------------
gnx_err_t GANXO_API gnx_disasm_instruction(
	gnx_handle_t handle,
	const void *src,
	size_t *size)
{
	GET_DISASM;
	return gnx_disasm_instruction_(dis, src, size);
}

//--------------------------------------------------------------------------
bool GANXO_API gnx_disasm_is_call(gnx_handle_t handle)
{
	GET_DISASM;
	return gnx_disasm_is_call_(dis);
}

//--------------------------------------------------------------------------
GANXO_EXPORT bool GANXO_API gnx_disasm_is_align(
    gnx_handle_t handle, 
    size_t *size)
{
    GET_DISASM;
    return gnx_disasm_is_align_(dis, size);
}

//--------------------------------------------------------------------------
bool GANXO_API gnx_disasm_is_ret(gnx_handle_t handle)
{
	GET_DISASM;
	return gnx_disasm_is_ret_(dis);
}

//--------------------------------------------------------------------------
bool GANXO_API gnx_disasm_is_jump(
	gnx_handle_t handle,
	bool *conditional)
{
	GET_DISASM;
	return gnx_disasm_is_jump_(dis, conditional);
}

//--------------------------------------------------------------------------
const void *GANXO_API gnx_disasm_skip_jumps(
	gnx_handle_t handle,
	const void *src)
{
	GET_DISASM;

	const uint8_t *addr = (const uint8_t *)src;
	while (true)
	{
		if (gnx_disasm_instruction_(dis, addr, NULL) != GNX_ERR_OK)
			break;

		uint64_t target;
		if (!gnx_disasm_follow_jmp_(dis, &target))
			break;

		// Follow
        #pragma warning(push)
        #pragma warning(disable: 4305)
		addr = (const uint8_t *)target;
        #pragma  warning(pop)
	}
	return addr;
}

//--------------------------------------------------------------------------
// Copy a single instruction
gnx_err_t GANXO_API gnx_disasm_copy_instruction(
	gnx_handle_t handle,
	const void **src,
	void **dest)
{
	GET_DISASM;

    // Disassemble the instruction
	size_t src_inst_size, dest_inst_size;
	gnx_err_t err = gnx_disasm_instruction_(
		dis, 
		*src,
		&src_inst_size);

	if (err != GNX_ERR_OK)
		return err;

    // Get branch information
	branch_info_t bi;
	bool is_branch = gnx_disasm_branch_info_(
		dis,
		&bi);

	uint32_t info = bi.info;

	// Is this a jump or call instruction?
	bool is_call_or_jmp = GNX_HAS_FLAG(info, GNX_DIS_BI_IS_CALL | GNX_DIS_BI_IS_JMP);

	// Is this a jump or a call but with absolute addressing instead of relative addressing?
	bool as_is =     	!is_branch
			    	||	!is_call_or_jmp 
			        || (is_call_or_jmp && !GNX_HAS_FLAG(info, GNX_DIS_BI_IS_REL8 | GNX_DIS_BI_IS_REL32));

    // The simplest case: just bitwise copy the instruction
	if (as_is)
	{
		memcpy(
			*dest,
			*src,
			dest_inst_size = src_inst_size);
	}
	else
	{
		// We have a relocatable relative instruction
		err = gnx_disasm_relocate_instruction_(
			dis, 
			*src, 
			*dest,
			&bi,
			&dest_inst_size);
	}

    // Advance source and destination
	if (err == GNX_ERR_OK)
	{
		*dest = (void *)((uint8_t *)*dest + dest_inst_size);
		*src = (void *)((uint8_t *)*src + src_inst_size);
	}

	return err;
}
