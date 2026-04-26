#include "test_shared.hpp"

#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shlobj.h>

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

std::string selectOpenFile(const Window& window, const wchar_t* filter, const wchar_t* title) {
    HMODULE comdlg32 = LoadLibraryA("comdlg32.dll");
    wchar_t buf[MAX_PATH] = {0};
    OPENFILENAMEW ofn { sizeof(ofn) };
    ofn.hwndOwner = glfwGetWin32Window(window.window);
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = buf;
    ofn.lpstrTitle = title;
    ofn.nMaxFile = sizeof(buf);
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    BOOL res = ((BOOL(*)(OPENFILENAMEW*))GetProcAddress(comdlg32, "GetOpenFileNameW"))(&ofn);

    if (!res) return {};
    return wstringToString(std::wstring(buf));
}

std::string selectSaveFile(const Window& window, const wchar_t* filter, const wchar_t* title) {
    HMODULE comdlg32 = LoadLibraryA("comdlg32.dll");
    wchar_t buf[MAX_PATH] = {0};
    OPENFILENAMEW ofn { sizeof(ofn) };
    ofn.hwndOwner = glfwGetWin32Window(window.window);
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = buf;
    ofn.lpstrTitle = title;
    ofn.nMaxFile = sizeof(buf);
    ofn.Flags = OFN_OVERWRITEPROMPT;
    BOOL res = ((BOOL(*)(OPENFILENAMEW*))GetProcAddress(comdlg32, "GetSaveFileNameW"))(&ofn);

    if (!res) return {};
    return wstringToString(std::wstring(buf));
}

void showErrorMsg(const Window& window, const wchar_t* msg) {
    MessageBoxW(glfwGetWin32Window(window.window), msg, L"Error", MB_OK | MB_ICONERROR);
}

std::string getDirectory(const std::string& path) {
    auto pos = path.find_last_of("\\/");
    if (pos == std::string::npos) return {};
    return path.substr(0, pos);
}

bool showYesNoMsg(const Window& window, const wchar_t* msg) {
    return MessageBoxW(glfwGetWin32Window(window.window), msg, L"Confirm", MB_YESNO | MB_ICONQUESTION) == IDYES;
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

int main() {
    Window window {};
    window.hidden = true;
    window.init();

    ProgressDialog pd {};
    pd.setTitle(L"Open RPE Recorder");
    pd.start();

    pd.setLine(1, L"选择谱面...");
    auto infoPath = selectOpenFile(window,  L"Info File (info.txt)\0info.txt\0All Files (*.*)\0*.*\0", L"打开 info.txt");
    if (infoPath.empty()) return 0;

    pd.setLine(1, L"加载谱面信息...");
    auto dirPath = getDirectory(infoPath) + "/";

    easy_phi::Data infoData;
    if (!easy_phi::Data::FromFile(&infoData, infoPath)) {
        showErrorMsg(window, L"无法打开 info.txt");
        return 1;
    }

    auto infos = easy_phi::parseRPEChartInfo(infoData);
    if (infos.empty()) {
        showErrorMsg(window, L"info.txt 中找不到谱面信息");
        return 1;
    }

    auto info = infos[0];
    auto audioPath = dirPath + info.song;
    auto imagePath = dirPath + info.picture;
    auto chartPath = dirPath + info.chart;

    auto checkLoad = [&](std::optional<std::string> res, std::wstring msg) {
        if (res.has_value()) {
            showErrorMsg(window, msg.c_str());
            TerminateProcess(GetCurrentProcess(), EXIT_FAILURE);
        }
    };

    pd.setLine(1, L"加载谱面...");
    checkLoad(window.loadChart(chartPath, dirPath), L"无法加载谱面");

    window.chart.meta.title = info.name;
    window.chart.meta.difficulty = info.level;

    pd.setLine(1, L"加载曲绘...");
    checkLoad(window.loadBgImage(imagePath), L"无法加载曲绘");

    pd.setLine(1, L"加载音频...");
    checkLoad(window.loadMainSound(audioPath), L"无法加载音频");

    if (showYesNoMsg(window, L"是否预览 (不进行视频渲染) ?")) {
        pd.close();

        window.setHidden(false);
        window.vsync = true;
        window.resetSurface();
        window.startMainSound();

        while (ma_sound_is_playing(window.mainSound)) {
            double t = getMaSoundPosition(window.mainSound);

            if (!window.mainloopFrame(t)) {
                break;
            }
        }
    } else {
        uint64_t width = 1920, height = 1080;

        window.width = width;
        window.height = height;
        window.resetSurface();

        pd.setLine(1, L"选择视频...");
        auto videoPath = selectSaveFile(window,  L"视频文件 (*.mp4)\0*.mp4\0All Files (*.*)\0*.*\0", L"保存视频文件");
        if (videoPath.empty()) return 0;

        pd.setLine(1, L"初始化视频...");
        VideoCap cap(videoPath.c_str(), width, height, 60.0);

        pd.setLine(1, L"解码音频...");
        auto pcm = decodePcm16FromMaSound(window.mainSound);
        if (!pcm.second) {
            showErrorMsg(window, L"无法解码音频");
            return 1;
        }

        pd.setLine(1, L"渲染音频...");
        auto renderHitsoundsResult = window.renderHitsounds(pcm.first);
        if (renderHitsoundsResult.has_value()) {
            std::wstring msg = L"渲染打击音效失败: ";
            msg += renderHitsoundsResult.value();
            showErrorMsg(window, msg.c_str());
        }

        pd.setLine(1, L"写入音频...");
        cap.writeAudio(pcm.first);

        pd.setLine(1, L"初始化...");

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

        uint64_t surfacePoolSize = std::max<uint64_t>(1, std::min<uint64_t>(512, 1920 * 1080 / (width * height)));
        std::vector<sk_sp<SkSurface>> surfacePool(surfacePoolSize);
        auto masterSurfaceRef = window.skSurface;
        for (uint64_t i = 0; i < surfacePoolSize; i++) {
            surfacePool[i] = masterSurfaceRef->makeSurface(width, height);
            if (!surfacePool[i]) {
                std::cerr << "Failed to create SkSurface" << std::endl;
                return 1;
            }
        }
        
        uint64_t frameCut = 0;
        UserData ud {};
        ud.cap = &cap;
        ud.frameQueue = &frameQueue;
        ud.queueMaxSize = std::max<uint64_t>(1, std::min<uint64_t>(512, 1920 * 1080 * 8 / (width * height)));
        std::thread frameWriterThread(frameWriter);
        uint64_t surfaceIndex = 0;
        FPSCalc fpsCalc;

        while (true) {
            double t = frameCut / cap.fps;
            if (t > window.calculateFrameConfig.songLength) break;

            window.skSurface = surfacePool[surfaceIndex];
            window.skCanvas = window.skSurface->getCanvas();
            window.mainloopFrame(t, true);
            
            window.skSurface->asyncRescaleAndReadPixelsYUV420(
                SkYUVColorSpace::kJPEG_Full_SkYUVColorSpace,
                window.skSurfaceColorSpace,
                SkIRect{0, 0, (int32_t)width, (int32_t)height},
                SkISize{(int32_t)width, (int32_t)height},
                SkImage::RescaleGamma::kLinear,
                SkImage::RescaleMode::kNearest,
                readPixelsCallback,
                &ud
            );

            if (surfaceIndex == surfacePoolSize - 1) window.skGrCtx->flushAndSubmit();
            surfaceIndex = (surfaceIndex + 1) % surfacePoolSize;
            frameCut++;
            fpsCalc.frame();

            uint64_t totalFrames = window.calculateFrameConfig.songLength * cap.fps;
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
            window.skGrCtx->flushAndSubmit();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        frameQueue.enqueue(nullptr);
        frameWriterThread.join();

        pd.setLine(1, L"释放资源...");
    }

    return 0;
}
