#include <assert.h>

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
};

static struct gui_state gui_state;

static char gui_disasm_buf[64];

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

    ImGui::BeginChild("Actions", ImVec2(235, 42), true);

    if (ImGui::Button("Reset", ImVec2(67, 25))) {
        psx_soft_reset();
    }
	
    ImGui::SameLine();
	
    button_text = gui_state.should_run_frame ? "Break" : "Continue";

    if (ImGui::Button(button_text, ImVec2(67, 25))) {
        gui_state.should_run_frame = !gui_state.should_run_frame;
    }
    
    ImGui::SameLine();

    if (ImGui::Button("Step", ImVec2(67, 25))) {
        gui_state.should_run_frame = false;
        psx_step();
    }

    ImGui::EndChild();
}

static void
gui_render_debug_cpu_registers()
{
    ImGui::BeginChild("Registers", ImVec2(235, 302), true);

    for (int i = 0; i < R3000_NR_REGISTERS; i+=2) {
        ImGui::Text("%s: 0x%08x", r3000_register_name(i), r3000_read_reg(i));
        ImGui::SameLine();
        ImGui::Text("%s: 0x%08x", r3000_register_name(i+1), r3000_read_reg(i+1));
    }
    
    ImGui::Text("$hi: 0x%08x", r3000_read_lo());
    ImGui::SameLine();
    ImGui::Text("$lo: 0x%08x", r3000_read_lo());

    ImGui::EndChild();
}

static void
gui_render_debug_cpu_disasm()
{
    ImGui::BeginChild("Disassembly", ImVec2(400, 437), true);

    uint32_t pc = r3000_read_pc();
    uint32_t len = 12 * sizeof(uint32_t);

    for (uint32_t i = pc - len; i <= pc + len; i += sizeof(uint32_t)) {
        uint32_t instruction = r3000_debug_read_memory32(i);

        uint8_t b0, b1, b2, b3;

        b0 = instruction >> 24;
        b1 = instruction >> 16;
        b2 = instruction >>  8;
        b3 = instruction >>  0;

        char is_pc = i == pc ? '*' : ' ';

        r3000_disassembler_disassemble(gui_disasm_buf, 64, instruction, i);
        ImGui::Text("%c 0x%08x:  %02x %02x %02x %02x  %s", is_pc, i,
                    b0, b1, b2, b3, gui_disasm_buf);
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
