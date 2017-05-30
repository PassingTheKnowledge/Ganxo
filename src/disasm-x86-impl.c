//
// This file is included by disasm.c and it contains Intel-x86 specific implementations
// and helper functions
//

//--------------------------------------------------------------------------
GANXO_EXPORT gnx_err_t GANXO_API gnx_asm_gen_relbranch(
    gnx_handle_t dishandle,
    bool call_or_jmp,
    const void *target,
    void **dest)
{
    (void)dishandle;
    uint8_t *pdest = (uint8_t *)*dest;
    *pdest++ = call_or_jmp ? 0xE8 : 0xE9;
#pragma warning(push)
#pragma warning(disable: 4213)
    *((int32_t *)pdest)++ = 
        (int32_t)(target) - (int32_t)(pdest - 1) - (1 + sizeof(int32_t));
#pragma warning(pop)
    *dest = pdest;
    return GNX_ERR_OK;
}

//--------------------------------------------------------------------------
static inline bool gnx_disasm_is_align_(
    gnx_disasm_t *dis, 
    size_t *size)
{
    switch (dis->insn->id)
    {
        case X86_INS_NOP:
        case X86_INS_FNOP:
        case X86_INS_INT3:
            break;

        // XCHG and MOV may be considered / used for alignment purposes
        case X86_INS_MOV:
        case X86_INS_XCHG:
        {
            cs_x86 *x86 = &(dis->insn->detail->x86);
            // The instruction is considered a NOP or alignment instruction if:
            // - It has two registers operands
            // - The same register is exchanged
            if (x86->op_count == 2 &&
                x86->operands[0].type == X86_OP_REG && x86->operands[1].type == X86_OP_REG &&
                x86->operands[0].reg == x86->operands[1].reg)
            {
                break;
            }
            // fall-through...
        }
        default:
            return false;
    }

    // Return the instruction size
    if (size != NULL)
        *size = dis->insn->size;

    return true;
}

//--------------------------------------------------------------------------
inline bool gnx_disasm_is_jump_(
	gnx_disasm_t *dis,
	bool *conditional)
{
	cs_x86 *x86 = &(dis->insn->detail->x86);

	// Quick checks:
	// - We need just one operand (fast check)
	// - Jump group
	// - Unconditional jump (looks at no flags)
	if (x86->op_count != 1 || !cs_insn_group(dis->cs, dis->insn, CS_GRP_JUMP))
		return false;

	// Do we care about checking if it is a conditional jump or not?
	uint8_t c = dis->insn->detail->regs_read_count;
	if (conditional != NULL && c != 0)
	{
		uint8_t *r = dis->insn->detail->regs_read;
		do
		{
			--c;
			register uint8_t reg = r[c];
			if (reg == X86_REG_EFLAGS || reg == X86_REG_RCX || reg == X86_REG_CX || reg == X86_REG_ECX)
			{
				*conditional = true;
				break;
			}
		} while (c != 0);
	}

	return true;
}

//--------------------------------------------------------------------------
static bool gnx_disasm_branch_info_(
	gnx_disasm_t *dis,
	branch_info_t *bi)
{
	bool conditional = false;

	bi->info = 0;
	if (gnx_disasm_is_jump_(dis, &conditional))
	{
		bi->info |= GNX_DIS_BI_IS_JMP;
		if (conditional)
			bi->info |= GNX_DIS_BI_IS_COND;
	}
	else if (gnx_disasm_is_call_(dis))
	{
		bi->info |= GNX_DIS_BI_IS_CALL;
	}
	else if (gnx_disasm_is_ret_(dis))
	{
		bi->info |= GNX_DIS_BI_IS_RET;
	}
	else
	{
		// Not a supported branching instruction
		return false;
	}
	cs_x86 *x86 = &(dis->insn->detail->x86);

	uint8_t b = x86->opcode[0];
	if (conditional)
	{
		// 7x cb - opcode (rel8)
		if (((b & 0xf0) >> 4) == 0x07)
		{
			bi->index = b & 0xf;
			bi->info |= GNX_DIS_BI_IS_REL8;
		}
		// 0f 8x cd - opcode (rel32)
		else if (b == 0x0f)
		{
			b = x86->opcode[1];
			if ((b & 0xf0) >> 4 != 0x08)
				return false;

			bi->index = b & 0xf;
			bi->info |= GNX_DIS_BI_IS_REL32;
		}
		// E3 cb - opcode (JECX)
		else if (b == 0xE3)
		{
			// note: Capstone hides the 0x67 prefix from the opcode byte and exposes
			//       it in the details->x86->prefix[3] (indicates address-size override (X86_PREFIX_ADDRSIZE))
			bi->info |= GNX_DIS_BI_IS_REL8 |
				(dis->insn->detail->x86.prefix[3] == X86_PREFIX_ADDRSIZE ? GNX_DIS_BI_IS_X86_JCX : GNX_DIS_BI_IS_X86_JECX);
            bi->index = 0;
		}
		else
		{
			// Unknown conditional jump
			return false;
		}
	}
	// Non conditionals
	else
	{
		// rel8 jmp
		if (b == 0xEB)
		{
			bi->info |= GNX_DIS_BI_IS_REL8;
		}
		// rel32 jmp|call
		else if (b == 0xE9 || b == 0xE8)
		{
			bi->info |= GNX_DIS_BI_IS_REL32;
		}
	}

	bi->target = x86->operands[0].imm;

	return true;
}

//--------------------------------------------------------------------------
static bool gnx_disasm_follow_jmp_(
	gnx_disasm_t *dis,
	uint64_t *target)
{
	cs_x86 *x86 = &(dis->insn->detail->x86);
	cs_x86_op *op0 = x86->operands + 0;

	// Quick checks:
	// - We need just one operand (fast check)
	// - Jump group
	// - Unconditional jump (looks at no flags)
	if (	x86->op_count != 1
		|| !cs_insn_group(dis->cs, dis->insn, CS_GRP_JUMP)
		|| cs_reg_read(dis->cs, dis->insn, X86_REG_EFLAGS)
		|| op0->size != 4)
	{
		return false;
	}

	// Immediate jump
	if (op0->type == X86_OP_IMM)
	{
		*target = op0->imm;
	}
	// Ensure this is indirect with no variables (no registers reference)
	else if (	op0->type == X86_OP_MEM
			 &&	op0->mem.index == X86_REG_INVALID
			 && op0->mem.base == X86_REG_INVALID)
	{
		// Dereference the address
		*target = (*(uint32_t *)(uint32_t)op0->mem.disp);
	}
	else
	{
		return false;
	}

	return true;
}

//--------------------------------------------------------------------------
// x86/32bits instruction relocation routine
static gnx_err_t gnx_disasm_relocate_instruction_(
	gnx_disasm_t *dis,
	const void *_src,
	void *_dest,
	const branch_info_t *bi,
	size_t *instr_sz)
{
    (void)dis;
	uint8_t *dest = (uint8_t *)_dest;
	
	// The IP where the JMP/CALL are taking place
	uint8_t *_ip  = (uint8_t *)_dest;

	const uint8_t *src = (const uint8_t *)_src;
	register uint32_t info = bi->info;
	int opcode_size = 1;

	// rel8+[un]conditional jump?
	if (GNX_HAS_FLAG(info, GNX_DIS_BI_IS_REL8))
	{
		// There are no rel8 calls
		if (!GNX_HAS_FLAG(info, GNX_DIS_BI_IS_JMP))
			return GNX_ERR_INST_COPY;

		uint8_t index = bi->index;

        // Is this a JCX or JECX? we cannot relocate without generating a TEST+JZ rel32 instructions
		if (GNX_HAS_FLAG(info, GNX_DIS_BI_IS_X86_JCX | GNX_DIS_BI_IS_X86_JECX))
		{
			// Generate size prefix for the JCXZ
			if (GNX_HAS_FLAG(info, GNX_DIS_BI_IS_X86_JCX))
				*dest++ = 0x66;

            // test [e]cx, [e]cx
			*dest++ = 0x85; *dest++ = 0xC9;

            // Emit a synthetic JZ
			index = 4;

			// Adjust the originating IP
			_ip = dest;
		}

		// conditional rel8 jump
		if (GNX_HAS_FLAG(info, GNX_DIS_BI_IS_COND))
		{
			// Convert relative conditional rel8 jump to rel32 conditional jump
			*dest++ = 0xf;
			*dest++ = (0x80 + index);
			opcode_size = 2;
		}
		// unconditional rel8 jmp
		else
		{
            // Convert to JMP rel32
			*dest++ = 0xE9;
		}
	}
	// rel32+[un]conditional jump/call?
	else if (GNX_HAS_FLAG(info, GNX_DIS_BI_IS_REL32) && GNX_HAS_FLAG(info, GNX_DIS_BI_IS_JMP | GNX_DIS_BI_IS_CALL))
	{
		// Copy the same opcode
		*dest++ = *src++;

		// Conditional jumps have two opcodes
		if (GNX_HAVE_FLAGS(info, GNX_DIS_BI_IS_COND | GNX_DIS_BI_IS_JMP))
		{
			*dest++ = *src++;
			opcode_size = 2;
		}
	}
    // Unknown instruction. Don't know how to relocate
	else
	{
		return GNX_ERR_INST_COPY;
	}

	// Ready to adjust the new target:
	// target_addr = IP + opcode_addr + sizeof_instruction(@IP)
	// -> opcode_addr = target_addr - (IP + sizeof_instruction(@IP))
    
    #pragma warning(push)
    #pragma warning(disable: 4213)
    *((int32_t *)dest)++ = (int32_t)(bi->target) - (int32_t)(_ip) - (opcode_size + sizeof(int32_t));

    #pragma warning(pop)

	// Compute relocated instruction size
	*instr_sz = (size_t)dest - (size_t)_dest;

	return GNX_ERR_OK;
}