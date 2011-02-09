/*
 * Process Hacker - 
 *   process tree list
 * 
 * Copyright (C) 2010-2011 wj32
 * 
 * This file is part of Process Hacker.
 * 
 * Process Hacker is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Process Hacker is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Process Hacker.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <phapp.h>
#include <settings.h>
#include <phplug.h>

VOID PhpEnableColumnCustomDraw(
    __in HWND hwnd,
    __in ULONG Id
    );

VOID PhpRemoveProcessNode(
    __in PPH_PROCESS_NODE ProcessNode
    );

VOID PhpUpdateNeedCyclesInformation();

VOID PhpUpdateProcessNodeCycles(
    __inout PPH_PROCESS_NODE ProcessNode
    );

BOOLEAN NTAPI PhpProcessTreeListCallback(
    __in HWND hwnd,
    __in PH_TREELIST_MESSAGE Message,
    __in_opt PVOID Parameter1,
    __in_opt PVOID Parameter2,
    __in_opt PVOID Context
    );

BOOLEAN PhpApplyProcessTreeFiltersToNode(
    __in PPH_PROCESS_NODE Node
    );

static HWND ProcessTreeListHandle;
static ULONG ProcessTreeListSortColumn;
static PH_SORT_ORDER ProcessTreeListSortOrder;

static PPH_HASH_ENTRY ProcessNodeHashSet[256] = PH_HASH_SET_INIT; // hashtable of all nodes
static PPH_LIST ProcessNodeList; // list of all nodes, used when sorting is enabled
static PPH_LIST ProcessNodeRootList; // list of root nodes

BOOLEAN PhProcessTreeListStateHighlighting = TRUE;
static PPH_POINTER_LIST ProcessNodeStateList = NULL; // list of nodes which need to be processed

static PPH_LIST ProcessTreeFilterList = NULL;
static BOOLEAN NeedCyclesInformation = FALSE;

VOID PhProcessTreeListInitialization()
{
    ProcessNodeList = PhCreateList(40);
    ProcessNodeRootList = PhCreateList(10);
}

VOID PhInitializeProcessTreeList(
    __in HWND hwnd
    )
{
    ProcessTreeListHandle = hwnd;
    SendMessage(ProcessTreeListHandle, WM_SETFONT, (WPARAM)PhIconTitleFont, FALSE);
    TreeList_EnableExplorerStyle(hwnd);

    TreeList_SetCallback(hwnd, PhpProcessTreeListCallback);

    TreeList_SetMaxId(hwnd, PHPRTLC_MAXIMUM - 1);

    // Default columns
    PhAddTreeListColumn(hwnd, PHPRTLC_NAME, TRUE, L"Name", 200, PH_ALIGN_LEFT, 0, 0);
    PhAddTreeListColumn(hwnd, PHPRTLC_PID, TRUE, L"PID", 50, PH_ALIGN_RIGHT, 1, DT_RIGHT);
    PhAddTreeListColumn(hwnd, PHPRTLC_CPU, TRUE, L"CPU", 45, PH_ALIGN_RIGHT, 2, DT_RIGHT);
    PhAddTreeListColumn(hwnd, PHPRTLC_IOTOTAL, TRUE, L"I/O Total", 70, PH_ALIGN_RIGHT, 3, DT_RIGHT);
    PhAddTreeListColumn(hwnd, PHPRTLC_PRIVATEBYTES, TRUE, L"Private Bytes", 70, PH_ALIGN_RIGHT, 4, DT_RIGHT);
    PhAddTreeListColumn(hwnd, PHPRTLC_USERNAME, TRUE, L"User Name", 140, PH_ALIGN_LEFT, 5, 0);
    PhAddTreeListColumn(hwnd, PHPRTLC_DESCRIPTION, TRUE, L"Description", 180, PH_ALIGN_LEFT, 6, 0);

    PhAddTreeListColumn(hwnd, PHPRTLC_COMPANYNAME, FALSE, L"Company Name", 180, PH_ALIGN_LEFT, -1, 0);
    PhAddTreeListColumn(hwnd, PHPRTLC_VERSION, FALSE, L"Version", 100, PH_ALIGN_LEFT, -1, 0);
    PhAddTreeListColumn(hwnd, PHPRTLC_FILENAME, FALSE, L"File Name", 180, PH_ALIGN_LEFT, -1, DT_PATH_ELLIPSIS);
    PhAddTreeListColumn(hwnd, PHPRTLC_COMMANDLINE, FALSE, L"Command Line", 180, PH_ALIGN_LEFT, -1, 0);

    PhAddTreeListColumn(hwnd, PHPRTLC_PEAKPRIVATEBYTES, FALSE, L"Peak Private Bytes", 70, PH_ALIGN_RIGHT, -1, DT_RIGHT);
    PhAddTreeListColumn(hwnd, PHPRTLC_WORKINGSET, FALSE, L"Working Set", 70, PH_ALIGN_RIGHT, -1, DT_RIGHT);
    PhAddTreeListColumn(hwnd, PHPRTLC_PEAKWORKINGSET, FALSE, L"Peak Working Set", 70, PH_ALIGN_RIGHT, -1, DT_RIGHT);
    PhAddTreeListColumn(hwnd, PHPRTLC_PRIVATEWS, FALSE, L"Private WS", 70, PH_ALIGN_RIGHT, -1, DT_RIGHT);
    PhAddTreeListColumn(hwnd, PHPRTLC_SHAREDWS, FALSE, L"Shared WS", 70, PH_ALIGN_RIGHT, -1, DT_RIGHT);
    PhAddTreeListColumn(hwnd, PHPRTLC_SHAREABLEWS, FALSE, L"Shareable WS", 70, PH_ALIGN_RIGHT, -1, DT_RIGHT);
    PhAddTreeListColumn(hwnd, PHPRTLC_VIRTUALSIZE, FALSE, L"Virtual Size", 70, PH_ALIGN_RIGHT, -1, DT_RIGHT);
    PhAddTreeListColumn(hwnd, PHPRTLC_PEAKVIRTUALSIZE, FALSE, L"Peak Virtual Size", 70, PH_ALIGN_RIGHT, -1, DT_RIGHT);
    PhAddTreeListColumn(hwnd, PHPRTLC_PAGEFAULTS, FALSE, L"Page Faults", 70, PH_ALIGN_RIGHT, -1, DT_RIGHT);
    PhAddTreeListColumn(hwnd, PHPRTLC_SESSIONID, FALSE, L"Session ID", 45, PH_ALIGN_RIGHT, -1, DT_RIGHT);
    PhAddTreeListColumn(hwnd, PHPRTLC_PRIORITYCLASS, FALSE, L"Priority Class", 100, PH_ALIGN_LEFT, -1, 0);
    PhAddTreeListColumn(hwnd, PHPRTLC_BASEPRIORITY, FALSE, L"Base Priority", 45, PH_ALIGN_RIGHT, -1, DT_RIGHT);

    PhAddTreeListColumn(hwnd, PHPRTLC_THREADS, FALSE, L"Threads", 45, PH_ALIGN_RIGHT, -1, DT_RIGHT);
    PhAddTreeListColumn(hwnd, PHPRTLC_HANDLES, FALSE, L"Handles", 45, PH_ALIGN_RIGHT, -1, DT_RIGHT);
    PhAddTreeListColumn(hwnd, PHPRTLC_GDIHANDLES, FALSE, L"GDI Handles", 45, PH_ALIGN_RIGHT, -1, DT_RIGHT);
    PhAddTreeListColumn(hwnd, PHPRTLC_USERHANDLES, FALSE, L"USER Handles", 45, PH_ALIGN_RIGHT, -1, DT_RIGHT);
    PhAddTreeListColumn(hwnd, PHPRTLC_IORO, FALSE, L"I/O R+O", 70, PH_ALIGN_RIGHT, -1, DT_RIGHT);
    PhAddTreeListColumn(hwnd, PHPRTLC_IOW, FALSE, L"I/O W", 70, PH_ALIGN_RIGHT, -1, DT_RIGHT);
    PhAddTreeListColumn(hwnd, PHPRTLC_INTEGRITY, FALSE, L"Integrity", 100, PH_ALIGN_LEFT, -1, 0);
    PhAddTreeListColumn(hwnd, PHPRTLC_IOPRIORITY, FALSE, L"I/O Priority", 70, PH_ALIGN_LEFT, -1, 0);
    PhAddTreeListColumn(hwnd, PHPRTLC_PAGEPRIORITY, FALSE, L"Page Priority", 45, PH_ALIGN_RIGHT, -1, DT_RIGHT);
    PhAddTreeListColumn(hwnd, PHPRTLC_STARTTIME, FALSE, L"Start Time", 100, PH_ALIGN_LEFT, -1, 0);
    PhAddTreeListColumn(hwnd, PHPRTLC_TOTALCPUTIME, FALSE, L"Total CPU Time", 90, PH_ALIGN_LEFT, -1, 0);
    PhAddTreeListColumn(hwnd, PHPRTLC_KERNELCPUTIME, FALSE, L"Kernel CPU Time", 90, PH_ALIGN_RIGHT, -1, DT_RIGHT);
    PhAddTreeListColumn(hwnd, PHPRTLC_USERCPUTIME, FALSE, L"User CPU Time", 90, PH_ALIGN_RIGHT, -1, DT_RIGHT);
    PhAddTreeListColumn(hwnd, PHPRTLC_VERIFICATIONSTATUS, FALSE, L"Verification Status", 70, PH_ALIGN_LEFT, -1, 0);
    PhAddTreeListColumn(hwnd, PHPRTLC_VERIFIEDSIGNER, FALSE, L"Verified Signer", 100, PH_ALIGN_LEFT, -1, 0);
    PhAddTreeListColumn(hwnd, PHPRTLC_RELATIVESTARTTIME, FALSE, L"Relative Start Time", 180, PH_ALIGN_LEFT, -1, 0);
    PhAddTreeListColumn(hwnd, PHPRTLC_BITS, FALSE, L"Bits", 50, PH_ALIGN_LEFT, -1, 0);
    PhAddTreeListColumn(hwnd, PHPRTLC_ELEVATION, FALSE, L"Elevation", 60, PH_ALIGN_LEFT, -1, 0);
    PhAddTreeListColumn(hwnd, PHPRTLC_WINDOWTITLE, FALSE, L"Window Title", 120, PH_ALIGN_LEFT, -1, 0);
    PhAddTreeListColumn(hwnd, PHPRTLC_WINDOWSTATUS, FALSE, L"Window Status", 60, PH_ALIGN_LEFT, -1, 0);
    PhAddTreeListColumn(hwnd, PHPRTLC_CYCLES, FALSE, L"Cycles", 110, PH_ALIGN_RIGHT, -1, DT_RIGHT);
    PhAddTreeListColumn(hwnd, PHPRTLC_CYCLESDELTA, FALSE, L"Cycles Delta", 90, PH_ALIGN_RIGHT, -1, DT_RIGHT);
    PhAddTreeListColumn(hwnd, PHPRTLC_CPUHISTORY, FALSE, L"CPU History", 100, PH_ALIGN_LEFT, -1, 0);
    PhAddTreeListColumn(hwnd, PHPRTLC_PRIVATEBYTESHISTORY, FALSE, L"Private Bytes History", 100, PH_ALIGN_LEFT, -1, 0);
    PhAddTreeListColumn(hwnd, PHPRTLC_IOHISTORY, FALSE, L"I/O History", 100, PH_ALIGN_LEFT, -1, 0);

    PhpEnableColumnCustomDraw(hwnd, PHPRTLC_CPUHISTORY);
    PhpEnableColumnCustomDraw(hwnd, PHPRTLC_PRIVATEBYTESHISTORY);
    PhpEnableColumnCustomDraw(hwnd, PHPRTLC_IOHISTORY);

    TreeList_SetTriState(hwnd, TRUE);
    TreeList_SetSort(hwnd, 0, NoSortOrder);
}

static VOID PhpEnableColumnCustomDraw(
    __in HWND hwnd,
    __in ULONG Id
    )
{
    PH_TREELIST_COLUMN column;

    column.Id = Id;
    column.Flags = 0;
    column.Visible = FALSE;
    column.CustomDraw = TRUE;
    TreeList_SetColumn(hwnd, &column, TLCM_FLAGS);
}

VOID PhLoadSettingsProcessTreeList()
{
    ULONG sortColumn;
    PH_SORT_ORDER sortOrder;

    PhLoadTreeListColumnsFromSetting(L"ProcessTreeListColumns", ProcessTreeListHandle);

    sortOrder = PhGetIntegerSetting(L"ProcessTreeListSortOrder");

    if (sortOrder != NoSortOrder)
    {
        sortColumn = PhGetIntegerSetting(L"ProcessTreeListSortColumn");
        TreeList_SetSort(ProcessTreeListHandle, sortColumn, sortOrder);
    }

    PhpUpdateNeedCyclesInformation();
}

VOID PhSaveSettingsProcessTreeList()
{
    ULONG sortColumn;
    PH_SORT_ORDER sortOrder;

    PhSaveTreeListColumnsToSetting(L"ProcessTreeListColumns", ProcessTreeListHandle);

    TreeList_GetSort(ProcessTreeListHandle, &sortColumn, &sortOrder);
    PhSetIntegerSetting(L"ProcessTreeListSortColumn", sortColumn);
    PhSetIntegerSetting(L"ProcessTreeListSortOrder", sortOrder);
}

FORCEINLINE BOOLEAN PhCompareProcessNode(
    __in PPH_PROCESS_NODE Value1,
    __in PPH_PROCESS_NODE Value2
    )
{
    return Value1->ProcessId == Value2->ProcessId;
}

FORCEINLINE ULONG PhHashProcessNode(
    __in PPH_PROCESS_NODE Value
    )
{
    return (ULONG)Value->ProcessId / 4;
}

PPH_PROCESS_NODE PhAddProcessNode(
    __in PPH_PROCESS_ITEM ProcessItem,
    __in ULONG RunId
    )
{
    PPH_PROCESS_NODE processNode;
    PPH_PROCESS_NODE parentNode;
    ULONG i;

    processNode = PhAllocate(sizeof(PH_PROCESS_NODE));
    memset(processNode, 0, sizeof(PH_PROCESS_NODE));
    PhInitializeTreeListNode(&processNode->Node);

    if (PhProcessTreeListStateHighlighting && RunId != 1)
    {
        PhChangeShState(
            &processNode->Node,
            &processNode->ShState,
            &ProcessNodeStateList,
            NewItemState,
            PhCsColorNew,
            NULL
            );
    }

    processNode->ProcessId = ProcessItem->ProcessId;
    processNode->ProcessItem = ProcessItem;
    PhReferenceObject(ProcessItem);

    memset(processNode->TextCache, 0, sizeof(PH_STRINGREF) * PHPRTLC_MAXIMUM);
    processNode->Node.TextCache = processNode->TextCache;
    processNode->Node.TextCacheSize = PHPRTLC_MAXIMUM;

    processNode->Children = PhCreateList(1);

    // Find this process' parent and add the process to it if we found it.
    if (
        (parentNode = PhFindProcessNode(ProcessItem->ParentProcessId)) &&
        parentNode->ProcessItem->CreateTime.QuadPart <= ProcessItem->CreateTime.QuadPart
        )
    {
        PhAddItemList(parentNode->Children, processNode);
        processNode->Parent = parentNode;
    }
    else
    {
        // No parent, add to root list.
        processNode->Parent = NULL;
        PhAddItemList(ProcessNodeRootList, processNode);
    }

    // Find this process' children and move them to this node.

    for (i = 0; i < ProcessNodeRootList->Count; i++)
    {
        PPH_PROCESS_NODE node = ProcessNodeRootList->Items[i];

        if (
            node != processNode && // for cases where the parent PID = PID (e.g. System Idle Process) 
            node->ProcessItem->ParentProcessId == ProcessItem->ProcessId &&
            ProcessItem->CreateTime.QuadPart <= node->ProcessItem->CreateTime.QuadPart
            )
        {
            node->Parent = processNode;
            PhAddItemList(processNode->Children, node);
        }
    }

    for (i = 0; i < processNode->Children->Count; i++)
    {
        PhRemoveItemList(
            ProcessNodeRootList,
            PhFindItemList(ProcessNodeRootList, processNode->Children->Items[i])
            );
    }

    PhAddEntryHashSet(
        ProcessNodeHashSet,
        PH_HASH_SET_SIZE(ProcessNodeHashSet),
        &processNode->HashEntry,
        PhHashProcessNode(processNode)
        );
    PhAddItemList(ProcessNodeList, processNode);

    if (PhCsCollapseServicesOnStart)
    {
        static PH_STRINGREF servicesBaseName = PH_STRINGREF_INIT(L"\\services.exe");
        static BOOLEAN servicesFound = FALSE;
        static PPH_STRING servicesFileName = NULL;

        if (!servicesFound)
        {
            if (!servicesFileName)
            {
                PPH_STRING systemDirectory;

                systemDirectory = PhGetSystemDirectory();
                servicesFileName = PhConcatStringRef2(&systemDirectory->sr, &servicesBaseName);
                PhDereferenceObject(systemDirectory);
            }

            // If this process is services.exe, collapse the node and free the string.
            if (
                ProcessItem->FileName &&
                PhEqualString(ProcessItem->FileName, servicesFileName, TRUE)
                )
            {
                processNode->Node.Expanded = FALSE;
                PhDereferenceObject(servicesFileName);
                servicesFileName = NULL;
                servicesFound = TRUE;
            }
        }
    }

    if (ProcessTreeFilterList)
        processNode->Node.Visible = PhpApplyProcessTreeFiltersToNode(processNode);

    TreeList_NodesStructured(ProcessTreeListHandle);

    return processNode;
}

PPH_PROCESS_NODE PhFindProcessNode(
    __in HANDLE ProcessId
    )
{
    PH_PROCESS_NODE lookupNode;
    PPH_HASH_ENTRY entry;
    PPH_PROCESS_NODE node;

    lookupNode.ProcessId = ProcessId;
    entry = PhFindEntryHashSet(
        ProcessNodeHashSet,
        PH_HASH_SET_SIZE(ProcessNodeHashSet),
        PhHashProcessNode(&lookupNode)
        );

    for (; entry; entry = entry->Next)
    {
        node = CONTAINING_RECORD(entry, PH_PROCESS_NODE, HashEntry);

        if (PhCompareProcessNode(&lookupNode, node))
            return node;
    }

    return NULL;
}

VOID PhRemoveProcessNode(
    __in PPH_PROCESS_NODE ProcessNode
    )
{
    if (PhProcessTreeListStateHighlighting)
    {
        PhChangeShState(
            &ProcessNode->Node,
            &ProcessNode->ShState,
            &ProcessNodeStateList,
            RemovingItemState,
            PhCsColorRemoved,
            ProcessTreeListHandle
            );
    }
    else
    {
        PhpRemoveProcessNode(ProcessNode);
    }
}

VOID PhpRemoveProcessNode(
    __in PPH_PROCESS_NODE ProcessNode
    )
{
    ULONG index;
    ULONG i;

    if (ProcessNode->Parent)
    {
        // Remove the node from its parent.

        if ((index = PhFindItemList(ProcessNode->Parent->Children, ProcessNode)) != -1)
            PhRemoveItemList(ProcessNode->Parent->Children, index);
    }
    else
    {
        // Remove the node from the root list.

        if ((index = PhFindItemList(ProcessNodeRootList, ProcessNode)) != -1)
            PhRemoveItemList(ProcessNodeRootList, index);
    }

    // Move the node's children to the root list.
    for (i = 0; i < ProcessNode->Children->Count; i++)
    {
        PPH_PROCESS_NODE node = ProcessNode->Children->Items[i];

        node->Parent = NULL;
        PhAddItemList(ProcessNodeRootList, node);
    }

    // Remove from hashtable/list and cleanup.

    PhRemoveEntryHashSet(ProcessNodeHashSet, PH_HASH_SET_SIZE(ProcessNodeHashSet), &ProcessNode->HashEntry);

    if ((index = PhFindItemList(ProcessNodeList, ProcessNode)) != -1)
        PhRemoveItemList(ProcessNodeList, index);

    PhDereferenceObject(ProcessNode->Children);

    if (ProcessNode->WindowText) PhDereferenceObject(ProcessNode->WindowText);

    if (ProcessNode->TooltipText) PhDereferenceObject(ProcessNode->TooltipText);

    if (ProcessNode->IoTotalText) PhDereferenceObject(ProcessNode->IoTotalText);
    if (ProcessNode->PrivateBytesText) PhDereferenceObject(ProcessNode->PrivateBytesText);
    if (ProcessNode->PeakPrivateBytesText) PhDereferenceObject(ProcessNode->PeakPrivateBytesText);
    if (ProcessNode->WorkingSetText) PhDereferenceObject(ProcessNode->WorkingSetText);
    if (ProcessNode->PeakWorkingSetText) PhDereferenceObject(ProcessNode->PeakWorkingSetText);
    if (ProcessNode->PrivateWsText) PhDereferenceObject(ProcessNode->PrivateWsText);
    if (ProcessNode->SharedWsText) PhDereferenceObject(ProcessNode->SharedWsText);
    if (ProcessNode->ShareableWsText) PhDereferenceObject(ProcessNode->ShareableWsText);
    if (ProcessNode->VirtualSizeText) PhDereferenceObject(ProcessNode->VirtualSizeText);
    if (ProcessNode->PeakVirtualSizeText) PhDereferenceObject(ProcessNode->PeakVirtualSizeText);
    if (ProcessNode->PageFaultsText) PhDereferenceObject(ProcessNode->PageFaultsText);
    if (ProcessNode->IoRoText) PhDereferenceObject(ProcessNode->IoRoText);
    if (ProcessNode->IoWText) PhDereferenceObject(ProcessNode->IoWText);
    if (ProcessNode->StartTimeText) PhDereferenceObject(ProcessNode->StartTimeText);
    if (ProcessNode->RelativeStartTimeText) PhDereferenceObject(ProcessNode->RelativeStartTimeText);
    if (ProcessNode->WindowTitleText) PhDereferenceObject(ProcessNode->WindowTitleText);
    if (ProcessNode->CyclesText) PhDereferenceObject(ProcessNode->CyclesText);
    if (ProcessNode->CyclesDeltaText) PhDereferenceObject(ProcessNode->CyclesDeltaText);

    PhDeleteGraphBuffers(&ProcessNode->CpuGraphBuffers);
    PhDeleteGraphBuffers(&ProcessNode->PrivateGraphBuffers);
    PhDeleteGraphBuffers(&ProcessNode->IoGraphBuffers);

    PhDereferenceObject(ProcessNode->ProcessItem);

    PhFree(ProcessNode);

    TreeList_NodesStructured(ProcessTreeListHandle);
}

VOID PhUpdateProcessNode(
    __in PPH_PROCESS_NODE ProcessNode
    )
{
    memset(ProcessNode->TextCache, 0, sizeof(PH_STRINGREF) * PHPRTLC_MAXIMUM);

    if (ProcessNode->TooltipText)
    {
        PhDereferenceObject(ProcessNode->TooltipText);
        ProcessNode->TooltipText = NULL;
    }

    PhInvalidateTreeListNode(&ProcessNode->Node, TLIN_COLOR | TLIN_ICON);
}

VOID PhTickProcessNodes()
{
    // Text invalidation, node updates
    {
        ULONG i;

        for (i = 0; i < ProcessNodeList->Count; i++)
        {
            PPH_PROCESS_NODE node = ProcessNodeList->Items[i];

            // The name and PID never change, so we don't invalidate that.
            memset(&node->TextCache[2], 0, sizeof(PH_STRINGREF) * (PHPRTLC_MAXIMUM - 2));
            node->ValidMask = 0;

            // Invalidate graph buffers.
            node->CpuGraphBuffers.Valid = FALSE;
            node->PrivateGraphBuffers.Valid = FALSE;
            node->IoGraphBuffers.Valid = FALSE;

            // Updates cycles if necessary.
            if (NeedCyclesInformation)
                PhpUpdateProcessNodeCycles(node);
        }

        if (ProcessTreeListSortOrder != NoSortOrder)
        {
            // Force a rebuild to sort the items.
            TreeList_NodesStructured(ProcessTreeListHandle);
        }
    }

    // State highlighting
    PH_TICK_SH_STATE(PH_PROCESS_NODE, ShState, ProcessNodeStateList, PhpRemoveProcessNode, PhCsHighlightingDuration, ProcessTreeListHandle, FALSE);

    InvalidateRect(ProcessTreeListHandle, NULL, FALSE);
}

static FLOAT PhpCalculateInclusiveCpuUsage(
    __in PPH_PROCESS_NODE ProcessNode
    )
{
    FLOAT cpuUsage;
    ULONG i;

    cpuUsage = ProcessNode->ProcessItem->CpuUsage;

    for (i = 0; i < ProcessNode->Children->Count; i++)
    {
        cpuUsage += PhpCalculateInclusiveCpuUsage(ProcessNode->Children->Items[i]);
    }

    return cpuUsage;
}

static VOID PhpUpdateProcessNodeWsCounters(
    __inout PPH_PROCESS_NODE ProcessNode
    )
{
    if (!(ProcessNode->ValidMask & PHPN_WSCOUNTERS))
    {
        BOOLEAN success = FALSE;
        HANDLE processHandle;

        if (NT_SUCCESS(PhOpenProcess(
            &processHandle,
            PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
            ProcessNode->ProcessItem->ProcessId
            )))
        {
            if (NT_SUCCESS(PhGetProcessWsCounters(
                processHandle,
                &ProcessNode->WsCounters
                )))
                success = TRUE;

            NtClose(processHandle);
        }

        if (!success)
            memset(&ProcessNode->WsCounters, 0, sizeof(PH_PROCESS_WS_COUNTERS));

        ProcessNode->ValidMask |= PHPN_WSCOUNTERS;
    }
}

static VOID PhpUpdateProcessNodeGdiUserHandles(
    __inout PPH_PROCESS_NODE ProcessNode
    )
{
    if (!(ProcessNode->ValidMask & PHPN_GDIUSERHANDLES))
    {
        if (ProcessNode->ProcessItem->QueryHandle)
        {
            ProcessNode->GdiHandles = GetGuiResources(ProcessNode->ProcessItem->QueryHandle, GR_GDIOBJECTS);
            ProcessNode->UserHandles = GetGuiResources(ProcessNode->ProcessItem->QueryHandle, GR_USEROBJECTS);
        }
        else
        {
            ProcessNode->GdiHandles = 0;
            ProcessNode->UserHandles = 0;
        }

        ProcessNode->ValidMask |= PHPN_GDIUSERHANDLES;
    }
}

static VOID PhpUpdateProcessNodeIoPagePriority(
    __inout PPH_PROCESS_NODE ProcessNode
    )
{
    if (!(ProcessNode->ValidMask & PHPN_IOPAGEPRIORITY))
    {
        if (ProcessNode->ProcessItem->QueryHandle)
        {
            if (!NT_SUCCESS(PhGetProcessIoPriority(ProcessNode->ProcessItem->QueryHandle, &ProcessNode->IoPriority)))
                ProcessNode->IoPriority = -1;
            if (!NT_SUCCESS(PhGetProcessPagePriority(ProcessNode->ProcessItem->QueryHandle, &ProcessNode->PagePriority)))
                ProcessNode->PagePriority = -1;
        }
        else
        {
            ProcessNode->IoPriority = -1;
            ProcessNode->PagePriority = -1;
        }

        ProcessNode->ValidMask |= PHPN_IOPAGEPRIORITY;
    }
}

static BOOL CALLBACK PhpEnumProcessNodeWindowsProc(
    __in HWND hwnd,
    __in LPARAM lParam
    )
{
    PPH_PROCESS_NODE processNode = (PPH_PROCESS_NODE)lParam;
    ULONG threadId;
    ULONG processId;

    threadId = GetWindowThreadProcessId(hwnd, &processId);

    if (UlongToHandle(processId) == processNode->ProcessId)
    {
        HWND parentWindow;

        if (
            !IsWindowVisible(hwnd) || // skip invisible windows
            ((parentWindow = GetParent(hwnd)) && IsWindowVisible(parentWindow)) || // skip windows with visible parents
            GetWindowTextLength(hwnd) == 0 // skip windows with no title
            )
            return TRUE;

        processNode->WindowHandle = hwnd;
        return FALSE;
    }

    return TRUE;
}

static VOID PhpUpdateProcessNodeWindow(
    __inout PPH_PROCESS_NODE ProcessNode
    )
{
    if (!(ProcessNode->ValidMask & PHPN_WINDOW))
    {
        ProcessNode->WindowHandle = NULL;
        EnumWindows(PhpEnumProcessNodeWindowsProc, (LPARAM)ProcessNode);

        PhSwapReference(&ProcessNode->WindowText, NULL);

        if (ProcessNode->WindowHandle)
        {
            ProcessNode->WindowText = PhGetWindowText(ProcessNode->WindowHandle);
            ProcessNode->WindowHung = !!IsHungAppWindow(ProcessNode->WindowHandle);
        }

        ProcessNode->ValidMask |= PHPN_WINDOW;
    }
}

static VOID PhpUpdateNeedCyclesInformation()
{
    PH_TREELIST_COLUMN column;

    NeedCyclesInformation = FALSE;

    if (!WINDOWS_HAS_CYCLE_TIME)
        return;

    column.Id = PHPRTLC_CYCLES;
    TreeList_GetColumn(ProcessTreeListHandle, &column);

    if (column.Visible)
    {
        NeedCyclesInformation = TRUE;
        return;
    }

    column.Id = PHPRTLC_CYCLESDELTA;
    TreeList_GetColumn(ProcessTreeListHandle, &column);

    if (column.Visible)
    {
        NeedCyclesInformation = TRUE;
        return;
    }
}

static VOID PhpUpdateProcessNodeCycles(
    __inout PPH_PROCESS_NODE ProcessNode
    )
{
    if (ProcessNode->ProcessId == SYSTEM_IDLE_PROCESS_ID)
    {
        PULARGE_INTEGER idleThreadCycleTimes;
        ULONG64 cycleTime;
        ULONG i;

        // System Idle Process requires special treatment.

        idleThreadCycleTimes = PhAllocate(
            sizeof(ULARGE_INTEGER) * (ULONG)PhSystemBasicInformation.NumberOfProcessors
            );

        if (NT_SUCCESS(NtQuerySystemInformation(
            SystemProcessorIdleCycleTimeInformation,
            idleThreadCycleTimes,
            sizeof(ULARGE_INTEGER) * (ULONG)PhSystemBasicInformation.NumberOfProcessors,
            NULL
            )))
        {
            cycleTime = 0;

            for (i = 0; i < (ULONG)PhSystemBasicInformation.NumberOfProcessors; i++)
                cycleTime += idleThreadCycleTimes[i].QuadPart;

            PhUpdateDelta(&ProcessNode->CyclesDelta, cycleTime);
        }

        PhFree(idleThreadCycleTimes);
    }
    else if (ProcessNode->ProcessItem->QueryHandle)
    {
        ULONG64 cycleTime;

        if (NT_SUCCESS(PhGetProcessCycleTime(ProcessNode->ProcessItem->QueryHandle, &cycleTime)))
        {
            PhUpdateDelta(&ProcessNode->CyclesDelta, cycleTime);
        }
    }

    if (ProcessNode->CyclesDelta.Value != 0)
        PhSwapReference2(&ProcessNode->CyclesText, PhFormatUInt64(ProcessNode->CyclesDelta.Value, TRUE));
    else
        PhSwapReference2(&ProcessNode->CyclesText, NULL);

    if (ProcessNode->CyclesDelta.Delta != 0)
        PhSwapReference2(&ProcessNode->CyclesDeltaText, PhFormatUInt64(ProcessNode->CyclesDelta.Delta, TRUE));
    else
        PhSwapReference2(&ProcessNode->CyclesDeltaText, NULL);
}

#define SORT_FUNCTION(Column) PhpProcessTreeListCompare##Column

#define BEGIN_SORT_FUNCTION(Column) static int __cdecl PhpProcessTreeListCompare##Column( \
    __in const void *_elem1, \
    __in const void *_elem2 \
    ) \
{ \
    PPH_PROCESS_NODE node1 = *(PPH_PROCESS_NODE *)_elem1; \
    PPH_PROCESS_NODE node2 = *(PPH_PROCESS_NODE *)_elem2; \
    PPH_PROCESS_ITEM processItem1 = node1->ProcessItem; \
    PPH_PROCESS_ITEM processItem2 = node2->ProcessItem; \
    int sortResult = 0;

#define END_SORT_FUNCTION \
    if (sortResult == 0) \
        sortResult = intptrcmp((LONG_PTR)processItem1->ProcessId, (LONG_PTR)processItem2->ProcessId); \
    \
    return PhModifySort(sortResult, ProcessTreeListSortOrder); \
}

BEGIN_SORT_FUNCTION(Name)
{
    sortResult = PhCompareString(processItem1->ProcessName, processItem2->ProcessName, TRUE);
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(Pid)
{
    // Use signed int so DPCs and Interrupts are placed above System Idle Process.
    sortResult = intptrcmp((LONG_PTR)processItem1->ProcessId, (LONG_PTR)processItem2->ProcessId);
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(Cpu)
{
    sortResult = singlecmp(processItem1->CpuUsage, processItem2->CpuUsage);
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(IoTotal)
{
    sortResult = uint64cmp(
        processItem1->IoReadDelta.Delta + processItem1->IoWriteDelta.Delta + processItem1->IoOtherDelta.Delta,
        processItem2->IoReadDelta.Delta + processItem2->IoWriteDelta.Delta + processItem2->IoOtherDelta.Delta
        );
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(PrivateBytes)
{
    sortResult = uintptrcmp(processItem1->VmCounters.PagefileUsage, processItem2->VmCounters.PagefileUsage);
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(UserName)
{
    sortResult = PhCompareStringWithNull(processItem1->UserName, processItem2->UserName, TRUE);
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(Description)
{
    sortResult = PhCompareStringWithNull(
        processItem1->VersionInfo.FileDescription,
        processItem2->VersionInfo.FileDescription,
        TRUE
        );
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(CompanyName)
{
    sortResult = PhCompareStringWithNull(
        processItem1->VersionInfo.CompanyName,
        processItem2->VersionInfo.CompanyName,
        TRUE
        );
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(Version)
{
    sortResult = PhCompareStringWithNull(
        processItem1->VersionInfo.FileVersion,
        processItem2->VersionInfo.FileVersion,
        TRUE
        );
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(FileName)
{
    sortResult = PhCompareStringWithNull(
        processItem1->FileName,
        processItem2->FileName,
        TRUE
        );
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(CommandLine)
{
    sortResult = PhCompareStringWithNull(
        processItem1->CommandLine,
        processItem2->CommandLine,
        TRUE
        );
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(PeakPrivateBytes)
{
    sortResult = uintptrcmp(processItem1->VmCounters.PeakPagefileUsage, processItem2->VmCounters.PeakPagefileUsage);
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(WorkingSet)
{
    sortResult = uintptrcmp(processItem1->VmCounters.WorkingSetSize, processItem2->VmCounters.WorkingSetSize);
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(PeakWorkingSet)
{
    sortResult = uintptrcmp(processItem1->VmCounters.PeakWorkingSetSize, processItem2->VmCounters.PeakWorkingSetSize);
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(PrivateWs)
{
    PhpUpdateProcessNodeWsCounters(node1);
    PhpUpdateProcessNodeWsCounters(node2);
    sortResult = uintcmp(node1->WsCounters.NumberOfPrivatePages, node2->WsCounters.NumberOfPrivatePages);
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(SharedWs)
{
    PhpUpdateProcessNodeWsCounters(node1);
    PhpUpdateProcessNodeWsCounters(node2);
    sortResult = uintcmp(node1->WsCounters.NumberOfSharedPages, node2->WsCounters.NumberOfSharedPages);
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(ShareableWs)
{
    PhpUpdateProcessNodeWsCounters(node1);
    PhpUpdateProcessNodeWsCounters(node2);
    sortResult = uintcmp(node1->WsCounters.NumberOfShareablePages, node2->WsCounters.NumberOfShareablePages);
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(VirtualSize)
{
    sortResult = uintptrcmp(processItem1->VmCounters.VirtualSize, processItem2->VmCounters.VirtualSize);
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(PeakVirtualSize)
{
    sortResult = uintptrcmp(processItem1->VmCounters.PeakVirtualSize, processItem2->VmCounters.PeakVirtualSize);
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(PageFaults)
{
    sortResult = uintcmp(processItem1->VmCounters.PageFaultCount, processItem2->VmCounters.PageFaultCount);
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(SessionId)
{
    sortResult = uintcmp(processItem1->SessionId, processItem2->SessionId);
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(BasePriority)
{
    sortResult = intcmp(processItem1->BasePriority, processItem2->BasePriority);
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(Threads)
{
    sortResult = uintcmp(processItem1->NumberOfThreads, processItem2->NumberOfThreads);
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(Handles)
{
    sortResult = uintcmp(processItem1->NumberOfHandles, processItem2->NumberOfHandles);
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(GdiHandles)
{
    sortResult = uintcmp(node1->GdiHandles, node2->GdiHandles);
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(UserHandles)
{
    sortResult = uintcmp(node1->UserHandles, node2->UserHandles);
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(IoRo)
{
    sortResult = uint64cmp(
        processItem1->IoReadDelta.Delta + processItem1->IoOtherDelta.Delta,
        processItem2->IoReadDelta.Delta + processItem2->IoOtherDelta.Delta
        );
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(IoW)
{
    sortResult = uint64cmp(
        processItem1->IoWriteDelta.Delta,
        processItem2->IoWriteDelta.Delta
        );
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(Integrity)
{
    sortResult = uintcmp(processItem1->IntegrityLevel, processItem2->IntegrityLevel);
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(IoPriority)
{
    sortResult = uintcmp(node1->IoPriority, node2->IoPriority);
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(PagePriority)
{
    sortResult = uintcmp(node1->PagePriority, node2->PagePriority);
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(StartTime)
{
    sortResult = int64cmp(processItem1->CreateTime.QuadPart, processItem2->CreateTime.QuadPart);
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(TotalCpuTime)
{
    sortResult = uint64cmp(
        processItem1->KernelTime.QuadPart + processItem1->UserTime.QuadPart,
        processItem2->KernelTime.QuadPart + processItem2->UserTime.QuadPart
        );
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(KernelCpuTime)
{
    sortResult = uint64cmp(
        processItem1->KernelTime.QuadPart,
        processItem2->KernelTime.QuadPart
        );
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(UserCpuTime)
{
    sortResult = uint64cmp(
        processItem1->UserTime.QuadPart,
        processItem2->UserTime.QuadPart
        );
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(VerificationStatus)
{
    sortResult = intcmp(processItem1->VerifyResult == VrTrusted, processItem2->VerifyResult == VrTrusted);
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(VerifiedSigner)
{
    sortResult = PhCompareStringWithNull(
        processItem1->VerifySignerName,
        processItem2->VerifySignerName,
        TRUE
        );
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(Reserved1)
{
    sortResult = 0;
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(RelativeStartTime)
{
    sortResult = -int64cmp(processItem1->CreateTime.QuadPart, processItem2->CreateTime.QuadPart);
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(Bits)
{
    sortResult = intcmp(processItem1->IsWow64, processItem2->IsWow64);
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(Elevation)
{
    sortResult = intcmp(processItem1->ElevationType, processItem2->ElevationType);
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(WindowTitle)
{
    PhpUpdateProcessNodeWindow(node1);
    PhpUpdateProcessNodeWindow(node2);
    sortResult = PhCompareStringWithNull(node1->WindowText, node2->WindowText, TRUE);
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(WindowStatus)
{
    PhpUpdateProcessNodeWindow(node1);
    PhpUpdateProcessNodeWindow(node2);
    sortResult = intcmp(node1->WindowHung, node2->WindowHung);
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(Cycles)
{
    sortResult = uint64cmp(node1->CyclesDelta.Value, node2->CyclesDelta.Value);
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(CyclesDelta)
{
    sortResult = uint64cmp(node1->CyclesDelta.Delta, node2->CyclesDelta.Delta);
}
END_SORT_FUNCTION

BOOLEAN NTAPI PhpProcessTreeListCallback(
    __in HWND hwnd,
    __in PH_TREELIST_MESSAGE Message,
    __in_opt PVOID Parameter1,
    __in_opt PVOID Parameter2,
    __in_opt PVOID Context
    )
{
    PPH_PROCESS_NODE node;

    switch (Message)
    {
    case TreeListGetChildren:
        {
            PPH_TREELIST_GET_CHILDREN getChildren = Parameter1;

            node = (PPH_PROCESS_NODE)getChildren->Node;

            if (ProcessTreeListSortOrder == NoSortOrder)
            {
                if (!node)
                {
                    getChildren->Children = (PPH_TREELIST_NODE *)ProcessNodeRootList->Items;
                    getChildren->NumberOfChildren = ProcessNodeRootList->Count;
                }
                else
                {
                    getChildren->Children = (PPH_TREELIST_NODE *)node->Children->Items;
                    getChildren->NumberOfChildren = node->Children->Count;
                }
            }
            else
            {
                if (!node)
                {
                    static PVOID sortFunctions[] =
                    {
                        SORT_FUNCTION(Name),
                        SORT_FUNCTION(Pid),
                        SORT_FUNCTION(Cpu),
                        SORT_FUNCTION(IoTotal),
                        SORT_FUNCTION(PrivateBytes),
                        SORT_FUNCTION(UserName),
                        SORT_FUNCTION(Description),
                        SORT_FUNCTION(CompanyName),
                        SORT_FUNCTION(Version),
                        SORT_FUNCTION(FileName),
                        SORT_FUNCTION(CommandLine),
                        SORT_FUNCTION(PeakPrivateBytes),
                        SORT_FUNCTION(WorkingSet),
                        SORT_FUNCTION(PeakWorkingSet),
                        SORT_FUNCTION(PrivateWs),
                        SORT_FUNCTION(SharedWs),
                        SORT_FUNCTION(ShareableWs),
                        SORT_FUNCTION(VirtualSize),
                        SORT_FUNCTION(PeakVirtualSize),
                        SORT_FUNCTION(PageFaults),
                        SORT_FUNCTION(SessionId),
                        SORT_FUNCTION(BasePriority), // Priority Class
                        SORT_FUNCTION(BasePriority),
                        SORT_FUNCTION(Threads),
                        SORT_FUNCTION(Handles),
                        SORT_FUNCTION(GdiHandles),
                        SORT_FUNCTION(UserHandles),
                        SORT_FUNCTION(IoRo),
                        SORT_FUNCTION(IoW),
                        SORT_FUNCTION(Integrity),
                        SORT_FUNCTION(IoPriority),
                        SORT_FUNCTION(PagePriority),
                        SORT_FUNCTION(StartTime),
                        SORT_FUNCTION(TotalCpuTime),
                        SORT_FUNCTION(KernelCpuTime),
                        SORT_FUNCTION(UserCpuTime),
                        SORT_FUNCTION(VerificationStatus),
                        SORT_FUNCTION(VerifiedSigner),
                        SORT_FUNCTION(Reserved1),
                        SORT_FUNCTION(RelativeStartTime),
                        SORT_FUNCTION(Bits),
                        SORT_FUNCTION(Elevation),
                        SORT_FUNCTION(WindowTitle),
                        SORT_FUNCTION(WindowStatus),
                        SORT_FUNCTION(Cycles),
                        SORT_FUNCTION(CyclesDelta),
                        SORT_FUNCTION(Cpu), // CPU History
                        SORT_FUNCTION(PrivateBytes), // Private Bytes History
                        SORT_FUNCTION(IoTotal) // I/O History
                    };
                    int (__cdecl *sortFunction)(const void *, const void *);

                    if (ProcessTreeListSortColumn < PHPRTLC_MAXIMUM)
                        sortFunction = sortFunctions[ProcessTreeListSortColumn];
                    else
                        sortFunction = NULL;

                    if (sortFunction)
                    {
                        // Don't use PhSortList to avoid overhead.
                        qsort(ProcessNodeList->Items, ProcessNodeList->Count, sizeof(PVOID), sortFunction);
                    }

                    getChildren->Children = (PPH_TREELIST_NODE *)ProcessNodeList->Items;
                    getChildren->NumberOfChildren = ProcessNodeList->Count;
                }
            }
        }
        return TRUE;
    case TreeListIsLeaf:
        {
            PPH_TREELIST_IS_LEAF isLeaf = Parameter1;

            node = (PPH_PROCESS_NODE)isLeaf->Node;

            if (ProcessTreeListSortOrder == NoSortOrder)
                isLeaf->IsLeaf = node->Children->Count == 0;
            else
                isLeaf->IsLeaf = TRUE;
        }
        return TRUE;
    case TreeListGetNodeText:
        {
            PPH_TREELIST_GET_NODE_TEXT getNodeText = Parameter1;
            PPH_PROCESS_ITEM processItem;

            node = (PPH_PROCESS_NODE)getNodeText->Node;
            processItem = node->ProcessItem;

            switch (getNodeText->Id)
            {
            case PHPRTLC_NAME:
                getNodeText->Text = processItem->ProcessName->sr;
                break;
            case PHPRTLC_PID:
                PhInitializeStringRef(&getNodeText->Text, processItem->ProcessIdString);
                break;
            case PHPRTLC_CPU:
                {
                    FLOAT cpuUsage;

                    if (!PhCsPropagateCpuUsage || node->Node.Expanded || ProcessTreeListSortOrder != NoSortOrder)
                    {
                        cpuUsage = processItem->CpuUsage * 100;
                    }
                    else
                    {
                        cpuUsage = PhpCalculateInclusiveCpuUsage(node) * 100;
                    }

                    if (cpuUsage >= 0.01)
                    {
                        _snwprintf(node->CpuUsageText, PH_INT32_STR_LEN, L"%.2f", cpuUsage);
                        PhInitializeStringRef(&getNodeText->Text, node->CpuUsageText);
                    }
                    else
                    {
                        PhInitializeEmptyStringRef(&getNodeText->Text);
                    }
                }
                break;
            case PHPRTLC_IOTOTAL:
                {
                    ULONG64 number;

                    if (processItem->IoReadDelta.Delta != processItem->IoReadDelta.Value) // delta is wrong on first run of process provider
                    {
                        number = processItem->IoReadDelta.Delta + processItem->IoWriteDelta.Delta + processItem->IoOtherDelta.Delta;
                        number *= 1000;
                        number /= PhCsUpdateInterval;
                    }
                    else
                    {
                        number = 0;
                    }

                    if (number != 0)
                    {
                        PH_FORMAT format[2];

                        PhInitFormatSize(&format[0], number);
                        PhInitFormatS(&format[1], L"/s");
                        PhSwapReference2(&node->IoTotalText, PhFormat(format, 2, 0));
                        getNodeText->Text = node->IoTotalText->sr;
                    }
                    else
                    {
                        PhInitializeEmptyStringRef(&getNodeText->Text);
                    }
                }
                break;
            case PHPRTLC_PRIVATEBYTES:
                PhSwapReference2(&node->PrivateBytesText, PhFormatSize(processItem->VmCounters.PagefileUsage, -1));
                getNodeText->Text = node->PrivateBytesText->sr;
                break;
            case PHPRTLC_USERNAME:
                getNodeText->Text = PhGetStringRefOrEmpty(processItem->UserName);
                break;
            case PHPRTLC_DESCRIPTION:
                getNodeText->Text = PhGetStringRefOrEmpty(processItem->VersionInfo.FileDescription);
                break;
            case PHPRTLC_COMPANYNAME:
                getNodeText->Text = PhGetStringRefOrEmpty(processItem->VersionInfo.CompanyName);
                break;
            case PHPRTLC_VERSION:
                getNodeText->Text = PhGetStringRefOrEmpty(processItem->VersionInfo.FileVersion);
                break;
            case PHPRTLC_FILENAME:
                getNodeText->Text = PhGetStringRefOrEmpty(processItem->FileName);
                break;
            case PHPRTLC_COMMANDLINE:
                getNodeText->Text = PhGetStringRefOrEmpty(processItem->CommandLine);
                break;
            case PHPRTLC_PEAKPRIVATEBYTES:
                PhSwapReference2(&node->PeakPrivateBytesText, PhFormatSize(processItem->VmCounters.PeakPagefileUsage, -1));
                getNodeText->Text = node->PeakPrivateBytesText->sr;
                break;
            case PHPRTLC_WORKINGSET:
                PhSwapReference2(&node->WorkingSetText, PhFormatSize(processItem->VmCounters.WorkingSetSize, -1));
                getNodeText->Text = node->WorkingSetText->sr;
                break;
            case PHPRTLC_PEAKWORKINGSET:
                PhSwapReference2(&node->PeakWorkingSetText, PhFormatSize(processItem->VmCounters.PeakWorkingSetSize, -1));
                getNodeText->Text = node->PeakWorkingSetText->sr;
                break;
            case PHPRTLC_PRIVATEWS:
                PhpUpdateProcessNodeWsCounters(node);
                PhSwapReference2(&node->PrivateWsText, PhFormatSize(UInt32x32To64(node->WsCounters.NumberOfPrivatePages, PAGE_SIZE), -1));
                getNodeText->Text = node->PrivateWsText->sr;
                break;
            case PHPRTLC_SHAREDWS:
                PhpUpdateProcessNodeWsCounters(node);
                PhSwapReference2(&node->SharedWsText, PhFormatSize(UInt32x32To64(node->WsCounters.NumberOfSharedPages, PAGE_SIZE), -1));
                getNodeText->Text = node->SharedWsText->sr;
                break;
            case PHPRTLC_SHAREABLEWS:
                PhpUpdateProcessNodeWsCounters(node);
                PhSwapReference2(&node->ShareableWsText, PhFormatSize(UInt32x32To64(node->WsCounters.NumberOfShareablePages, PAGE_SIZE), -1));
                getNodeText->Text = node->ShareableWsText->sr;
                break;
            case PHPRTLC_VIRTUALSIZE:
                PhSwapReference2(&node->VirtualSizeText, PhFormatSize(processItem->VmCounters.VirtualSize, -1));
                getNodeText->Text = node->VirtualSizeText->sr;
                break;
            case PHPRTLC_PEAKVIRTUALSIZE:
                PhSwapReference2(&node->PeakVirtualSizeText, PhFormatSize(processItem->VmCounters.PeakVirtualSize, -1));
                getNodeText->Text = node->PeakVirtualSizeText->sr;
                break;
            case PHPRTLC_PAGEFAULTS:
                PhSwapReference2(&node->PageFaultsText, PhFormatUInt64(processItem->VmCounters.PageFaultCount, TRUE));
                getNodeText->Text = node->PageFaultsText->sr;
                break;
            case PHPRTLC_SESSIONID:
                PhInitializeStringRef(&getNodeText->Text, processItem->SessionIdString);
                break;
            case PHPRTLC_PRIORITYCLASS:
                PhInitializeStringRef(&getNodeText->Text, PhGetProcessPriorityClassString(processItem->PriorityClass));
                break;
            case PHPRTLC_BASEPRIORITY:
                PhPrintInt32(node->BasePriorityText, processItem->BasePriority);
                PhInitializeStringRef(&getNodeText->Text, node->BasePriorityText);
                break;
            case PHPRTLC_THREADS:
                PhPrintUInt32(node->ThreadsText, processItem->NumberOfThreads);
                PhInitializeStringRef(&getNodeText->Text, node->ThreadsText);
                break;
            case PHPRTLC_HANDLES:
                PhPrintUInt32(node->HandlesText, processItem->NumberOfHandles);
                PhInitializeStringRef(&getNodeText->Text, node->HandlesText);
                break;
            case PHPRTLC_GDIHANDLES:
                PhpUpdateProcessNodeGdiUserHandles(node);
                PhPrintUInt32(node->GdiHandlesText, node->GdiHandles);
                PhInitializeStringRef(&getNodeText->Text, node->GdiHandlesText);
                break;
            case PHPRTLC_USERHANDLES:
                PhpUpdateProcessNodeGdiUserHandles(node);
                PhPrintUInt32(node->UserHandlesText, node->UserHandles);
                PhInitializeStringRef(&getNodeText->Text, node->UserHandlesText);
                break;
            case PHPRTLC_IORO:
                {
                    ULONG64 number;

                    if (processItem->IoReadDelta.Delta != processItem->IoReadDelta.Value)
                    {
                        number = processItem->IoReadDelta.Delta + processItem->IoOtherDelta.Delta;
                        number *= 1000;
                        number /= PhCsUpdateInterval;
                    }
                    else
                    {
                        number = 0;
                    }

                    if (number != 0)
                    {
                        PH_FORMAT format[2];

                        PhInitFormatSize(&format[0], number);
                        PhInitFormatS(&format[1], L"/s");
                        PhSwapReference2(&node->IoRoText, PhFormat(format, 2, 0));
                        getNodeText->Text = node->IoRoText->sr;
                    }
                    else
                    {
                        PhInitializeEmptyStringRef(&getNodeText->Text);
                    }
                }
                break;
            case PHPRTLC_IOW:
                {
                    ULONG64 number;

                    if (processItem->IoReadDelta.Delta != processItem->IoReadDelta.Value)
                    {
                        number = processItem->IoWriteDelta.Delta;
                        number *= 1000;
                        number /= PhCsUpdateInterval;
                    }
                    else
                    {
                        number = 0;
                    }

                    if (number != 0)
                    {
                        PH_FORMAT format[2];

                        PhInitFormatSize(&format[0], number);
                        PhInitFormatS(&format[1], L"/s");
                        PhSwapReference2(&node->IoWText, PhFormat(format, 2, 0));
                        getNodeText->Text = node->IoWText->sr;
                    }
                    else
                    {
                        PhInitializeEmptyStringRef(&getNodeText->Text);
                    }
                }
                break;
            case PHPRTLC_INTEGRITY:
                if (processItem->IntegrityString)
                    PhInitializeStringRef(&getNodeText->Text, processItem->IntegrityString);
                else
                    PhInitializeEmptyStringRef(&getNodeText->Text);
                break;
            case PHPRTLC_IOPRIORITY:
                PhpUpdateProcessNodeIoPagePriority(node);

                if (node->IoPriority != -1)
                {
                    if (node->IoPriority < MaxIoPriorityTypes)
                        PhInitializeStringRef(&getNodeText->Text, PhIoPriorityHintNames[node->IoPriority]);
                }
                else
                {
                    PhInitializeEmptyStringRef(&getNodeText->Text);
                }

                break;
            case PHPRTLC_PAGEPRIORITY:
                PhpUpdateProcessNodeIoPagePriority(node);

                if (node->PagePriority != -1)
                {
                    PhPrintUInt32(node->PagePriorityText, node->PagePriority);
                    PhInitializeStringRef(&getNodeText->Text, node->PagePriorityText);
                }
                else
                {
                    PhInitializeEmptyStringRef(&getNodeText->Text);
                }

                break;
            case PHPRTLC_STARTTIME:
                {
                    SYSTEMTIME systemTime;

                    if (processItem->CreateTime.QuadPart != 0)
                    {
                        PhLargeIntegerToLocalSystemTime(&systemTime, &processItem->CreateTime);
                        PhSwapReference2(&node->StartTimeText, PhFormatDateTime(&systemTime));
                        getNodeText->Text = node->StartTimeText->sr;
                    }
                    else
                    {
                        PhInitializeEmptyStringRef(&getNodeText->Text);
                    }
                }
                break;
            case PHPRTLC_TOTALCPUTIME:
                PhPrintTimeSpan(node->TotalCpuTimeText,
                    processItem->KernelTime.QuadPart + processItem->UserTime.QuadPart,
                    PH_TIMESPAN_HMSM);
                PhInitializeStringRef(&getNodeText->Text, node->TotalCpuTimeText);
                break;
            case PHPRTLC_KERNELCPUTIME:
                PhPrintTimeSpan(node->KernelCpuTimeText, processItem->KernelTime.QuadPart, PH_TIMESPAN_HMSM);
                PhInitializeStringRef(&getNodeText->Text, node->KernelCpuTimeText);
                break;
            case PHPRTLC_USERCPUTIME:
                PhPrintTimeSpan(node->UserCpuTimeText, processItem->UserTime.QuadPart, PH_TIMESPAN_HMSM);
                PhInitializeStringRef(&getNodeText->Text, node->UserCpuTimeText);
                break;
            case PHPRTLC_VERIFICATIONSTATUS:
                PhInitializeStringRef(&getNodeText->Text,
                    processItem->VerifyResult == VrTrusted ? L"Trusted" : L"Not Trusted");
                break;
            case PHPRTLC_VERIFIEDSIGNER:
                getNodeText->Text = PhGetStringRefOrEmpty(processItem->VerifySignerName);
                break;
            case PHPRTLC_RELATIVESTARTTIME:
                {
                    if (processItem->CreateTime.QuadPart != 0)
                    {
                        LARGE_INTEGER currentTime;

                        PhQuerySystemTime(&currentTime);
                        PhSwapReference2(&node->RelativeStartTimeText,
                            PhFormatTimeSpanRelative(currentTime.QuadPart - processItem->CreateTime.QuadPart));
                        getNodeText->Text = node->RelativeStartTimeText->sr;
                    }
                    else
                    {
                        PhInitializeEmptyStringRef(&getNodeText->Text);
                    }
                }
                break;
            case PHPRTLC_BITS:
#ifdef _M_X64
                PhInitializeStringRef(&getNodeText->Text, processItem->IsWow64 ? L"32-bit" : L"64-bit");
#else
                PhInitializeStringRef(&getNodeText->Text, L"32-bit");
#endif
                break;
            case PHPRTLC_ELEVATION:
                {
                    PWSTR type;

                    if (WINDOWS_HAS_UAC)
                    {
                        switch (processItem->ElevationType)
                        {
                        case TokenElevationTypeDefault:
                            type = L"N/A";
                            break;
                        case TokenElevationTypeLimited:
                            type = L"Limited";
                            break;
                        case TokenElevationTypeFull:
                            type = L"Full";
                            break;
                        default:
                            type = L"N/A";
                            break;
                        }
                    }
                    else
                    {
                        type = L"";
                    }

                    PhInitializeStringRef(&getNodeText->Text, type);
                }
                break;
            case PHPRTLC_WINDOWTITLE:
                PhpUpdateProcessNodeWindow(node);
                PhSwapReference(&node->WindowTitleText, node->WindowText);
                getNodeText->Text = PhGetStringRef(node->WindowTitleText);
                break;
            case PHPRTLC_WINDOWSTATUS:
                PhpUpdateProcessNodeWindow(node);

                if (node->WindowHandle)
                    PhInitializeStringRef(&getNodeText->Text, node->WindowHung ? L"Not responding" : L"Running");
                else
                    PhInitializeEmptyStringRef(&getNodeText->Text);
                break;
            case PHPRTLC_CYCLES:
                getNodeText->Text = PhGetStringRef(node->CyclesText);
                break;
            case PHPRTLC_CYCLESDELTA:
                getNodeText->Text = PhGetStringRef(node->CyclesDeltaText);
                break;
            default:
                return FALSE;
            }

            getNodeText->Flags = TLC_CACHE;
        }
        return TRUE;
    case TreeListGetNodeColor:
        {
            PPH_TREELIST_GET_NODE_COLOR getNodeColor = Parameter1;
            PPH_PROCESS_ITEM processItem;

            node = (PPH_PROCESS_NODE)getNodeColor->Node;
            processItem = node->ProcessItem;

            if (PhPluginsEnabled)
            {
                PH_PLUGIN_GET_HIGHLIGHTING_COLOR getHighlightingColor;

                getHighlightingColor.Parameter = processItem;
                getHighlightingColor.BackColor = RGB(0xff, 0xff, 0xff);
                getHighlightingColor.Handled = FALSE;
                getHighlightingColor.Cache = FALSE;

                PhInvokeCallback(PhGetGeneralCallback(GeneralCallbackGetProcessHighlightingColor), &getHighlightingColor);

                if (getHighlightingColor.Handled)
                {
                    getNodeColor->BackColor = getHighlightingColor.BackColor;
                    getNodeColor->Flags = TLGNC_AUTO_FORECOLOR;

                    if (getHighlightingColor.Cache)
                        getNodeColor->Flags |= TLC_CACHE;

                    return TRUE;
                }
            }

            if (!processItem)
                ; // Dummy
            else if (PhCsUseColorDebuggedProcesses && processItem->IsBeingDebugged)
                getNodeColor->BackColor = PhCsColorDebuggedProcesses;
            else if (PhCsUseColorSuspended && processItem->IsSuspended)
                getNodeColor->BackColor = PhCsColorSuspended;
            else if (PhCsUseColorElevatedProcesses && processItem->IsElevated)
                getNodeColor->BackColor = PhCsColorElevatedProcesses;
            else if (PhCsUseColorPosixProcesses && processItem->IsPosix)
                getNodeColor->BackColor = PhCsColorPosixProcesses;
            else if (PhCsUseColorWow64Processes && processItem->IsWow64)
                getNodeColor->BackColor = PhCsColorWow64Processes;
            else if (PhCsUseColorJobProcesses && processItem->IsInSignificantJob)
                getNodeColor->BackColor = PhCsColorJobProcesses;
            //else if (
            //    PhCsUseColorPacked &&
            //    (processItem->VerifyResult != VrUnknown &&
            //    processItem->VerifyResult != VrNoSignature &&
            //    processItem->VerifyResult != VrTrusted
            //    ))
            //    getNodeColor->BackColor = PhCsColorPacked;
            else if (PhCsUseColorDotNet && processItem->IsDotNet)
                getNodeColor->BackColor = PhCsColorDotNet;
            else if (PhCsUseColorPacked && processItem->IsPacked)
                getNodeColor->BackColor = PhCsColorPacked;
            else if (PhCsUseColorServiceProcesses && processItem->ServiceList && processItem->ServiceList->Count != 0)
                getNodeColor->BackColor = PhCsColorServiceProcesses;
            else if (
                PhCsUseColorSystemProcesses &&
                processItem->UserName &&
                PhEqualString(processItem->UserName, PhLocalSystemName, TRUE)
                )
                getNodeColor->BackColor = PhCsColorSystemProcesses;
            else if (
                PhCsUseColorOwnProcesses &&
                processItem->UserName &&
                PhCurrentUserName &&
                PhEqualString(processItem->UserName, PhCurrentUserName, TRUE)
                )
                getNodeColor->BackColor = PhCsColorOwnProcesses;

            getNodeColor->Flags = TLC_CACHE | TLGNC_AUTO_FORECOLOR;
        }
        return TRUE;
    case TreeListGetNodeIcon:
        {
            PPH_TREELIST_GET_NODE_ICON getNodeIcon = Parameter1;

            node = (PPH_PROCESS_NODE)getNodeIcon->Node;

            if (node->ProcessItem->SmallIcon)
            {
                getNodeIcon->Icon = node->ProcessItem->SmallIcon;
            }
            else
            {
                PhGetStockApplicationIcon(&getNodeIcon->Icon, NULL);
            }

            getNodeIcon->Flags = TLC_CACHE;
        }
        return TRUE;
    case TreeListGetNodeTooltip:
        {
            PPH_TREELIST_GET_NODE_TOOLTIP getNodeTooltip = Parameter1;

            node = (PPH_PROCESS_NODE)getNodeTooltip->Node;

            if (!node->TooltipText)
                node->TooltipText = PhGetProcessTooltipText(node->ProcessItem);

            if (node->TooltipText)
                getNodeTooltip->Text = node->TooltipText->sr;
            else
                return FALSE;
        }
        return TRUE;
    case TreeListCustomDraw:
        {
            PPH_TREELIST_CUSTOM_DRAW customDraw = Parameter1;
            PPH_PROCESS_ITEM processItem;
            RECT rect;
            PH_GRAPH_DRAW_INFO drawInfo;
            POINT origPoint;

            node = (PPH_PROCESS_NODE)customDraw->Node;
            processItem = node->ProcessItem;
            rect = customDraw->CellRect;

            // Fix for the first column.
            if (customDraw->Column->DisplayIndex == 0)
                rect.left = customDraw->TextRect.left;

            // Generic graph pre-processing
            switch (customDraw->Column->Id)
            {
            case PHPRTLC_CPUHISTORY:
            case PHPRTLC_PRIVATEBYTESHISTORY:
            case PHPRTLC_IOHISTORY:
                memset(&drawInfo, 0, sizeof(PH_GRAPH_DRAW_INFO));
                drawInfo.Width = rect.right - rect.left;
                drawInfo.Height = rect.bottom - rect.top - 1; // leave a small gap
                drawInfo.Step = 2;
                drawInfo.BackColor = RGB(0x00, 0x00, 0x00);
                break;
            }

            // Specific graph processing
            switch (customDraw->Column->Id)
            {
            case PHPRTLC_CPUHISTORY:
                {
                    drawInfo.Flags = PH_GRAPH_USE_LINE_2;
                    drawInfo.LineColor1 = PhCsColorCpuKernel;
                    drawInfo.LineColor2 = PhCsColorCpuUser;
                    drawInfo.LineBackColor1 = PhHalveColorBrightness(PhCsColorCpuKernel);
                    drawInfo.LineBackColor2 = PhHalveColorBrightness(PhCsColorCpuUser);

                    PhGetDrawInfoGraphBuffers(
                        &node->CpuGraphBuffers,
                        &drawInfo,
                        processItem->CpuKernelHistory.Count
                        );

                    if (!node->CpuGraphBuffers.Valid)
                    {
                        PhCopyCircularBuffer_FLOAT(&processItem->CpuKernelHistory,
                            node->CpuGraphBuffers.Data1, drawInfo.LineDataCount);
                        PhCopyCircularBuffer_FLOAT(&processItem->CpuUserHistory,
                            node->CpuGraphBuffers.Data2, drawInfo.LineDataCount);
                        node->CpuGraphBuffers.Valid = TRUE;
                    }
                }
                break;
            case PHPRTLC_PRIVATEBYTESHISTORY:
                {
                    drawInfo.Flags = 0;
                    drawInfo.LineColor1 = PhCsColorPrivate;
                    drawInfo.LineBackColor1 = PhHalveColorBrightness(PhCsColorPrivate);

                    PhGetDrawInfoGraphBuffers(
                        &node->PrivateGraphBuffers,
                        &drawInfo,
                        processItem->PrivateBytesHistory.Count
                        );

                    if (!node->PrivateGraphBuffers.Valid)
                    {
                        ULONG i;
                        FLOAT total;
                        FLOAT max;

                        for (i = 0; i < drawInfo.LineDataCount; i++)
                        {
                            node->PrivateGraphBuffers.Data1[i] =
                                (FLOAT)PhGetItemCircularBuffer_SIZE_T(&processItem->PrivateBytesHistory, i);
                        }

                        // This makes it easier for the user to see what processes are hogging memory.
                        // Scaling is still *not* consistent across all graphs.
                        total = (FLOAT)PhPerfInformation.CommittedPages * PAGE_SIZE / 4; // divide by 4 to make the scaling a bit better
                        max = (FLOAT)processItem->VmCounters.PeakPagefileUsage;

                        if (max < total)
                            max = total;

                        if (max != 0)
                        {
                            // Scale the data.
                            PhxfDivideSingle2U(
                                node->PrivateGraphBuffers.Data1,
                                max,
                                drawInfo.LineDataCount
                                );
                        }

                        node->PrivateGraphBuffers.Valid = TRUE;
                    }
                }
                break;
            case PHPRTLC_IOHISTORY:
                {
                    drawInfo.Flags = PH_GRAPH_USE_LINE_2;
                    drawInfo.LineColor1 = PhCsColorIoReadOther;
                    drawInfo.LineColor2 = PhCsColorIoWrite;
                    drawInfo.LineBackColor1 = PhHalveColorBrightness(PhCsColorIoReadOther);
                    drawInfo.LineBackColor2 = PhHalveColorBrightness(PhCsColorIoWrite);

                    PhGetDrawInfoGraphBuffers(
                        &node->IoGraphBuffers,
                        &drawInfo,
                        processItem->IoReadHistory.Count
                        );

                    if (!node->IoGraphBuffers.Valid)
                    {
                        ULONG i;
                        FLOAT total;
                        FLOAT max = 0;

                        for (i = 0; i < drawInfo.LineDataCount; i++)
                        {
                            FLOAT data1;
                            FLOAT data2;

                            node->IoGraphBuffers.Data1[i] = data1 =
                                (FLOAT)PhGetItemCircularBuffer_ULONG64(&processItem->IoReadHistory, i) +
                                (FLOAT)PhGetItemCircularBuffer_ULONG64(&processItem->IoOtherHistory, i);
                            node->IoGraphBuffers.Data2[i] = data2 =
                                (FLOAT)PhGetItemCircularBuffer_ULONG64(&processItem->IoWriteHistory, i);

                            if (max < data1 + data2)
                                max = data1 + data2;
                        }

                        // Make the scaling a bit more consistent across the processes.
                        // It does *not* scale all graphs using the same maximum.
                        total = (FLOAT)(PhIoReadDelta.Delta + PhIoWriteDelta.Delta + PhIoOtherDelta.Delta);

                        if (max < total)
                            max = total;

                        if (max != 0)
                        {
                            // Scale the data.

                            PhxfDivideSingle2U(
                                node->IoGraphBuffers.Data1,
                                max,
                                drawInfo.LineDataCount
                                );
                            PhxfDivideSingle2U(
                                node->IoGraphBuffers.Data2,
                                max,
                                drawInfo.LineDataCount
                                );
                        }

                        node->IoGraphBuffers.Valid = TRUE;
                    }
                }
                break;
            }

            // Draw the graph.
            switch (customDraw->Column->Id)
            {
            case PHPRTLC_CPUHISTORY:
            case PHPRTLC_PRIVATEBYTESHISTORY:
            case PHPRTLC_IOHISTORY:
                SetViewportOrgEx(customDraw->Dc, rect.left, rect.top + 1, &origPoint); // + 1 for small gap
                PhDrawGraph(customDraw->Dc, &drawInfo);
                SetViewportOrgEx(customDraw->Dc, origPoint.x, origPoint.y, NULL);
                break;
            }
        }
        return TRUE;
    case TreeListSortChanged:
        {
            TreeList_GetSort(hwnd, &ProcessTreeListSortColumn, &ProcessTreeListSortOrder);
            // Force a rebuild to sort the items.
            TreeList_NodesStructured(hwnd);
        }
        return TRUE;
    case TreeListKeyDown:
        {
            switch ((SHORT)Parameter1)
            {
            case 'C':
                if (GetKeyState(VK_CONTROL) < 0)
                    SendMessage(PhMainWndHandle, WM_COMMAND, ID_PROCESS_COPY, 0);
                break;
            case VK_DELETE:
                if (GetKeyState(VK_SHIFT) >= 0)
                    SendMessage(PhMainWndHandle, WM_COMMAND, ID_PROCESS_TERMINATE, 0);
                else
                    SendMessage(PhMainWndHandle, WM_COMMAND, ID_PROCESS_TERMINATETREE, 0);

                break;
            case VK_RETURN:
                SendMessage(PhMainWndHandle, WM_COMMAND, ID_PROCESS_PROPERTIES, 0);
                break;
            }
        }
        return TRUE;
    case TreeListHeaderRightClick:
        {
            HMENU menu;
            HMENU subMenu;
            POINT point;

            menu = LoadMenu(PhInstanceHandle, MAKEINTRESOURCE(IDR_PROCESSHEADER));
            subMenu = GetSubMenu(menu, 0);
            GetCursorPos(&point);

            if ((UINT)TrackPopupMenu(
                subMenu,
                TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON | TPM_RETURNCMD,
                point.x,
                point.y,
                0,
                hwnd,
                NULL
                ) == ID_HEADER_CHOOSECOLUMNS)
            {
                PhShowChooseColumnsDialog(hwnd, hwnd);
                PhpUpdateNeedCyclesInformation();

                // Make sure the column we're sorting by is actually visible, 
                // and if not, don't sort any more.
                if (ProcessTreeListSortOrder != NoSortOrder)
                {
                    PH_TREELIST_COLUMN column;

                    column.Id = ProcessTreeListSortColumn;
                    TreeList_GetColumn(ProcessTreeListHandle, &column);

                    if (!column.Visible)
                        TreeList_SetSort(ProcessTreeListHandle, 0, NoSortOrder);
                }
            }

            DestroyMenu(menu);
        }
        return TRUE;
    case TreeListLeftDoubleClick:
        {
            SendMessage(PhMainWndHandle, WM_COMMAND, ID_PROCESS_PROPERTIES, 0);
        }
        return TRUE;
    case TreeListContextMenu:
        {
            PPH_TREELIST_MOUSE_EVENT mouseEvent = Parameter2;

            PhShowProcessContextMenu(mouseEvent->Location);
        }
        return TRUE;
    case TreeListNodePlusMinusChanged:
        {
            node = Parameter1;

            if (PhCsPropagateCpuUsage)
                PhUpdateProcessNode(node);
        }
        return TRUE;
    }

    return FALSE;
}

PPH_PROCESS_ITEM PhGetSelectedProcessItem()
{
    PPH_PROCESS_ITEM processItem = NULL;
    ULONG i;

    for (i = 0; i < ProcessNodeList->Count; i++)
    {
        PPH_PROCESS_NODE node = ProcessNodeList->Items[i];

        if (node->Node.Selected)
        {
            processItem = node->ProcessItem;
            break;
        }
    }

    return processItem;
}

VOID PhGetSelectedProcessItems(
    __out PPH_PROCESS_ITEM **Processes,
    __out PULONG NumberOfProcesses
    )
{
    PPH_LIST list;
    ULONG i;

    list = PhCreateList(2);

    for (i = 0; i < ProcessNodeList->Count; i++)
    {
        PPH_PROCESS_NODE node = ProcessNodeList->Items[i];

        if (node->Node.Selected)
        {
            PhAddItemList(list, node->ProcessItem);
        }
    }

    *Processes = PhAllocateCopy(list->Items, sizeof(PVOID) * list->Count);
    *NumberOfProcesses = list->Count;

    PhDereferenceObject(list);
}

VOID PhDeselectAllProcessNodes()
{
    TreeList_SetStateAll(ProcessTreeListHandle, 0, LVIS_SELECTED);
    InvalidateRect(ProcessTreeListHandle, NULL, TRUE);
}

VOID PhInvalidateAllProcessNodes()
{
    ULONG i;

    for (i = 0; i < ProcessNodeList->Count; i++)
    {
        PPH_PROCESS_NODE node = ProcessNodeList->Items[i];

        memset(node->TextCache, 0, sizeof(PH_STRINGREF) * PHPRTLC_MAXIMUM);
        PhInvalidateTreeListNode(&node->Node, TLIN_COLOR);
        node->ValidMask = 0;
    }

    InvalidateRect(ProcessTreeListHandle, NULL, TRUE);
}

VOID PhSelectAndEnsureVisibleProcessNode(
    __in PPH_PROCESS_NODE ProcessNode
    )
{
    PPH_PROCESS_NODE processNode;
    BOOLEAN needsRestructure = FALSE;

    PhDeselectAllProcessNodes();

    if (!ProcessNode->Node.Visible)
        return;

    // Expand recursively, upwards.

    processNode = ProcessNode->Parent;

    while (processNode)
    {
        if (!processNode->Node.Expanded)
            needsRestructure = TRUE;

        processNode->Node.Expanded = TRUE;
        processNode = processNode->Parent;
    }

    ProcessNode->Node.Selected = TRUE;
    ProcessNode->Node.Focused = TRUE;

    if (needsRestructure)
        TreeList_NodesStructured(ProcessTreeListHandle);

    PhInvalidateStateTreeListNode(ProcessTreeListHandle, &ProcessNode->Node);
    TreeList_EnsureVisible(ProcessTreeListHandle, &ProcessNode->Node, FALSE);
}

PPH_PROCESS_TREE_FILTER_ENTRY PhAddProcessTreeFilter(
    __in PPH_PROCESS_TREE_FILTER Filter,
    __in_opt PVOID Context
    )
{
    PPH_PROCESS_TREE_FILTER_ENTRY entry;

    entry = PhAllocate(sizeof(PH_PROCESS_TREE_FILTER_ENTRY));
    entry->Filter = Filter;
    entry->Context = Context;

    if (!ProcessTreeFilterList)
        ProcessTreeFilterList = PhCreateList(2);

    PhAddItemList(ProcessTreeFilterList, entry);

    return entry;
}

VOID PhRemoveProcessTreeFilter(
    __in PPH_PROCESS_TREE_FILTER_ENTRY Entry
    )
{
    ULONG index;

    if (!ProcessTreeFilterList)
        return;

    index = PhFindItemList(ProcessTreeFilterList, Entry);

    if (index != -1)
    {
        PhRemoveItemList(ProcessTreeFilterList, index);
        PhFree(Entry);
    }
}

BOOLEAN PhpApplyProcessTreeFiltersToNode(
    __in PPH_PROCESS_NODE Node
    )
{
    BOOLEAN show;
    ULONG i;

    show = TRUE;

    if (ProcessTreeFilterList)
    {
        for (i = 0; i < ProcessTreeFilterList->Count; i++)
        {
            PPH_PROCESS_TREE_FILTER_ENTRY entry;

            entry = ProcessTreeFilterList->Items[i];

            if (!entry->Filter(Node, entry->Context))
            {
                show = FALSE;
                break;
            }
        }
    }

    return show;
}

VOID PhApplyProcessTreeFilters()
{
    ULONG i;

    for (i = 0; i < ProcessNodeList->Count; i++)
    {
        PPH_PROCESS_NODE node;

        node = ProcessNodeList->Items[i];
        node->Node.Visible = PhpApplyProcessTreeFiltersToNode(node);

        if (!node->Node.Visible && node->Node.Selected)
        {
            node->Node.Selected = FALSE;
            PhInvalidateStateTreeListNode(ProcessTreeListHandle, &node->Node);
        }
    }

    TreeList_NodesStructured(ProcessTreeListHandle);
}

VOID PhCopyProcessTree()
{
    PPH_FULL_STRING text;

    text = PhGetTreeListText(ProcessTreeListHandle, PHPRTLC_MAXIMUM);
    PhSetClipboardStringEx(ProcessTreeListHandle, text->Buffer, text->Length);
    PhDereferenceObject(text);
}

VOID PhWriteProcessTree(
    __inout PPH_FILE_STREAM FileStream,
    __in ULONG Mode
    )
{
    PPH_LIST lines;
    ULONG i;

    lines = PhGetProcessTreeListLines(
        ProcessTreeListHandle,
        ProcessNodeList->Count,
        ProcessNodeRootList,
        Mode
        );

    for (i = 0; i < lines->Count; i++)
    {
        PPH_STRING line;

        line = lines->Items[i];
        PhWriteStringAsAnsiFileStream(FileStream, &line->sr);
        PhDereferenceObject(line);
        PhWriteStringAsAnsiFileStream2(FileStream, L"\r\n");
    }

    PhDereferenceObject(lines);
}
