#include "private.h"

//--------------------------------------------------------------------------
// First zero-bit location table
// Note: For the byte 0xFF the value 8 indicates that no zero-bits are found
static const uint8_t zbit_table[] = {
/*  0  1  2  3  4  5  6  7  */
	0, 1, 0, 2, 0, 1, 0, 3, /*   0-  7 */
	0, 1, 0, 2, 0, 1, 0, 4, /*   8- 15 */
	0, 1, 0, 2, 0, 1, 0, 3, /*  16- 23 */
	0, 1, 0, 2, 0, 1, 0, 5, /*  24- 31 */
	0, 1, 0, 2, 0, 1, 0, 3, /*  32- 39 */
	0, 1, 0, 2, 0, 1, 0, 4, /*  40- 47 */
	0, 1, 0, 2, 0, 1, 0, 3, /*  48- 55 */
	0, 1, 0, 2, 0, 1, 0, 6, /*  56- 63 */
	0, 1, 0, 2, 0, 1, 0, 3, /*  64- 73 */
	0, 1, 0, 2, 0, 1, 0, 4, /*  72- 79 */
	0, 1, 0, 2, 0, 1, 0, 3, /*  80- 87 */
	0, 1, 0, 2, 0, 1, 0, 5, /*  88- 95 */
	0, 1, 0, 2, 0, 1, 0, 3, /*  96-103 */
	0, 1, 0, 2, 0, 1, 0, 4, /* 104-111 */
	0, 1, 0, 2, 0, 1, 0, 3, /* 112-119 */
	0, 1, 0, 2, 0, 1, 0, 7, /* 120-127 */
	0, 1, 0, 2, 0, 1, 0, 3, /* 128-135 */
	0, 1, 0, 2, 0, 1, 0, 4, /* 136-143 */
	0, 1, 0, 2, 0, 1, 0, 3, /* 144-151 */
	0, 1, 0, 2, 0, 1, 0, 5, /* 152-159 */
	0, 1, 0, 2, 0, 1, 0, 3, /* 160-167 */
	0, 1, 0, 2, 0, 1, 0, 4, /* 168-175 */
	0, 1, 0, 2, 0, 1, 0, 3, /* 176-183 */
	0, 1, 0, 2, 0, 1, 0, 6, /* 184-191 */
	0, 1, 0, 2, 0, 1, 0, 3, /* 192-199 */
	0, 1, 0, 2, 0, 1, 0, 4, /* 200-207 */
	0, 1, 0, 2, 0, 1, 0, 3, /* 208-215 */
	0, 1, 0, 2, 0, 1, 0, 5, /* 216-223 */
	0, 1, 0, 2, 0, 1, 0, 3, /* 224-231 */
	0, 1, 0, 2, 0, 1, 0, 4, /* 232-239 */
	0, 1, 0, 2, 0, 1, 0, 3, /* 240-247 */
	0, 1, 0, 2, 0, 1, 0, 8, /* 248-255 */
};

//--------------------------------------------------------------------------
gnx_handle_t GANXO_API gnx_block_create(gnx_block_options_t *options)
{
    // Get the aligned chunk size;
    size_t chunk_aln_size;
    
    if (options->chunk_align == 0)
        chunk_aln_size = options->chunk_size;
    else
        chunk_aln_size = GNX_ALIGN_UP(
                            options->chunk_size, 
                            options->chunk_align);

	// Compute the initial number of chunks
	size_t nb_chunks = options->block_size / chunk_aln_size;
    if (nb_chunks == 0)
    {
        // Make sure the block size fits at least one chunk
        return GNX_INVALID_HANDLE;
    }

    // Allocate a block header
	gnx_block_header_t *bh = GNX_ALLOC(gnx_block_header_t);
    if (bh == NULL)
        return GNX_INVALID_HANDLE;

    bh->block_size = options->block_size;
    bh->chunk_size = chunk_aln_size;
    bh->nb_chunks = nb_chunks;
    bh->freebitmap_sz = GNX_ROUND_UP_DIV(nb_chunks, 8); // (We need one byte to describe 8 chunks; one bit per chunk)
	bh->vmflags = options->vmflags;

	bh->active_block = bh->last_block = bh->first_block = NULL;
    
	return (gnx_handle_t)bh;
}

//--------------------------------------------------------------------------
void GANXO_API gnx_block_free(gnx_handle_t handle)
{
    GET_BLOCK_HEADER;
    if (bh->first_block == NULL)
        return;

    gnx_block_t *start_block = bh->active_block;
    gnx_block_t *block = start_block;

    do
    {
        gnx_block_t *cur_block = block;
        block = block->next;

        // Free chunks
        gnx_vmfree(cur_block->chunk_base);

        // Free the block
        gnx_mfree(cur_block);
    } while (block != start_block);

    gnx_mfree(bh);
}

//--------------------------------------------------------------------------
// Simply allocates a block but does not link it with the block header
static gnx_block_t *alloc_block(gnx_block_header_t *bh)
{
    // Allocate memory for the block descriptor
    gnx_block_t *block = gnx_malloc(
        offsetof(gnx_block_t, free_bitmap) + bh->freebitmap_sz);

    do 
    {
        if (block == NULL)
            break;

        block->chunk_base = gnx_vmalloc(
            bh->block_size,
            bh->vmflags);
        if (block->chunk_base == NULL)
            break;

#ifdef GNX_DEBUG_BLOCK
        static int bid = 0;
        block->id = ++bid;
#endif
        memset(block->free_bitmap, 0, bh->freebitmap_sz);
        return block;

    } while (false);

    // Clean up
    if (block != NULL)
    {
        if (block->chunk_base != NULL)
            gnx_vmfree(block->chunk_base);
    }
    return NULL;
}

//--------------------------------------------------------------------------
void *GANXO_API gnx_block_chunk_alloc(gnx_handle_t handle)
{
	GET_BLOCK_HEADER;

    // First block ever?
	if (bh->first_block == NULL)
    {
        bh->last_block = bh->active_block = bh->first_block = alloc_block(bh);
        if (bh->first_block == NULL)
            return NULL;

        // Always link to the beginning
        bh->last_block->next = bh->first_block;
    }

    do
    {
        gnx_block_t *start_block = bh->active_block;
        gnx_block_t *block = start_block; 
        do
        {
            uint8_t *free_bitmap = block->free_bitmap;

            // Look for a free chunk slot in the block
            for (size_t n_8chunk = 0, bitmap_sz = bh->freebitmap_sz;
                 n_8chunk < bitmap_sz;
                 ++free_bitmap, ++n_8chunk)
            {
                uint8_t val = *free_bitmap;
                uint8_t bitpos;

                // Quick checks for special values
                if (val == 0) // slot is empty
                    bitpos = 0;
                else if (val == 0xff) // slot is full
                    continue;
                else
                    bitpos = zbit_table[val]; // Get the free bit position

                // What's the chunk number in the free bitmap
                size_t chunk_no = (n_8chunk * 8) + bitpos;
                if (chunk_no >= bh->nb_chunks)
                    break;

                // Mark as used
                *free_bitmap |= 1 << bitpos;

                // Remember the active block (speeds up future allocations)
                bh->active_block = block;

                // Return the stub's body start
                return block->chunk_base + (chunk_no * bh->chunk_size);
            }

            // Advance to next block (as long as we did not cycle back)
            block = block->next;
        } while (block != start_block);

        // At this point, all the allocated blocks are full, let's alloc a new block
        gnx_block_t *new_block = alloc_block(bh);
        if (new_block == NULL)
            return NULL;

        // Set active and last block then link the previous last block to new block
        bh->last_block->next = bh->active_block = new_block;

        // Set new last block as the new block
        bh->last_block = new_block;

        // Circular link to the beginning
        bh->last_block->next = bh->first_block;
    } while (true);
}

//--------------------------------------------------------------------------
gnx_err_t GANXO_API gnx_block_chunk_free(
    gnx_handle_t handle,
    void *chunk)
{
    GET_BLOCK_HEADER;

    gnx_block_t *start_block = bh->active_block;
    gnx_block_t *block = start_block;
    do
    {
        uint8_t *first_chunk = block->chunk_base;
        uint8_t *last_chunk  = first_chunk + bh->block_size;

        // Find the parent block
        if ((uint8_t *)chunk >= first_chunk && (uint8_t *)chunk < last_chunk)
        {
            // Compute the chunk's number so we can get its free bitmap position
            size_t chunk_no = ((uint8_t *)chunk - first_chunk) / bh->chunk_size;

            // Mark as free
            size_t idx = chunk_no / 8;
            size_t bit = chunk_no % 8;

            block->free_bitmap[idx] &= ~(1 << bit);

            bh->active_block = block;

            return GNX_ERR_OK;
        }
        block = block->next;
    } while (block != start_block);
    return GNX_ERR_INVALID_ARGS;
}

//--------------------------------------------------------------------------
// Changes the protection of all blocks (and their chunks therein)
gnx_err_t GANXO_API gnx_block_protect(
    gnx_handle_t handle, 
    gnx_mem_flags_t mem_prot)
{
    GET_BLOCK_HEADER;

    gnx_block_t *start_block = bh->active_block;
    gnx_block_t *block = start_block;
    if (start_block == NULL)
        return GNX_ERR_OK;

    gnx_err_t err;
    do
    {
        err = gnx_vmprotect(
            block->chunk_base, 
            bh->block_size, 
            mem_prot,
            NULL);

        if (err != GNX_ERR_OK)
            err = GNX_ERR_PARTIAL;

        block = block->next;
    } while (block != start_block);

    return GNX_ERR_OK;
}

//--------------------------------------------------------------------------
void GANXO_API gnx_block_chunk_iter_begin(
    gnx_handle_t handle,
    gnx_block_chunk_iterator_t *iterator)
{
    GET_BLOCK_HEADER;
    gnx_block_chunk_iterator_internal_t iter;

    iter.bh = bh;
    iter.chunk_no = 0;
    iter.block = bh->first_block;

    // Copy the internal iterator as an opaque structure back to the caller
    memcpy(
        iterator, 
        &iter, 
        sizeof(gnx_block_chunk_iterator_t));
}

//--------------------------------------------------------------------------
bool GANXO_API gnx_block_chunk_iter_next(
    gnx_block_chunk_iterator_t *iterator,
    void **chunk)
{
    gnx_block_chunk_iterator_internal_t *iter = (gnx_block_chunk_iterator_internal_t *)iterator;

    // There are no blocks at all
    if (iter->bh->first_block == NULL)
        return false;

    do 
    {
        // Get the beginning of the free bitmap of this block
        uint8_t *free_bitmap = iter->block->free_bitmap;

        // Continue with the last chunk number
        for (size_t i_chunk = iter->chunk_no, nb_chunks = iter->bh->nb_chunks; 
             i_chunk < nb_chunks; 
             ++i_chunk)
        {
            // Locate the byte position and the bit position in the free bitmap
            size_t idx = i_chunk / 8;
            size_t bit = i_chunk % 8;

            // Always be ahead by one position
            iter->chunk_no = i_chunk + 1;

            // Is this an occupied byte? If so, return it to the user
            if ((free_bitmap[idx] & (1 << bit)) != 0)
            {
                *chunk = iter->block->chunk_base + (i_chunk * iter->bh->chunk_size);
                return true;
            }
        }

        // Reset the chunk number (as we advance to next block)
        iter->chunk_no = 0;

        // Advance to next block as long as we did not cycle back
        iter->block = iter->block->next;
    } while (iter->block != iter->bh->first_block);

    // No more blocks, no more chunks
    return false;
}
