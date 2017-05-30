#ifndef __GANXO_INC__
#define __GANXO_INC__

/*

Ganxo - An opensource API hooking framework by Elias Bachaalany - < elias at passingtheknowledge.net >

*/

#include <stdint.h>
#include <stdbool.h>

/*! \file ganxo.h
    \brief Ganxo library header file.
           It contains all the support public APIs and data structures
*/


//--------------------------------------------------------------------------
// Architecture and platform definitions

//
// Architecture specific macros
#if defined(GANXO_ARCH_X86)
    #define GANXO_MAX_INSTR_SIZE 15
#elif defined(GANXO_ARCH_X64)
    #define GANXO_MAX_INSTR_SIZE 15
#else
    #error No architecture defined for Ganxo. Please specify either GANXO_ARCH_X86 or GANXO_ARCH_X64
#endif

//
// Verify that a platform was selected
#if !defined (GANXO_PLATFORM_WINDOWS)
    #error No platform defined for Ganxo. Please specify GANXO_PLATFORM_WINDOWS
#endif

//
// Compiler specifics
//
// Calling convention and exported functions
#ifdef _MSC_VER
    #define GANXO_API __cdecl
    #ifdef GANXO_LIBRARY
        #define GANXO_EXPORT __declspec(dllexport)
    #else
        #define GANXO_EXPORT
    #endif
#endif

//--------------------------------------------------------------------------
// Types and macros
//--------------------------------------------------------------------------

/// Ganxo opaque handle
typedef struct __gnx_handle_t { int __dummy; } *gnx_handle_t;

/// Invalid Ganxo handle value
#define GNX_INVALID_HANDLE ((gnx_handle_t)NULL)

/// Tests if all flags are set
#define GNX_HAVE_FLAGS(val, fl) ((val & (fl)) == (fl))

/// Tests if a single flag is set
#define GNX_HAS_FLAG(val, fl) ((val & (fl)) != 0)

/// Round up division
#define GNX_ROUND_UP_DIV(m, n) ( ((m) + (n) - 1) / (n) )

/// Align up
#define GNX_ALIGN_UP(val, aln) ( ((val) + ((aln)-1)) & ~((aln)-1) )

/// Returns the parent structure address given an address of a member
/// \note This is very similar to NT's CONTAINING_RECORD
#define GNX_CONTAINING_RECORD(ptr, type, member) \
    (type *)((char *)(ptr) - offsetof(type, member))

/// Ganxo virtual memory flags
typedef enum __gnx_mem_flags_t
{
    GNX_MEM_NONE    = 0x00000000,   ///< No memory access
    GNX_MEM_READ    = 0x00000001,   ///< Read flag is always present
    GNX_MEM_WRITE   = 0x00000002,   ///< Write
    GNX_MEM_EXEC    = 0x00000004,   ///< Executable

    GNX_MEM_RWX     = GNX_MEM_READ | GNX_MEM_WRITE | GNX_MEM_EXEC, ///< Read/Write/Execute
} gnx_mem_flags_t;

/// Ganxo error codes
typedef enum __gnx_error_t
{
    GNX_ERR_OK = 0,                             ///< Success
    GNX_ERR_FAILED,                             ///< Unspecified failure
    GNX_ERR_NO_MEM,                             ///< Not enough memory
    GNX_ERR_INVALID_ARGS,                       ///< Invalid arguments or flags
    GNX_ERR_NOT_SUPPORTED,                      /*!< Operation not supported. Usually due to third part libraries (ex: Capstone not 
                                                     having the desired architecture compiled in). */
    GNX_ERR_DISASM,                             ///< Disassembler error
    GNX_ERR_INST_COPY,                          /*!< Don't know how to copy instruction. The instruction
                                                     could be not relocatable or supported. */
    GNX_ERR_PARTIAL,                            ///< Operation succeeded only partially
    GNX_ERR_BUFFER_TOO_SMALL,                   ///< The destination buffer is too small.
    GNX_ERR_FUNCTION_TOO_SMALL,                 ///< Function is too small to be copied and replaced with a springboard.
    GNX_ERR_NOT_IMPLEMENTED                     ///< Functionality not implemented
} gnx_err_t;

//--------------------------------------------------------------------------
// Platform independent APIs (Memory functions, etc.)
//--------------------------------------------------------------------------

/// \name Platform independent functions prototypes
/// @{
typedef void *(GANXO_API *gnx_malloc_proto)(size_t size);
typedef void  (GANXO_API *gnx_mfree_proto)(void *block);

typedef void *(GANXO_API *gnx_vmalloc_proto)(
    size_t size,
    gnx_mem_flags_t flags);

typedef gnx_err_t (GANXO_API *gnx_vmfree_proto)(void *block);

typedef gnx_err_t (GANXO_API *gnx_vmprotect_proto)(
    const void *block,
    size_t size,
    gnx_mem_flags_t flags,
    gnx_mem_flags_t *old_flags ///< Optional. Contains the old protection value
    );

typedef gnx_err_t (GANXO_API *gnx_flush_instruction_cache_proto)(
    void *proc,             ///< Reserved. Pass NULL
    const void *address,    ///< Address
    size_t size             ///< Flush size
    );
/// @}

/// Platform independent APIs structure
typedef struct __gnx_platform_apis_t
{
    ///< Platform structure size. (Used to identify versions)
    uint32_t cb;

    ///< Memory allocate
    gnx_malloc_proto    malloc;

    ///< Memory free
    gnx_mfree_proto     mfree;

    ///< Virtual memory allocation
    /// \return The returned memory block is OS/architecture page aligned.
    gnx_vmalloc_proto   vmalloc;

    ///< Change virtual memory protection
    gnx_vmprotect_proto vmprotect;

    ///< Virtual memory free
    gnx_vmfree_proto    vmfree;

    ///< Flush instruction cache
    gnx_flush_instruction_cache_proto flush_instruction_cache;

} gnx_platform_apis_t;

/// Ganxo malloc() a type (ala C++'s new operator)
#define GNX_ALLOC(type) ((type *)gnx_malloc(sizeof(type)))

/// Ganxo mfree() a type (ala C++'s delete operator)
#define GNX_FREE(ptr) gnx_mfree((void *)(ptr))

/// Allocate memory
GANXO_EXPORT void *GANXO_API gnx_malloc(size_t size);

/// Free memory
GANXO_EXPORT void GANXO_API gnx_mfree(void *block);

/// Virtual alloc
GANXO_EXPORT void *GANXO_API gnx_vmalloc(
    size_t size, 
    gnx_mem_flags_t flags); 

/// Change virtual memory protection
/// \param flags New protection value
/// \param old_flags Optional out parameter to hold the previous protection value
GANXO_EXPORT gnx_err_t GANXO_API gnx_vmprotect(
    const void *block,
    size_t size,
    gnx_mem_flags_t flags,
    gnx_mem_flags_t *old_flags);

/// Virtual free
GANXO_EXPORT gnx_err_t GANXO_API gnx_vmfree(void *block);

/// Flush instruction cache
/// \param proc Process identification object (pass NULL)
/// \param address Address to flush
/// \param Size Size
GANXO_EXPORT gnx_err_t GANXO_API gnx_flush_instruction_cache(
    void *proc,
    const void *address,
    size_t size);

//--------------------------------------------------------------------------
// Ganxo functions
//--------------------------------------------------------------------------

/// Initialize the Ganxo library.
/// \note: Must be called before any other APIs.
GANXO_EXPORT gnx_err_t GANXO_API gnx_init(void);


/// Set platform APIs
/// You may call this function many times to update/switch the platform apis.
/// This is not a thread safe function.
/// \param apis Platform APIs pointers. If any function pointer that is set to NULL will
///             the system default API will be used. A copy of the passed parameters will be
///             stored in the Ganxo library.
GANXO_EXPORT gnx_err_t GANXO_API gnx_set_platform_apis(gnx_platform_apis_t *apis);


/// Create a new Ganxo workspace
GANXO_EXPORT gnx_err_t GANXO_API gnx_open(gnx_handle_t *handle);


/// Free a workspace
GANXO_EXPORT void GANXO_API gnx_close(gnx_handle_t handle);


//--------------------------------------------------------------------------
// Assembler & Disassembler functions
//--------------------------------------------------------------------------


/// Create a disassembler subsystem
GANXO_EXPORT gnx_handle_t GANXO_API gnx_disasm_create(void);

/// Returns the internal disassembler handler from the Ganxo workspace
GANXO_EXPORT gnx_handle_t GANXO_API gnx_disasm_from_workspace(gnx_handle_t wks_handle);

/// De-initialize disassembler
GANXO_EXPORT void GANXO_API gnx_disasm_free(gnx_handle_t dishandle);


/// Copy a single instruction and advances the source and destination
/// pointers accordingly.
/// \param src The source instruction address
/// \param dest The destination instruction address. The destination size must be
///        at least \sa GANXO_MAX_INSTR_SIZE.
/// \note The copied instruction may not necessarily look like the source one
///       because relative jumps and calls will be relocated accordingly.
GANXO_EXPORT gnx_err_t GANXO_API gnx_disasm_copy_instruction(
    gnx_handle_t dishandle,
    const void **src,
    void **dest);


/// Skip all unconditional jumps and land in the actual target.
/// \note This function will not follow indirect branches
/// \return The actual target of the passed address after all the jumps are skipped.
GANXO_EXPORT const void *GANXO_API gnx_disasm_skip_jumps(
    gnx_handle_t dishandle,
    const void *addr);


/// Disassemble a single instruction.
/// \param src The address to disassemble
/// \param size Optional output parameter that will contain the instruction size
/// \note The Disassembled instruction will be remembered as long
///       as no other instruction has been disassembled and overwritten the result.
///       For thread safety, create a disassembler instance in each thread (\ref gnx_disasm_create).
GANXO_EXPORT gnx_err_t GANXO_API gnx_disasm_instruction(
    gnx_handle_t dishandle,
    const void *src,
    size_t *size);


/// Checks if a previously disassembled instruction is a call (SysCall and INT included)
/// \note See \ref gnx_disasm_instruction
GANXO_EXPORT bool GANXO_API gnx_disasm_is_call(gnx_handle_t handle);


/// Checks if a previously disassembled instruction is a branch
/// \param conditional Returns whether this is a conditional or unconditional branch
GANXO_EXPORT bool GANXO_API gnx_disasm_is_jump(
    gnx_handle_t dishandle,
    bool *conditional);


/// Checks if a previously disassembled instruction is a RET (or IRET)
GANXO_EXPORT bool GANXO_API gnx_disasm_is_ret(gnx_handle_t handle);


/// Check if a previously disassembled instruction is used for alignment 
/// \return Optionally returns the alignment instruction size
GANXO_EXPORT bool GANXO_API gnx_disasm_is_align(
    gnx_handle_t handle,
    size_t *size);


/// Generate a relative jump or call instruction.
/// \param target This argument can be computed using the \sa GNX_DISASM_GET_X86_RELADDR_OPERAND
/// \param dest In/out argument pointing to the destination buffer that will contain the generated instruction. The destination size must be
///        at least \sa GANXO_MAX_INSTR_SIZE.
GANXO_EXPORT gnx_err_t GANXO_API gnx_asm_gen_relbranch(
    gnx_handle_t dishandle,
    bool call_or_jmp,
    const void *target,
    void **dest);

//--------------------------------------------------------------------------
// Memory blocks functions
//--------------------------------------------------------------------------

/// Each block will contain equal number of chunks.
/// There will be additional bytes taken out of the block size to describe
//  the block's meta information (free chunk bitmap, etc.)
typedef struct __gnx_block_options_t
{
    size_t block_size;          ///< Total block size. It should be aligned to the size of a page (4kb, etc.)
    size_t chunk_size;          ///< The chunk size in the block
    size_t chunk_align;         ///< Chunk alignment
    gnx_mem_flags_t vmflags;    ///< The mem flags used internally when vmalloc()ing a new block.
} gnx_block_options_t;


/// Opaque block chunk iterator
typedef struct __gnx_block_chunk_iterator_t
{
    char __dummy[sizeof(void *) * 3];
} gnx_block_chunk_iterator_t;


/// Create a new block
/// A block is very useful for allocating various equal sized chunks using the VM allocation routines.
/// When a block is full, a new block is automatically created. The returned block handle should be kept 
// alive during the lifetime of all the blocks and the chunks that are created.
GANXO_EXPORT gnx_handle_t GANXO_API gnx_block_create(gnx_block_options_t *options);


/// Frees all the blocks
GANXO_EXPORT void GANXO_API gnx_block_free(gnx_handle_t handle);


/// Allocates a chunk inside a free block. Previously freed chunks will be recycled without
/// causing a new memory allocations. If all blocks are full, a new block is automatically allocated.
GANXO_EXPORT void *GANXO_API gnx_block_chunk_alloc(gnx_handle_t handle);


/// Returns a previously allocated chunk to the freed chunks pool.
/// Note: No memory is reclaimed back to the operating system. A freed chunk
///       will be recycled for use in a subsequent \sa gnx_block_alloc_chunk call.
GANXO_EXPORT gnx_err_t GANXO_API gnx_block_chunk_free(
    gnx_handle_t handle,
    void *chunk);


/// Change the memory protection of all blocks (and their chunks therein)
/// \note May return \sa GNX_ERR_PARTIAL in case of partial success
GANXO_EXPORT gnx_err_t GANXO_API gnx_block_protect(
    gnx_handle_t handle,
    gnx_mem_flags_t mem_prot);


/// Initializes the iterator
GANXO_EXPORT void GANXO_API gnx_block_chunk_iter_begin(
    gnx_handle_t handle,
    gnx_block_chunk_iterator_t *iterator);


/// Iterate next
GANXO_EXPORT bool GANXO_API gnx_block_chunk_iter_next(
    gnx_block_chunk_iterator_t *iterator,
    void **chunk);


/// Allocate a new chunk of the desired type
#define GNX_ALLOC_CHUNK(handle, type) \
        (type *)gnx_block_chunk_alloc(handle)

//--------------------------------------------------------------------------
// Data structures
//--------------------------------------------------------------------------

/// Singly list item
/// Note: Embed it inside a structure or use the \sa GNX_SINGLY_LIST_ITEM_DEFINE macro
typedef struct __gnx_singly_list_item_t
{
    struct __gnx_singly_list_item_t *next;
} gnx_singly_list_item_t;

/// Macro that is used to declare the singly list entry inside a structure
/// \note This simply declares a known name for the singly list item.
///       This will facilitate the use of the other \ref GNX_SINGLY_LIST_ITEM_xxxx macros
#define GNX_SINGLY_LIST_ITEM_DEFINE \
    gnx_singly_list_item_t slist_entry

/// \defgroup GNX_SINGLY_LIST_ITEM_xxxx Singly list helper macros
/// @{
/// Pushes a structure into the singly linked list
#define GNX_SINGLY_LIST_ITEM_PUSH(head, pstruct) \
    gnx_singly_list_push((head), &((pstruct)->slist_entry))

/// Pops a structure from the singly list
#define GNX_SINGLY_LIST_ITEM_POP(head, type) \
    ((type *)((char *)gnx_singly_list_pop((head)) - offsetof(type, slist_entry)))

/// Returns the containing record of the singly list entry
#define GNX_SINGLY_LIST_RECORD(entry, type) \
    GNX_CONTAINING_RECORD(entry, type, slist_entry)

/// Initializes the singly list
#define GNX_SINGLY_LIST_DECLARE(head_name) \
    gnx_singly_list_item_t head_name; \
    gnx_singly_list_init(&head_name)

/// @}

/// Initializes the list head
inline void GANXO_API gnx_singly_list_init(
    gnx_singly_list_item_t *list_head)
{
    list_head->next = NULL;
}

/// Pushes an item to the list head.
inline void GANXO_API gnx_singly_list_push(
    gnx_singly_list_item_t *head,
    gnx_singly_list_item_t *entry)
{
    gnx_singly_list_item_t *head_next = head->next;
    head->next = entry;
    entry->next = head_next;
}


/// Pops an item from the list head
inline gnx_singly_list_item_t *GANXO_API gnx_singly_list_pop(
    gnx_singly_list_item_t *head)
{
    gnx_singly_list_item_t *next = head->next;
    if (next != NULL)
        head->next = next->next;

    return next;
}


/// Finds and unlinks a list entry
GANXO_EXPORT bool GANXO_API gnx_singly_list_remove(
    gnx_singly_list_item_t *head,
    gnx_singly_list_item_t *entry);


//--------------------------------------------------------------------------
// Transactions and function hooking
//--------------------------------------------------------------------------

/// Define user function hook params
#define GNX_ADD_HOOK_PARAMS(pOrig, hook) (void **)&(pOrig), (void *)(hook) 


/// Start a transaction
GANXO_EXPORT gnx_err_t gnx_transaction_begin(
    gnx_handle_t g,
    gnx_handle_t *handle);


/// Abort a transaction
GANXO_EXPORT gnx_err_t GANXO_API gnx_transaction_abort(
    gnx_handle_t handle);


/// Commit a transaction
GANXO_EXPORT gnx_err_t GANXO_API gnx_transaction_commit(
    gnx_handle_t handle);


/// Hook a function hook
/// \note The hook is not completed until the \sa gnx_transaction_commit or \sa gnx_transaction_abort is called
/// \retval GNX_ERR_FUNCTION_TOO_SMALL if the function could not be copied
/// \retval Not_GNX_ERR_OK Any other error value depending on what fails internally
GANXO_EXPORT gnx_err_t GANXO_API gnx_transaction_add_hook(
    gnx_handle_t handle,
    void **psrc,
    void *hook);


/// Add a remove function hook request to the transaction
GANXO_EXPORT gnx_err_t GANXO_API gnx_transaction_remove_hook(
    gnx_handle_t handle,
    void **psrc);

#endif
