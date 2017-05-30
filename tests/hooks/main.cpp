#include "stdafx.h"

#define RET_ON_ERR(err) if ((err) != GNX_ERR_OK) return (err);

//--------------------------------------------------------------------------
typedef VOID(WINAPI *Sleep_proto)(DWORD);
Sleep_proto orig_Sleep[2];

static DWORD g_SleepCount = 0;
VOID WINAPI My_Sleep(DWORD dwMilliSecs)
{
    ++g_SleepCount;
    orig_Sleep[0](10);
}

VOID WINAPI My_Sleep2(DWORD dwMilliSecs)
{
    ++g_SleepCount;
    orig_Sleep[1](10);
}

//-------------------------------------------------------------------------
typedef BOOL (WINAPI *DeleteFileA_proto)(LPCSTR lpFileName);

DeleteFileA_proto orig_DeleteFileA = DeleteFileA;

BOOL WINAPI my_DeleteFileA(LPCSTR lpFileName)
{
    return TRUE;
}

//-------------------------------------------------------------------------
typedef HANDLE(WINAPI *CreateFileA_proto)(
    LPCSTR lpFileName,
    DWORD dwDesiredAccess,
    DWORD dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    DWORD dwCreationDisposition,
    DWORD dwFlagsAndAttributes,
    HANDLE hTemplateFile);

CreateFileA_proto orig_CreateFileA = CreateFileA;
HANDLE WINAPI my_CreateFileA(
    LPCSTR lpFileName,
    DWORD dwDesiredAccess,
    DWORD dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    DWORD dwCreationDisposition,
    DWORD dwFlagsAndAttributes,
    HANDLE hTemplateFile)
{
    //printf("Opening('%s')\n", lpFileName);
    return INVALID_HANDLE_VALUE;
}

char g_szExeName[MAX_PATH];

//-------------------------------------------------------------------------
bool check_open_self()
{
    HANDLE hFile = CreateFileA(g_szExeName, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (hFile != INVALID_HANDLE_VALUE)
    {
        CloseHandle(hFile);
        return true;
    }
    return false;
}

//-------------------------------------------------------------------------
gnx_err_t test_hook_unhook_complete(gnx_handle_t gnx)
{
    auto c_orig_CreateFileA = ::CreateFileA;

    gnx_err_t err;

    gnx_handle_t transaction;
    err = gnx_transaction_begin(gnx, &transaction);
    RET_ON_ERR(err);

    // Hook
    err = gnx_transaction_add_hook(
        transaction,
        (void **)&orig_CreateFileA,
        my_CreateFileA);
    RET_ON_ERR(err);

    if (c_orig_CreateFileA == orig_CreateFileA)
    {
        printf("Springboard not created!\n");
        return GNX_ERR_FAILED;
    }

    err = gnx_transaction_commit(transaction);
    RET_ON_ERR(err);
    
    if (check_open_self())
    {
        printf("Hook does not seem to be working...\n");
        return GNX_ERR_FAILED;
    }

    // Unhook
    err = gnx_transaction_begin(gnx, &transaction);
    RET_ON_ERR(err);

    err = gnx_transaction_remove_hook(
        transaction,
        (void **)&orig_CreateFileA);
    RET_ON_ERR(err);

    gnx_transaction_commit(transaction);

    if (c_orig_CreateFileA != orig_CreateFileA)
    {
        printf("Springboard not destroyed!\n");
        return GNX_ERR_FAILED;
    }

    return err;
}

//-------------------------------------------------------------------------
gnx_err_t test_hook_abort(gnx_handle_t gnx)
{
    gnx_err_t err;

    gnx_handle_t transaction;
    err = gnx_transaction_begin(gnx, &transaction);
    RET_ON_ERR(err);

    auto c_orig_CreateFileA = CreateFileA;

    err = gnx_transaction_add_hook(
        transaction,
        (void **)&orig_CreateFileA,
        my_CreateFileA);
    RET_ON_ERR(err);

    if (c_orig_CreateFileA == orig_CreateFileA)
    {
        printf("Springboard not created!\n");
        return GNX_ERR_FAILED;
    }

    // Should succeed
    bool ok = check_open_self();
    if (!ok)
        return GNX_ERR_FAILED;

    err = gnx_transaction_abort(transaction);
    RET_ON_ERR(err);

    if (c_orig_CreateFileA != orig_CreateFileA)
    {
        printf("Springboard not destroyed!\n");
        return GNX_ERR_FAILED;
    }

    return err;
}

//-------------------------------------------------------------------------
gnx_err_t test_mixed_hook_unhook_complete(gnx_handle_t gnx)
{
    auto c_orig_CreateFileA = ::CreateFileA;
    auto c_orig_DeleteFileA = ::DeleteFileA;

    gnx_err_t err;

    gnx_handle_t transaction;
    err = gnx_transaction_begin(gnx, &transaction);
    RET_ON_ERR(err);

    // Hook - 1
    err = gnx_transaction_add_hook(
        transaction,
        (void **)&orig_CreateFileA,
        my_CreateFileA);
    RET_ON_ERR(err);

    if (c_orig_CreateFileA == orig_CreateFileA)
    {
        printf("CreateFileA springboard not created!\n");
        return GNX_ERR_FAILED;
    }

    err = gnx_transaction_commit(transaction);
    RET_ON_ERR(err);

    if (check_open_self())
    {
        printf("Hook does not seem to be working...\n");
        return GNX_ERR_FAILED;
    }

    // Cannot delete self
    BOOL expected_del = DeleteFileA(g_szExeName);

    // Hook and unhook
    err = gnx_transaction_begin(gnx, &transaction);
    RET_ON_ERR(err);

    err = gnx_transaction_remove_hook(
        transaction,
        (void **)&orig_CreateFileA);
    RET_ON_ERR(err);

    err = gnx_transaction_add_hook(
        transaction,
        (void **)&orig_DeleteFileA,
        my_DeleteFileA);

    if (c_orig_DeleteFileA == orig_DeleteFileA)
    {
        printf("DeleteFileA springboard not created!\n");
        return GNX_ERR_FAILED;
    }

    err = gnx_transaction_remove_hook(
        transaction,
        (void **)&orig_CreateFileA);
    RET_ON_ERR(err);

    err = gnx_transaction_commit(transaction);
    RET_ON_ERR(err);

    if (DeleteFileA(g_szExeName) == expected_del)
    {
        printf("DeleteFileA does not seem to be hooked\n");
        return err;
    }

    if (c_orig_CreateFileA != orig_CreateFileA)
    {
        printf("CreateFileA springboard not destroyed!\n");
        return GNX_ERR_FAILED;
    }

    return err;
}

//-------------------------------------------------------------------------
gnx_err_t test_hook2_unhook2_complete(gnx_handle_t gnx)
{
    Sleep_proto hook_Sleep[2] = { My_Sleep, My_Sleep2 };
    gnx_err_t err;

    g_SleepCount = 0;
    int accul = 0;
    gnx_handle_t transaction;
    for (int i = 0;i<_countof(orig_Sleep); i++)
    {
        err = gnx_transaction_begin(gnx, &transaction);
        RET_ON_ERR(err);

        // Hook
        orig_Sleep[i] = Sleep;
        err = gnx_transaction_add_hook(
            transaction,
            (void **)&orig_Sleep[i],
            hook_Sleep[i]);

        RET_ON_ERR(err);

        err = gnx_transaction_commit(transaction);
        RET_ON_ERR(err);

        Sleep(10);

        accul += i + 1;
        if (g_SleepCount != accul)
        {
            printf("Hook does not seem to be working\n");
            return GNX_ERR_FAILED;
        }
    }

    // Unhook
    err = gnx_transaction_begin(gnx, &transaction);
    RET_ON_ERR(err);

    err = gnx_transaction_remove_hook(
        transaction,
        (void **)&orig_Sleep);
    RET_ON_ERR(err);

    gnx_transaction_commit(transaction);

    return err;
}

//-------------------------------------------------------------------------
int main()
{
    GetModuleFileNameA(
        GetModuleHandle(NULL),
        g_szExeName,
        _countof(g_szExeName));

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

    err = test_hook_unhook_complete(gnx);
    RET_ON_ERR(err);

    err = test_hook_abort(gnx);
    RET_ON_ERR(err);

    err = test_mixed_hook_unhook_complete(gnx);
    RET_ON_ERR(err);

    err = test_hook2_unhook2_complete(gnx);
    RET_ON_ERR(err);

    gnx_close(gnx);

    return 0;
}