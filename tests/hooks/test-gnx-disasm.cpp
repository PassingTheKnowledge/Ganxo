namespace test_disasm
{

    const int nb_instr = 24;
unsigned char align_code[] = {
    0x90, 0x66, 0x90, 0x0F, 0x1F, 0x00, 0x0F, 0x1F, 0x40, 0x00, 0x0F, 0x1F, 0x44, 0x00, 0x00, 0x66,
    0x0F, 0x1F, 0x44, 0x00, 0x00, 0x0F, 0x1F, 0x80, 0x00, 0x00, 0x00, 0x00, 0x0F, 0x1F, 0x84, 0x00,
    0x00, 0x00, 0x00, 0x00, 0xCC, 0x66, 0x0F, 0x1F, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00, 0x66, 0x66,
    0x0F, 0x1F, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00, 0x66, 0x66, 0x66, 0x0F, 0x1F, 0x84, 0x00, 0x00,
    0x00, 0x00, 0x00, 0xD9, 0xD0, 0x87, 0xDB, 0x66, 0x87, 0xDB, 0x87, 0xC9, 0x66, 0x66, 0x66, 0x87,
    0xC9, 0x87, 0xF6, 0x89, 0xED, 0x89, 0xC0, 0x89, 0xFF, 0x66, 0x89, 0xFF, 0x66, 0x66, 0x89, 0xC0,
    0x89, 0xDB

/*
 00000000: 90                             nop
 00000001: 6690                           nop
 00000003: 0F1F00                         nop          [eax]
 00000006: 0F1F4000                       nop          [eax][0]
 0000000A: 0F1F440000                     nop          [eax][eax][0]
 0000000F: 660F1F440000                   nop          [eax][eax][0]
 00000015: 0F1F8000000000                 nop          [eax][0]
 0000001C: 0F1F840000000000               nop          [eax][eax][0]
 00000024: CC                             int          3
 00000025: 660F1F840000000000             nop          [eax][eax][0]
 0000002E: 66660F1F840000000000           nop          [eax][eax][0]
 00000038: 6666660F1F840000000000         nop          [eax][eax][0]
 00000043: D9D0                           fnop
 00000045: 87DB                           xchg         ebx,ebx
 00000047: 6687DB                         xchg         bx,bx
 0000004A: 87C9                           xchg         ecx,ecx
 0000004C: 66666687C9                     xchg         cx,cx
 00000051: 87F6                           xchg         esi,esi
 00000053: 89ED                           mov          ebp,ebp
 00000055: 89C0                           mov          eax,eax
 00000057: 89FF                           mov          edi,edi
 00000059: 6689FF                         mov          di,di
 0000005C: 666689C0                       mov          ax,ax
 00000060: 89DB                           mov          ebx,ebx
*/
};

//--------------------------------------------------------------------------
void test_asm()
{
    void *dest = gnx_vmalloc(
        4096, 
        GNX_MEM_RWX);

    gnx_handle_t dis = gnx_disasm_create();

    void *pdest = dest;
    gnx_asm_gen_relbranch(
        dis, 
        false,
        ::Sleep,
        &dest);

    size_t sz = size_t(dest) - size_t(pdest);

    typedef VOID(WINAPI *Sleep_proto)(DWORD);
    ((Sleep_proto)pdest)(1000);

    gnx_vmfree(dest);
    gnx_disasm_free(dis);
}

//--------------------------------------------------------------------------
void test_align()
{
    gnx_handle_t dis = gnx_disasm_create();
    if (dis == GNX_INVALID_HANDLE)
    {
        printf("Failed to initialize disassembler\n");
        return;
    }

    for (int i = 0, c = 0; c < nb_instr; ++c)
    {
        size_t sz;
        gnx_disasm_instruction(
            dis, 
            align_code + i, &sz);

        if (!gnx_disasm_is_align(dis, &sz))
        {
            printf("Not an alignment instruction @ %d!\n", i);
            break;
        }
        i += sz;
    }
}

} // namespace