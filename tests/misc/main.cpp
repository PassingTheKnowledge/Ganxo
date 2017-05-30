#include "stdafx.h"

#include "test-gnx-ds.cpp"
#include "test-gnx-block.cpp"
#include "test-gnx-disasm.cpp"

int main()
{
    gnx_init();
    test_disasm::test_align();
    test_block::test_block_2();
    exit(0);
    return 0;
}