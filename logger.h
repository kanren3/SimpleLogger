#ifndef _LOGGER_H_
#define _LOGGER_H_

#ifdef __cplusplus
extern "C" {
#endif

#define MSG_UNICODE    0x00000001
#define MSG_ANSI       0x00000002

#define MSG_POOL_SIZE  (PAGE_SIZE * 10)

    typedef struct _MESSAGE_HEADER {
        LARGE_INTEGER Time;
        ULONG Type;
        ULONG Length;
        UCHAR Body[1];
    } MESSAGE_HEADER, * PMESSAGE_HEADER;

#define MESSAGE_HEADER_LENGTH \
    FIELD_OFFSET(MESSAGE_HEADER, Body)

#define GetMessageLength(Header) \
    (MESSAGE_HEADER_LENGTH + Header->Length)

#define GetNextMessage(Header) \
    ((PMESSAGE_HEADER)((PUCHAR)(Header) + GetMessageLength(Header)))

#define IdlePoolToMessage(Pool, Length) \
    ((PMESSAGE_HEADER)((PUCHAR)(Pool) + Length))

#define LogMemoryCopy __movsb

#define LogNonPagePoolAlloca(NumberOfBytes) \
    ExAllocatePoolWithTag(NonPagedPool, NumberOfBytes, 'LOG')

#define LogPoolFree(Pointer) \
    ExFreePoolWithTag(Pointer, 'LOG')

    typedef VOID
        (NTAPI* MSG_HANDLER)(
            __in PMESSAGE_HEADER Message);

    VOID NTAPI
        LogPostMessage(
            __in PVOID Data,
            __in ULONG Type,
            __in ULONG Length
        );

    VOID NTAPI
        LogUninitialize(
            VOID
        );

    NTSTATUS NTAPI
        LogInitialize(
            __in ULONG FlushTime,
            __in MSG_HANDLER Handler
        );

    VOID NTAPI
        LogSyncPrint(
            __in PCSTR Format,
            __in ...
        );

    VOID NTAPI
        LogAsyncPrint(
            __in PCSTR Format,
            __in ...
        );

    VOID NTAPI
        LogDbgPrintHandler(
            __in PMESSAGE_HEADER Message
        );

#ifdef __cplusplus
}
#endif

#endif
