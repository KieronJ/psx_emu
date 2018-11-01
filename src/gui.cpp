#include <assert.h>
#include <stdlib.h>
#include <string.h>

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
}

struct gui_state {
    bool should_quit;
    bool should_run_frame;

    bool debug_cpu;
    bool debug_ram;
    bool debug_bios;

    bool debug_modify_register;
    unsigned int debug_modify_register_value;

    bool debug_modify_disasm;
    uint32_t debug_modify_disasm_address;
};

static struct gui_state gui_state;

static MemoryEditor gui_memedit_ram;
static MemoryEditor gui_memedit_bios;

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
    gui_state.should_run_frame = true;

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

    ImGui::BeginChild("Actions", ImVec2(235, 30));

    if (ImGui::Button("Reset", ImVec2(70, 25))) {
        psx_soft_reset();
    }
	
    ImGui::SameLine();
	
    button_text = gui_state.should_run_frame ? "Break" : "Continue";

    if (ImGui::Button(button_text, ImVec2(70, 25))) {
        gui_state.should_run_frame = !gui_state.should_run_frame;
    }
    
    ImGui::SameLine();

    if (ImGui::Button("Step", ImVec2(70, 25))) {
        gui_state.should_run_frame = false;
        psx_step();
    }

    ImGui::EndChild();
}

static bool
gui_set_hex_value_button(const char *title, char *src, uint32_t *dest)
{
    if (ImGui::Button(title)) {
        if (strlen(src) != 0) {
            *dest = strtoul(src, NULL, 16);
            src[0] = '\0';
            return true;
        }
    }

    return false;
}

static void
gui_render_modify_register(void)
{
    static char value[9] = "";

    ImGuiWindowFlags window_flags;
    ImGuiInputTextFlags input_flags;
    unsigned int reg;
    const char *reg_name;
    uint32_t new_val;
    char title[32];

    window_flags = ImGuiWindowFlags_AlwaysAutoResize;
    input_flags = ImGuiInputTextFlags_CharsHexadecimal;
    reg = gui_state.debug_modify_register_value;
    reg_name = r3000_register_name(reg);
    snprintf(title, sizeof(title), "Enter value for register %s", reg_name);

    ImGui::OpenPopup(title);

    if (ImGui::BeginPopupModal(title, NULL, window_flags)) {
        ImGui::InputText("", value, sizeof(value), input_flags);

        if (gui_set_hex_value_button("Set", value, &new_val)) {
            r3000_write_reg(reg, new_val);
            gui_state.debug_modify_register = false;
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel")) {
            gui_state.debug_modify_register = false;
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
    uint32_t address, new_val;
    char title[32], disassembly[64];

    window_flags = ImGuiWindowFlags_AlwaysAutoResize;
    input_flags = ImGuiInputTextFlags_CharsHexadecimal;
    address = gui_state.debug_modify_disasm_address;
    snprintf(title, sizeof(title), "Enter value for 0x%08x", address);

    ImGui::OpenPopup(title);

    if (ImGui::BeginPopupModal(title, NULL, window_flags)) {
        ImGui::InputText("", value, sizeof(value), input_flags);

        if (strlen(value) != 0) {
            new_val = strtoul(value, NULL, 16);
            r3000_disassembler_disassemble(disassembly, sizeof(disassembly), new_val, address);
            ImGui::Text("%s", disassembly);
        }

        if (gui_set_hex_value_button("Set", value, &new_val)) {
            r3000_debug_write_memory32(address, new_val);
            gui_state.debug_modify_disasm = false;
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel")) {
            gui_state.debug_modify_disasm = false;
            value[0] = '\0';
        }

        ImGui::EndPopup();
    }
}

static void
gui_render_debug_cpu_register(unsigned int i)
{
    char text[32];
    ImGuiSelectableFlags flags;

    snprintf(text, sizeof(text), "%s: 0x%08x",
             r3000_register_name(i), r3000_read_reg(i));
    flags = ImGuiSelectableFlags_AllowDoubleClick;

    if(ImGui::Selectable(text, false, flags)) {
        if (ImGui::IsMouseDoubleClicked(0)) {
            gui_state.debug_modify_register = true;
            gui_state.debug_modify_register_value = i;
        }    
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
    ImGuiSelectableFlags flags;
    uint32_t pc;
    char disassembly[64];

    flags = ImGuiSelectableFlags_AllowDoubleClick;

    pc = r3000_read_pc();

    if (address == pc) {
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 99, 71, 255));
    }

    gui_format_disassembly(disassembly, sizeof(disassembly), address);

    if (ImGui::Selectable(disassembly, false, flags)) {
        if (ImGui::IsMouseDoubleClicked(0)) {
            gui_state.debug_modify_disasm = true;
            gui_state.debug_modify_disasm_address = address;
        } 
    }

    if (address == pc) {
        ImGui::PopStyleColor();
    }
}

static void
gui_render_debug_cpu_disasm(void)
{
    int pc_row;
    float height;

    ImGui::BeginChild("Disassembly", ImVec2(400, ImGui::GetFontSize() * 32), true);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1));
    ImGuiListClipper clipper(0x8000000);

    while (clipper.Step()) {
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
            gui_render_debug_cpu_disasm_instruction(0xbfc00000 + i * 4);
        }
    }

    if (gui_state.should_run_frame) {
        height = ImGui::GetTextLineHeightWithSpacing();
        pc_row = (r3000_read_pc() - 0xbfc00000) / 4;

        ImGui::SetScrollY((pc_row - 14) * height);
    }
    
    ImGui::PopStyleVar();
    ImGui::EndChild();
}

static void
gui_render_debug_cpu(void)
{
    ImGui::Begin("CPU", &gui_state.debug_cpu,
                 ImGuiWindowFlags_AlwaysAutoResize);

    ImGui::BeginGroup();

    gui_render_debug_cpu_actions();
    gui_render_debug_cpu_registers();

    ImGui::EndGroup();

    ImGui::SameLine();

    gui_render_debug_cpu_disasm();
   
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

            ImGui::MenuItem("Quit", "ESC", &gui_state.should_quit);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Debug")) {
            ImGui::MenuItem("CPU", NULL, &gui_state.debug_cpu);
            ImGui::MenuItem("RAM", NULL, &gui_state.debug_ram);
            ImGui::MenuItem("BIOS", NULL, &gui_state.debug_bios);
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

    if (gui_state.debug_modify_register) {
        gui_render_modify_register();
    }

    if (gui_state.debug_modify_disasm) {
        gui_render_modify_disasm();
    }

    ImGui::Render();
}

void
gui_draw(void)
{
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

bool
gui_should_quit(void)
{
    return gui_state.should_quit;
}

bool
gui_should_run_frame(void)
{
    return gui_state.should_run_frame;
}
