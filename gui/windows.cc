#include "gui.h"
#include "imgui-util.h"
#include <stdio.h>

static void AddChunkList(
    mqMachine *mach, MemoryWindowState &s, MemoryWindowAction &a)
{
    mqMemory const *mem = mach->memory;
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
        if(MQ_CHUNKPTR_ISNULL(mem->chunks[i]))
            continue;

        ImGui::SetNextItemAllowOverlap();
        bool clicked = ImGui::Selectable(addr, (int)i == s.selectedChunk);
        if(MQ_CHUNKPTR_ISBUFFER(mem->chunks[i])) {
            ImGui::SameLine(0, 7);
            ImGui::PushFont(fontSans);
            ImGui::TextDisabled("(Buffer)");
            ImGui::PopFont();
        }

        if(clicked && s.selectedChunk != (int)i) {
            s.selectedChunk = i;
            s.selectedPage = -1;
            s.selectedIO = -1;
            a.type = a.Type::MWA_VIEW_HEX;
            a.address = i << 20;

            /* Autoselect page #0 if there is exactly one non-null page */
            if(MQ_CHUNKPTR_ISDETAILS(mem->chunks[i])) {
                mqChunk *ch = MQ_CHUNKPTR_DETAILS(mem->chunks[i]);
                int nonnullPages = 0;
                for(int i = 0; i < 256 && nonnullPages < 2; i++)
                    nonnullPages += ch->pages[i] != MQ_PAGEPTR_NULL;
                if(nonnullPages == 1)
                    s.selectedPage = 0;
            }
        }

        totalChunks++;
    }

    ImGui::PopFont();
    ImGui::EndListBox();
    ImGui::Text("Total: %d chunks", totalChunks);
}

static void AddPageList(mqMachine *mach, MemoryWindowState &s, u32 chunkBase,
    mqChunkPointer chunkPtr, MemoryWindowAction &a)
{
    (void)mach;
    if(MQ_CHUNKPTR_ISNULL(chunkPtr))
        return;
    if(MQ_CHUNKPTR_ISBUFFER(chunkPtr)) {
        ImGui::Text("This is a buffer chunk.");
        return;
    }
    mqChunk *chunk = MQ_CHUNKPTR_DETAILS(chunkPtr);

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
        if(MQ_PAGEPTR_ISNULL(chunk->pages[i]))
            continue;

        ImGui::SetNextItemAllowOverlap();
        bool clicked = ImGui::Selectable(addr, (int)i == s.selectedPage);
        ImGui::SameLine(0, 7);
        ImGui::PushFont(fontSans);
        if(MQ_PAGEPTR_ISBUFFER(chunk->pages[i]))
            ImGui::TextDisabled("(Buffer)");
        else if(MQ_PAGEPTR_ISMMIOPAGE(chunk->pages[i]))
            ImGui::TextDisabled("(IO)");
        ImGui::PopFont();

        if(clicked) {
            s.selectedPage = i;
            s.selectedIO = -1;
            a.type = a.Type::MWA_VIEW_HEX;
            a.address = chunkBase + (i << 12);
        }

        totalPages++;
    }

    ImGui::PopFont();
    ImGui::EndListBox();
    ImGui::Text("Total: %d pages", totalPages);
}

static void AddMMIOList(mqMachine *mach, MemoryWindowState &s, u32 addr,
        mqPagePointer pagePtr, MemoryWindowAction &a)
{
    if(MQ_PAGEPTR_ISNULL(pagePtr))
        return;
    if(MQ_PAGEPTR_ISBUFFER(pagePtr)) {
        ImGui::Text("This is a buffer page.");
        return;
    }
    mqMMIOPage *mmpg = MQ_PAGEPTR_MMIOPAGE(pagePtr);

    ImVec2 avl = ImGui::GetContentRegionAvail();
    avl.x -= 4;
    avl.y -= ImGui::GetTextLineHeightWithSpacing();
    // TODO: More clearly split list and IO details
    avl.y /= 2;
    if(!ImGui::BeginListBox("##memory-ios", avl))
        return;

    ImGui::PushFont(fontMono);

    char str[16];

    for(int i = 0; i < mmpg->length; i++) {
        sprintf(str, "%08x", addr + i);
        if(!mmpg->map[i])
            continue;

        mqMMIO *io = &mmpg->io[mmpg->map[i] - 1];

        ImGui::SetNextItemAllowOverlap();
        bool clicked = ImGui::Selectable(str, i == s.selectedIO);
        if(io->name) {
            ImGui::SameLine(0, 7);
            ImGui::TextDisabled(io->name);
        }

        if(clicked)
            s.selectedIO = i;
    }

    ImGui::PopFont();
    ImGui::EndListBox();
    ImGui::Text("Total: %d IOs", mmpg->ioCount);

    if(s.selectedIO >= 0) {
        mqMMIO *io = &mmpg->io[mmpg->map[s.selectedIO] - 1];
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::PushFont(fontMono);
        ImGui::TextUnformatted(io->name);
        ImGui::PopFont();
    }

    (void)mach;
    (void)a;
}

MemoryWindowAction AddMemoryWindowContents(
    mqMachine *mach, MemoryWindowState &s)
{
    mqMemory const *mem = mach->memory;
    MemoryWindowAction a;

    mqChunkPointer chunkPtr = MQ_CHUNKPTR_NULL;
    mqChunk *chunk = NULL;
    mqPagePointer pagePtr = MQ_PAGEPTR_NULL;

    //=== Reset selection if invalid after a change ===//

    if(s.selectedChunk >= 0) {
        chunkPtr = mem->chunks[s.selectedChunk];
        if(MQ_CHUNKPTR_ISNULL(chunkPtr))
            s.selectedChunk = -1;
        else if(!MQ_CHUNKPTR_ISBUFFER(chunkPtr))
            chunk = MQ_CHUNKPTR_DETAILS(chunkPtr);
    }

    if(s.selectedPage >= 0) {
        if(chunk) {
            pagePtr = chunk->pages[s.selectedPage];
            if(MQ_PAGEPTR_ISNULL(pagePtr))
                s.selectedPage = -1;
        }
        else s.selectedPage = -1;
    }

    //=== General memory statistics ===//

    // TODO: Avoid recomputation of memory stats every frame?!
    struct mqMemory_Stats stats = mq_memory_stats(mem);

    ImGui::Text("1 MB Chunks: %d (%d buffer, %d mixed including %d pure MMIO)",
                stats.totalChunks, stats.bufferChunks, stats.detailedChunks,
                stats.pureMMIOChunks);
    ImGui::Text("4 kB Pages: %d mapped", stats.bufferPages);

    //=== List of chunks and pages ===//

    if(ImGui::BeginTable("memtable", 3, ImGuiTableFlags_SizingStretchSame)) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();

        ImGui::SeparatorTextD("Chunks");
        AddChunkList(mach, s, a);
        ImGui::TableNextColumn();

        ImGui::SeparatorTextD("Pages");
        if(s.selectedChunk >= 0)
            AddPageList(mach, s, s.selectedChunk << 20, chunkPtr, a);
        ImGui::TableNextColumn();

        ImGui::SeparatorTextD("MMIO");
        if(s.selectedPage >= 0)
            AddMMIOList(mach, s, (s.selectedChunk << 20) + (s.selectedPage << 12), pagePtr, a);

        ImGui::EndTable();
    }

    return a;
}

MemoryWindowAction AddMemoryWindow(mqMachine *mach, MemoryWindowState &state)
{
    MemoryWindowAction a;

    if(ImGui::Begin("Memory", nullptr, ImGuiWindowFlags_HorizontalScrollbar))
        a = AddMemoryWindowContents(mach, state);
    ImGui::End();

    return a;
}

