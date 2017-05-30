namespace test_block
{
//--------------------------------------------------------------------------
void test_block_1()
{
    size_t chunk_size = GNX_ALIGN_UP(300, 16);
    gnx_block_options_t block_opt = {
        1 * 1024,
        chunk_size,
        16,
        GNX_MEM_RWX
    };

    gnx_handle_t bh = gnx_block_create(&block_opt);

    uint8_t *chunk;
    uint8_t ch = 'A' - 1;

    // block1
    *(chunk = (uint8_t *)gnx_block_chunk_alloc(bh)) = 0x10;
    auto b1_0 = chunk;
    *(chunk = (uint8_t *)gnx_block_chunk_alloc(bh)) = 0x11;
    auto b1_1 = chunk;
    *(chunk = (uint8_t *)gnx_block_chunk_alloc(bh)) = 0x12;

    // block2
    *(chunk = (uint8_t *)gnx_block_chunk_alloc(bh)) = 0x20;
    auto b2_0 = chunk;
    *(chunk = (uint8_t *)gnx_block_chunk_alloc(bh)) = 0x21;
    auto b2_1 = chunk;
    *(chunk = (uint8_t *)gnx_block_chunk_alloc(bh)) = 0x22;
    auto b2_2 = chunk;

    // block3
    *(chunk = (uint8_t *)gnx_block_chunk_alloc(bh)) = 0x30;
    auto b3_0 = chunk;
    *(chunk = (uint8_t *)gnx_block_chunk_alloc(bh)) = 0x31;
    auto b3_1 = chunk;
    *(chunk = (uint8_t *)gnx_block_chunk_alloc(bh)) = 0x32;
    auto b3_2 = chunk;

    // block4
    *(chunk = (uint8_t *)gnx_block_chunk_alloc(bh)) = 0x40;
    auto b4_0 = chunk;
    *(chunk = (uint8_t *)gnx_block_chunk_alloc(bh)) = 0x41;
    auto b4_1 = chunk;
    *(chunk = (uint8_t *)gnx_block_chunk_alloc(bh)) = 0x42;
    auto b4_2 = chunk;

    // block5
    *(chunk = (uint8_t *)gnx_block_chunk_alloc(bh)) = 0x50;
    auto b5_0 = chunk;
    *(chunk = (uint8_t *)gnx_block_chunk_alloc(bh)) = 0x51;
    auto b5_1 = chunk;
    *(chunk = (uint8_t *)gnx_block_chunk_alloc(bh)) = 0x52;
    auto b5_2 = chunk;

    gnx_block_chunk_free(bh, b2_2);
    gnx_block_chunk_free(bh, b3_1);

    // block3
    (chunk = (uint8_t *)gnx_block_chunk_alloc(bh));
    assert(*chunk == 0x31);
    // block2
    (chunk = (uint8_t *)gnx_block_chunk_alloc(bh));
    assert(*chunk == 0x22);

    // block6
    *(chunk = (uint8_t *)gnx_block_chunk_alloc(bh)) = 0x60;
    auto b6_0 = chunk;
    gnx_block_chunk_free(bh, b6_0);
    (chunk = (uint8_t *)gnx_block_chunk_alloc(bh));
    assert(*chunk == 0x60);
    *(chunk = (uint8_t *)gnx_block_chunk_alloc(bh)) = 0x61;

 
    gnx_block_chunk_free(bh, b1_0);
    gnx_block_chunk_free(bh, b2_1);
    gnx_block_chunk_free(bh, b5_2);

    gnx_block_chunk_iterator_t it;
    gnx_block_chunk_iter_begin(bh, &it);
    while (gnx_block_chunk_iter_next(&it, (void **)&chunk))
        printf("Chunk byte = %02x\n", *chunk);

    gnx_block_free(bh);
}

//--------------------------------------------------------------------------
void test_block_2()
{
    gnx_block_options_t block_opt = {
        32 * 9,
        20,
        16,
        GNX_MEM_RWX
    };

    gnx_handle_t bh = gnx_block_create(&block_opt);

    uint8_t *chunk;

    // block1
    for (int i=0; i<9;++i)
        *(chunk = (uint8_t *)gnx_block_chunk_alloc(bh)) = 0x11 + i;

    // block2
    for (int i = 0; i < 9; ++i)
        *(chunk = (uint8_t *)gnx_block_chunk_alloc(bh)) = 0x21 + i;

    gnx_block_chunk_iterator_t it;
    gnx_block_chunk_iter_begin(bh, &it);
    while (gnx_block_chunk_iter_next(&it, (void **)&chunk))
    {
        printf("Chunk byte = %02x\n", *chunk);
    }

    gnx_block_free(bh);
}
} // namespace