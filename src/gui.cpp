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
    int debug_modify_register_value;
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

static void
gui_render_modify_register(void)
{
    static char text[9] = "";

    unsigned int reg;
    const char *reg_name;
    char message[20];
    ImGuiWindowFlags window_flags;
    ImGuiInputTextFlags input_flags;

    reg = gui_state.debug_modify_register_value;
    reg_name = r3000_register_name(reg);
    snprintf(message, sizeof(message), "Set register %s", reg_name);
    window_flags = ImGuiWindowFlags_AlwaysAutoResize;
    input_flags = ImGuiInputTextFlags_CharsHexadecimal;

    ImGui::OpenPopup(message);

    if (ImGui::BeginPopupModal(message, NULL, window_flags)) {
        ImGui::InputText("", text, sizeof(text), input_flags);

        if (ImGui::Button("Set")) { 
            if (strlen(text) != 0) {
                r3000_write_reg(reg, strtoul(text, NULL, 16));
                gui_state.debug_modify_register = false;
                memset(text, 0, sizeof(text));
            }
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel")) {
            gui_state.debug_modify_register = false;
            memset(text, 0, sizeof(text));
        }

        ImGui::EndPopup();
    }
}

static void
gui_render_debug_cpu_register(unsigned int i)
{
    char text[32];
    ImGuiSelectableFlags flags;

    snprintf(text, 32, "%s: 0x%08x",
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
gui_render_debug_cpu_disasm(void)
{
    uint32_t pc, len, instruction;
    uint32_t b0, b1, b2, b3;
    char disasm_buf[64];
    char is_pc;

    ImGui::BeginChild("Disassembly", ImVec2(400, 437), true);

    pc = r3000_read_pc();
    len = 12 * sizeof(uint32_t);

    for (uint32_t i = pc - len; i <= pc + len; i += sizeof(uint32_t)) {
        instruction = r3000_debug_read_memory32(i);

        b0 = instruction >> 24;
        b1 = instruction >> 16;
        b2 = instruction >>  8;
        b3 = instruction >>  0;

        is_pc = i == pc ? '*' : ' ';

        r3000_disassembler_disassemble(disasm_buf, sizeof(disasm_buf),
                                       instruction, i);
        ImGui::Text("%c 0x%08x:  %02x %02x %02x %02x  %s", is_pc, i,
                    b0, b1, b2, b3, disasm_buf);
    }

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
