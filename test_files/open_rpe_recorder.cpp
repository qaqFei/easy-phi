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

    bool enablePerformanceCollection = false;
    double musicVol = 1.0, sfxVol = 1.0;

    int recordWidth = 1920, recordHeight = 1080;
    double recordFPS = 60.0;

    void fromRegistry() {
        RegAPI api(appKey);
        
        DWORD lastVersion;
        if (api.readDword(L"version", lastVersion)) {
            if (lastVersion != currentVersion) {
                showWarnMsg(nullptr, L"配置文件版本过新, 可能会导致部分配置丢失");
            }
        }
        api.writeDword(L"version", currentVersion);

        api.readBool(L"enablePerformanceCollection", enablePerformanceCollection);
        api.readDouble(L"musicVol", musicVol);
        api.readDouble(L"sfxVol", sfxVol);

        api.readDword(L"recordWidth", recordWidth);
        api.readDword(L"recordHeight", recordHeight);
        api.readDouble(L"recordFPS", recordFPS);

        clampValues();
    }

    void saveToRegistry() {
        RegAPI api(appKey);

        api.writeBool(L"enablePerformanceCollection", enablePerformanceCollection);
        api.writeDouble(L"musicVol", musicVol);
        api.writeDouble(L"sfxVol", sfxVol);

        api.writeDword(L"recordWidth", recordWidth);
        api.writeDword(L"recordHeight", recordHeight);
        api.writeDouble(L"recordFPS", recordFPS);
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
    Window backendWin {};
    backendWin.hidden = true;
    backendWin.init();

    std::string chartRoot = "",
                chartPath = "",
                imagePath = "",
                audioPath = "";

    std::optional<easy_phi::ParsedRPEChartInfo> chartInfo;

    int chartFileInput, chartRootInput, bgFileInput, audioFileInput;
    int enablePerformanceCollectionCheckBox;
    int musicVolInput, sfxVolInput;
    int recordWidthInput, recordHeightInput, recordFPSInput;

    Settings settings {};
    settings.fromRegistry();

    auto win = Win32Window::Make({
        .title = L"Open RPE Recorder"
    });

    win->registerWidget(Widgets::Label({ .text = L"谱面选择" }));
    win->nextRow();

    win->registerWidget(Widgets::Button({ .text = L"从 info.txt ...", .onClick = [&]() {
        auto infoPath = selectOpenFile(win.get(),  L"Info File (info.txt)\0info.txt\0All Files (*.*)\0*.*\0", L"打开 info.txt");
        if (infoPath.empty()) return;

        auto chartRoot = getDirectory(infoPath) + "/";
        WidgetStatics::TextInput::setText(win->refWidget(chartRootInput), Win32Utils::stringToWstring(chartRoot));

        easy_phi::Data infoData;
        if (!easy_phi::Data::FromFile(&infoData, infoPath)) {
            showErrorMsg(win.get(), L"无法打开 info.txt");
            return;
        }

        auto infos = easy_phi::parseRPEChartInfo(infoData);
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

            WidgetStatics::TextInput::setText(win->refWidget(id), Win32Utils::stringToWstring(easy_phi::formatToStdString("%.10g", value)));
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

        checkbox(enablePerformanceCollectionCheckBox, settings.enablePerformanceCollection);
        doubleInput(musicVolInput, settings.musicVol);
        doubleInput(sfxVolInput, settings.sfxVol);

        intInput(recordWidthInput, settings.recordWidth);
        intInput(recordHeightInput, settings.recordHeight);
        doubleInput(recordFPSInput, settings.recordFPS);

        isSyncingSettings = false;
    };

    auto settingsChanged = [&]() {
        settings.onChanged();
        syncSettingsToUI(true);
    };

    win->registerWidget(Widgets::Label({ .text = L"设置 (通用)" }));
    win->nextRow();

    enablePerformanceCollectionCheckBox = win->registerWidget(Widgets::CheckBox({ .text = L"收集性能数据到云端", .onChange = [&](bool checked) {
        if (isSyncingSettings) return;
        settings.enablePerformanceCollection = checked;
        settingsChanged();
    } }));
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

    win->registerWidget(Widgets::Label({ .text = L"设置 (录制)" }));
    win->nextRow();

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

    win->nextRow();

    double loadingChartTook;

    auto load = [&]() {
        auto check = [&](std::optional<std::string> res, std::wstring msg) {
            if (res.has_value()) {
                showErrorMsg(win.get(), msg.c_str());
                return false;
            }

            return true;
        };

        if (!check(backendWin.loadChart(chartPath, chartRoot, &loadingChartTook), L"无法加载谱面")) return false;

        if (chartInfo.has_value()) {
            auto& info = chartInfo.value();
            backendWin.chart.meta.title = info.name;
            backendWin.chart.meta.difficulty = info.level;
        }

        if (!check(backendWin.loadBgImage(imagePath), L"无法加载曲绘")) return false;
        if (!check(backendWin.loadMainSound(audioPath), L"无法加载音频")) return false;

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

        TelemetryDeckClient::Performance::ChartPlayback::Completed performanceInfo {
            .baseInfo = TelemetryDeckClient::Performance::BaseInfo::make(),
            .chartHash = backendWin.chart.rawHash,
            .loadingTook = loadingChartTook
        };

        backendWin.setHidden(false);
        backendWin.vsync = true;
        backendWin.resetSurface();
        backendWin.startMainSound();

        ma_sound_set_volume(backendWin.mainSound, settings.musicVol);
        for (auto& [_, sfxs] : backendWin.noteHitsounds) {
            for (auto& sfx : sfxs) {
                ma_sound_set_volume(sfx, settings.sfxVol);
            }
        }

        while (ma_sound_is_playing(backendWin.mainSound)) {
            double t = getMaSoundPosition(backendWin.mainSound);

            if (!backendWin.mainloopFrame(t, {
                .pccfi = &performanceInfo.frames.emplace_back()
            })) {
                ma_sound_stop(backendWin.mainSound);
                break;
            }
        }

        if (settings.enablePerformanceCollection) {
            std::thread([=]() {
                TelemetryDeckClient::Performance::ChartPlayback::completed(performanceInfo);
            }).detach();
        }
        backendWin.setHidden(true);
    } }));
    win->registerWidget(Widgets::Button({ .text = L"渲染视频", .onClick = [&]() {
        auto videoPath = selectSaveFile(win.get(),  L"视频文件 (*.mp4)\0*.mp4\0All Files (*.*)\0*.*\0", L"保存视频文件");
        if (videoPath.empty()) return;
        ProgressDialog pd {};
        pd.setTitle(L"Open RPE Record -- 渲染视频");
        pd.start();

        pd.setLine(1, L"初始化...");
        WinHiddenGuard whguard(win.get());

        backendWin.width = settings.recordWidth;
        backendWin.height = settings.recordHeight;
        backendWin.resetSurface();

        VideoCap cap(videoPath.c_str(), settings.recordWidth, settings.recordHeight, settings.recordFPS);
        
        using FrameType = std::unique_ptr<const SkImage::AsyncReadResult>;
        using FrameQueueType = ThreadSafeQueue<FrameType>;
        struct UserData {
            VideoCap* cap;
            FrameQueueType* frameQueue;
            uint64_t queueMaxSize;
        };

        auto readPixelsCallback = [](void* user, FrameType result) {
            auto& ud = *(UserData*)user;
            if (!result) {
                std::cerr << "Failed to read pixels from SkSurface" << std::endl;
                TerminateProcess(GetCurrentProcess(), EXIT_FAILURE);
            }

            if (result->count() != 3) {
                std::cerr << "Unexpected number of planes: " << result->count() << std::endl;
                TerminateProcess(GetCurrentProcess(), EXIT_FAILURE);
            }

            while (ud.frameQueue->size_approx() >= ud.queueMaxSize) {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
            ud.frameQueue->enqueue(std::move(result));
        };

        FrameQueueType frameQueue;
        auto frameWriter = [&]() {
            FrameType frame;
            while (true) {
                frameQueue.wait_dequeue(frame);
                if (!frame) break;
                
                cap.writeVideoFrame(
                    (void*)frame->data(0),
                    (void*)frame->data(1),
                    (void*)frame->data(2),
                    frame->rowBytes(0),
                    frame->rowBytes(1),
                    frame->rowBytes(2)
                );
            }
        };

        uint64_t surfacePoolSize = std::max<uint64_t>(1, std::min<uint64_t>(512, 1920 * 1080 / (settings.recordWidth * settings.recordHeight)));
        std::vector<sk_sp<SkSurface>> surfacePool(surfacePoolSize);
        auto masterSurfaceRef = backendWin.skSurface;
        for (uint64_t i = 0; i < surfacePoolSize; i++) {
            surfacePool[i] = masterSurfaceRef->makeSurface(settings.recordWidth, settings.recordHeight);
            if (!surfacePool[i]) {
                std::cerr << "Failed to create SkSurface" << std::endl;
                return;
            }
        }

        pd.setLine(1, L"加载...");
        if (!load()) return;
        
        TelemetryDeckClient::Performance::VideoRender::Completed performanceInfo {
            .baseInfo = TelemetryDeckClient::Performance::BaseInfo::make(),
            .chartHash = backendWin.chart.rawHash,
            .loadingTook = loadingChartTook,
            .screenSize = { (double)settings.recordWidth, (double)settings.recordHeight },
            .frameRate = settings.recordFPS,
        };

        double renderSt = globalTimer();

        pd.setLine(1, L"解码音频...");
        auto pcm = decodePcm16FromMaSound(backendWin.mainSound);
        if (!pcm.second) {
            showErrorMsg(win.get(), L"无法解码音频");
            return;
        }

        pd.setLine(1, L"渲染音频...");
        auto renderHitsoundsResult = backendWin.renderHitsounds(pcm.first, {
            .musicVol = settings.musicVol,
            .sfxVol = settings.sfxVol,
        });
        if (renderHitsoundsResult.has_value()) {
            std::wstring msg = L"渲染打击音效失败: ";
            msg += renderHitsoundsResult.value();
            showErrorMsg(win.get(), msg.c_str());
            return;
        }
        
        pd.setLine(1, L"写入音频...");
        cap.writeAudio(pcm.first);

        pd.setLine(1, L"渲染视频...");
        uint64_t frameCut = 0;
        UserData ud {};
        ud.cap = &cap;
        ud.frameQueue = &frameQueue;
        ud.queueMaxSize = std::max<uint64_t>(1, std::min<uint64_t>(512, 1920 * 1080 * 8 / (settings.recordWidth * settings.recordHeight)));
        std::thread frameWriterThread(frameWriter);
        uint64_t surfaceIndex = 0;
        FPSCalc fpsCalc;
        
        while (true) {
            double t = frameCut / cap.fps;
            if (t > backendWin.calculateFrameConfig.songLength) break;

            backendWin.skSurface = surfacePool[surfaceIndex];
            backendWin.skCanvas = backendWin.skSurface->getCanvas();
            backendWin.mainloopFrame(t, {
                .isRenderingVideo = true
            });
            
            backendWin.skSurface->asyncRescaleAndReadPixelsYUV420(
                SkYUVColorSpace::kJPEG_Full_SkYUVColorSpace,
                backendWin.skSurfaceColorSpace,
                SkIRect{0, 0, (int32_t)settings.recordWidth, (int32_t)settings.recordHeight},
                SkISize{(int32_t)settings.recordWidth, (int32_t)settings.recordHeight},
                SkImage::RescaleGamma::kLinear,
                SkImage::RescaleMode::kNearest,
                readPixelsCallback,
                &ud
            );

            if (surfaceIndex == surfacePoolSize - 1) backendWin.skGrCtx->flushAndSubmit();
            surfaceIndex = (surfaceIndex + 1) % surfacePoolSize;
            frameCut++;
            fpsCalc.frame();

            uint64_t totalFrames = backendWin.calculateFrameConfig.songLength * cap.fps;
            pd.setProgress(frameCut, totalFrames);

            {
                std::wstring msg = L"渲染视频... (";
                msg += cap.getCodecName();
                msg += L") ";
                msg += std::to_wstring(frameCut) + L"/" + std::to_wstring(totalFrames) + L" (" + std::to_wstring((uint64_t)fpsCalc.fps) + L" fps)";
                pd.setLine(1, msg.c_str());
            }
        }

        while (cap.writtenVideoFrameCount != frameCut) {
            backendWin.skGrCtx->flushAndSubmit();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        frameQueue.enqueue(nullptr);
        frameWriterThread.join();

        performanceInfo.frameCount = frameCut;
        performanceInfo.renderTotalTook = globalTimer() - renderSt;

        pd.setLine(1, L"释放资源...");
        if (settings.enablePerformanceCollection) {
            std::thread([=]() {
                TelemetryDeckClient::Performance::VideoRender::completed(performanceInfo);
            }).detach();
        }
        backendWin.resetSurface();

        std::wstring msg(L"渲染完成, 已保存到 ");
        msg += Win32Utils::stringToWstring(videoPath);
        showInfoMsg(win.get(), msg.c_str());
    } }));

    win->createWidgets();
    win->doGrid();
    win->resizeToGridBounds();
    syncSettingsToUI();

    if (settings.getIsFirstRun()) {
        const wchar_t* openPerformanceCollectionMsg = L"是否开启性能数据收集?\n\n开启后, 您的性能数据将会被发送到Telemetry Deck, 以帮助开发者改进软件。\n我们同时也会收集一些设备信息, 例如CPU型号, GPU型号, 操作系统版本等, 这些信息不会包含您的个人隐私信息, 如果您不希望我们收集这些信息, 可以关闭此选项。";
        if (showYesNoMsg(win.get(), openPerformanceCollectionMsg)) {
            WidgetStatics::CheckBox::toggle(win->refWidget(enablePerformanceCollectionCheckBox), true);
        }
    }

    win->mainloop();
    return 0;
}
