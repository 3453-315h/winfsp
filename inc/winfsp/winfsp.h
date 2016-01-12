/**
 * @file winfsp/winfsp.h
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#ifndef WINFSP_WINFSP_H_INCLUDED
#define WINFSP_WINFSP_H_INCLUDED

#define WIN32_NO_STATUS
#include <windows.h>
#undef WIN32_NO_STATUS
#include <winternl.h>
#include <ntstatus.h>

#if defined(WINFSP_DLL_INTERNAL)
#define FSP_API                         __declspec(dllexport)
#else
#define FSP_API                         __declspec(dllimport)
#endif

#include <winfsp/fsctl.h>

/*
 * File System
 */
typedef struct _FSP_FILE_SYSTEM FSP_FILE_SYSTEM;
typedef VOID FSP_FILE_SYSTEM_DISPATCHER(FSP_FILE_SYSTEM *, FSP_FSCTL_TRANSACT_REQ *);
typedef NTSTATUS FSP_FILE_SYSTEM_OPERATION(FSP_FILE_SYSTEM *, FSP_FSCTL_TRANSACT_REQ *);
typedef struct _FSP_FILE_SYSTEM_INTERFACE
{
    NTSTATUS (*GetSecurity)(FSP_FILE_SYSTEM *FileSystem,
        PWSTR FileName, PDWORD PFileAttributes,
        PSECURITY_DESCRIPTOR SecurityDescriptor, SIZE_T *PSecurityDescriptorSize);
#if 0
    NTSTATUS (*Create)(FSP_FILE_SYSTEM *FileSystem,
        FSP_FSCTL_TRANSACT_REQ *Request, FSP_FILE_NODE **PFileNode);
    NTSTATUS (*Open)(FSP_FILE_SYSTEM *FileSystem,
        FSP_FSCTL_TRANSACT_REQ *Request, FSP_FILE_NODE **PFileNode);
    NTSTATUS (*Overwrite)(FSP_FILE_SYSTEM *FileSystem,
        FSP_FSCTL_TRANSACT_REQ *Request, BOOLEAN ReplaceFileAttributes, FSP_FILE_NODE *FileNode);
    VOID (*Cleanup)(FSP_FILE_SYSTEM *FileSystem,
        FSP_FSCTL_TRANSACT_REQ *Request, FSP_FILE_NODE *FileNode, BOOLEAN Delete);
    VOID (*Close)(FSP_FILE_SYSTEM *FileSystem,
        FSP_FSCTL_TRANSACT_REQ *Request, FSP_FILE_NODE *FileNode);
#endif
} FSP_FILE_SYSTEM_INTERFACE;
typedef struct _FSP_FILE_SYSTEM
{
    UINT16 Version;
    WCHAR VolumePath[FSP_FSCTL_VOLUME_NAME_SIZEMAX / sizeof(WCHAR)];
    HANDLE VolumeHandle;
    PVOID UserContext;
    NTSTATUS DispatcherResult;
    FSP_FILE_SYSTEM_DISPATCHER *Dispatcher;
    FSP_FILE_SYSTEM_DISPATCHER *EnterOperation, *LeaveOperation;
    FSP_FILE_SYSTEM_OPERATION *Operations[FspFsctlTransactKindCount];
    const FSP_FILE_SYSTEM_INTERFACE *Interface;
} FSP_FILE_SYSTEM;

FSP_API NTSTATUS FspFileSystemCreate(PWSTR DevicePath,
    const FSP_FSCTL_VOLUME_PARAMS *VolumeParams,
    const FSP_FILE_SYSTEM_INTERFACE *Interface,
    FSP_FILE_SYSTEM **PFileSystem);
FSP_API VOID FspFileSystemDelete(FSP_FILE_SYSTEM *FileSystem);
FSP_API NTSTATUS FspFileSystemLoop(FSP_FILE_SYSTEM *FileSystem);

FSP_API VOID FspFileSystemDirectDispatcher(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request);
FSP_API VOID FspFileSystemPoolDispatcher(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request);
static inline
VOID FspFileSystemSetDispatcher(FSP_FILE_SYSTEM *FileSystem,
    FSP_FILE_SYSTEM_DISPATCHER *Dispatcher,
    FSP_FILE_SYSTEM_DISPATCHER *EnterOperation,
    FSP_FILE_SYSTEM_DISPATCHER *LeaveOperation)
{
    FileSystem->Dispatcher = Dispatcher;
    FileSystem->EnterOperation = EnterOperation;
    FileSystem->LeaveOperation = LeaveOperation;
}
static inline
VOID FspFileSystemSetOperation(FSP_FILE_SYSTEM *FileSystem,
    ULONG Index,
    FSP_FILE_SYSTEM_OPERATION *Operation)
{
    FileSystem->Operations[Index] = Operation;
}

static inline
VOID FspFileSystemGetDispatcherResult(FSP_FILE_SYSTEM *FileSystem,
    NTSTATUS *PDispatcherResult)
{
    /* 32-bit reads are atomic */
    *PDispatcherResult = FileSystem->DispatcherResult;
    MemoryBarrier();
}
static inline
VOID FspFileSystemSetDispatcherResult(FSP_FILE_SYSTEM *FileSystem,
    NTSTATUS DispatcherResult)
{
    if (NT_SUCCESS(DispatcherResult))
        return;
    InterlockedCompareExchange(&FileSystem->DispatcherResult, DispatcherResult, 0);
}

static inline
VOID FspFileSystemEnterOperation(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request)
{
    if (0 != FileSystem->EnterOperation)
        FileSystem->EnterOperation(FileSystem, Request);
}
static inline
VOID FspFileSystemLeaveOperation(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request)
{
    if (0 != FileSystem->LeaveOperation)
        FileSystem->LeaveOperation(FileSystem, Request);
}

FSP_API NTSTATUS FspFileSystemSendResponse(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_RSP *Response);
FSP_API NTSTATUS FspFileSystemSendResponseWithStatus(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, NTSTATUS Result);

/*
 * File System Operations
 */
FSP_API PGENERIC_MAPPING FspGetFileGenericMapping(VOID);
FSP_API NTSTATUS FspAccessCheck(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, BOOLEAN CheckParentDirectory, BOOLEAN AllowTraverseCheck,
    DWORD DesiredAccess, PDWORD PGrantedAccess);
FSP_API NTSTATUS FspFileSystemOpCreate(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request);
FSP_API NTSTATUS FspFileSystemOpOverwrite(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request);
FSP_API NTSTATUS FspFileSystemOpCleanup(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request);
FSP_API NTSTATUS FspFileSystemOpClose(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request);

/*
 * Path Handling
 */
FSP_API VOID FspPathPrefix(PWSTR Path, PWSTR *PPrefix, PWSTR *PRemain);
FSP_API VOID FspPathSuffix(PWSTR Path, PWSTR *PRemain, PWSTR *PSuffix);
FSP_API VOID FspPathCombine(PWSTR Prefix, PWSTR Suffix);

/*
 * Utility
 */
FSP_API NTSTATUS FspNtStatusFromWin32(DWORD Error);
FSP_API VOID FspDebugLog(const char *format, ...);

#if 0
/*
 * File Nodes
 */
typedef struct _FSP_FILE_NODE
{
    /* public */
    PVOID UserContext;
    UINT64 AllocationSize;
    UINT64 FileSize;
    /* private */
    CRITICAL_SECTION CriticalSection;
    LONG OpenCount;
    struct
    {
        BOOLEAN DeleteOnClose:1;
        BOOLEAN DeletePending:1;
    } Flags;
    struct
    {
        ULONG OpenCount;
        ULONG Readers;
        ULONG Writers;
        ULONG Deleters;
        ULONG SharedRead;
        ULONG SharedWrite;
        ULONG SharedDelete;
    } ShareAccess;
} FSP_FILE_NODE;
static inline
VOID FspFileNodeInit(FSP_FILE_NODE *FileNode)
{
    memset(FileNode, 0, sizeof *FileNode);
    InitializeCriticalSection(&FileNode->CriticalSection);
}
static inline
VOID FspFileNodeFini(FSP_FILE_NODE *FileNode)
{
    DeleteCriticalSection(&FileNode->CriticalSection);
}
static inline
VOID FspFileNodeLock(FSP_FILE_NODE *FileNode)
{
    EnterCriticalSection(&FileNode->CriticalSection);
}
static inline
VOID FspFileNodeUnlock(FSP_FILE_NODE *FileNode)
{
    LeaveCriticalSection(&FileNode->CriticalSection);
}
static inline
VOID FspFileNodeOpen(FSP_FILE_NODE *FileNode)
{
    InterlockedIncrement(&FileNode->OpenCount);
}
static inline
LONG FspFileNodeClose(FSP_FILE_NODE *FileNode)
{
    return InterlockedDecrement(&FileNode->OpenCount);
}

/*
 * File System
 */
typedef struct _FSP_FILE_SYSTEM FSP_FILE_SYSTEM;
typedef VOID FSP_FILE_SYSTEM_DISPATCHER(FSP_FILE_SYSTEM *, FSP_FSCTL_TRANSACT_REQ *);
typedef NTSTATUS FSP_FILE_SYSTEM_OPERATION(FSP_FILE_SYSTEM *, FSP_FSCTL_TRANSACT_REQ *);
typedef struct _FSP_FILE_SYSTEM_INTERFACE
{
    NTSTATUS (*GetSecurity)(FSP_FILE_SYSTEM *FileSystem,
        PWSTR FileName, PDWORD PFileAttributes,
        PSECURITY_DESCRIPTOR SecurityDescriptor, SIZE_T *PSecurityDescriptorSize);
    NTSTATUS (*Create)(FSP_FILE_SYSTEM *FileSystem,
        FSP_FSCTL_TRANSACT_REQ *Request, FSP_FILE_NODE **PFileNode);
    NTSTATUS (*Open)(FSP_FILE_SYSTEM *FileSystem,
        FSP_FSCTL_TRANSACT_REQ *Request, FSP_FILE_NODE **PFileNode);
    NTSTATUS (*Overwrite)(FSP_FILE_SYSTEM *FileSystem,
        FSP_FSCTL_TRANSACT_REQ *Request, BOOLEAN ReplaceFileAttributes, FSP_FILE_NODE *FileNode);
    VOID (*Cleanup)(FSP_FILE_SYSTEM *FileSystem,
        FSP_FSCTL_TRANSACT_REQ *Request, FSP_FILE_NODE *FileNode, BOOLEAN Delete);
    VOID (*Close)(FSP_FILE_SYSTEM *FileSystem,
        FSP_FSCTL_TRANSACT_REQ *Request, FSP_FILE_NODE *FileNode);
} FSP_FILE_SYSTEM_INTERFACE;
typedef struct _FSP_FILE_SYSTEM
{
    UINT16 Version;
    WCHAR VolumePath[FSP_FSCTL_VOLUME_NAME_SIZEMAX / sizeof(WCHAR)];
    HANDLE VolumeHandle;
    PVOID UserContext;
    NTSTATUS DispatcherResult;
    FSP_FILE_SYSTEM_DISPATCHER *Dispatcher;
    FSP_FILE_SYSTEM_DISPATCHER *EnterOperation, *LeaveOperation;
    FSP_FILE_SYSTEM_OPERATION *Operations[FspFsctlTransactKindCount];
    const FSP_FILE_SYSTEM_INTERFACE *Interface;
} FSP_FILE_SYSTEM;

FSP_API NTSTATUS FspFileSystemCreate(PWSTR DevicePath,
    const FSP_FSCTL_VOLUME_PARAMS *VolumeParams,
    const FSP_FILE_SYSTEM_INTERFACE *Interface,
    FSP_FILE_SYSTEM **PFileSystem);
FSP_API VOID FspFileSystemDelete(FSP_FILE_SYSTEM *FileSystem);
FSP_API NTSTATUS FspFileSystemLoop(FSP_FILE_SYSTEM *FileSystem);

FSP_API VOID FspFileSystemDirectDispatcher(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request);
FSP_API VOID FspFileSystemPoolDispatcher(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request);
static inline
VOID FspFileSystemSetDispatcher(FSP_FILE_SYSTEM *FileSystem,
    FSP_FILE_SYSTEM_DISPATCHER *Dispatcher,
    FSP_FILE_SYSTEM_DISPATCHER *EnterOperation,
    FSP_FILE_SYSTEM_DISPATCHER *LeaveOperation)
{
    FileSystem->Dispatcher = Dispatcher;
    FileSystem->EnterOperation = EnterOperation;
    FileSystem->LeaveOperation = LeaveOperation;
}
static inline
VOID FspFileSystemSetOperation(FSP_FILE_SYSTEM *FileSystem,
    ULONG Index,
    FSP_FILE_SYSTEM_OPERATION *Operation)
{
    FileSystem->Operations[Index] = Operation;
}

static inline
VOID FspFileSystemGetDispatcherResult(FSP_FILE_SYSTEM *FileSystem,
    NTSTATUS *PDispatcherResult)
{
    /* 32-bit reads are atomic */
    *PDispatcherResult = FileSystem->DispatcherResult;
    MemoryBarrier();
}
static inline
VOID FspFileSystemSetDispatcherResult(FSP_FILE_SYSTEM *FileSystem,
    NTSTATUS DispatcherResult)
{
    if (NT_SUCCESS(DispatcherResult))
        return;
    InterlockedCompareExchange(&FileSystem->DispatcherResult, DispatcherResult, 0);
}

static inline
VOID FspFileSystemEnterOperation(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request)
{
    if (0 != FileSystem->EnterOperation)
        FileSystem->EnterOperation(FileSystem, Request);
}
static inline
VOID FspFileSystemLeaveOperation(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request)
{
    if (0 != FileSystem->LeaveOperation)
        FileSystem->LeaveOperation(FileSystem, Request);
}

FSP_API NTSTATUS FspFileSystemSendResponse(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_RSP *Response);
FSP_API NTSTATUS FspFileSystemSendResponseWithStatus(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, NTSTATUS Result);

/*
 * File System Operations
 */
FSP_API PGENERIC_MAPPING FspGetFileGenericMapping(VOID);
FSP_API NTSTATUS FspAccessCheck(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, BOOLEAN CheckParentDirectory, BOOLEAN AllowTraverseCheck,
    DWORD DesiredAccess, PDWORD PGrantedAccess);
FSP_API NTSTATUS FspShareAccessCheck(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, DWORD GrantedAccess, FSP_FILE_NODE *FileNode);
FSP_API VOID FspShareAccessRemove(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FILE_NODE *FileNode);
FSP_API NTSTATUS FspFileSystemPreCreateCheck(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, BOOLEAN AllowTraverseCheck, PDWORD PGrantedAccess);
FSP_API VOID FspFileSystemPostCreateCheck(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, DWORD GrantedAccess, FSP_FILE_NODE *FileNode);
FSP_API NTSTATUS FspFileSystemPreOpenCheck(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, BOOLEAN AllowTraverseCheck, PDWORD PGrantedAccess);
FSP_API NTSTATUS FspFileSystemPostOpenCheck(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, DWORD GrantedAccess, FSP_FILE_NODE *FileNode);
FSP_API NTSTATUS FspFileSystemPreOverwriteCheck(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, BOOLEAN AllowTraverseCheck, PDWORD PGrantedAccess);
FSP_API NTSTATUS FspFileSystemPostOverwriteCheck(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, DWORD GrantedAccess, FSP_FILE_NODE *FileNode);
FSP_API NTSTATUS FspFileSystemOpCreate(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request);
FSP_API NTSTATUS FspFileSystemOpCleanup(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request);
FSP_API NTSTATUS FspFileSystemOpClose(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request);
FSP_API NTSTATUS FspFileSystemSendCreateResponse(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, UINT_PTR Information,
    FSP_FILE_NODE *FileNode, DWORD GrantedAccess);
FSP_API NTSTATUS FspFileSystemSendCloseResponse(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request);
FSP_API NTSTATUS FspFileSystemSendCleanupResponse(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request);
#endif

#endif
