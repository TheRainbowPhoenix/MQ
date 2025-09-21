#include "gui.h"
#include "imgui-util.h"
#include <stdio.h>
#include <mq/modules/mmu.h>
#include <mq/modules/intc.h>

static void AddChunkList(
    mqMemory const *omem, MemoryWindowState &s, MemoryWindowAction &a)
{
    char addr[16];
    int totalChunks = 0;
    (void)a;

    ImVec2 avl = ImGui::GetContentRegionAvail();
    avl.x -= 4;
    avl.y -= ImGui::GetTextLineHeightWithSpacing();
    if(!ImGui::BeginListBox("##memory-chunks", avl))
        return;

    ImGui::PushFont(fontMono);

    for(uint i = 0; i < 0x1000; i++) {
        sprintf(addr, "%08x", i << 20);
        if(omem->chunks[i] == MQ_CHUNKPTR_NULL)
            continue;

        ImGui::SetNextItemAllowOverlap();
        bool clicked = ImGui::Selectable(addr, (int)i == s.selectedChunk);
        if(MQ_CHUNKPTR_ISBUFFER(omem->chunks[i])) {
            ImGui::SameLine(0, 7);
            ImGui::PushFont(fontSans);
            // TODO: Get buffer name!
            ImGui::TextDisabled("(%s)", "Buffer");
            ImGui::PopFont();
        }

        if(clicked && s.selectedChunk != (int)i) {
            s.selectedChunk = i;
            s.selectedPage = -1;
            s.selectedIO = -1;

            /* Autoselect page #0 if there is exactly one non-null page */
            mqChunk *ch = MQ_CHUNKPTR_GET(omem->chunks[i]);
            if(ch) {
                int nonnullPages = 0;
                int firstPage = -1;
                for(int i = 0; i < 256 && nonnullPages < 2; i++) {
                    if(ch->pages[i] != MQ_PAGEPTR_NULL) {
                        nonnullPages++;
                        firstPage = i;
                    }
                }
                if(nonnullPages == 1)
                    s.selectedPage = firstPage;
            }
        }

        totalChunks++;
    }

    ImGui::PopFont();
    ImGui::EndListBox();
    ImGui::Text("Total: %d chunks", totalChunks);
}

static void AddPageList(MemoryWindowState &s, u32 chunkBase,
    mqChunkPointer chunkPtr, MemoryWindowAction &a)
{
    (void)a;
    if(chunkPtr == MQ_CHUNKPTR_NULL)
        return;
    if(MQ_CHUNKPTR_ISBUFFER(chunkPtr)) {
        ImGui::Text("This is a buffer chunk.");
        return;
    }
    mqChunk *chunk = MQ_CHUNKPTR_GET(chunkPtr);

    ImVec2 avl = ImGui::GetContentRegionAvail();
    avl.x -= 4;
    avl.y -= ImGui::GetTextLineHeightWithSpacing();
    if(!ImGui::BeginListBox("##memory-pages", avl))
        return;

    ImGui::PushFont(fontMono);

    char addr[16];
    int totalPages = 0;

    for(uint i = 0; i < 256; i++) {
        sprintf(addr, "%08x", chunkBase + (i << 12));
        if(chunk->pages[i] == MQ_PAGEPTR_NULL)
            continue;

        ImGui::SetNextItemAllowOverlap();
        bool clicked = ImGui::Selectable(addr, (int)i == s.selectedPage);
        ImGui::SameLine(0, 7);
        ImGui::PushFont(fontSans);
        if(MQ_PAGEPTR_ISBUFFER(chunk->pages[i]))
            ImGui::TextDisabled("(Buffer)");
        else
            ImGui::TextDisabled("(IO)");
        ImGui::PopFont();

        if(clicked && s.selectedPage != (int)i) {
            s.selectedPage = i;
            s.selectedIO = -1;
        }

        totalPages++;
    }

    ImGui::PopFont();
    ImGui::EndListBox();
    ImGui::Text("Total: %d pages", totalPages);
}

static void AddMMIOList(MemoryWindowState &s, u32 addr, mqPagePointer pagePtr,
    MemoryWindowAction &a)
{
    if(pagePtr == MQ_PAGEPTR_NULL)
        return;
    if(MQ_PAGEPTR_ISBUFFER(pagePtr)) {
        ImGui::Text("This is a buffer page.");
        return;
    }
    mqPage *pg = MQ_PAGEPTR_GET(pagePtr);

    ImVec2 avl = ImGui::GetContentRegionAvail();
    avl.x -= 4;
    avl.y -= ImGui::GetTextLineHeightWithSpacing();
    // TODO: More clearly split list and IO details
    avl.y = (avl.y * 2) / 3;
    if(!ImGui::BeginListBox("##memory-ios", avl))
        return;

    ImGui::PushFont(fontMono);

    char str[16];
    int total = 0;

    for(int i = 0; i < pg->length; i++) {
        sprintf(str, "%08x", addr + i);
        if(!pg->map[i])
            continue;

        mqMMIO *io = &pg->io[pg->map[i] - 1];
        total++;

        ImGui::SetNextItemAllowOverlap();
        bool clicked = ImGui::Selectable(str, i == s.selectedIO);
        if(io->name) {
            ImGui::SameLine(0, 7);
            ImGui::TextDisabled("%s", io->name);
        }

        if(clicked)
            s.selectedIO = i;
    }

    ImGui::PopFont();
    ImGui::EndListBox();
    ImGui::Text("Total: %d IOs", total);

    if(s.selectedIO >= 0) {
        mqMMIO *io = &pg->io[pg->map[s.selectedIO] - 1];
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::PushFont(fontMono);
        ImGui::TextUnformatted(io->name);
        ImGui::PopFont();
    }

    (void)a;
}

MemoryWindowAction AddMemoryWindowContents(
    mqMachine *omach, MemoryWindowState &s)
{
    mqMemory const *omem = omach->memory;
    MemoryWindowAction a;

    mqChunkPointer chunkPtr = MQ_CHUNKPTR_NULL;
    mqChunk *chunk = NULL;
    mqPagePointer pagePtr = MQ_PAGEPTR_NULL;

    //=== Reset selection if invalid after a change ===//

    if(s.selectedChunk >= 0) {
        chunkPtr = omem->chunks[s.selectedChunk];
        if(chunkPtr == MQ_CHUNKPTR_NULL)
            s.selectedChunk = -1;
        else if(!MQ_CHUNKPTR_ISBUFFER(chunkPtr))
            chunk = MQ_CHUNKPTR_GET(chunkPtr);
    }

    if(s.selectedPage >= 0) {
        if(chunk) {
            pagePtr = chunk->pages[s.selectedPage];
            if(pagePtr == MQ_PAGEPTR_NULL)
                s.selectedPage = -1;
        }
        else s.selectedPage = -1;
    }

    //=== General memory statistics ===//

    // TODO: Avoid recomputation of memory stats every frame?!
    struct mqMemory_Stats stats = mq_memory_stats(omem);

    ImGui::Text("1 MB Chunks: %d (%d buffer, %d mixed including %d pure MMIO)",
                stats.totalChunks, stats.bufferChunks, stats.detailedChunks,
                stats.pureMMIOChunks);
    ImGui::Text("4 kB Pages: %d mapped", stats.bufferPages);

    //=== List of chunks and pages ===//

    if(ImGui::BeginTable("memtable", 3, ImGuiTableFlags_SizingStretchSame)) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();

        ImGui::SeparatorTextD("Chunks");
        AddChunkList(omem, s, a);
        ImGui::TableNextColumn();

        ImGui::SeparatorTextD("Pages");
        if(s.selectedChunk >= 0)
            AddPageList(s, s.selectedChunk << 20, chunkPtr, a);
        ImGui::TableNextColumn();

        ImGui::SeparatorTextD("MMIO");
        if(s.selectedPage >= 0)
            AddMMIOList(s, (s.selectedChunk << 20) + (s.selectedPage << 12),
                pagePtr, a);

        ImGui::EndTable();
    }

    return a;
}

MemoryWindowAction AddMemoryWindow(mqMachine *omach, MemoryWindowState &state)
{
    MemoryWindowAction a;

    if(ImGui::Begin("Memory tree", nullptr,
        ImGuiWindowFlags_HorizontalScrollbar))
        a = AddMemoryWindowContents(omach, state);
    ImGui::End();

    return a;
}

MemoryBuffersWindowAction AddMemoryBuffersWindowContents(
    mqMachine *omach, MemoryBuffersWindowState &state)
{
    MemoryBuffersWindowAction a;
    mqMemory *omem = omach->memory;

    uint totalSize = 0;
    for(int i = 0; i < omem->bufferCount; i++)
        totalSize += omem->buffers[i].size;

    ImGui::Text("%d buffers (total size %.1f MB)",
            omem->bufferCount, (float)totalSize / 1e6);

    ImGui::BeginTable("membuffers", 3,
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg);
    ImGui::TableSetupColumn("Address");
    ImGui::TableSetupColumn("Size");
    ImGui::TableSetupColumn("Origin");
    ImGui::TableHeadersRow();

    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    if(ImGui::Selectable("Virtual address space", state.selectedBuffer == 0,
            ImGuiSelectableFlags_SpanAllColumns |
            ImGuiSelectableFlags_AllowOverlap)) {
        state.selectedBuffer = 0;
        a.type = MemoryBuffersWindowAction::Type::MBWA_VIEW_HEX;
        a.buffer = NULL;
        a.offset = 0;
        a.size = 0xffffffff;
        a.address = 0;
    }
    ImGui::TableNextColumn();
    ImGui::Text("4 GiB");
    ImGui::TableNextColumn();
    ImGui::Text("Emulator logic");

    u32 previousBlockEnd = -1;
    u32 blockAddress, blockSize;
    void *blockStorage;
    u32 startAddress = 0;
    mqMemoryBuffer *blockBuffer;
    int blockOffset = 0;
    bool blockFits = true;
    char str[64];
    int i = 0;

    while(mq_memory_findBlock(omem, startAddress, &blockAddress,
        &blockSize, &blockStorage)) {
        blockBuffer = mq_memory_getBufferOwning(omem, blockStorage);
        if(blockBuffer) {
            blockOffset = (char *)blockStorage - (char *)blockBuffer->data;
            blockFits = blockOffset + blockSize <= blockBuffer->size;
        }

        if(blockAddress == previousBlockEnd) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextError("Non-contiguity violation!");
        }

        ImGui::TableNextRow();
        ImGui::TableNextColumn();

        sprintf(str, "%08x", blockAddress);
        ImGui::PushFont(fontMono);
        if(ImGui::Selectable(str, state.selectedBuffer == i + 1,
                ImGuiSelectableFlags_SpanAllColumns |
                ImGuiSelectableFlags_AllowOverlap)) {
            state.selectedBuffer = i + 1;
            if(blockBuffer) {
                a.type = MemoryBuffersWindowAction::Type::MBWA_VIEW_HEX;
                a.buffer = blockBuffer;
                a.offset = blockOffset;
                a.size = blockSize;
                a.address = blockAddress;
            }
        }
        ImGui::PopFont();
        ImGui::TableNextColumn();

        std::string sizeStr = memorySizeString(blockSize);
        ImGui::Text(sizeStr.c_str());
        ImGui::TableNextColumn();

        if(blockBuffer) {
            ImGui::TextMono("%s", blockBuffer->name);
            if(blockOffset || blockSize != blockBuffer->size) {
                ImGui::SameLine(0, 0);
                ImGui::Text(" [%s, %s)",
                    memorySizeString(blockOffset).c_str(),
                    memorySizeString(blockOffset + blockSize).c_str());
            }
            if(!blockFits) {
                ImGui::SameLine(0, 0);
                ImGui::TextError(" Overflow");
            }
        }
        else {
            ImGui::TextErrorMono("%p", blockStorage);
        }

        if(__builtin_add_overflow(blockAddress, blockSize, &startAddress))
            break;
        previousBlockEnd = startAddress;
        i++;
    }
    ImGui::EndTable();

    return a;
}

MemoryBuffersWindowAction AddMemoryBuffersWindow(
    mqMachine *omach, MemoryBuffersWindowState &state)
{
    MemoryBuffersWindowAction a;

    if(ImGui::Begin("Memory buffers"))
        a = AddMemoryBuffersWindowContents(omach, state);
    ImGui::End();

    return a;
}

MMUWindowAction AddMMUWindow(mqMachine *mach)
{
    MMUWindowAction a;

    if(ImGui::Begin("MMU"))
        a = AddMMUWindowContents(mach);
    ImGui::End();

    return a;
}

MMUWindowAction AddMMUWindowContents(mqMachine *mach)
{
    MMUWindowAction a;

    mqMMU *MMU = mq_mmu_get(mach);
    if(!MMU) {
        ImGui::Text("Machine does not have an MMU module.");
        return a;
    }

    if(ImGui::Button("Bind"))
        a.type = MMUWindowAction::Type::MMUWA_BIND;
    ImGui::SameLine();
    if(ImGui::Button("Unbind"))
        a.type = MMUWindowAction::Type::MMUWA_UNBIND;

    if(ImGui::BeginTable("UTLB", 13,
            ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter |
            ImGuiTableFlags_ScrollY)) {
        ImGui::TableSetupColumn("id", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("VPN", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("D", ImGuiTableColumnFlags_WidthFixed, 10);
        ImGui::TableSetupColumn("V", ImGuiTableColumnFlags_WidthFixed, 10);
        ImGui::TableSetupColumn("ASID", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("PPN", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("V", ImGuiTableColumnFlags_WidthFixed, 10);
        ImGui::TableSetupColumn("SZ", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("PR", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("C", ImGuiTableColumnFlags_WidthFixed, 10);
        ImGui::TableSetupColumn("D", ImGuiTableColumnFlags_WidthFixed, 10);
        ImGui::TableSetupColumn("SH", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("WT", ImGuiTableColumnFlags_WidthFixed);
        ImGui::PushFont(fontMono);
        ImGui::TableHeadersRow();
        ImGui::PopFont();

        char const *SZ_str[4] = { "1 kB", "4 kB", "64 kB", "1 MB" };
        char const *PR_str[4] = { "K:r", "K:rw", "U:r", "U:rw" };

        for(int i = 0; i < 64; i++) {
            auto addr = mq_mmu_decode_address(MMU->UTLB[i].addr);
            auto data = mq_mmu_decode_data(MMU->UTLB[i].data);
            int SZ = (data.SZ1 << 1) + data.SZ0;

            auto color = ImGui::GetStyle().Colors[ImGuiCol_Text];
            if(addr.V && data.V)
                {}
            else if(!addr.V && !data.V)
                color = ImGui::GetStyle().Colors[ImGuiCol_TextDisabled];
            else
                color = ImGui::GetStyle().Colors[ImGuiCol_PlotLinesHovered];
            ImGui::PushStyleColor(ImGuiCol_Text, color);

            ImGui::TableNextRow();

            ImGui::TableNextColumn();
            ImGui::Text("%d", i);

            ImGui::TableNextColumn();
            ImGui::PushFont(fontMono);
            ImGui::Text("%08x", addr.VPN << 10);
            ImGui::PopFont();

            ImGui::TableNextColumn();
            ImGui::Text("%d", addr.D);

            ImGui::TableNextColumn();
            ImGui::Text("%d", addr.V);

            ImGui::TableNextColumn();
            ImGui::Text("%d", addr.ASID);

            ImGui::TableNextColumn();
            ImGui::PushFont(fontMono);
            ImGui::Text("%08x", data.PPN << 10);
            ImGui::PopFont();

            ImGui::TableNextColumn();
            ImGui::Text("%d", data.V);

            ImGui::TableNextColumn();
            if(SZ >= 0 && SZ < 4)
                ImGui::Text("%s", SZ_str[SZ]);
            else
                ImGui::Text("%d", SZ);

            ImGui::TableNextColumn();
            if(data.PR >= 0 && data.PR < 4)
                ImGui::Text("%s", PR_str[data.PR]);
            else
                ImGui::Text("%d", data.PR);

            ImGui::TableNextColumn();
            ImGui::Text("%d", data.C);

            ImGui::TableNextColumn();
            ImGui::Text("%d", data.D);

            ImGui::TableNextColumn();
            ImGui::Text(data.SH ? "SH" : "NS");

            ImGui::TableNextColumn();
            ImGui::Text(data.WT ? "WT" : "CB");

            ImGui::PopStyleColor();
        }

        ImGui::EndTable();
    }

    return a;
}

void AddInterruptsWindow(mqMachine *mach)
{
    if(ImGui::Begin("Interrupts"))
        AddInterruptsWindowContents(mach);
    ImGui::End();
}

void AddInterruptsWindowContents(mqMachine *mach)
{
    mqCpu *cpu = &mach->cpu;
    int IMASK = (cpu->spRegs[SH_SR] >> 4) & 0xf;
    int INTMU = (cpu->CPUOPM >> 3) & 1;

    mqINTC *INTC = mq_intc_get(mach);
    mqMMU *MMU = mq_mmu_get(mach);

    char const *INTEVT_name = "";
    if(cpu->INTEVT) {
        mqInt intID = mq_intc_interruptForEventCode(cpu->INTEVT);
        INTEVT_name = mq_intc_interruptName(intID);
    }
    char const *EXPEVT_name = "";
    if(cpu->EXPEVT) {
        int excID = mq_cpu_exceptionForEventCode(cpu->EXPEVT);
        EXPEVT_name = mq_cpu_exceptionName(excID);
    }

    ImGui::TextMono("IMASK:");
    ImGui::SameLine(0, 0);
    ImGui::Text(" %d (%s)", IMASK,
        INTMU ? "updated when accepting interrupts"
              : "unchanged when accepting interrupts");
    ImGui::TextMono("EXPEVT: 0x%03x", cpu->EXPEVT);
    ImGui::SameLine(0, 0);
    ImGui::Text(" %s", EXPEVT_name);
    ImGui::SameLine();
    ImGui::TextMono("TRA: %08x", cpu->TRA);
    if(MMU) {
        ImGui::SameLine();
        ImGui::TextMono("TEA: %08x", MMU->TEA);
    }
    ImGui::TextMono("INTEVT: 0x%03x", cpu->INTEVT);
    ImGui::SameLine(0, 0);
    ImGui::Text(" %s (%d)", INTEVT_name, cpu->INTPRIO);

    static bool showRaw = false;
    static bool hideMasked = false;
    ImGui::Checkbox2("Show raw", &showRaw);
    ImGui::SameLine();
    ImGui::Checkbox2("Hide masked/disabled", &hideMasked);

    if(showRaw) {
        ImGui::PushFont(fontMono);
        ImGui::Text("IMR");
        for(int i = 0; INTC && i < 13; i++) {
            ImGui::SameLine(0, 0);
            ImGui::Text(" %02x", INTC->IMR[i]);
        }
        ImGui::Text("IPR");
        for(int i = 0; INTC && i < 12; i++) {
            ImGui::SameLine(0, 0);
            ImGui::Text(" %04x", INTC->IPR[i]);
        }
        ImGui::PopFont();
    }

    ImVec2 size = ImGui::GetContentRegionAvail();
    ImVec2 halfSize(size.x / 2 - 5, size.y);

    if(ImGui::BeginTable("Interrupts", 5,
            ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter |
            ImGuiTableFlags_ScrollY,
            halfSize)) {
        ImGui::TableSetupColumn("Code", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Prio.", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Count", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableHeadersRow();

        for(int i = 0; i < MQ_INT_NUM; i++) {
            mqINTC_InterruptInfo const *info = mq_intc_interruptInfo((mqInt)i);
            bool masked =
                INTC ? mq_intc_isInterruptMasked(INTC, (mqInt)i) : false;
            bool raised = INTC ? INTC->interruptStatus[i] : false;
            int priority = INTC ? mq_intc_interruptPriority(INTC, (mqInt)i) : 0;
            int count = INTC ? INTC->statsInterruptCount[i] : 0;

            if(hideMasked && (masked || !priority))
                continue;

            auto color = ImGui::GetStyle().Colors[ImGuiCol_Text];
            char const *status = "-";
            if(masked) {
                color = ImGui::GetStyle().Colors[ImGuiCol_TextDisabled];
                status = "Masked";
            }
            else if(priority == 0) {
                color = ImGui::GetStyle().Colors[ImGuiCol_TextDisabled];
                status = "Disabled";
            }
            else if(raised) {
                color = ImGui::GetStyle().Colors[ImGuiCol_PlotLinesHovered];
                status = "Raised";
            }
            ImGui::PushStyleColor(ImGuiCol_Text, color);

            ImGui::TableNextRow();

            ImGui::TableNextColumn();
            ImGui::TextMono("0x%03x", info->INTEVT);

            char const *name = mq_intc_interruptName((mqInt)i);
            ImGui::TableNextColumn();
            ImGui::Text("%s", name ? name : "(null)");

            ImGui::TableNextColumn();
            ImGui::Text("%d", priority);

            ImGui::TableNextColumn();
            ImGui::Text("%s", status);

            ImGui::TableNextColumn();
            ImGui::Text("%d", count);

            ImGui::PopStyleColor();
        }

        ImGui::EndTable();
    }
    ImGui::SameLine();
    if(ImGui::BeginTable("Exceptions", 3,
            ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter |
            ImGuiTableFlags_ScrollY,
            halfSize)) {
        ImGui::TableSetupColumn("Code", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableHeadersRow();

        for(int i = 0; i < SH_NUM_EXCEPTIONS; i++) {
            bool raised = (cpu->excMask & (1 << i)) != 0;

            auto color = ImGui::GetStyle().Colors[ImGuiCol_Text];
            if(raised)
                color = ImGui::GetStyle().Colors[ImGuiCol_PlotLinesHovered];
            ImGui::PushStyleColor(ImGuiCol_Text, color);

            ImGui::TableNextRow();

            ImGui::TableNextColumn();
            ImGui::PushFont(fontMono);
            ImGui::Text("0x000");
            ImGui::PopFont();

            char const *name = mq_cpu_exceptionName(i);
            ImGui::TableNextColumn();
            ImGui::Text("%s", name ? name : "(null)");

            ImGui::TableNextColumn();
            ImGui::Text(raised ? "raised" : "-");

            ImGui::PopStyleColor();
        }

        ImGui::EndTable();
    }
}

HexViewerWindowAction AddHexViewerWindowContents(
    mqMachine *omach, HexViewerWindowState &state, ImGui::HexViewer &HV)
{
    HexViewerWindowAction a;

    /* Check whether the current buffer exists */
    mqMemory *omem = omach->memory;
    mqMemoryBuffer const *currentBuffer = NULL;

    if(state.currentBufferName != "") {
        currentBuffer =
            mq_memory_getBuffer(omem, state.currentBufferName.c_str());
        /* Machine changed, selection doesn't exist anymore. */
        if(!currentBuffer)
            state.currentBufferName = "";
    }

    if(currentBuffer) {
        ImGui::TextMono("%s", currentBuffer->name);
        ImGui::SameLine(0, 0);
        if(state.currentBufferOffset != 0 ||
           state.currentBufferSize != (int)currentBuffer->size) {
            std::string startStr = memorySizeString(
                state.currentBufferOffset);
            std::string endStr = memorySizeString(
                state.currentBufferOffset + state.currentBufferSize);
            ImGui::Text(" section [%s, %s)",
                startStr.c_str(), endStr.c_str());
        }
        else {
            std::string sizeStr = memorySizeString(state.currentBufferSize);
            ImGui::Text(" (%s)", sizeStr.c_str());
        }
    }
    else {
        ImGui::Text("Virtual address space (4 GiB)");
    }

    u64 Min = HV.MinCursor();
    u64 Max = HV.MaxCursor();
    if(Max > Min) {
        float progress = (float)(HV.Cursor - Min) / (Max - Min);
        char str[64];
        sprintf(str, "%.2f%%", 100 * progress);
        ImGui::SameLine(ImGui::GetContentRegionAvail().x -
                        ImGui::CalcTextSize(str).x);
        ImGui::TextUnformatted(str);
    }

    ImGui::PushFont(fontMono);
    ImGui::AddHexViewer(HV);
    ImGui::PopFont();

    return a;
}

HexViewerWindowAction AddHexViewerWindow(
    mqMachine *mach, HexViewerWindowState &state, ImGui::HexViewer &HV)
{
    HexViewerWindowAction a;

    if(ImGui::Begin("Hex Viewer"))
        a = AddHexViewerWindowContents(mach, state, HV);
    ImGui::End();

    return a;
}
