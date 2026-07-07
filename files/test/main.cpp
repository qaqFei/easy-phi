namespace test_main {
    using gframerate_meter::FramerateMeter;
    using gregapi::RegAPI;
    using gdata::Data;
    using gsp::gsp;
    using namespace gnumeric::types;
    using namespace gopengl::GL;
    using namespace geasy_phi;
    using namespace gwin32ui;
    using namespace gtext_renderer;

    TextManager::Renderer createTextRendererFromData(const Data& data) {
        auto tr = TextRenderer::Make();
        tr->loadFont(data);
        return [tr](const std::string& text, uint64 fontSize) {
            return tr->render(text, fontSize);
        };
    }

    struct WindowBase {
        gsp<gglfw3::Window> window;
        float64 frameBusyWaitPercentage = 0.8;
        bool fullscreen;
        std::pair<uint64, uint64> surfaceSize;
        std::string chartDir;

        gsp<GL33Context> glCtx;
        FramerateMeter framerateMeter;

        TakeOvererComponents::AudioManager* audioManagerRef;

        void busyWait(float64 frameSt, bool printInfo) {
            if (!window->getSwapInterval()) return;

            float64 waitSt = gtime::steady();
            auto refreshRate = gglfw3::Monitor::MakePrimary().refreshRate();

            volatile uint8* dummy = nullptr;
            while ((gtime::steady() - frameSt) < frameBusyWaitPercentage / refreshRate) {
                dummy++;
            }

            if (printInfo) {
                std::cout << "wait took " << ((gtime::steady() - waitSt) * 1000) << " ms" << '\n';
            }
        }

        struct MainloopConfigBase {
            std::optional<float64> time;
            bool isRenderingVideo;
        };

        template <typename TakeOverer>
        bool mainloopFrame(
            const MainloopConfigBase& config,
            const TakeOverer& renderer,
            std::function<typename TakeOverer::element_type::RenderConfig(const typename TakeOverer::element_type::RenderConfig&)> renderConfigurer = [](auto c) { return c; },
            std::function<void(typename TakeOverer::element_type::RenderResultInfo&)> callback = [](auto i) {}
        ) {
            auto frameSt = gtime::steady();

            if (window->shouldClose()) return false;

            auto windowSize = window->getSize();

            if (!config.isRenderingVideo) {
                surfaceSize.first = windowSize.first;
                surfaceSize.second = windowSize.second;
            }

            renderer->calcConfig.screenSize = { (float64)surfaceSize.first, (float64)surfaceSize.second };
            
            auto& resultInfo = renderer->render(renderConfigurer({
                .base = {
                    .time = config.time,
                    .disableHitsound = config.isRenderingVideo
                }
            }));

            callback(resultInfo);

            if (!config.isRenderingVideo) {
                std::cout << "calculate took: " << (resultInfo.base.calculatedTook * 1000) << " ms" << '\n';
                std::cout << "gl operations took: " << (resultInfo.base.glOperationsTook * 1000) << " ms" << '\n';

                gglfw3::pollEvents();
                busyWait(frameSt, !config.isRenderingVideo);
                window->swapBuffers();
            }

            if (!config.isRenderingVideo) {
                std::cout << "frame took " << ((gtime::steady() - frameSt) * 1000) << " ms" << '\n';
                std::cout << "draw calls count: " << glCtx->drawCallsCount << '\n';
                std::cout << "framerate: " << framerateMeter.get() << '\n';
                std::cout << std::string(80, '-') << '\n';
                std::cout << std::flush;
            }

            glCtx->frameEnded();
            framerateMeter.frame();

            return true;
        }
    };

    void createGLfwWindow(WindowBase& wbase) {
        wbase.window = gglfw3::Window::Make();
        wbase.window->hint330Core()->hintMsaa(4);
        if (wbase.fullscreen) wbase.window->setFullscreen();
        else wbase.window->setSizeOfMonitor(0.6);

        wbase.window->create();
        wbase.glCtx = GL33Context::Make(MakeGL33CoreInterface(gglfw3::getProcAddress));
        
        wbase.window->mouseButtonCallback = [wbase = &wbase](int button, int action, int mods) {
            if (action == GLFW_PRESS) {
                if (button == GLFW_MOUSE_BUTTON_RIGHT) {
                    auto& audioManager = *wbase->audioManagerRef;
                    float64 mouseX = wbase->window->getMousePos().first;
                    auto progress = mouseX / wbase->window->getSize().first;
                    audioManager.seekBgm(audioManager.getBgmLength() * progress);
                }
            }
        };

        std::ios::sync_with_stdio(false);
    }

    struct PhiWindow {
        WindowBase base;

        gsp<PhiTakeOverer> renderer;

        void init() {
            createGLfwWindow(base);

            renderer = PhiTakeOverer::Make();
            renderer->noteTextureDataLoader = PhiStaticResourceHelpers::noteTextureDataLoader;
            renderer->hitEffectDataLoader = PhiStaticResourceHelpers::hitEffectDataLoader;
            renderer->hitsoundDataLoader = PhiStaticResourceHelpers::hitsoundDataLoader;
            renderer->shaderDataLoader = PhiStaticResourceHelpers::createShaderDataLoaderFromChartDir([this] { return base.chartDir; });
            renderer->storyboardDataLoader = PhiStaticResourceHelpers::createStoryboardDataLoaderFromChartDir([this] { return base.chartDir; });

            renderer->glCtx = base.glCtx;
            renderer->sharedComp.textureDecoder = gimage::decode;
            renderer->textManager.renderer = createTextRendererFromData(PhiStaticResourceHelpers::getFontData());
            renderer->audioManager.decoder = gminiaudio::decode;
            renderer->audioManager.engine = gminiaudio::makeAudioEngine();
            renderer->init();

            base.audioManagerRef = &renderer->audioManager;
        }

        auto loadChart(const std::string& path, const std::string& chartDir) {
            base.chartDir = chartDir;

            auto resultInfo = renderer->loadChart({
                .data = Data::MakeFromFile(path),
                .extraData = Data::MakeFromFileOptional(PhiStoryboardHelpers::nameToPath(base.chartDir, "extra.json"))
            });

            resultInfo.checkAndThrow();
            return resultInfo;
        }

        struct MainloopConfig {
            WindowBase::MainloopConfigBase base;
        };

        bool mainloopFrame(const MainloopConfig& config) {
            return base.mainloopFrame<decltype(renderer)>(config.base, renderer);
        }
    };

    struct MilWindow {
        WindowBase base;

        gsp<MilTakeOverer> renderer;

        void init() {
            createGLfwWindow(base);

            renderer = MilTakeOverer::Make();
            renderer->lineHeadTextureLoader = MilStaticResourceHelpers::lineHeadTextureLoader;
            renderer->noteTextureDataLoader = MilStaticResourceHelpers::noteTextureDataLoader;
            renderer->hitsoundDataLoader = MilStaticResourceHelpers::hitsoundDataLoader;
            renderer->pauseButtonTextureDataLoader = MilStaticResourceHelpers::pauseButtonTextureDataLoader;
            
            renderer->glCtx = base.glCtx;
            renderer->sharedComp.textureDecoder = gimage::decode;
            renderer->textManager.renderer = createTextRendererFromData(MilStaticResourceHelpers::getFontData());
            renderer->audioManager.decoder = gminiaudio::decode;
            renderer->audioManager.engine = gminiaudio::makeAudioEngine();
            renderer->init();

            base.audioManagerRef = &renderer->audioManager;
        }

        auto loadChart(const std::string& path, const std::string& chartDir) {
            base.chartDir = chartDir;
            
            auto resultInfo = renderer->loadChart({
                .data = Data::MakeFromFile(path)
            });
            resultInfo.checkAndThrow();

            return resultInfo;
        }

        struct MainloopConfig {
            WindowBase::MainloopConfigBase base;
        };
        
        bool mainloopFrame(const MainloopConfig& config) {
            return base.mainloopFrame<decltype(renderer)>(config.base, renderer);
        }
    };

    std::string getDirectory(const std::string& path) {
        auto pos = path.find_last_of("\\/");
        if (pos == std::string::npos) return {};
        return path.substr(0, pos);
    }

    struct Settings {
        static constexpr const wchar_t* appKey = L"Open-RPE-Recorder";
        static constexpr DWORD currentVersion = 0;

        float64 musicVol = 1.0, sfxVol = 1.0;
        float64 noteScaling = 1.0;

        int32 recordWidth = 1920, recordHeight = 1080;
        float64 recordFPS = 60.0;
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
            musicVol = std::clamp<float64>(musicVol, 0.0, 1.0);
            sfxVol = std::clamp<float64>(sfxVol, 0.0, 1.0);

            recordWidth = std::clamp<int32>(recordWidth, 1, 65536);
            recordHeight = std::clamp<int32>(recordHeight, 1, 65536);
            recordFPS = std::clamp<float64>(recordFPS, 0.01, 65536.0);
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

    #if defined(APP_TYPE_OPEN_RPE_RECORDER)
    void entrypoint() {
        int argc; char** argv;
        grain::get_args(&argc, &argv);
        
        std::vector<std::string> args(argv, argv + argc);
        auto hasArg = [&](const std::string& arg) {
            return std::find(args.begin(), args.end(), arg) != args.end();
        };

        PhiWindow backendWin {};
        backendWin.init();
        backendWin.base.window->setHidden(true);

        std::string chartRoot = "", chartPath = "", imagePath = "", audioPath = "";
        std::optional<ParsedRPEChartInfo> chartInfo;

        int32 chartFileInput, chartRootInput, bgFileInput, audioFileInput;
        int32 musicVolInput, sfxVolInput;
        int32 noteScalingInput;
        int32 recordWidthInput, recordHeightInput, recordFPSInput;
        int32 recordSfxRandshakeCheckBox;

        Settings settings {};
        settings.fromRegistry();

        auto win = Win32Window::Make({
            .title = L"Open RPE Recorder"
        });

        win->registerWidget(Widgets::Button({ .text = L"↗Github", .onClick = [&]() {
            ShellExecute(nullptr, "open", "https://github.com/qaqFei/easy-phi", nullptr, nullptr, SW_SHOWNORMAL);
        } }));
        win->nextRow();

        win->registerWidget(Widgets::Label({ .text = L"谱面选择" }));
        win->nextRow();

        win->registerWidget(Widgets::Button({ .text = L"从 info.txt ...", .onClick = [&]() {
            auto infoPathW = selectOpenFile(win.get(),  L"Info File (info.txt)\0info.txt\0All Files (*.*)\0*.*\0", L"打开 info.txt");
            auto infoPath = Win32Utils::wstringToString(infoPathW);
            if (infoPath.empty()) return;

            auto chartRoot = getDirectory(infoPath) + "/";
            WidgetStatics::TextInput::setText(win->refWidget(chartRootInput), Win32Utils::stringToWstring(chartRoot));

            Data infoData;
            if (!Data::MakeFromFile(infoData, infoPath)) {
                showErrorMsg(win.get(), L"无法打开 info.txt");
                return;
            }

            auto infos = ParsedRPEChartInfo::parse(infoData);
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
            auto folderW = selectFolder(win.get(), L"打开谱面根目录");
            auto folder = Win32Utils::wstringToString(folderW);
            if (folder.empty()) return;
            WidgetStatics::TextInput::setText(win->refWidget(chartRootInput), Win32Utils::stringToWstring(folder));
        } }));
        win->nextRow();

        win->registerWidget(Widgets::Label({ .text = L"谱面文件: " }));
        chartFileInput = win->registerWidget(Widgets::TextInput({ .text = L"", .onChange = [&](const std::wstring& ws) {
            chartPath = Win32Utils::wstringToString(ws);
        } }));
        win->registerWidget(Widgets::Button({ .text = L"浏览", .onClick = [&]() {
            auto pathW = selectOpenFile(win.get(),  L"Chart File (*.json)\0*.json\0All Files (*.*)\0*.*\0", L"打开谱面文件");
            auto path = Win32Utils::wstringToString(pathW);
            if (path.empty()) return;
            WidgetStatics::TextInput::setText(win->refWidget(chartFileInput), Win32Utils::stringToWstring(path));
        } }));
        win->nextRow();

        win->registerWidget(Widgets::Label({ .text = L"曲绘文件: " }));
        bgFileInput = win->registerWidget(Widgets::TextInput({ .text = L"", .onChange = [&](const std::wstring& ws) {
            imagePath = Win32Utils::wstringToString(ws);
        } }));
        win->registerWidget(Widgets::Button({ .text = L"浏览", .onClick = [&]() {
            auto pathW = selectOpenFile(win.get(),  L"Image File (*.png, *.jpg, *.jpeg)\0*.png;*.jpg;*.jpeg\0All Files (*.*)\0*.*\0", L"打开曲绘文件");
            auto path = Win32Utils::wstringToString(pathW);
            if (path.empty()) return;
            WidgetStatics::TextInput::setText(win->refWidget(bgFileInput), Win32Utils::stringToWstring(path));
        } }));
        win->nextRow();

        win->registerWidget(Widgets::Label({ .text = L"音频文件: " }));
        audioFileInput = win->registerWidget(Widgets::TextInput({ .text = L"", .onChange = [&](const std::wstring& ws) {
            audioPath = Win32Utils::wstringToString(ws);
        } }));
        win->registerWidget(Widgets::Button({ .text = L"浏览", .onClick = [&]() {
            auto pathW = selectOpenFile(win.get(),  L"Audio File (*.mp3, *.wav, *.ogg, *.flac, *.m4a, *.aac)\0*.mp3;*.wav;*.ogg;*.flac;*.m4a;*.aac\0All Files (*.*)\0*.*\0", L"打开音频文件");
            auto path = Win32Utils::wstringToString(pathW);
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

            auto float64Input = [&](int id, float64 value) {
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

            float64Input(musicVolInput, settings.musicVol);
            float64Input(sfxVolInput, settings.sfxVol);
            float64Input(noteScalingInput, settings.noteScaling);

            intInput(recordWidthInput, settings.recordWidth);
            intInput(recordHeightInput, settings.recordHeight);
            float64Input(recordFPSInput, settings.recordFPS);
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

        float64 loadingChartTook;

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

            backendWin.base.window->setHidden(false);
            backendWin.base.window->setSwapInterval(hasArg("--disable-vsync") ? 0 : 1);
            backendWin.renderer->audioManager.startBgm();

            backendWin.renderer->audioManager.setBgmVolume(settings.musicVol);
            backendWin.renderer->audioManager.setSfxVolume(settings.sfxVol);

            while (!backendWin.renderer->audioManager.getBpmIsEnded()) {
                if (!backendWin.mainloopFrame({})) {
                    backendWin.renderer->audioManager.stopBgm();
                    break;
                }
            }

            backendWin.base.window->setHidden(true);
        } }));
        win->registerWidget(Widgets::Button({ .text = L"渲染视频", .onClick = [&]() {
            auto videoPathW = selectSaveFile(win.get(),  L"视频文件 (*.mp4)\0*.mp4\0All Files (*.*)\0*.*\0", L"保存视频文件");
            auto videoPath = Win32Utils::wstringToString(videoPathW);
            if (videoPath.empty()) return;

            gwin_progress_dialog::ProgressDialog pd {};
            pd.setTitle(L"Open RPE Record -- 渲染视频");
            pd.start();

            pd.setLine(1, L"初始化...");
            WinHiddenGuard whguard(win.get());

            gvideoenc::VideoCap cap {};
            cap.init(videoPath, settings.recordWidth, settings.recordHeight, settings.recordFPS);
            
            using FrameType = std::optional<uint64>;
            gthread_safe_queue::ThreadSafeQueue<FrameType> frameQueue;

            auto videoRecorder = VideoRecorder::Make(
                backendWin.base.glCtx,
                settings.recordWidth, settings.recordHeight,
                [&](uint64 slotIndex) { frameQueue.enqueue(slotIndex); },
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

            backendWin.base.surfaceSize = { settings.recordWidth, settings.recordHeight };
            backendWin.base.window->setSwapInterval(0);

            pd.setLine(1, L"加载...");
            if (!load()) return;
            
            pd.setLine(1, L"渲染音频...");
            auto mixedAudio = backendWin.renderer->mixFinalBgm(backendWin.renderer->chart, {
                .musicVol = settings.musicVol,
                .sfxVol = settings.sfxVol,
                .sfxRandshake = settings.recordSfxRandshake
            });

            pd.setLine(1, L"写入音频...");
            cap.writeAudio(mixedAudio);

            pd.setLine(1, L"渲染视频...");
            uint64 frameCut = 0;
            std::thread frameWriterThread(frameWriter);
            FramerateMeter fpsMeter {};
            
            while (true) {
                float64 t = frameCut / settings.recordFPS;
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

                uint64 totalFrames = backendWin.renderer->calcConfig.songLength * settings.recordFPS;
                pd.setProgress(frameCut, totalFrames);

                {
                    std::wstring msg = L"渲染视频... ";
                    msg += std::to_wstring(frameCut) + L"/" + std::to_wstring(totalFrames) + L" (" + std::to_wstring((uint64)fpsMeter.get()) + L" fps)";
                    pd.setLine(1, msg.c_str());
                }
            }

            videoRecorder->finish();

            frameQueue.enqueue(std::nullopt);
            frameWriterThread.join();

            pd.setLine(1, L"释放资源...");
            std::wstring msg = L"渲染完成, 已保存到 " + Win32Utils::stringToWstring(videoPath);
            showInfoMsg(win.get(), msg.c_str());
        } }));

        win->createWidgets();
        win->doGrid();
        win->resizeToGridBounds();
        syncSettingsToUI();

        win->mainloop();
    }
    #elif defined(APP_TYPE_TEST_MIL)
    void entrypoint() {
        int argc; char** argv;
        grain::get_args(&argc, &argv);
        
        std::vector<std::string> args(argv, argv + argc);
        auto hasArg = [&](const std::string& arg) {
            return std::find(args.begin(), args.end(), arg) != args.end();
        };

        std::string chartPath, imagePath, audioPath, storyboardAssetsPath;
        std::vector<std::string> chartNames = {
            "Dum! Dum!! Dum!!! - Tatsunoshin",
            "Fly To Meteor (Milthm Edit) - ShooTinGStaR + xzadudu179 + Cyberspace",
            "FULi AUTO SHOOTER - MYUKKE",
            "INFP.mp3 - TsukiP",
            "Moonflutter - Reku Mochizuki",
            "oiiaioooooiai - 黑瘦的鱼头",
            "Regnaissance - AiSS vs. Abit",
            "WATER - A-39 & 沙包P",
            "Welcome to Milthm - TsuKiP",
            "☹ - Gray Planet",
            "サイクルの欠片 - TsukiP",
            "粗线条的雨 - 有棵里里",
            "雨之城 - CsLrisEto",
            "靈 - MYTK",
            "驟雨の狭間 - Silentroom"
        };
        
        SetConsoleOutputCP(CP_UTF8);
        SetConsoleCP(CP_UTF8);

        for (uint64 i = 0; i < (uint64)chartNames.size(); ++i) {
            std::cout << (i + 1) << ". " << chartNames[i] << std::endl;
        }

        uint64 choice;
        std::cout << ">> ";
        std::cin >> choice;
        choice -= 1;

        if (choice < 0 || choice >= (uint64)chartNames.size()) {
            std::cout << "Invalid choice" << std::endl;
            return;
        }

        std::string prefix = "./milcharts/";
        chartPath = prefix + chartNames[choice] + "/.json";
        imagePath = prefix + chartNames[choice] + "/.png";
        audioPath = prefix + chartNames[choice] + "/.wav";
        storyboardAssetsPath = prefix + chartNames[choice];

        MilWindow window {};
        window.base.fullscreen = hasArg("--fullscreen");
        window.init();
        window.base.window->setSwapInterval(hasArg("--disable-vsync") ? 0 : 1);

        window.loadChart(chartPath, storyboardAssetsPath);
        window.renderer->loadIllustion(imagePath);
        window.renderer->audioManager.load(audioPath);

        window.renderer->audioManager.startBgm();

        while (!window.renderer->audioManager.getBpmIsEnded()) {
            if (!window.mainloopFrame({})) {
                break;
            }
        }
    }
    #else
        #error "APP_TYPE is not defined"
        void entrypoint() {}
    #endif
}

void entrypoint() {
    test_main::entrypoint();
}
