#ifndef _LOGGER_H_
#define _LOGGER_H_

EXTERN_C_START

#define LOGGER_STATE_RUNING     0
#define LOGGER_STATE_STOPPING   2
#define LOGGER_STATE_STOPED     3

#define MSG_FORMAT_UNICODE  0
#define MSG_FORMAT_ANSI     1

#define MSG_POOL_MAX_LENGTH (32 * PAGE_SIZE)

typedef struct _MESSAGE_HEADER {
    UINT32 Format;
    UINT32 Length;
    UCHAR Body[1];
} MESSAGE_HEADER, *PMESSAGE_HEADER;

#define MESSAGE_HEADER_LENGTH FIELD_OFFSET(MESSAGE_HEADER, Body)

#define GetMessageFullLength(Header) (MESSAGE_HEADER_LENGTH + Header->Length)

#define GetNextMessage(Header) ((PMESSAGE_HEADER)((PUINT8)(Header) + GetMessageFullLength(Header)))

#define PoolToMessage(Buffer, Offset) ((PMESSAGE_HEADER)((PUCHAR)(Buffer) + Offset))

VOID NTAPI
LogSyncPrint (
    IN PCSTR Format,
    IN ...
);

VOID NTAPI
LogPrint (
    IN PCSTR Format,
    IN ...
);

VOID NTAPI
LogUninitialize (
    VOID
);

NTSTATUS NTAPI
LogInitialize (
    IN UINT32 FlushTime
);

EXTERN_C_END

#endif
