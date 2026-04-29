#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")
#endif

#include "GUIManager.hpp"
#include "AIEnhancer.hpp"
#include "AudioProcessor.hpp"
#include <imgui.h>
#include <implot.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>
#include <shobjidl.h> // For IFileOpenDialog
#include <iostream>
#include <thread>
#include <filesystem>
#include <mutex>

struct GUIManager::Impl {
    AIEnhancer enhancer;
    AudioProcessor processor;
};

// Global for D&D callback
static GUIManager* g_instance = nullptr;

GUIManager::GUIManager() : pimpl(std::make_unique<Impl>()) {
    g_instance = this;
}
GUIManager::~GUIManager() { 
    g_instance = nullptr;
    cleanup(); 
}

void GUIManager::addLog(const std::string& msg) {
    std::lock_guard<std::mutex> lock(log_mutex);
    logs.push_back(msg);
}

// Safe Windows File Open Dialog
static std::string openFileDialog() {
    std::string result = "";
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (SUCCEEDED(hr)) {
        IFileOpenDialog *pFileOpen;
        hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL, IID_IFileOpenDialog, reinterpret_cast<void**>(&pFileOpen));
        if (SUCCEEDED(hr)) {
            COMDLG_FILTERSPEC fileTypes[] = { { L"WAV Files", L"*.wav" }, { L"All Files", L"*.*" } };
            pFileOpen->SetFileTypes(2, fileTypes);
            hr = pFileOpen->Show(NULL);
            if (SUCCEEDED(hr)) {
                IShellItem *pItem;
                hr = pFileOpen->GetResult(&pItem);
                if (SUCCEEDED(hr)) {
                    PWSTR pszFilePath;
                    hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
                    if (SUCCEEDED(hr)) {
                        int size = WideCharToMultiByte(CP_UTF8, 0, pszFilePath, -1, NULL, 0, NULL, NULL);
                        result.resize(size - 1);
                        WideCharToMultiByte(CP_UTF8, 0, pszFilePath, -1, &result[0], size, NULL, NULL);
                        CoTaskMemFree(pszFilePath);
                    }
                    pItem->Release();
                }
            }
            pFileOpen->Release();
        }
        CoUninitialize();
    }
    return result;
}

bool GUIManager::init() {
    if (!glfwInit()) return false;
    window = glfwCreateWindow(1280, 800, "Resemble Enhance Native - Debug V60", nullptr, nullptr);
    if (!window) return false;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    
    // Set Drag & Drop Callback
    glfwSetDropCallback(window, [](GLFWwindow* w, int count, const char** paths) {
        if (count > 0 && g_instance) {
            std::string path = paths[0];
            // Remove quotes if any
            if (path.front() == '"') path.erase(0, 1);
            if (path.back() == '"') path.pop_back();
            strncpy(g_instance->input_path, path.c_str(), sizeof(g_instance->input_path) - 1);
            g_instance->addLog("[System] File dropped: " + path);
        }
    });

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    
    // Load Japanese Font
    ImGuiIO& io = ImGui::GetIO();
    const char* font_path = "C:\\Windows\\Fonts\\meiryo.ttc"; // Meiryo
    if (!std::filesystem::exists(font_path)) font_path = "C:\\Windows\\Fonts\\msjh.ttc";
    if (!std::filesystem::exists(font_path)) font_path = "C:\\Windows\\Fonts\\msgothic.ttc";
    
    if (std::filesystem::exists(font_path)) {
        if (io.Fonts->AddFontFromFileTTF(font_path, 18.0f, nullptr, io.Fonts->GetGlyphRangesJapanese())) {
            addLog("[System] Font loaded: " + std::string(font_path));
        } else {
            addLog("[System] Failed to load font: " + std::string(font_path));
        }
    } else {
        addLog("[System] No suitable Japanese font found in C:\\Windows\\Fonts.");
    }

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    addLog("Dashboard Ready. Searching for AI Models...");
    
    namespace fs = std::filesystem;
    fs::path base_path;
    
#ifdef _WIN32
    wchar_t szPath[MAX_PATH];
    if (GetModuleFileNameW(NULL, szPath, MAX_PATH)) {
        base_path = fs::path(szPath).parent_path();
    } else {
        base_path = fs::current_path();
    }
#else
    base_path = fs::current_path();
#endif

    fs::path found_path;
    bool found = false;

    // Search upwards from the executable directory
    fs::path search_ptr = base_path;
    for (int i = 0; i < 5; ++i) { // Search up to 5 levels up
        fs::path candidate = search_ptr / "models_onnx";
        
        // Detailed check
        if (fs::exists(candidate) && fs::is_directory(candidate)) {
            // Further verify it's the right one (optional but good)
            if (fs::exists(candidate / "hparams.txt")) {
                found_path = candidate;
                found = true;
                break;
            }
        }
        
        if (search_ptr.has_parent_path() && search_ptr != search_ptr.root_path()) {
            search_ptr = search_ptr.parent_path();
        } else {
            break;
        }
    }

    if (found) {
        addLog("[System] Found models at: " + fs::absolute(found_path).string());
        pimpl->enhancer.setLogCallback([this](const std::string& msg) { addLog(msg); });
        if (pimpl->enhancer.loadModels(found_path.string(), false)) {
            addLog("[System] AI Models loaded successfully.");
        } else {
            addLog("[Error] Models folder found, but loading failed. Check console.");
        }
    } else {
        addLog("[Error] Could not find 'models_onnx' in current or parent directories.");
        addLog("Checked up to: " + search_ptr.string());
        addLog("Please ensure 'models_onnx' is in the project root.");
    }
    return true;
}

void GUIManager::run() {
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        renderUI();
        ImGui::Render();
        int dw, dh;
        glfwGetFramebufferSize(window, &dw, &dh);
        glViewport(0, 0, dw, dh);
        glClearColor(0.08f, 0.08f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }
}

void GUIManager::renderWaveform(const char* label, const std::vector<float>& data, ImVec4 color) {
    if (ImPlot::BeginPlot(label, ImVec2(-1, 180), ImPlotFlags_NoMenus)) {
        ImPlot::SetupAxes(nullptr, nullptr, ImPlotAxisFlags_NoDecorations, ImPlotAxisFlags_NoDecorations);
        ImPlot::SetupAxisLimits(ImAxis_Y1, -1.1, 1.1, ImGuiCond_Always);
        ImPlot::SetNextLineStyle(color);
        
        std::lock_guard<std::mutex> lock(data_mutex);
        if (!data.empty()) {
            ImPlot::PlotLine("Wave", data.data(), (int)data.size());
        }
        ImPlot::EndPlot();
    }
}

void GUIManager::renderUI() {
    ImGui::SetNextWindowPos(ImVec2(0,0));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    ImGui::Begin("Dashboard", nullptr, ImGuiWindowFlags_NoDecoration);

    // Language Toggle in Top-Right
    ImGui::SetCursorPos(ImVec2(ImGui::GetWindowWidth() - 150, 5));
    if (ImGui::Button(UIStrings::get("lang_btn", current_lang).c_str(), ImVec2(140, 20))) {
        current_lang = (current_lang == Language::JP) ? Language::EN : Language::JP;
    }

    // Title and System Status
    ImGui::SetCursorPos(ImVec2(10, 5));
    ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), UIStrings::get("app_title", current_lang).c_str());
    
    ImGui::SameLine(ImGui::GetWindowWidth() - 280);
    if (is_processing) {
        ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), UIStrings::get("status_proc", current_lang).c_str());
    } else {
        ImGui::TextColored(ImVec4(0, 1, 0.5f, 1), UIStrings::get("status_ready", current_lang).c_str());
    }
    ImGui::Separator();

    // 1. File Input Section
    ImGui::TextUnformatted(UIStrings::get("sec_1", current_lang).c_str());
    ImGui::SetNextItemWidth(-130);
    ImGui::InputText("##path", input_path, sizeof(input_path));
    ImGui::SameLine();
    if (ImGui::Button(UIStrings::get("browse", current_lang).c_str(), ImVec2(120, 25))) {
        std::string selected = openFileDialog();
        if (!selected.empty()) strncpy(input_path, selected.c_str(), sizeof(input_path) - 1);
    }
    ImGui::TextDisabled(UIStrings::get("tip_dd", current_lang).c_str());

    // 2. Settings Section
    ImGui::Spacing();
    ImGui::TextUnformatted(UIStrings::get("sec_2", current_lang).c_str());
    ImGui::BeginChild("SettingsFrame", ImVec2(0, 130), true);
    
    ImGui::Columns(2, "settings_cols", false);
    ImGui::SetColumnWidth(0, 300);
    
    ImGui::SliderFloat(UIStrings::get("denoise", current_lang).c_str(), &denoise_strength, 0.0f, 1.0f, "%.2f");
    static int solver_steps = 32;
    static float tau = 0.5f;
    ImGui::SliderInt(UIStrings::get("steps", current_lang).c_str(), &solver_steps, 16, 128);
    ImGui::SliderFloat(UIStrings::get("tau", current_lang).c_str(), &tau, 0.0f, 1.0f, "%.2f");
    
    ImGui::NextColumn();
    
    if (ImGui::Button(UIStrings::get("load", current_lang).c_str(), ImVec2(180, 40))) {
        std::vector<float> tmp;
        if (pimpl->processor.loadAudio(input_path, 44100, tmp)) {
            std::lock_guard<std::mutex> lock(data_mutex);
            original_waveform = std::move(tmp);
            addLog(UIStrings::get("log_loaded", current_lang));
        }
    }
    if (ImGui::Button(UIStrings::get("enhance", current_lang).c_str(), ImVec2(180, 40)) && !is_processing) {
        if (!original_waveform.empty()) {
            pimpl->enhancer.setParameters(solver_steps, tau);
            processAudio();
        } else {
            addLog(UIStrings::get("log_err_load", current_lang));
        }
    }

    ImGui::Columns(1);
    ImGui::EndChild();

    // 3. Visualization & Output Section
    ImGui::Spacing();
    ImGui::TextUnformatted(UIStrings::get("sec_3", current_lang).c_str());
    
    if (is_processing) {
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.2f, 0.6f, 1.0f, 1.0f));
        ImGui::ProgressBar(progress, ImVec2(-1, 20));
        ImGui::PopStyleColor();
    }

    // Waveforms
    if (!original_waveform.empty()) {
        renderWaveform(UIStrings::get("wave_orig", current_lang).c_str(), original_waveform, ImVec4(0.4f, 0.6f, 1.0f, 1.0f));
    }
    
    if (!enhanced_waveform.empty()) {
        renderWaveform(UIStrings::get("wave_enh", current_lang).c_str(), enhanced_waveform, ImVec4(0.4f, 1.0f, 0.4f, 1.0f));
    }

    // Buttons Row
    ImGui::Spacing();
    bool has_enhanced = !enhanced_waveform.empty();
    if (has_enhanced) {
        if (ImGui::Button(UIStrings::get("save", current_lang).c_str(), ImVec2(120, 35))) {
            char filename[MAX_PATH] = "enhanced_output.wav";
            OPENFILENAMEA ofn;
            ZeroMemory(&ofn, sizeof(ofn));
            ofn.lStructSize = sizeof(ofn);
            ofn.lpstrFile = filename;
            ofn.nMaxFile = MAX_PATH;
            ofn.lpstrFilter = "WAV Files\0*.wav\0";
            ofn.Flags = OFN_OVERWRITEPROMPT;
            if (GetSaveFileNameA(&ofn)) {
                std::lock_guard<std::mutex> lock(data_mutex);
                pimpl->processor.saveAudio(filename, enhanced_waveform, 44100, "wav");
                addLog(UIStrings::get("log_saved", current_lang));
            }
        }
        ImGui::SameLine();
        if (ImGui::Button(UIStrings::get("play_enh", current_lang).c_str(), ImVec2(140, 35))) {
            std::lock_guard<std::mutex> lock(data_mutex);
            pimpl->processor.saveAudio("temp_enh.wav", enhanced_waveform, 44100, "wav");
            #ifdef _WIN32
            PlaySoundA("temp_enh.wav", NULL, SND_FILENAME | SND_ASYNC);
            #endif
        }
        ImGui::SameLine();
    }
    
    if (!original_waveform.empty()) {
        if (ImGui::Button(UIStrings::get("play_orig", current_lang).c_str(), ImVec2(140, 35))) {
            std::lock_guard<std::mutex> lock(data_mutex);
            pimpl->processor.saveAudio("temp_orig.wav", original_waveform, 44100, "wav");
            #ifdef _WIN32
            PlaySoundA("temp_orig.wav", NULL, SND_FILENAME | SND_ASYNC);
            #endif
        }
        ImGui::SameLine();
        if (ImGui::Button(UIStrings::get("stop", current_lang).c_str(), ImVec2(80, 35))) {
            #ifdef _WIN32
            PlaySoundA(NULL, NULL, 0);
            #endif
        }
    }

    // 4. Log Section
    ImGui::Spacing();
    ImGui::TextUnformatted(UIStrings::get("sec_4", current_lang).c_str());
    ImGui::SameLine();
    if (ImGui::Button(UIStrings::get("copy", current_lang).c_str(), ImVec2(100, 20))) {
        std::string all_logs;
        {
            std::lock_guard<std::mutex> lock(log_mutex);
            for (const auto& l : logs) all_logs += l + "\n";
        }
        ImGui::SetClipboardText(all_logs.c_str());
    }

    ImGui::BeginChild("LogRegion", ImVec2(0, 250), true, ImGuiWindowFlags_HorizontalScrollbar);
    {
        std::lock_guard<std::mutex> lock(log_mutex);
        for (const auto& l : logs) {
            if (l.find("[Error]") != std::string::npos)
                ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), l.c_str());
            else if (l.find("[System]") != std::string::npos)
                ImGui::TextColored(ImVec4(0.4f, 1, 0.4f, 1), l.c_str());
            else
                ImGui::TextUnformatted(l.c_str());
        }
        
        static int last_log_size = 0;
        if ((int)logs.size() > last_log_size) {
            ImGui::SetScrollHereY(1.0f);
            last_log_size = (int)logs.size();
        }
    }
    ImGui::EndChild();
    ImGui::End();
}

void GUIManager::processAudio() {
    if (original_waveform.empty()) return;
    is_processing = true;
    std::thread([this]() {
        addLog("[AI] Starting Enhancement Pipeline...");
        std::vector<float> input_copy;
        float strength;
        {
            std::lock_guard<std::mutex> lock(data_mutex);
            input_copy = original_waveform;
            strength = denoise_strength;
        }

        if (input_copy.empty()) {
            addLog("[AI] Error: No input audio.");
            is_processing = false;
            return;
        }

        std::vector<float> output;
        try {
            progress = 0.0f;
            auto cb = [this](float p) { progress = p; };
            if (pimpl->enhancer.process(input_copy, strength, output, cb)) {
                {
                    std::lock_guard<std::mutex> lock(data_mutex);
                    enhanced_waveform = std::move(output);
                }
                addLog("[AI] Success! Audio enhanced.");
                pimpl->processor.saveAudio("enhanced_output.wav", enhanced_waveform, 44100, "wav");
                addLog("[AI] Saved to enhanced_output.wav");
            } else {
                addLog("[AI] Processing failed. Check console for details.");
            }
        } catch (const std::exception& e) {
            addLog("[AI] EXCEPTION: " + std::string(e.what()));
        } catch (...) {
            addLog("[AI] CRITICAL: Unknown Error.");
        }
        progress = 0.0f;
        is_processing = false;
    }).detach();
}

void GUIManager::cleanup() {
    if (window) {
        ImPlot::DestroyContext();
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        glfwDestroyWindow(window);
        glfwTerminate();
        window = nullptr;
    }
}
void GUIManager::renderLog() {}
