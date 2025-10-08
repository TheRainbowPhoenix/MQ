#include "gui.h"
#include "imgui-util.h"
#include <imgui_internal.h>
#include <mq/modules/mmu.h>
#include <mq/modules/intc.h>
#include <mq/system/heap.h>
#include <stdio.h>

void GUIWindow::render(mqMachine *omach)
{
    char title[256];
    snprintf(title, sizeof title, "%s##%s.%d", m_title, m_title, m_instanceId);

    if(ImGui::Begin(title, 0, m_flags))
        renderContents(omach);
    ImGui::End();
}

void GUI::Render(mqMachine *omach)
{
    bool open = ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_O, ImGuiInputFlags_RouteGlobal);
    actions.appQuit |= ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Q,
        ImGuiInputFlags_RouteGlobal);

    if(ImGui::BeginCustomMenuBar()) {
        if(ImGui::BeginCustomMenuChild("##menutitle", {30,0}, {1,4})) {
            ImGui::MoveCursorScreenPos({0, 3});
            ImGui::PushFont(fontBold);
            ImGui::Text("MQ");
            ImGui::PopFont();
        }
        ImGui::EndCustomMenuChild();

        if(ImGui::BeginCustomMenu("File")) {
            open |= ImGui::MenuItem("Open add-in...", "Ctrl+O");
            actions.appQuit |= ImGui::MenuItem("Quit", "Ctrl+Q");
        }
        ImGui::EndCustomMenu();
        if(ImGui::BeginCustomMenu("Machine")) {
            if(ImGui::MenuItem("Reset to blank FX add-in"))
                actions.machineInitialize = MQ_MACHINE_INITIALIZE_ADDIN_FX;
            if(ImGui::MenuItem("Reset to blank CG add-in"))
                actions.machineInitialize = MQ_MACHINE_INITIALIZE_ADDIN_CG;
            actions.machineGenerateMonoFrame |=
                ImGui::MenuItem("Generate B&W frame");
            actions.machineGenerateRGBFrame |=
                ImGui::MenuItem("Generate RGB frame");
        }
        ImGui::EndCustomMenu();

        if(ImGui::BeginCustomMenuChild("##menutools", {0,0}, {1,4})) {
            ImGui::CustomMenuSeparator();
            ImGui::SameLine(0, 6);

            ImGui::BeginDisabled(!omach->initialized);
            bool paused = omach->cyclesPending == 0;
            bool stuck = omach->stuck;

            if(paused && ImGui::IconButton(0, "Run"))
                actions.machineSetPendingCycles = -1;
            if(!paused && ImGui::IconButton(1, "Pause"))
                actions.machineSetPendingCycles = 0;
            ImGui::SameLine(0, 6);

            if(ImGui::IconButton(2, "Step", !paused))
                actions.machineSetPendingCycles = 1;
            ImGui::SameLine(0, 6);
            ImGui::EndDisabled();

            ImGui::MoveCursorScreenPos({0, 3});
            if(!omach->initialized)
                ImGui::TextDisabled("Not initialized");
            else if(stuck)
                ImGui::TextColored({1,.3,.3,1}, "Stuck!");
            else
                ImGui::Text(paused ? "Paused" : "Running...");
        }
        ImGui::EndCustomMenuChild();
    }
    ImGui::EndCustomMenuBar();

    /* On native builds tihs fills inputFile instantly, while on emscripten
       this fills it asynchronously and we'll get it in a future frame */
    if(open)
        openFileDialog(&inputFile);

    auto dock = ImGui::DockSpaceOverViewport();

    Windows.Display->render(omach);
    Windows.Keyboard->render(omach);
    Windows.Control->render(omach);
    Windows.Messages->render(omach);
    Windows.CPU->render(omach);
    Windows.Interrupts->render(omach);
    Windows.MemoryTree->render(omach);
    Windows.MemoryBuffers->render(omach);
    Windows.Heap->render(omach);
    Windows.MMU->render(omach);
    Windows.HexViewer->render(omach);
    Windows.Record->render(omach);

    static bool first_frame = true;
    if(first_frame)
        gui.DockWindowsStyle1(gui.Windows, dock);
    first_frame = false;

    if(Windows.Control->showDemoWindow())
        ImGui::ShowDemoWindow();
}
void GUI::DockWindowsStyle1(GUIWindowSet const &Windows, ImGuiID dock)
{
    auto dock_left_top = ImGui::DockBuilderSplitNode(dock,
        ImGuiDir_Left, 0.70, nullptr, &dock);
    auto dock_left_bottom = ImGui::DockBuilderSplitNode(dock_left_top,
        ImGuiDir_Down, 0.5f, nullptr, &dock_left_top);
    auto dock_left_top_right = ImGui::DockBuilderSplitNode(dock_left_top,
        ImGuiDir_Right, 0.65f, nullptr, &dock_left_top);
    auto dock_left_bottom_right = ImGui::DockBuilderSplitNode(
        dock_left_bottom,
        ImGuiDir_Right, 0.48f, nullptr, &dock_left_bottom);
    auto dock_right_bottom = ImGui::DockBuilderSplitNode(dock,
        ImGuiDir_Down, 0.6f, nullptr, &dock);

    auto DB = [&](GUIWindow &w, auto &dock){
        ImGui::DockBuilderDockWindow(
            (std::string(w.title()) + "##" +
             std::string(w.title()) + ".-1").c_str(),
            dock);
    };
    DB(*Windows.Display, dock);
    DB(*Windows.Keyboard, dock_right_bottom);
    DB(*Windows.Control, dock_left_top);
    DB(*Windows.Record, dock_left_top);
    DB(*Windows.Messages, dock_left_top_right);
    DB(*Windows.CPU, dock_left_top_right);
    DB(*Windows.Interrupts, dock_left_top_right);
    DB(*Windows.MemoryTree, dock_left_bottom);
    DB(*Windows.MemoryBuffers, dock_left_bottom);
    DB(*Windows.Heap, dock_left_bottom);
    DB(*Windows.MMU, dock_left_bottom);
    DB(*Windows.HexViewer, dock_left_bottom_right);
    ImGui::DockBuilderFinish(dock);
}

void ControlWindow::renderContents(mqMachine *omach)
{
#ifndef AZUR_PLATFORM_EMSCRIPTEN
    struct mallinfo2 mi = mallinfo2();
    /* This info is not available with AddressSanitizer's wrapper's */
    if(mi.arena || mi.hblkhd)
        ImGui::Text("Memory allocated: %.1f MB heap + %.1f MB mmap\n",
            (float)mi.arena / 1e6, (float)mi.hblkhd / 1e6);

    // TODO: Better alternative on Linux:
    // 1. Open /proc/self/status
    // 2. Parse for VmRSS (Resident Set Size) and VmSwap (in swap)
    // 3. VmRSS is divided in RssAnon (± heap), RssFile (fixed), RssShmem
    // 4. For users, we're interested in VmRSS (+ VmSwap)
    // 5. For debugging, we're interested in RssAnon (≈ mi.arena)
    // Reference:
    //   top(1), "Linux Memory Types"
    // Or, for the proper programmatic interface:
    // 1. Open /proc/self/statm
    // 2. Read all numbers, multiplied by sysconf(_SC_PAGESIZE)
    // 3. [size, resident, shared, text, _, data/stack, _]
    //    * size is VmSize -> useless
    //    * shared won't be used
    //    * text is fixed
    //    * data/stack counts unmapped, non-resident pages -> useless
    // 4. Keep using mallinfo() for memory stats
    // Reference:
    //   proc_pid_statm(5)
#endif

    if(omach->initialized) {
        ImGui::Text("Cycle:");
        ImGui::SameLine();
        if(ImGui::Button("1"))
            gui.actions.machineSetPendingCycles = 1;
        ImGui::SameLine();
        if(ImGui::Button("10"))
            gui.actions.machineSetPendingCycles = 10;
        ImGui::SameLine();
        if(ImGui::Button("100"))
            gui.actions.machineSetPendingCycles = 100;
        ImGui::SameLine();
        if(ImGui::Button("1000"))
            gui.actions.machineSetPendingCycles = 1000;
        ImGui::SameLine();
        if(ImGui::Button("10k"))
            gui.actions.machineSetPendingCycles = 10000;
    }
    else {
        ImGui::Text("Machine is not initialized.");
    }
    if(omach->stuck)
        ImGui::Text("Machine is stuck!");

    if(omach->cyclesPending > 0)
        ImGui::Text("Cycles pending: %d", omach->cyclesPending);
    else if(omach->cyclesPending == 0)
        ImGui::Text("Paused");
    else
        ImGui::Text("Running...");

    char str[256];
    bool disabled = gui.current_program_path.empty();
    char const *path = gui.current_program_path.c_str();

    if(disabled)
        ImGui::BeginDisabled();

    if(gui.watch_enabled)
        snprintf(str, sizeof str, "Watching input file: %s", path);
    else if(!disabled)
        snprintf(str, sizeof str, "Watch input file (%s)", path);
    else
        snprintf(str, sizeof str, "Watch input file");

    if(ImGui::Checkbox2(str, &gui.watch_enabled))
        gui.actions.fileUpdateWatch = gui.watch_enabled;
    if(gui.watch_info.fd >= 0) {
        ImGui::SameLine(0, 0);
        ImGui::TextDisabled(" (%d.%d)",
            gui.watch_info.fd, gui.watch_info.wd);
    }
    if(disabled)
        ImGui::EndDisabled();

    if(gui.workingFolderAddins.size() == 0)
        ImGui::Text("(No add-ins in working folder)");
    else
        ImGui::Text("Reset and load:");

    int spaceLeft = 0;
    for(uint i = 0; i < gui.workingFolderAddins.size(); i++) {
        char const *addin = gui.workingFolderAddins[i].c_str();
        /* Check if we have enough space (32 for button + spacing) */
        int spaceNeeded = ImGui::CalcTextSize(addin).x + 32;
        if(spaceLeft < spaceNeeded)
            spaceLeft = ImGui::GetContentRegionAvail().x;
        else
            ImGui::SameLine();

        if(ImGui::Button(addin))
            gui.actions.fileLoadPath = gui.workingFolderAddins[i];
        spaceLeft -= spaceNeeded;
    }

    ImGui::Checkbox2("Show demo window", &m_showDemoWindow);
}

void MessagesWindow::renderContents(mqMachine *omach)
{
    (void)omach;

    gui.actions.appClearConsole |= ImGui::Button("Clear");

    gui.ConsoleText.lock();
    ImGui::AddRichTextFrame(gui.ConsoleText, gui.ConsoleView);
    gui.ConsoleText.unlock();
}

void CPUWindow::renderContents(mqMachine *omach)
{
    ImGui::Text("Sleeping: %d", (int)omach->cpu.sleeping);
    ImGui::PushFont(fontMono);

    ImGui::BeginGroup();
    for(int i = 0; i < 16; i++)
        ImGui::Text("r%d:%s %08x", i, i < 10 ? " " : "", omach->cpu.r[i]);
    ImGui::EndGroup();

    ImGui::SameLine(0, 40);
    ImGui::BeginGroup();
    ImGui::Text("pc:    %08x", omach->cpu.pc);
    ImGui::Text("gbr:   %08x", omach->cpu.spRegs[SH_GBR]);
    ImGui::Text("mach:  %08x", omach->cpu.spRegs[SH_MACH]);
    ImGui::Text("macl:  %08x", omach->cpu.spRegs[SH_MACL]);
    ImGui::Text("pr:    %08x", omach->cpu.spRegs[SH_PR]);

    u32 SR = omach->cpu.spRegs[SH_SR];
    ImGui::Text("sr:    %08x", omach->cpu.spRegs[SH_SR]);
    ImGui::Text(" MD=%d RB=%d BL=%d",
        (SR >> 30) & 1, (SR >> 29 & 1), (SR >> 28) & 1);
    ImGui::Text(" IMASK=%d",
        (SR >> 4) & 0xf);
    ImGui::Text(" RC=%d",
        (SR >> 16) & 0xfff);
    ImGui::EndGroup();

    ImGui::SameLine(0, 40);
    ImGui::BeginGroup();
    ImGui::Text("vbr:     %08x", omach->cpu.spRegs[SH_VBR]);
    ImGui::Text("ssr:     %08x", omach->cpu.spRegs[SH_SSR]);
    ImGui::Text("spc:     %08x", omach->cpu.spRegs[SH_SPC]);
    ImGui::Text("sgr:     %08x", omach->cpu.spRegs[SH_SGR]);
    ImGui::Text("dbr:     %08x", omach->cpu.spRegs[SH_DBR]);
    ImGui::Text("dsr:     %08x", omach->cpu.spRegs[SH_DSR]);
    for(int i = 0; i < 8; i++)
        ImGui::Text("r%d_bank: %08x", i, omach->cpu.spRegs[SH_RnBANK + i]);
    // ImGui::Text();
    ImGui::EndGroup();

    ImGui::SameLine(0, 40);
    ImGui::BeginGroup();
    ImGui::Text("[DSP]");
    ImGui::Text("a0:  %02x.%08x",
        omach->cpu.spRegs[SH_A0G], omach->cpu.spRegs[SH_A0]);
    ImGui::Text("a1:  %02x.%08x",
        omach->cpu.spRegs[SH_A1G], omach->cpu.spRegs[SH_A1]);
    ImGui::Text("x0:     %08x", omach->cpu.spRegs[SH_X0]);
    ImGui::Text("x1:     %08x", omach->cpu.spRegs[SH_X1]);
    ImGui::Text("y0:     %08x", omach->cpu.spRegs[SH_Y0]);
    ImGui::Text("y1:     %08x", omach->cpu.spRegs[SH_Y1]);
    ImGui::Text("m0:     %08x", omach->cpu.spRegs[SH_M0]);
    ImGui::Text("m1:     %08x", omach->cpu.spRegs[SH_M1]);
    ImGui::Text("mod:    %08x", omach->cpu.spRegs[SH_MOD]);
    ImGui::Text("rs:     %08x", omach->cpu.spRegs[SH_RS]);
    ImGui::Text("re:     %08x", omach->cpu.spRegs[SH_RE]);
    // ImGui::Text();
    ImGui::EndGroup();

    ImGui::PopFont();
}

void MemoryTreeWindow::AddChunkList(mqMemory const *omem)
{
    char addr[16];
    int totalChunks = 0;

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
        bool clicked = ImGui::Selectable(addr, (int)i == m_selectedChunk);
        if(MQ_CHUNKPTR_ISBUFFER(omem->chunks[i])) {
            ImGui::SameLine(0, 7);
            ImGui::PushFont(fontSans);
            // TODO: Get buffer name!
            ImGui::TextDisabled("(%s)", "Buffer");
            ImGui::PopFont();
        }

        if(clicked && m_selectedChunk != (int)i) {
            m_selectedChunk = i;
            m_selectedPage = -1;
            m_selectedIO = -1;

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
                    m_selectedPage = firstPage;
            }
        }

        totalChunks++;
    }

    ImGui::PopFont();
    ImGui::EndListBox();
    ImGui::Text("Total: %d chunks", totalChunks);
}

void MemoryTreeWindow::AddPageList(u32 chunkBase, mqChunkPointer chunkPtr)
{
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
        bool clicked = ImGui::Selectable(addr, (int)i == m_selectedPage);
        ImGui::SameLine(0, 7);
        ImGui::PushFont(fontSans);
        if(MQ_PAGEPTR_ISBUFFER(chunk->pages[i]))
            ImGui::TextDisabled("(Buffer)");
        else
            ImGui::TextDisabled("(IO)");
        ImGui::PopFont();

        if(clicked && m_selectedPage != (int)i) {
            m_selectedPage = i;
            m_selectedIO = -1;
        }

        totalPages++;
    }

    ImGui::PopFont();
    ImGui::EndListBox();
    ImGui::Text("Total: %d pages", totalPages);
}

void MemoryTreeWindow::AddMMIOList(u32 addr, mqPagePointer pagePtr)
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
        bool clicked = ImGui::Selectable(str, i == m_selectedIO);
        if(io->name) {
            ImGui::SameLine(0, 7);
            ImGui::TextDisabled("%s", io->name);
        }

        if(clicked)
            m_selectedIO = i;
    }

    ImGui::PopFont();
    ImGui::EndListBox();
    ImGui::Text("Total: %d IOs", total);

    if(m_selectedIO >= 0) {
        mqMMIO *io = &pg->io[pg->map[m_selectedIO] - 1];
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::PushFont(fontMono);
        ImGui::TextUnformatted(io->name);
        ImGui::PopFont();
    }
}

void MemoryTreeWindow::renderContents(mqMachine *omach)
{
    mqMemory const *omem = omach->memory;

    mqChunkPointer chunkPtr = MQ_CHUNKPTR_NULL;
    mqChunk *chunk = NULL;
    mqPagePointer pagePtr = MQ_PAGEPTR_NULL;

    //=== Reset selection if invalid after a change ===//

    if(m_selectedChunk >= 0) {
        chunkPtr = omem->chunks[m_selectedChunk];
        if(chunkPtr == MQ_CHUNKPTR_NULL)
            m_selectedChunk = -1;
        else if(!MQ_CHUNKPTR_ISBUFFER(chunkPtr))
            chunk = MQ_CHUNKPTR_GET(chunkPtr);
    }

    if(m_selectedPage >= 0) {
        if(chunk) {
            pagePtr = chunk->pages[m_selectedPage];
            if(pagePtr == MQ_PAGEPTR_NULL)
                m_selectedPage = -1;
        }
        else m_selectedPage = -1;
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
        AddChunkList(omem);
        ImGui::TableNextColumn();

        ImGui::SeparatorTextD("Pages");
        if(m_selectedChunk >= 0)
            AddPageList(m_selectedChunk << 20, chunkPtr);
        ImGui::TableNextColumn();

        ImGui::SeparatorTextD("MMIO");
        if(m_selectedPage >= 0)
            AddMMIOList(
                (m_selectedChunk << 20) + (m_selectedPage << 12), pagePtr);

        ImGui::EndTable();
    }
}

void MemoryTreeWindow::resetState()
{
    m_selectedChunk = -1;
    m_selectedPage = -1;
    m_selectedIO = -1;
}

void MemoryBuffersWindow::renderContents(mqMachine *omach)
{
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
    if(ImGui::Selectable("Virtual address space", m_selectedBuffer == 0,
            ImGuiSelectableFlags_SpanAllColumns |
            ImGuiSelectableFlags_AllowOverlap)) {
        m_selectedBuffer = 0;
        gui.actions.setViewHex(NULL, 0, 0xffffffff, 0);
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
        if(ImGui::Selectable(str, m_selectedBuffer == i + 1,
                ImGuiSelectableFlags_SpanAllColumns |
                ImGuiSelectableFlags_AllowOverlap)) {
            m_selectedBuffer = i + 1;
            if(blockBuffer) {
                gui.actions.setViewHex(
                    blockBuffer, blockOffset, blockSize, blockAddress);
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
}

void MemoryBuffersWindow::resetState()
{
    m_selectedBuffer = -1;
}

void MMUWindow::renderContents(mqMachine *omach)
{
    mqMMU *MMU = mq_mmu_get(omach);
    if(!MMU) {
        ImGui::Text("Machine does not have an MMU module.");
        return;
    }

    gui.actions.machineMMUBind |= ImGui::Button("Bind");
    ImGui::SameLine();
    gui.actions.machineMMUUnbind |= ImGui::Button("Unbind");

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
}

void InterruptsWindow::renderContents(mqMachine *omach)
{
    mqCpu *cpu = &omach->cpu;
    int IMASK = (cpu->spRegs[SH_SR] >> 4) & 0xf;
    int INTMU = (cpu->CPUOPM >> 3) & 1;

    mqINTC *INTC = mq_intc_get(omach);
    mqMMU *MMU = mq_mmu_get(omach);

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

bool HexViewerWindow::ReadByte(u64 addr, u8 *result, void *userdata)
{
    mqMachine const *omach = (mqMachine const *)userdata;
    if(!omach || !omach->memory)
        return false;
    u32 v;
    bool b = mq_memory_read_pure(omach->memory, addr, 1, &v);
    if(b)
        *result = v;
    return b;
}

void HexViewerWindow::resetState()
{
    m_currentBufferName = "";
    m_currentBufferOffset = 0;
    m_currentBufferSize = 0;
}

void HexViewerWindow::renderContents(mqMachine *omach)
{
    ImGui::HexViewer &HV = m_HexViewer;
    HV.ReadByte = HexViewerWindow::ReadByte;
    HV.ReadByteUserdata = omach;

    /* Check whether the current buffer exists */
    mqMemory *omem = omach->memory;
    mqMemoryBuffer const *currentBuffer = NULL;

    if(m_currentBufferName != "") {
        currentBuffer =
            mq_memory_getBuffer(omem, m_currentBufferName.c_str());
        /* Machine changed, selection doesn't exist anymore. */
        if(!currentBuffer)
            m_currentBufferName = "";
    }

    if(currentBuffer) {
        ImGui::TextMono("%s", currentBuffer->name);
        ImGui::SameLine(0, 0);
        if(m_currentBufferOffset != 0 ||
           m_currentBufferSize != (int)currentBuffer->size) {
            std::string startStr = memorySizeString(
                m_currentBufferOffset);
            std::string endStr = memorySizeString(
                m_currentBufferOffset + m_currentBufferSize);
            ImGui::Text(" section [%s, %s)",
                startStr.c_str(), endStr.c_str());
        }
        else {
            std::string sizeStr = memorySizeString(m_currentBufferSize);
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
}

void HexViewerWindow::viewBuffer(
    std::string bufferName, u32 address, u32 offset, u32 size)
{
    m_HexViewer.Cursor = address;
    m_HexViewer.MinAddress = address;
    m_HexViewer.MaxAddress = address + (size - 1);
    m_currentBufferName = bufferName;
    m_currentBufferOffset = offset;
    m_currentBufferSize = size;
}

void HeapWindow::renderContents(mqMachine *omach)
{
    (void)omach;

    // TODO: Heap should be attached to Casiowin module, not a global!
    u32 heapStart, heapEnd;
    bool initialized = mq_heap_isInitialized(&heapStart, &heapEnd);
    if(initialized) {
        ImGui::Text("Heap from %08x to %08x", heapStart, heapEnd);

        mq_heap_debug_t dbg = mq_heap_debuginfo();
        #define X(NAME) \
            ImGui::Text(#NAME ":"); \
            ImGui::SameLine(); \
            ImGui::Text(dbg.NAME ? "true" : "false");
        X(sequence_covers)
        X(sequence_terminator)
        X(sequence_coherent_used)
        X(sequence_footer_size)
        X(sequence_merged_free)
        X(list_structure)
        X(index_covers)
        X(index_class_separation)
        #undef X
    }
    else {
        ImGui::Text("System heap is not initialized");
        gui.actions.machineSystemHeapInitialize |= ImGui::Button("Initialize");
    }
}

void DisplayWindow::render(mqMachine *omach)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    GUIWindow::render(omach);
    ImGui::PopStyleVar();
}

void DisplayWindow::renderContents(mqMachine *omach)
{
    (void)omach;

    m_DGW.AddWindow();
    ImVec2 TL(m_DGW.x(), m_DGW.y() + m_DGW.height());
    ImVec2 BR(TL.x + m_DGW.width(), TL.y + m_DGW.padding().w);
    ImGui::PushClipRect(TL, BR, false);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(TL, BR, 0xff1c1712);

    ImGui::SetCursorScreenPos({TL.x + 4, TL.y + 4});
    ImGui::Text("%dx%d - center (%.1f,%.1f) - zoom %d%%",
        m_DGW.width(), m_DGW.height(), m_DGW.viewX(), m_DGW.viewY(),
        (int)(m_DGW.viewScale() * 100));

    ImVec2 cursor = ImGui::GetIO().MousePos;
    if(m_DGW.inWindow(cursor)) {
        ImVec2 pointing = m_DGW.viewLocation(cursor.x, cursor.y);
        ImGui::SameLine(0);
        ImGui::Text("- pointing at (%.1f,%.1f)", pointing.x, pointing.y);
    }

    ImGui::PopClipRect();
}

void KeyboardWindow::renderContents(mqMachine *omach)
{
    mqKeyboard *kbd = omach->keyboard;
    if(!kbd) {
        ImGui::Text("Machine has no keyboard!");
        return;
    }

    for(uint i = 0; i < kbd->keyCount; i++) {
        mqKeyboardKey *key = &kbd->keyInfo[i];
        float x = key->geometry.x, y = key->geometry.y;
        float w = key->geometry.w, h = key->geometry.h;
        char str[64];
        snprintf(str, sizeof str, "%s##key%d", key->name, i);
        ImGui::SetCursorPos({x, y});
        if(mq_keyboard_isKeyPressed(kbd, i)) {
            // TODO: Visual effect for keyboard-based key presses
            // (or add a shortcut to the button-not sure what's best)
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5);
            ImGui::Button(str, {w, h});
            ImGui::PopStyleVar();
        }
        else
            ImGui::Button(str, {w, h});
        gui.actions.physicalKeysAssigned[i] = ImGui::IsItemActive();
    }

    if(ImGui::IsWindowFocused()) {
        ImGui::SetNextFrameWantCaptureKeyboard(true);
        if(ImGui::IsKeyDown(ImGuiKey_LeftArrow))
            gui.actions.logicalKeysAssigned[MQ_KEY_LEFT] = true;
        if(ImGui::IsKeyDown(ImGuiKey_UpArrow))
            gui.actions.logicalKeysAssigned[MQ_KEY_UP] = true;
        if(ImGui::IsKeyDown(ImGuiKey_DownArrow))
            gui.actions.logicalKeysAssigned[MQ_KEY_DOWN] = true;
        if(ImGui::IsKeyDown(ImGuiKey_RightArrow))
            gui.actions.logicalKeysAssigned[MQ_KEY_RIGHT] = true;
        if(ImGui::IsKeyDown(ImGuiKey_LeftShift))
            gui.actions.logicalKeysAssigned[MQ_KEY_SHIFT] = true;
        if(ImGui::IsKeyDown(ImGuiKey_Enter))
            gui.actions.logicalKeysAssigned[MQ_KEY_EXE] = true;
        if(ImGui::IsKeyDown(ImGuiKey_Escape))
            gui.actions.logicalKeysAssigned[MQ_KEY_EXIT] = true;
        if(ImGui::IsKeyDown(ImGuiKey_F1))
            gui.actions.logicalKeysAssigned[MQ_KEY_F1] = true;
        if(ImGui::IsKeyDown(ImGuiKey_F2))
            gui.actions.logicalKeysAssigned[MQ_KEY_F2] = true;
        if(ImGui::IsKeyDown(ImGuiKey_F3))
            gui.actions.logicalKeysAssigned[MQ_KEY_F3] = true;
        if(ImGui::IsKeyDown(ImGuiKey_F4))
            gui.actions.logicalKeysAssigned[MQ_KEY_F4] = true;
        if(ImGui::IsKeyDown(ImGuiKey_F5))
            gui.actions.logicalKeysAssigned[MQ_KEY_F5] = true;
        if(ImGui::IsKeyDown(ImGuiKey_F6))
            gui.actions.logicalKeysAssigned[MQ_KEY_F6] = true;
    }
}

//=== Record =================================================================//

void RecordWindow::renderContents(mqMachine *omach)
{
    const ImGuiStyle& style = ImGui::GetStyle();
    auto w_button = ImGui::GetContentRegionAvail().x;
    w_button = (w_button - (style.ItemInnerSpacing.x * 2)) / 3;

    ImGui::SeparatorTextD("Video recorder");
    // calculate at runtime the width of all button
    // trick stolen from `/imgui/imgui_widgets.cpp#L5665-L5666`
    ImGui::TextWrapped(
        "You can start recording the virtual screen by selecting an "
        "addin and then pressing start."
    );
    ImGui::Spacing();

    /* no addin selected */
    if(!omach->initialized) {
        ImGui::ButtonWSized("Start", w_button, true);
        ImGui::SameLine(0, style.ItemInnerSpacing.x);
        ImGui::ButtonWSized("Pause", w_button, true);
        ImGui::SameLine(0, style.ItemInnerSpacing.x);
        ImGui::ButtonWSized("Stop", w_button, true);
        ImGui::Spacing();
        ImGui::TextCenteredColor("No addin selected", 0xff0000);
        return;
    }

    /* addin selected, but no record requested */
    mqRecord *backend = &gui.record_info;
    if(backend->status == MQ_RECORD_STATUS_UNINIT) {
        mqRecordRequest request = {
            .frameRate = 50,
            .scale_factor = 2,
            .filename = "record.mp4",
        };
        if(ImGui::ButtonWSized("Start", w_button, false)) {
            if(record_init(backend, &request, &gui.actions.display) != 0) {
                mq_log(MQ_LOG_ERROR, "%s", backend->error);
            } else {
                record_set_status(backend, MQ_RECORD_STATUS_START);
                if(omach->cyclesPending == 0)
                    gui.actions.machineSetPendingCycles = -1;
            }
            record_show(backend);
        }
        ImGui::SameLine(0, style.ItemInnerSpacing.x);
        ImGui::ButtonWSized("Pause", w_button, true);
        ImGui::SameLine(0, style.ItemInnerSpacing.x);
        ImGui::ButtonWSized("Stop", w_button, true);
        ImGui::Spacing();
        const char *text = "Start recording and emulation";
        if(omach->cyclesPending != 0)
            text = "Start recording";
        ImGui::TextCenteredColor(text, 0x00ffff);
        return;
    }

    /* start/pause/stop button */

    if (omach->cyclesPending == 0)
        record_set_status(backend, MQ_RECORD_STATUS_PAUSED);

    bool started = (backend->status != MQ_RECORD_STATUS_PAUSED);
    bool paused = (backend->status != MQ_RECORD_STATUS_START);
    if(ImGui::ButtonWSized("Continue", w_button, started))
        record_set_status(backend, MQ_RECORD_STATUS_START);
    ImGui::SameLine(0, style.ItemInnerSpacing.x);
    if(ImGui::ButtonWSized("Pause", w_button, paused))
        record_set_status(backend, MQ_RECORD_STATUS_PAUSED);
    ImGui::SameLine(0, style.ItemInnerSpacing.x);
    if(ImGui::ButtonWSized("Stop", w_button, false)) {
        record_quit(backend);
        record_set_status(backend, MQ_RECORD_STATUS_UNINIT);
    }
    if(gui.actions.display.dirty) {
        if(record_add_frame(backend, &gui.actions.display) != 0)
            mq_log(MQ_LOG_ERROR, "%s", backend->error);
    }

    /* recording information */

    ImGui::Spacing();
    ImGui::TextDisabled(
        "Start time: %02d:%02d:%02d",
        backend->stats.time_min,
        backend->stats.time_sec,
        backend->stats.time_ms
    );
    ImGui::TextDisabled("Elapsed: %dms", backend->stats.total_ms);
    ImGui::TextDisabled("Nb. frames: %d", backend->stats.iframe);
    ImGui::TextDisabled("Nb. error: %d", backend->stats.nb_error);
    ImGui::TextDisabled("Output: %s", backend->stats.pathname_out);
    ImGui::TextDisabled("Status: %s", backend->stats.status);

    /* recording status */

    ImGui::Spacing();
    if (backend->status == MQ_RECORD_STATUS_PAUSED) {
        ImGui::TextCenteredColor("Paused", 0x00ffff);
    } else if(backend->status == MQ_RECORD_STATUS_START) {
        ImGui::TextCenteredColor("Recording", 0x00ff00);
    } else {
        ImGui::TextCenteredColor("Error", 0xff0000);
    }
}

void RecordWindow::resetState()
{
    mqRecord *backend = &gui.record_info;
    if(backend->status == MQ_RECORD_STATUS_START) {
        mq_log(MQ_LOG_WARNING, "Record: stopping current recording");
        record_quit(backend);
    }
}
