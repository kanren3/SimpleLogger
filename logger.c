#include <definitions.h>
#include "intrinsics.h"
#include "runtime.h"
#include "logger.h"

UINT32 LogQueueLock;
UINT32 LogFlushTime;

PVOID LogPushPoolBuffer;
PVOID LogPullPoolBuffer;

UINT32 LogPushPoolLength;
UINT32 LogPullPoolLength;

UINT32 LogQueuerState;
PETHREAD LogQueuerThread;

VOID NTAPI
LogSyncPrint (
    IN PCSTR Format,
    IN ...
)
{
    va_list ArgList;

    va_start(ArgList, Format);

    vDbgPrintExWithPrefix("[mirror]\t",
                          DPFLTR_IHVDRIVER_ID,
                          DPFLTR_ERROR_LEVEL,
                          Format,
                          ArgList);

    va_end(ArgList);
}

VOID NTAPI
LogSendMessage (
    IN PVOID Buffer,
    IN UINT32 Length,
    IN UINT32 Format
)
{
    PMESSAGE_HEADER Message = NULL;
    KIRQL Irql;

    RtAcquireSpinLock(&LogQueueLock, &Irql);

    if (NULL != LogPushPoolBuffer) {
        if (LogPushPoolLength < MSG_POOL_MAX_LENGTH) {
            if ((Length + MESSAGE_HEADER_LENGTH) <= (MSG_POOL_MAX_LENGTH - LogPushPoolLength)) {
                Message = PoolToMessage(LogPushPoolBuffer, LogPushPoolLength);
                Message->Format = Format;
                Message->Length = Length;

                RtlCopyMemory(Message->Body, Buffer, Length);
                LogPushPoolLength += GetMessageFullLength(Message);
            }
        }
    }

    RtReleaseSpinLock(&LogQueueLock, Irql);
}

VOID NTAPI
LogPrint (
    IN PCSTR Format,
    IN ...
)
{
    va_list ArgList;
    CHAR Buffer[512];
    UINT Length;

    va_start(ArgList, Format);

    Length = _vsnprintf(Buffer, sizeof(Buffer), Format, ArgList);

    LogSendMessage(Buffer, Length + 1, MSG_FORMAT_ANSI);

    va_end(ArgList);
}

VOID NTAPI
LogMsgPrintCallback (
    IN PMESSAGE_HEADER Message
)
{
    switch (Message->Format) {
    case MSG_FORMAT_ANSI:
        LogSyncPrint("%s", Message->Body);
        break;
    case MSG_FORMAT_UNICODE:
        LogSyncPrint("%ws", Message->Body);
        break;
    }
}

VOID NTAPI
LogMsgHandler (
    IN PVOID PoolBuffer,
    IN UINT32 PoolLength
)
{
    PMESSAGE_HEADER Message = NULL;

    Message = LogPullPoolBuffer;

    while (0 != LogPullPoolLength) {
        LogMsgPrintCallback(Message);
        LogPullPoolLength -= GetMessageFullLength(Message);
        Message = GetNextMessage(Message);
    }
}

VOID NTAPI
LogMsgQueuer (
    IN PVOID StartContext
)
{
    LARGE_INTEGER Interval;
    KIRQL Irql;

    while (LogQueuerState != LOGGER_STATE_STOPED) {
        InterlockedCompareExchange(&LogQueuerState,
                                   LOGGER_STATE_STOPED,
                                   LOGGER_STATE_STOPPING);

        RtAcquireSpinLock(&LogQueueLock, &Irql);

        LogPullPoolLength = LogPushPoolLength;
        LogPushPoolLength = 0;

        RtlCopyMemory(LogPullPoolBuffer,
                      LogPushPoolBuffer,
                      LogPullPoolLength);

        RtReleaseSpinLock(&LogQueueLock, Irql);

        LogMsgHandler(LogPullPoolBuffer, LogPullPoolLength);

        Interval.QuadPart = Int32x32To64(LogFlushTime, -10 * 1000);
        KeDelayExecutionThread(KernelMode, TRUE, &Interval);
    }

    PsTerminateSystemThread(STATUS_SUCCESS);
}

VOID NTAPI
LogUninitialize (
    VOID
)
{
    NTSTATUS Status;
    KIRQL Irql;
    PVOID PushPoolBuffer;
    PVOID PullPoolBuffer;

    InterlockedExchange(&LogQueuerState, LOGGER_STATE_STOPPING);
    KeAlertThread(LogQueuerThread, KernelMode);

    Status = KeWaitForSingleObject(LogQueuerThread,
                                   Executive,
                                   KernelMode,
                                   FALSE,
                                   NULL);

    ObDereferenceObject(LogQueuerThread);
    LogQueuerThread = NULL;

    RtAcquireSpinLock(&LogQueueLock, &Irql);

    PushPoolBuffer = LogPushPoolBuffer;
    PullPoolBuffer = LogPullPoolBuffer;

    LogPushPoolBuffer = NULL;
    LogPullPoolBuffer = NULL;

    RtReleaseSpinLock(&LogQueueLock, Irql);

    RtFreeMemory(PushPoolBuffer);
    RtFreeMemory(PullPoolBuffer);
}

NTSTATUS NTAPI
LogInitialize (
    IN UINT32 FlushTime
)
{
    NTSTATUS Status;
    OBJECT_ATTRIBUTES ObjectAttributes;
    HANDLE ThreadHandle;

    LogQueuerState = LOGGER_STATE_RUNING;
    LogPushPoolLength = 0;
    LogPullPoolLength = 0;

    LogFlushTime = FlushTime;

    RtInitializeSpinLock(&LogQueueLock);

    LogPushPoolBuffer = RtAllocateMemory(MSG_POOL_MAX_LENGTH);

    if (NULL == LogPushPoolBuffer) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    LogPullPoolBuffer = RtAllocateMemory(MSG_POOL_MAX_LENGTH);

    if (NULL == LogPullPoolBuffer) {
        RtFreeMemory(LogPushPoolBuffer);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    InitializeObjectAttributes(&ObjectAttributes, NULL, 0, NULL, NULL);

    Status = PsCreateSystemThread(&ThreadHandle,
                                  THREAD_ALL_ACCESS,
                                  &ObjectAttributes,
                                  NULL,
                                  NULL,
                                  LogMsgQueuer,
                                  NULL);

    if (FALSE == NT_SUCCESS(Status)) {
        RtFreeMemory(LogPushPoolBuffer);
        RtFreeMemory(LogPullPoolBuffer);

        LogPushPoolBuffer = NULL;
        LogPullPoolBuffer = NULL;

        return Status;
    }

    ObReferenceObjectByHandle(ThreadHandle,
                              SYNCHRONIZE,
                              NULL,
                              KernelMode,
                              &LogQueuerThread,
                              NULL);

    ZwClose(ThreadHandle);

    return STATUS_SUCCESS;
}
