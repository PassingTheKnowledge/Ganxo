#include "stdafx.h"

//--------------------------------------------------------------------------
typedef VOID(WINAPI *Sleep_proto)(DWORD);
typedef int (WINAPI *MessageBoxA_proto)(HWND hWnd, LPCSTR lpText, LPCSTR lpCaption, UINT uType);

Sleep_proto orig_Sleep = Sleep;

VOID WINAPI My_Sleep(DWORD dwMilliSecs)
{
    printf("MySleep() triggered! calling original sleep...");
    orig_Sleep(dwMilliSecs);
    printf("done!\n");
}

MessageBoxA_proto orig_MessageBoxA = MessageBoxA;

int WINAPI my_MessageBoxA(HWND hWnd, LPCSTR lpText, LPCSTR lpCaption, UINT uType)
{
    printf("MessageBoxA(hWnd=%08p, lpText='%s', lpCaption='%s', uType=%04X)\n",
        hWnd, lpText, lpCaption, uType);
    return IDOK;
}

//--------------------------------------------------------------------------
int main()
{
    gnx_err_t err = gnx_init();

    if (err != GNX_ERR_OK)
    {
        printf("Failed to initialize!\n");
        return -1;
    }

    // Create a new workspace instance
    gnx_handle_t gnx;
    if (gnx_open(&gnx) != GNX_ERR_OK)
    {
        printf("Failed to create workspace\n");
        return -1;
    }

    gnx_handle_t transaction;
    gnx_transaction_begin(gnx, &transaction);

    // Hook
    err = gnx_transaction_add_hook(
        transaction, 
        (void **)&orig_Sleep, 
        My_Sleep);
    err = gnx_transaction_add_hook(
        transaction, 
        (void **)&orig_MessageBoxA, 
        my_MessageBoxA);

    err = gnx_transaction_commit(transaction);

    // Call the hooked functions
    ::Sleep(10);
    
    MessageBoxA(0, "Hello", "Info", MB_OK);

    // Unhook
    gnx_transaction_begin(gnx, &transaction);
    gnx_transaction_remove_hook(
        transaction, 
        (void **)&orig_Sleep);
    gnx_transaction_remove_hook(transaction, (void **)&orig_MessageBoxA);
    gnx_transaction_commit(transaction);

    gnx_close(gnx);

    return 0;
}