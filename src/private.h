#ifndef __GANXO_PRIVATE_INC__
#define __GANXO_PRIVATE_INC__

#include <string.h>

#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable: 4820)
    #include <capstone.h>
    #pragma warning(pop)
#endif

#include <ganxo.h>

//--------------------------------------------------------------------------
// Disasm structures and macros
//--------------------------------------------------------------------------
#if defined(GANXO_ARCH_X86)
	#define GNX_CS_ARCH CS_ARCH_X86
	#define GNX_CS_MODE CS_MODE_32

	// E9 xx xx xx xx ; jmp rel32
	#define GANXO_JUMP_TO_SPRINGBOARD_SIZE 5

#elif defined(GANXO_ARCH_X64)
	#define GNX_CS_ARCH CS_ARCH_X86
	#define GNX_CS_MODE CS_MODE_64

	// FF 25 00 00 00 00       jmp cs:LongAddress
	// 00 00 00 00 00 00 00 00 LongAddress dq ?
	#define GANXO_JUMP_TO_SPRINGBOARD_SIZE 14
#endif

// The reasoning is that in the least we need GANXO_JUMP_TO_SPRINGBOARD_SIZE. If instructions are shorter, then
// then we need to also copy the next instruction which could be as big as the maximum instruction size.
#define GANXO_MAX_SPRINGBOARD_SIZE (GANXO_JUMP_TO_SPRINGBOARD_SIZE + GANXO_MAX_INSTR_SIZE)

// Return the Ganxo disasm structure from the handle
#define GET_DISASM gnx_disasm_t *dis = (gnx_disasm_t *)handle

/// Work structure for the disassembler utilities
typedef struct __gnx_disasm_t
{
	csh cs; ///< Capstone handle
	cs_insn *insn; ///< preallocated instruction
} gnx_disasm_t;


/// User hook springboard format
typedef struct __userhook_springboard_t
{
    uint8_t springboard[GANXO_MAX_SPRINGBOARD_SIZE];
    uint8_t backup[GANXO_MAX_SPRINGBOARD_SIZE];
    uint32_t backup_sz;
    void    *func_addr;
    void    *func_addr_final;
} userhook_springboard_t;

//--------------------------------------------------------------------------
// Block/chunks macros and structures
//--------------------------------------------------------------------------

/// Return the Ganxo block header structure from the handle
#define GET_BLOCK_HEADER \
    gnx_block_header_t *bh = (gnx_block_header_t *)handle

/// Return the chunk size
#define gnx_block_chunk_get_size(handle) \
    ((gnx_block_header_t *)handle)->chunk_size

/// Block definition
typedef struct __gnx_block_t
{
#ifdef GNX_DEBUG_BLOCK
    int id;
#endif
	struct __gnx_block_t *next; ///< Next linked block
    uint8_t *chunk_base;        ///< First chunk base address
    uint8_t free_bitmap[1];     ///< Variable size bitmap denoting the free chunks in the block
    uint8_t padding[3];
} gnx_block_t;

/// Block header, it describes everything about a block header.
typedef struct __gnx_block_header_t
{
	gnx_block_t *first_block;	///< Location of the first block
    gnx_block_t *active_block;  ///< Active block
    gnx_block_t *last_block;    ///< Last block in the chain
	gnx_mem_flags_t vmflags;	///< Memory flags used with the vmalloc() when allocating future blocks
	size_t block_size;			///< Size of each block.
	size_t chunk_size;			///< Size of each chunk in the block
	size_t nb_chunks;			///< Number of chunks in a block
    size_t freebitmap_sz;       ///< Size of the free chunks bitmap
} gnx_block_header_t;

/// Helper iterator structure to walk all the allocated chunks
typedef struct __gnx_block_chunk_iterator_internal_t
{
    gnx_block_header_t *bh;
    size_t chunk_no;
    gnx_block_t *block;
} gnx_block_chunk_iterator_internal_t;

// STATIC ASSERT: Verify that the opaque iterator matches the size of the internal iterator
typedef char __ASSERT_BLOCK_CHUNK_ITERATOR_SIZE[
        sizeof(gnx_block_chunk_iterator_t) == sizeof(gnx_block_chunk_iterator_internal_t) 
        ? 1 : -1];

//--------------------------------------------------------------------------
// Ganxo workspace structures
//--------------------------------------------------------------------------

#define GET_WORKSPACE gnx_workspace_t *ws = (gnx_workspace_t *)handle

/// This is the main workspace structure for the whole library
typedef struct __gnx_workspace_t
{
	gnx_handle_t dis;               ///< Disassembler
	gnx_handle_t user_hooks;        ///< The block handle for user-hooks springboards
} gnx_workspace_t;

#endif