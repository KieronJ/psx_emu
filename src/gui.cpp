#include <assert.h>
#include <stddef.h>
#include <string.h>

#include <ctime>
#include <string>
#include <vector>

#include <SDL2/SDL.h>

#include <imgui/imgui.h>
#include <imgui/imgui_impl_opengl3.h>
#include <imgui/imgui_impl_sdl.h>
#include <imgui/imgui_memory_editor.h>

extern "C" {
#include "gui.h"
#include "psx.h"
#include "r3000.h"
#include "r3000_disassembler.h"
#include "spu.h"
}

struct gui_state {
    bool quit;
    bool step;

    bool cont;
    bool cont_prev;

    bool debug_cpu;
    bool debug_ram;
    bool debug_bios;
    bool debug_sram;
    bool debug_tty;

    bool disasm_lock;
    bool disasm_lock_jump;
    uint32_t disasm_lock_address;

    bool modify_register;
    unsigned int modify_register_value;

    bool modify_disasm;
    uint32_t modify_disasm_address;
};

static struct gui_state gui_state;

static MemoryEditor gui_memedit_ram;
static MemoryEditor gui_memedit_bios;
static MemoryEditor gui_memedit_sram;

static std::vector<std::string> gui_tty_entries;

void
gui_setup(SDL_Window *window, SDL_GLContext context)
{
    assert(window);
    assert(context);

    ImGui::CreateContext();
    ImGui_ImplSDL2_InitForOpenGL(window, context);
    ImGui_ImplOpenGL3_Init();

    /* TODO: Setup custom style */
    ImGui::StyleColorsDark();

    gui_state = {};
    gui_state.cont = true;

    gui_memedit_ram.OptShowOptions = false;
    gui_memedit_ram.OptShowDataPreview = false;
    gui_memedit_ram.OptUpperCaseHex = false;

    gui_memedit_bios.OptShowOptions = false;
    gui_memedit_bios.OptShowDataPreview = false;
    gui_memedit_bios.OptUpperCaseHex = false;
}

void
gui_shutdown(void)
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
}

void
gui_process_event(SDL_Event event)
{
    ImGui_ImplSDL2_ProcessEvent(&event);
}

static void
gui_render_debug_cpu_actions(void)
{
    const char *button_text;

    button_text = gui_state.cont ? "Break" : "Continue";

    ImGui::BeginChild("Actions", ImVec2(235, 30));

    if (ImGui::Button("Reset", ImVec2(70, 25))) {
        psx_soft_reset();
    }

    ImGui::SameLine();

    if (ImGui::Button(button_text, ImVec2(70, 25))) {
        gui_state.cont = !gui_state.cont;

        if (gui_state.cont) {
            gui_state.disasm_lock = false;
        }
    }

    ImGui::SameLine();

    if (ImGui::Button("Step", ImVec2(70, 25))) {
        gui_state.step = true;
        gui_state.cont = false;
        gui_state.disasm_lock = false;

        psx_step();
    }

    ImGui::EndChild();
}

static void
gui_render_modify_register(void)
{
    static char value[9] = "";

    ImGuiWindowFlags window_flags;
    ImGuiInputTextFlags input_flags;

    bool enter, button;

    unsigned int reg;
    const char *reg_name;
    char title[32];

    window_flags = ImGuiWindowFlags_AlwaysAutoResize;
    input_flags = ImGuiInputTextFlags_CharsHexadecimal |
                  ImGuiInputTextFlags_EnterReturnsTrue;

    reg = gui_state.modify_register_value;
    reg_name = r3000_register_name(reg);
    snprintf(title, sizeof(title), "Enter value for register %s", reg_name);

    ImGui::OpenPopup(title);

    if (ImGui::BeginPopupModal(title, NULL, window_flags)) {
        enter = ImGui::InputText("", value, sizeof(value), input_flags);
        button = ImGui::Button("Set");

        if (enter || button) {
            if(reg == 0) {
                r3000_debug_force_pc(strtoul(value, NULL, 16));
            } else {
                r3000_write_reg(reg, strtoul(value, NULL, 16));
            }

            gui_state.modify_register = false;
            value[0] = '\0';
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel")) {
            gui_state.modify_register = false;
            value[0] = '\0';
        }

        ImGui::EndPopup();
    }
}

static void
gui_render_modify_disasm(void)
{
    static char value[9] = "";

    ImGuiWindowFlags window_flags;
    ImGuiInputTextFlags input_flags;

    bool enter, button;

    uint32_t address, new_val;
    char title[32], disassembly[64];

    window_flags = ImGuiWindowFlags_AlwaysAutoResize;
    input_flags = ImGuiInputTextFlags_CharsHexadecimal |
                  ImGuiInputTextFlags_EnterReturnsTrue;

    address = gui_state.modify_disasm_address;
    snprintf(title, sizeof(title), "Enter value for 0x%08x", address);

    ImGui::OpenPopup(title);

    if (ImGui::BeginPopupModal(title, NULL, window_flags)) {
        new_val = strtoul(value, NULL, 16);

        enter = ImGui::InputText("", value, sizeof(value), input_flags);

        r3000_disassembler_disassemble(disassembly, sizeof(disassembly), new_val, address);
        ImGui::Text("%s", disassembly);

        button = ImGui::Button("Set");

        if (enter || button) {
            r3000_debug_write_memory32(address, strtoul(value, NULL, 16));

            gui_state.modify_disasm = false;
            value[0] = '\0';
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel")) {
            gui_state.modify_disasm = false;
            value[0] = '\0';
        }

        ImGui::EndPopup();
    }
}

static bool
gui_render_select_dclick(const char *text, unsigned int mouse_button)
{
    ImGuiSelectableFlags flags;

    flags = ImGuiSelectableFlags_AllowDoubleClick;

    if (ImGui::Selectable(text, false, flags) &&
        ImGui::IsMouseDoubleClicked(mouse_button)) {
        return true;
    }

    return false;
}

static void
gui_render_debug_cpu_register(unsigned int i)
{
    char text[32];
    const char *name;
    uint32_t value;

    if (i == 0) {
        name = "$pc";
        value = r3000_read_pc();
    } else {
        name = r3000_register_name(i);
        value = r3000_read_reg(i);
    }

    snprintf(text, sizeof(text), "%s: 0x%08x", name, value);

    if (gui_render_select_dclick(text, 0)) {
        gui_state.modify_register = true;
        gui_state.modify_register_value = i;
    }
}

static void
gui_render_debug_cpu_registers(void)
{
    ImGui::BeginChild("Registers", ImVec2(235, 302));
    ImGui::Columns(2);

    for (int i = 0; i < R3000_NR_REGISTERS; i++) {
        gui_render_debug_cpu_register(i);
        ImGui::NextColumn();
    }

    ImGui::Columns(1);
    ImGui::EndChild();
}

static void
gui_format_disassembly(char *buffer, size_t n, uint32_t address)
{
    uint32_t instruction;
    char buf[n];

    instruction = r3000_debug_read_memory32(address);

    r3000_disassembler_disassemble(buf, sizeof(buf), instruction, address);
    snprintf(buffer, n, "0x%08x: %08x %s", address, instruction, buf);
}

static void
gui_render_debug_cpu_disasm_instruction(uint32_t address)
{
    uint32_t pc;
    char disassembly[64];

    pc = r3000_read_pc();

    if (address == pc) {
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 99, 71, 255));
    }

    gui_format_disassembly(disassembly, sizeof(disassembly), address);

    if (gui_render_select_dclick(disassembly, 0)) {
        gui_state.modify_disasm = true;
        gui_state.modify_disasm_address = address;
    }

    if (address == pc) {
        ImGui::PopStyleColor();
    }
}

static bool
gui_break(void)
{
    return gui_state.cont_prev && !gui_state.cont;
}

static void
gui_render_debug_cpu_disasm_window(void)
{
    ImVec2 size;

    uint32_t address;
    float height;

    size = ImVec2(400, ImGui::GetFontSize() * 32);

    address = gui_state.disasm_lock ? gui_state.disasm_lock_address
                                      : r3000_read_pc();

    ImGui::BeginChild("Disassembly", size, true);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1));

    for (int i = -1024; i <= 1024; ++i) {
        gui_render_debug_cpu_disasm_instruction(address + i * 4);
    }

    if (gui_state.step || gui_state.disasm_lock_jump || gui_break()) {
        gui_state.disasm_lock_jump = false;

        height = ImGui::GetTextLineHeightWithSpacing();
        ImGui::SetScrollY(1010 * height);
    }

    ImGui::PopStyleVar();
    ImGui::EndChild();
}

static void
gui_render_debug_cpu_disasm_jump(void)
{
    static char value[9] = "";

    ImGuiInputTextFlags flags;

    flags = ImGuiInputTextFlags_CharsHexadecimal |
            ImGuiInputTextFlags_EnterReturnsTrue;

    ImGui::Text("Jump to address:");

    ImGui::SameLine();

    if (ImGui::InputText("", value, sizeof(value), flags)) {
        gui_state.disasm_lock = true;
        gui_state.disasm_lock_jump = true;
        gui_state.disasm_lock_address = strtoul(value, NULL, 16);
    }
}

static void
gui_render_debug_cpu(void)
{
    gui_state.step = false;
    gui_state.cont_prev = gui_state.cont;

    ImGui::Begin("CPU", &gui_state.debug_cpu,
                 ImGuiWindowFlags_AlwaysAutoResize);

    ImGui::BeginGroup();
    gui_render_debug_cpu_actions();
    gui_render_debug_cpu_registers();
    ImGui::EndGroup();

    ImGui::SameLine();

    ImGui::BeginGroup();
    gui_render_debug_cpu_disasm_window();
    gui_render_debug_cpu_disasm_jump();
    ImGui::EndGroup();

    ImGui::End();
}

static void
gui_render_debug_tty_output(void)
{
    ImVec2 size;
    ImGuiWindowFlags flags;
    const char *entry;

    size = ImVec2(600, ImGui::GetFontSize() * 32);
    flags = ImGuiWindowFlags_HorizontalScrollbar;

    ImGui::BeginChild("TTY Output", size, true, flags);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1));

    ImGuiListClipper clipper(gui_tty_entries.size());

    while (clipper.Step()) {
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
            entry = gui_tty_entries.at(i).c_str();

            if (gui_render_select_dclick(entry, 0)) {
                ImGui::SetClipboardText(entry);
                printf("gui: info: copied to clipboard\n");
            }
        }
    }

    ImGui::PopStyleVar();
    ImGui::EndChild();
}

static void
gui_render_debug_tty(void)
{
    ImGuiWindowFlags flags;

    flags = ImGuiWindowFlags_AlwaysAutoResize;
    ImGui::Begin("TTY", &gui_state.debug_tty, flags);

    gui_render_debug_tty_output();

    if (ImGui::Button("Clear")) {
        gui_tty_entries.clear();
    }

    ImGui::End();
}

void
gui_render(SDL_Window *window)
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame(window);
    ImGui::NewFrame();

    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Soft Reset", NULL)) {
                psx_soft_reset();
            }

            if (ImGui::MenuItem("Hard Reset", NULL)) {
                psx_hard_reset();
            }

            ImGui::Separator();

            ImGui::MenuItem("Quit", "ESC", &gui_state.quit);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Debug")) {
            ImGui::MenuItem("CPU", NULL, &gui_state.debug_cpu);
            ImGui::MenuItem("RAM", NULL, &gui_state.debug_ram);
            ImGui::MenuItem("BIOS", NULL, &gui_state.debug_bios);
            ImGui::MenuItem("SPU RAM", NULL, &gui_state.debug_sram);
            ImGui::MenuItem("TTY", NULL, &gui_state.debug_tty);
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }

    if (gui_state.debug_cpu) {
        gui_render_debug_cpu();
    }

    if (gui_state.debug_ram) {
        gui_memedit_ram.DrawWindow("Memory", psx_debug_ram(), PSX_RAM_SIZE);
    }

    if (gui_state.debug_bios) {
        gui_memedit_bios.DrawWindow("BIOS", psx_debug_bios(), PSX_BIOS_SIZE);
    }

    if (gui_state.debug_sram) {
        gui_memedit_sram.DrawWindow("SPU RAM", spu_debug_ram(), SPU_RAM_SIZE);
    }

    if (gui_state.debug_tty) {
        gui_render_debug_tty();
    }

    if (gui_state.modify_register) {
        gui_render_modify_register();
    }

    if (gui_state.modify_disasm) {
        gui_render_modify_disasm();
    }

    ImGui::Render();
}

void
gui_draw(void)
{
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}


static std::string
gui_get_timestamp_string(void)
{
    std::time_t now;
    char buf[32];

    now = std::time(nullptr);
    strftime(buf, sizeof(buf), "%X", localtime(&now));
    return buf;
}

void
gui_add_tty_entry(const char *str, size_t len)
{
    std::string entry;

    entry = gui_get_timestamp_string() + "\t" + std::string(str, len);
    gui_tty_entries.push_back(entry);
}

bool
gui_should_quit(void)
{
    return gui_state.quit;
}

bool
gui_should_continue(void)
{
    return gui_state.cont;
}
