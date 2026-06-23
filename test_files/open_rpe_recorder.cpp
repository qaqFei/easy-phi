#include "test_shared.hpp"
#include "win32ui.hpp"
#include "regapi.hpp"

#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shlobj.h>
#include <thread>

struct KeepingCWD {
    KeepingCWD() {
        cwd = std::filesystem::current_path();
    }

    ~KeepingCWD() {
        try {
            std::filesystem::current_path(cwd);
        } catch (...) {}
    }

    private:
    std::filesystem::path cwd;
};

std::string wstringToString(const std::wstring& wstr) {
    if (wstr.empty()) return {};
    
    int size_needed = WideCharToMultiByte(
        CP_UTF8,
        0,
        wstr.c_str(),
        static_cast<int>(wstr.length()),
        nullptr,
        0,
        nullptr,
        nullptr
    );
    
    if (size_needed == 0) return {};
    
    std::string result(size_needed, '\0');
    
    int bytes_converted = WideCharToMultiByte(
        CP_UTF8,
        0,
        wstr.c_str(),
        static_cast<int>(wstr.length()),
        result.data(),
        size_needed,
        nullptr,
        nullptr
    );
    
    if (bytes_converted == 0) return {};
    result.resize(bytes_converted);
    return result;
}

std::string selectOpenFile(Win32Window* win, const wchar_t* filter, const wchar_t* title) {
    KeepingCWD kcwd;

    HMODULE comdlg32 = LoadLibraryA("comdlg32.dll");
    wchar_t buf[MAX_PATH] = {0};
    OPENFILENAMEW ofn { sizeof(ofn) };
    ofn.hwndOwner = win ? win->hWnd : NULL;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = buf;
    ofn.lpstrTitle = title;
    ofn.nMaxFile = sizeof(buf);
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    BOOL res = ((BOOL(*)(OPENFILENAMEW*))GetProcAddress(comdlg32, "GetOpenFileNameW"))(&ofn);

    if (!res) return {};
    return wstringToString(std::wstring(buf));
}

std::string selectSaveFile(Win32Window* win, const wchar_t* filter, const wchar_t* title) {
    KeepingCWD kcwd;

    HMODULE comdlg32 = LoadLibraryA("comdlg32.dll");
    wchar_t buf[MAX_PATH] = {0};
    OPENFILENAMEW ofn { sizeof(ofn) };
    ofn.hwndOwner = win ? win->hWnd : NULL;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = buf;
    ofn.lpstrTitle = title;
    ofn.nMaxFile = sizeof(buf);
    ofn.Flags = OFN_OVERWRITEPROMPT;
    BOOL res = ((BOOL(*)(OPENFILENAMEW*))GetProcAddress(comdlg32, "GetSaveFileNameW"))(&ofn);

    if (!res) return {};
    return wstringToString(std::wstring(buf));
}

std::string selectFolder(Win32Window* win, const wchar_t* title) {
    KeepingCWD kcwd;

    wchar_t buf[MAX_PATH] = {0};
    BROWSEINFOW bi  = { 0 };
    bi.hwndOwner = win ? win->hWnd : NULL;
    bi.pszDisplayName = buf;
    bi.lpszTitle = title;
    bi.ulFlags = BIF_RETURNONLYFSDIRS;
    LPITEMIDLIST idl = SHBrowseForFolderW(&bi);
    if (!idl) return {};
    SHGetPathFromIDListW(idl, buf);
    return wstringToString(std::wstring(buf));
}

void showErrorMsg(Win32Window* win, const wchar_t* msg) {
    MessageBoxW(win ? win->hWnd : NULL, msg, L"Error", MB_OK | MB_ICONERROR);
}

void showWarnMsg(Win32Window* win, const wchar_t* msg) {
    MessageBoxW(win ? win->hWnd : NULL, msg, L"Warning", MB_OK | MB_ICONWARNING);
}

void showInfoMsg(Win32Window* win, const wchar_t* msg) {
    MessageBoxW(win ? win->hWnd : NULL, msg, L"Info", MB_OK | MB_ICONINFORMATION);
}

std::string getDirectory(const std::string& path) {
    auto pos = path.find_last_of("\\/");
    if (pos == std::string::npos) return {};
    return path.substr(0, pos);
}

bool showYesNoMsg(Win32Window* win, const wchar_t* msg) {
    return MessageBoxW(win ? win->hWnd : NULL, msg, L"Confirm", MB_YESNO | MB_ICONQUESTION) == IDYES;
}

struct ProgressDialog {
    IProgressDialog* pd;

    ProgressDialog() {
        CoInitialize(nullptr);
        
        CoCreateInstance(
            CLSID_ProgressDialog, nullptr, CLSCTX_ALL,
            IID_IProgressDialog, (void**)&pd
        );
    }

    void setTitle(const wchar_t* title) {
        pd->SetTitle(title);
    }

    void setLine(int line, const wchar_t* text) {
        pd->SetLine(1, text, FALSE, nullptr);
    }

    void setCancelMsg(const wchar_t* msg) {
        pd->SetCancelMsg(msg, nullptr);
    }

    void setProgress(int current, int total) {
        pd->SetProgress(current, total);
    }

    void start() {
        pd->StartProgressDialog(
            nullptr, nullptr,
            PROGDLG_NORMAL | PROGDLG_AUTOTIME | PROGDLG_NOCANCEL,
            nullptr
        );
    }

    void close() {
        if (!pd) return;
        pd->StopProgressDialog();
        pd->Release();
        CoUninitialize();
        pd = nullptr;
    }

    ~ProgressDialog() {
        close();
    }
};

struct Settings {
    static constexpr const wchar_t* appKey = L"Open-RPE-Recorder";
    static constexpr DWORD currentVersion = 0;

    double musicVol = 1.0, sfxVol = 1.0;
    double noteScaling = 1.0;

    int recordWidth = 1920, recordHeight = 1080;
    double recordFPS = 60.0;
    bool recordSfxRandshake = false;

    bool disableH264QSV = false;

    void fromRegistry() {
        RegAPI api(appKey);
        
        DWORD lastVersion;
        if (api.readDword(L"version", lastVersion)) {
            if (lastVersion != currentVersion) {
                showWarnMsg(nullptr, L"配置文件版本过新, 可能会导致部分配置丢失");
            }
        }
        api.writeDword(L"version", currentVersion);

        api.readDouble(L"musicVol", musicVol);
        api.readDouble(L"sfxVol", sfxVol);
        api.readDouble(L"noteScaling", noteScaling);

        api.readDword(L"recordWidth", recordWidth);
        api.readDword(L"recordHeight", recordHeight);
        api.readDouble(L"recordFPS", recordFPS);
        api.readBool(L"recordSfxRandshake", recordSfxRandshake);

        clampValues();
    }

    void saveToRegistry() {
        RegAPI api(appKey);

        api.writeDouble(L"musicVol", musicVol);
        api.writeDouble(L"sfxVol", sfxVol);
        api.writeDouble(L"noteScaling", noteScaling);

        api.writeDword(L"recordWidth", recordWidth);
        api.writeDword(L"recordHeight", recordHeight);
        api.writeDouble(L"recordFPS", recordFPS);
        api.writeBool(L"recordSfxRandshake", recordSfxRandshake);
    }

    void clampValues() {
        musicVol = std::clamp<double>(musicVol, 0.0, 1.0);
        sfxVol = std::clamp<double>(sfxVol, 0.0, 1.0);

        recordWidth = std::clamp<int>(recordWidth, 1, 65536);
        recordHeight = std::clamp<int>(recordHeight, 1, 65536);
        recordFPS = std::clamp<double>(recordFPS, 0.01, 65536.0);
    }

    void onChanged() {
        clampValues();
        saveToRegistry();
    }

    bool getIsFirstRun() {
        RegAPI api(appKey);

        bool isFirstRun = true;
        api.readBool(L"isFirstRunFlag", isFirstRun);
        api.writeBool(L"isFirstRunFlag", false);
        return isFirstRun;
    }
};

int main() {
    PhiWindow backendWin {};
    backendWin.base.hidden = true;
    backendWin.init();

    std::string chartRoot = "",
                chartPath = "",
                imagePath = "",
                audioPath = "";

    std::optional<easy_phi::ParsedRPEChartInfo> chartInfo;

    int chartFileInput, chartRootInput, bgFileInput, audioFileInput;
    int musicVolInput, sfxVolInput;
    int noteScalingInput;
    int recordWidthInput, recordHeightInput, recordFPSInput;
    int recordSfxRandshakeCheckBox;

    Settings settings {};
    settings.fromRegistry();

    auto win = Win32Window::Make({
        .title = L"Open RPE Recorder"
    });

    win->registerWidget(Widgets::Button({ .text = L"↗Github", .onClick = [&]() {
        ShellExecute(nullptr, "open", BUILD_REPO_GITHUB, nullptr, nullptr, SW_SHOWNORMAL);
    } }));
    win->nextRow();

    win->registerWidget(Widgets::Label({ .text = L"谱面选择" }));
    win->nextRow();

    win->registerWidget(Widgets::Button({ .text = L"从 info.txt ...", .onClick = [&]() {
        auto infoPath = selectOpenFile(win.get(),  L"Info File (info.txt)\0info.txt\0All Files (*.*)\0*.*\0", L"打开 info.txt");
        if (infoPath.empty()) return;

        auto chartRoot = getDirectory(infoPath) + "/";
        WidgetStatics::TextInput::setText(win->refWidget(chartRootInput), Win32Utils::stringToWstring(chartRoot));

        easy_phi::Data infoData;
        if (!easy_phi::Data::MakeFromFile(infoData, infoPath)) {
            showErrorMsg(win.get(), L"无法打开 info.txt");
            return;
        }

        auto infos = easy_phi::ParsedRPEChartInfo::parse(infoData);
        if (infos.empty()) {
            showErrorMsg(win.get(), L"info.txt 中找不到谱面信息");
        }

        auto& info = infos[0];
        WidgetStatics::TextInput::setText(win->refWidget(chartFileInput), Win32Utils::stringToWstring(chartRoot + info.chart));
        WidgetStatics::TextInput::setText(win->refWidget(bgFileInput), Win32Utils::stringToWstring(chartRoot + info.picture));
        WidgetStatics::TextInput::setText(win->refWidget(audioFileInput), Win32Utils::stringToWstring(chartRoot + info.song));
        chartInfo = info;
    } }));
    win->nextRow();

    win->registerWidget(Widgets::Label({ .text = L"谱面根目录: " }));
    chartRootInput = win->registerWidget(Widgets::TextInput({ .text = L"", .onChange = [&](const std::wstring& ws) {
        chartRoot = Win32Utils::wstringToString(ws);
    } }));
    win->registerWidget(Widgets::Button({ .text = L"浏览", .onClick = [&]() {
        auto folder = selectFolder(win.get(), L"打开谱面根目录");
        if (folder.empty()) return;
        WidgetStatics::TextInput::setText(win->refWidget(chartRootInput), Win32Utils::stringToWstring(folder));
    } }));
    win->nextRow();

    win->registerWidget(Widgets::Label({ .text = L"谱面文件: " }));
    chartFileInput = win->registerWidget(Widgets::TextInput({ .text = L"", .onChange = [&](const std::wstring& ws) {
        chartPath = Win32Utils::wstringToString(ws);
    } }));
    win->registerWidget(Widgets::Button({ .text = L"浏览", .onClick = [&]() {
        auto path = selectOpenFile(win.get(),  L"Chart File (*.json)\0*.json\0All Files (*.*)\0*.*\0", L"打开谱面文件");
        if (path.empty()) return;
        WidgetStatics::TextInput::setText(win->refWidget(chartFileInput), Win32Utils::stringToWstring(path));
    } }));
    win->nextRow();

    win->registerWidget(Widgets::Label({ .text = L"曲绘文件: " }));
    bgFileInput = win->registerWidget(Widgets::TextInput({ .text = L"", .onChange = [&](const std::wstring& ws) {
        imagePath = Win32Utils::wstringToString(ws);
    } }));
    win->registerWidget(Widgets::Button({ .text = L"浏览", .onClick = [&]() {
        auto path = selectOpenFile(win.get(),  L"Image File (*.png, *.jpg, *.jpeg)\0*.png;*.jpg;*.jpeg\0All Files (*.*)\0*.*\0", L"打开曲绘文件");
        if (path.empty()) return;
        WidgetStatics::TextInput::setText(win->refWidget(bgFileInput), Win32Utils::stringToWstring(path));
    } }));
    win->nextRow();

    win->registerWidget(Widgets::Label({ .text = L"音频文件: " }));
    audioFileInput = win->registerWidget(Widgets::TextInput({ .text = L"", .onChange = [&](const std::wstring& ws) {
        audioPath = Win32Utils::wstringToString(ws);
    } }));
    win->registerWidget(Widgets::Button({ .text = L"浏览", .onClick = [&]() {
        auto path = selectOpenFile(win.get(),  L"Audio File (*.mp3, *.wav, *.ogg, *.flac, *.m4a, *.aac)\0*.mp3;*.wav;*.ogg;*.flac;*.m4a;*.aac\0All Files (*.*)\0*.*\0", L"打开音频文件");
        if (path.empty()) return;
        WidgetStatics::TextInput::setText(win->refWidget(audioFileInput), Win32Utils::stringToWstring(path));
    } }));
    win->nextRow();

    win->nextRow();

    bool isSyncingSettings = true;

    auto syncSettingsToUI = [&](bool allowEmpty = false) {
        isSyncingSettings = true;

        auto checkbox = [&](int id, bool checked) {
            WidgetStatics::CheckBox::toggle(win->refWidget(id), checked);
        };

        auto doubleInput = [&](int id, double value) {
            auto& text = win->refWidget(id).store.textInput.text;

            try {
                if ((allowEmpty && text.empty()) || std::stold(text) == value) {
                    return;
                }
            } catch (...) { }

            WidgetStatics::TextInput::setText(win->refWidget(id), Win32Utils::stringToWstring(std::format("{:.10g}", value)));
        };

        auto intInput = [&](int id, int value) {
            auto& text = win->refWidget(id).store.textInput.text;

            try {
                if ((allowEmpty && text.empty()) || std::stoi(text) == value) {
                    return;
                }
            } catch (...) { }

            WidgetStatics::TextInput::setText(win->refWidget(id), std::to_wstring(value));
        };

        doubleInput(musicVolInput, settings.musicVol);
        doubleInput(sfxVolInput, settings.sfxVol);
        doubleInput(noteScalingInput, settings.noteScaling);

        intInput(recordWidthInput, settings.recordWidth);
        intInput(recordHeightInput, settings.recordHeight);
        doubleInput(recordFPSInput, settings.recordFPS);
        checkbox(recordSfxRandshakeCheckBox, settings.recordSfxRandshake);

        isSyncingSettings = false;
    };

    auto settingsChanged = [&]() {
        settings.onChanged();
        syncSettingsToUI(true);
    };

    win->registerWidget(Widgets::Label({ .text = L"设置 (通用)" }));
    win->nextRow();

    win->registerWidget(Widgets::Label({ .text = L"音乐音量: " }));
    musicVolInput = win->registerWidget(Widgets::TextInput({ .text = L"", .onChange = [&](const std::wstring& ws) {
        if (isSyncingSettings) return;
        try { settings.musicVol = std::stold(ws); }
        catch (...) { }
        settingsChanged();
    }, .onUnfocus = syncSettingsToUI }));
    win->nextRow();

    win->registerWidget(Widgets::Label({ .text = L"音效音量: " }));
    sfxVolInput = win->registerWidget(Widgets::TextInput({ .text = L"", .onChange = [&](const std::wstring& ws) {
        if (isSyncingSettings) return;
        try { settings.sfxVol = std::stold(ws); }
        catch (...) { }
        settingsChanged();
    }, .onUnfocus = syncSettingsToUI }));
    win->nextRow();

    win->registerWidget(Widgets::Label({ .text = L"音符缩放: " }));
    noteScalingInput = win->registerWidget(Widgets::TextInput({ .text = L"", .onChange = [&](const std::wstring& ws) {
        if (isSyncingSettings) return;
        try { settings.noteScaling = std::stold(ws); }
        catch (...) { }
        settingsChanged();
    }, .onUnfocus = syncSettingsToUI }));
    win->nextRow();

    win->nextRow();

    win->registerWidget(Widgets::Label({ .text = L"设置 (视频参数)" }));
    win->nextRow();

    win->registerWidget(Widgets::Label({ .text = L"分辨率: " }));
    recordWidthInput = win->registerWidget(Widgets::TextInput({ .text = L"", .onChange = [&](const std::wstring& ws) {
        if (isSyncingSettings) return;
        try { settings.recordWidth = std::stoi(ws); }
        catch (...) { }
        settingsChanged();
    }, .onUnfocus = syncSettingsToUI }));
    win->registerWidget(Widgets::Label({ .text = L"x" }));
    recordHeightInput = win->registerWidget(Widgets::TextInput({ .text = L"", .onChange = [&](const std::wstring& ws) {
        if (isSyncingSettings) return;
        try { settings.recordHeight = std::stoi(ws); }
        catch (...) { }
        settingsChanged();
    }, .onUnfocus = syncSettingsToUI }));
    win->nextRow();

    win->registerWidget(Widgets::Label({ .text = L"帧率: " }));
    recordFPSInput = win->registerWidget(Widgets::TextInput({ .text = L"", .onChange = [&](const std::wstring& ws) {
        if (isSyncingSettings) return;
        try { settings.recordFPS = std::stold(ws); }
        catch (...) { }
        settingsChanged();
    }, .onUnfocus = syncSettingsToUI }));
    win->nextRow();

    recordSfxRandshakeCheckBox = win->registerWidget(Widgets::CheckBox({ .text = L"打击音随机抖动", .onChange = [&](bool checked) {
        if (isSyncingSettings) return;
        settings.recordSfxRandshake = checked;
        settingsChanged();
    } }));
    win->registerWidget(Widgets::Button({ .text = L"?", .onClick = [&]() {
        showInfoMsg(win.get(), L"由于本家即使同时打击音符, 打击音效也并不是在同一时刻播放, 启用该选项后, 打击音效会在一定范围内随机延迟播放, 以模拟本家多押的神秘听感。");
    } }));
    win->nextRow();

    win->nextRow();

    double loadingChartTook;

    auto load = [&]() {
        try {
            auto loadResult = backendWin.loadChart(chartPath, chartRoot);
            loadingChartTook = loadResult.totalTook();

            if (chartInfo.has_value()) {
                auto& info = chartInfo.value();
                backendWin.renderer->chart.meta.title = info.name;
                backendWin.renderer->chart.meta.difficulty = info.level;
            }

            backendWin.renderer->chart.options.noteScaling *= settings.noteScaling;
            backendWin.renderer->loadIllustion(imagePath);
            backendWin.renderer->audioManager.load(audioPath);
        } catch (const std::exception& e) {
            auto msg = Win32Utils::stringToWstring(e.what());
            showErrorMsg(win.get(), msg.c_str());
            return false;
        }

        return true;
    };

    struct WinHiddenGuard {
        Win32Window* win;
        WinHiddenGuard(Win32Window* win) : win(win) { win->setHidden(true); }
        ~WinHiddenGuard() { win->setHidden(false); }
    };

    win->registerWidget(Widgets::Button({ .text = L"预览", .onClick = [&]() {
        if (!load()) return;
        WinHiddenGuard whguard(win.get());

        backendWin.base.setHidden(false);
        backendWin.base.setVSync(true);
        backendWin.renderer->audioManager.startBgm();

        backendWin.renderer->audioManager.setBgmVolume(settings.musicVol);
        backendWin.renderer->audioManager.setSfxVolume(settings.sfxVol);

        while (!backendWin.renderer->audioManager.getBpmIsEnded()) {
            if (!backendWin.mainloopFrame({})) {
                backendWin.renderer->audioManager.stopBgm();
                break;
            }
        }

        backendWin.base.setHidden(true);
    } }));
    win->registerWidget(Widgets::Button({ .text = L"渲染视频", .onClick = [&]() {
        auto videoPath = selectSaveFile(win.get(),  L"视频文件 (*.mp4)\0*.mp4\0All Files (*.*)\0*.*\0", L"保存视频文件");
        if (videoPath.empty()) return;
        ProgressDialog pd {};
        pd.setTitle(L"Open RPE Record -- 渲染视频");
        pd.start();

        pd.setLine(1, L"初始化...");
        WinHiddenGuard whguard(win.get());

        VideoCap cap {};
        cap.init(videoPath, settings.recordWidth, settings.recordHeight, settings.recordFPS);
        
        using FrameType = std::optional<uint64_t>;
        using FrameQueueType = ThreadSafeQueue<FrameType>;
        struct UserData {
            VideoCap* cap;
            FrameQueueType* frameQueue;
        };

        FrameQueueType frameQueue;

        auto videoRecorder = VideoRecorder::Make(
            backendWin.base.glCtx,
            settings.recordWidth, settings.recordHeight,
            [&](uint64_t slotIndex) {
                frameQueue.enqueue(slotIndex);
            },
            {
                .callbackIsThreadSafe = true
            }
        );

        auto frameWriter = [&]() {
            FrameType frame;
            while (true) {
                frameQueue.wait_dequeue(frame);
                if (!frame.has_value()) break;

                auto* yuv = videoRecorder->referYUVFrame(frame.value());
                cap.writeVideoFrame(yuv->dataPtrs().data(), yuv->rowBytes().data());
                videoRecorder->returnYUVFrame(frame.value());
            }
        };

        backendWin.base.width = settings.recordWidth;
        backendWin.base.height = settings.recordHeight;

        pd.setLine(1, L"加载...");
        if (!load()) return;
        
        double renderSt = globalTimer();

        pd.setLine(1, L"渲染音频...");
        auto mixedAudio = backendWin.renderer->mixFinalBgm(backendWin.renderer->chart, {
            .musicVol = settings.musicVol,
            .sfxVol = settings.sfxVol,
            .sfxRandshake = settings.recordSfxRandshake
        });

        pd.setLine(1, L"写入音频...");
        cap.writeAudio(mixedAudio);

        pd.setLine(1, L"渲染视频...");
        uint64_t frameCut = 0;
        UserData ud {};
        ud.cap = &cap;
        ud.frameQueue = &frameQueue;
        std::thread frameWriterThread(frameWriter);
        uint64_t surfaceIndex = 0;
        FramerateMeter fpsMeter {};
        
        while (true) {
            double t = frameCut / settings.recordFPS;
            if (t > backendWin.renderer->calcConfig.songLength) break;

            auto reGuard = videoRecorder->useFrame();
            backendWin.mainloopFrame({
                .base = {
                    .time = t,
                    .isRenderingVideo = true
                }
            });

            frameCut++;
            fpsMeter.frame();

            uint64_t totalFrames = backendWin.renderer->calcConfig.songLength * settings.recordFPS;
            pd.setProgress(frameCut, totalFrames);

            {
                std::wstring msg = L"渲染视频... ";
                msg += std::to_wstring(frameCut) + L"/" + std::to_wstring(totalFrames) + L" (" + std::to_wstring((uint64_t)fpsMeter.get()) + L" fps)";
                pd.setLine(1, msg.c_str());
            }
        }

        videoRecorder->finish();

        frameQueue.enqueue(std::nullopt);
        frameWriterThread.join();

        pd.setLine(1, L"释放资源...");
        std::wstring msg(L"渲染完成, 已保存到 ");
        msg += Win32Utils::stringToWstring(videoPath);
        showInfoMsg(win.get(), msg.c_str());
    } }));

    win->createWidgets();
    win->doGrid();
    win->resizeToGridBounds();
    syncSettingsToUI();

    win->mainloop();
    return 0;
}
