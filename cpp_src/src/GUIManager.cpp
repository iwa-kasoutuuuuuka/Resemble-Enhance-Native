#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")
#endif

#include "GUIManager.hpp"
#include "AIEnhancer.hpp"
#include "AudioProcessor.hpp"
#include "DSPModule.hpp"
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
        models_dir = found_path.string(); // Save for reload
        addLog("[System] Found models at: " + fs::absolute(found_path).string());
        pimpl->enhancer.setLogCallback([this](const std::string& msg) { addLog(msg); });
        if (pimpl->enhancer.loadModels(models_dir, use_gpu)) {
            addLog(use_gpu ? "[System] AI Models loaded OK (GPU: DirectML)" : "[System] AI Models loaded OK (CPU Mode)");
        } else {
            addLog("[Error] Models folder found, but loading failed.");
        }
    } else {
        addLog("[Error] Could not find 'models_onnx'.");
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
    if (ImPlot::BeginPlot(label, ImVec2(-1, 140), ImPlotFlags_NoMenus)) {
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

void GUIManager::renderSpectrogram(const char* label, const std::vector<float>& data, int w, int h) {
    if (w <= 0 || h <= 0 || data.empty()) return;
    if (ImPlot::BeginPlot(label, ImVec2(-1, 180), ImPlotFlags_NoMenus)) {
        ImPlot::SetupAxes(nullptr, nullptr, ImPlotAxisFlags_NoDecorations, ImPlotAxisFlags_NoDecorations);
        ImPlot::PlotHeatmap("Spec", data.data(), h, w, 0.0f, 1.0f, nullptr, ImPlotPoint(0,0), ImPlotPoint(w,h));
        ImPlot::EndPlot();
    }
}

void GUIManager::renderUI() {
    ImGui::SetNextWindowPos(ImVec2(0,0));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    ImGui::Begin("Dashboard", nullptr, ImGuiWindowFlags_NoDecoration);

    // Language Toggle
    ImGui::SetCursorPos(ImVec2(ImGui::GetWindowWidth() - 150, 5));
    if (ImGui::Button(UIStrings::get("lang_btn", current_lang).c_str(), ImVec2(140, 20))) {
        current_lang = (current_lang == Language::JP) ? Language::EN : Language::JP;
    }

    // Title and Hardware Status
    ImGui::SetCursorPos(ImVec2(10, 5));
    ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), UIStrings::get("app_title", current_lang).c_str());
    
    ImGui::SameLine(ImGui::GetWindowWidth() - 350);
    if (use_gpu) {
        ImGui::TextColored(ImVec4(0.4f, 1, 0.4f, 1), UIStrings::get("device_gpu", current_lang).c_str());
    } else {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1), UIStrings::get("device_cpu", current_lang).c_str());
    }

    ImGui::SameLine(ImGui::GetWindowWidth() - 250);
    if (is_processing) {
        ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), UIStrings::get("status_proc", current_lang).c_str());
    } else {
        ImGui::TextColored(ImVec4(0, 1, 0.5f, 1), UIStrings::get("status_ready", current_lang).c_str());
    }
    
    ImGui::Separator();

    // Main Content with Tabs
    if (ImGui::BeginTabBar("MainTabs")) {
        // TAB 1: SINGLE FILE
        if (ImGui::BeginTabItem(UIStrings::get("tab_single", current_lang).c_str())) {
            is_batch_mode = false;
            
            ImGui::Spacing();
            ImGui::TextUnformatted(UIStrings::get("sec_1", current_lang).c_str());
            ImGui::SetNextItemWidth(-130);
            ImGui::InputText("##path", input_path, sizeof(input_path));
            ImGui::SameLine();
            if (ImGui::Button(UIStrings::get("browse", current_lang).c_str(), ImVec2(120, 25))) {
                std::string selected = openFileDialog();
                if (!selected.empty()) strncpy(input_path, selected.c_str(), sizeof(input_path) - 1);
            }

            if (ImGui::Button(UIStrings::get("load", current_lang).c_str(), ImVec2(120, 30))) {
                std::vector<float> tmp;
                if (pimpl->processor.loadAudio(input_path, 44100, tmp)) {
                    std::lock_guard<std::mutex> lock(data_mutex);
                    original_waveform = std::move(tmp);
                    // Generate Spectrogram
                    DSPModule::STFTConfig cfg = { 2048, 420, 2048 };
                    auto stft = DSPModule::extractSTFT(original_waveform, cfg);
                    original_spectrogram = stft.magnitude;
                    spec_w = stft.n_frames;
                    spec_h = stft.n_bins;
                    addLog(UIStrings::get("log_loaded", current_lang));
                }
            }
            ImGui::SameLine();
            if (ImGui::Button(UIStrings::get("enhance", current_lang).c_str(), ImVec2(150, 30)) && !is_processing) {
                if (!original_waveform.empty()) processAudio();
            }

            // Visuals
            if (ImGui::BeginChild("Visuals", ImVec2(0, 350), false)) {
                if (!original_waveform.empty()) {
                    ImGui::TextUnformatted(UIStrings::get("wave_orig", current_lang).c_str());
                    renderWaveform("##orig_wave", original_waveform, ImVec4(0.4f, 0.6f, 1.0f, 1.0f));
                    renderSpectrogram("##orig_spec", original_spectrogram, spec_w, spec_h);
                }
                if (!enhanced_waveform.empty()) {
                    ImGui::TextUnformatted(UIStrings::get("wave_enh", current_lang).c_str());
                    renderWaveform("##enh_wave", enhanced_waveform, ImVec4(0.4f, 1.0f, 0.4f, 1.0f));
                    renderSpectrogram("##enh_spec", enhanced_spectrogram, spec_w, spec_h);
                }
                ImGui::EndChild();
            }
            ImGui::EndTabItem();
        }

        // TAB 2: BATCH MODE
        if (ImGui::BeginTabItem(UIStrings::get("tab_batch", current_lang).c_str())) {
            is_batch_mode = true;
            ImGui::Spacing();
            if (ImGui::Button(UIStrings::get("add_queue", current_lang).c_str(), ImVec2(150, 30))) {
                std::string selected = openFileDialog();
                if (!selected.empty()) batch_queue.push_back(selected);
            }
            ImGui::SameLine();
            if (ImGui::Button(UIStrings::get("clear_queue", current_lang).c_str(), ImVec2(150, 30))) {
                batch_queue.clear();
            }
            ImGui::SameLine();
            if (ImGui::Button(UIStrings::get("start_batch", current_lang).c_str(), ImVec2(150, 30)) && !is_processing) {
                if (!batch_queue.empty()) {
                    std::thread([this]() {
                        is_processing = true;
                        auto queue_copy = batch_queue;
                        for (size_t i = 0; i < queue_copy.size(); ++i) {
                            addLog("[Batch] Processing (" + std::to_string(i+1) + "/" + std::to_string(queue_copy.size()) + "): " + queue_copy[i]);
                            std::vector<float> wav, out;
                            if (pimpl->processor.loadAudio(queue_copy[i], 44100, wav)) {
                                if (pimpl->enhancer.process(wav, denoise_strength, out, nullptr)) {
                                    std::string out_name = std::filesystem::path(queue_copy[i]).replace_extension("").string() + "_enhanced.wav";
                                    pimpl->processor.saveAudio(out_name, out, 44100, "wav");
                                    addLog("[Batch] Saved: " + out_name);
                                }
                            }
                            progress = (float)(i + 1) / queue_copy.size();
                        }
                        is_processing = false;
                        addLog("[System] Batch Processing Complete!");
                    }).detach();
                }
            }

            ImGui::TextUnformatted(UIStrings::get("sec_batch", current_lang).c_str());
            ImGui::BeginChild("BatchList", ImVec2(0, 200), true);
            for (const auto& file : batch_queue) {
                ImGui::BulletText("%s", std::filesystem::path(file).filename().string().c_str());
            }
            ImGui::EndChild();
            ImGui::EndTabItem();
        }

        // TAB 3: SETTINGS
        if (ImGui::BeginTabItem("Settings")) {
            ImGui::Spacing();
            ImGui::SliderFloat(UIStrings::get("denoise", current_lang).c_str(), &denoise_strength, 0.0f, 1.0f, "%.2f");
            static int solver_steps = 32;
            static float tau = 0.5f;
            if (ImGui::SliderInt(UIStrings::get("steps", current_lang).c_str(), &solver_steps, 16, 128)) pimpl->enhancer.setParameters(solver_steps, tau);
            if (ImGui::SliderFloat(UIStrings::get("tau", current_lang).c_str(), &tau, 0.0f, 1.0f, "%.2f")) pimpl->enhancer.setParameters(solver_steps, tau);

            ImGui::Separator();
            if (ImGui::Checkbox(UIStrings::get("gpu_accel", current_lang).c_str(), &use_gpu)) {
                if (!models_dir.empty()) {
                    std::thread([this]() {
                        is_processing = true;
                        addLog("[System] Reloading Models for Hardware Change...");
                        pimpl->enhancer.loadModels(models_dir, use_gpu);
                        is_processing = false;
                    }).detach();
                }
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    // Bottom Bar
    ImGui::SetCursorPos(ImVec2(10, ImGui::GetWindowHeight() - 210));
    ImGui::Separator();
    if (is_processing) ImGui::ProgressBar(progress, ImVec2(-1, 20));

    ImGui::BeginChild("LogRegion", ImVec2(0, 160), true);
    {
        std::lock_guard<std::mutex> lock(log_mutex);
        for (const auto& l : logs) {
            if (l.find("[Error]") != std::string::npos) ImGui::TextColored(ImVec4(1,0.4f,0.4f,1), l.c_str());
            else if (l.find("[System]") != std::string::npos) ImGui::TextColored(ImVec4(0.4f,1,0.4f,1), l.c_str());
            else ImGui::TextUnformatted(l.c_str());
        }
        ImGui::SetScrollHereY(1.0f);
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

        std::vector<float> output;
        progress = 0.0f;
        auto cb = [this](float p) { progress = p; };
        if (pimpl->enhancer.process(input_copy, strength, output, cb)) {
            {
                std::lock_guard<std::mutex> lock(data_mutex);
                enhanced_waveform = std::move(output);
                // Update Enhanced Spectrogram
                DSPModule::STFTConfig cfg = { 2048, 420, 2048 };
                auto stft = DSPModule::extractSTFT(enhanced_waveform, cfg);
                enhanced_spectrogram = stft.magnitude;
            }
            addLog("[AI] Success!");
        } else {
            addLog("[AI] Processing failed.");
        }
        is_processing = false;
        progress = 0.0f;
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
