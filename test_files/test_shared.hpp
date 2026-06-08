#define EASY_PHI_TEXT_RENDERER
#define EASY_PHI_IMAGE_DECODER
#define EASY_PHI_MINIAUDIO_AUDIO_ENGINE
#define EASY_PHI_PHI_RESOURCE
#define EASY_PHI_MIL_RESOURCE
#include <easy_phi.hpp>

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
extern "C" {
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
}
#include <cpr/cpr.h>

#include <condition_variable>
#include <queue>

using namespace easy_phi;
using namespace GL;

#define PCM_FIXED_SAMPLE_RATE 44100
#define PCM_FIXED_CHANNELS 2

struct VideoCap {
    const char* path;
    int width; int height; double fps;
    bool isOpened = false;

    AVFormatContext* fmtCtx;

    AVCodecContext* vCodecCtx = nullptr;
    AVStream* vStream = nullptr;
    int vFrameIdx = 0;
    uint64_t writtenVideoFrameCount = 0;

    AVFrame* vSwFrame = nullptr;

    AVStream* aStream = nullptr;
    AVCodecContext* aCodecCtx = nullptr;
    int aFramePts = 0;
    bool wroteAudio = false;

    VideoCap(
        const char* path,
        int width, int height, double fps
    ) {
        this->path = path;
        this->width = width; this->height = height; this->fps = fps;

        AVCodec* vCodec = (AVCodec*)avcodec_find_encoder(AV_CODEC_ID_H264);
        fmtCtx = avformat_alloc_context();
        fmtCtx->oformat = av_guess_format("mp4", nullptr, nullptr);
        vStream = avformat_new_stream(fmtCtx, vCodec);
        vCodecCtx = avcodec_alloc_context3(vCodec);
        vCodecCtx->width = width;
        vCodecCtx->height = height;
        vCodecCtx->time_base = {1, (int)fps};
        vCodecCtx->framerate = {(int)fps, 1};
        vCodecCtx->pix_fmt = AV_PIX_FMT_YUV420P;
        vCodecCtx->gop_size = std::max(10, (int)fps * 2);
        vCodecCtx->max_b_frames = 2;
        vCodecCtx->rc_min_rate = 0;
        vCodecCtx->rc_max_rate = 0;

        AVDictionary* vopts = nullptr;
        av_dict_set(&vopts, "preset", "superfast", 0);
        av_dict_set(&vopts, "tune", "film", 0);
        av_dict_set(&vopts, "crf", "23", 0);
        av_dict_set(&vopts, "refs", "1", 0);
        av_dict_set(&vopts, "rc-lookahead", "20", 0);
        avcodec_open2(vCodecCtx, vCodec, &vopts);
        av_dict_free(&vopts);

        vSwFrame = av_frame_alloc();
        vSwFrame->format = AV_PIX_FMT_YUV420P;
        vSwFrame->width = width;
        vSwFrame->height = height;
        
        avcodec_parameters_from_context(vStream->codecpar, vCodecCtx);

        const AVCodec* aCodec = avcodec_find_encoder(AV_CODEC_ID_AAC);
        aStream = avformat_new_stream(fmtCtx, aCodec);
        aStream->time_base = {1, PCM_FIXED_SAMPLE_RATE};
        aCodecCtx = avcodec_alloc_context3(aCodec);
        aCodecCtx->sample_fmt = AV_SAMPLE_FMT_S16;
        aCodecCtx->bit_rate = 192000;
        aCodecCtx->sample_rate = PCM_FIXED_SAMPLE_RATE;
        aCodecCtx->ch_layout.nb_channels = PCM_FIXED_CHANNELS;
        av_channel_layout_default(&aCodecCtx->ch_layout, aCodecCtx->ch_layout.nb_channels);
        aCodecCtx->time_base = {1, PCM_FIXED_SAMPLE_RATE};

        AVDictionary* aopts = nullptr;
        avcodec_open2(aCodecCtx, aCodec, &aopts);
        av_dict_free(&aopts);
        avcodec_parameters_from_context(aStream->codecpar, aCodecCtx);
        avio_open(&fmtCtx->pb, path, AVIO_FLAG_WRITE);
        avformat_write_header(fmtCtx, nullptr) < 0;

        isOpened = true;
    }

    void flush() {
        AVPacket* packet = av_packet_alloc();

        while (avcodec_receive_packet(vCodecCtx, packet) == 0) {
            av_packet_rescale_ts(packet, vCodecCtx->time_base, vStream->time_base);
            packet->stream_index = vStream->index;
            av_interleaved_write_frame(fmtCtx, packet);
            av_packet_unref(packet);
        }

        while (avcodec_receive_packet(aCodecCtx, packet) == 0) {
            av_packet_rescale_ts(packet, aCodecCtx->time_base, aStream->time_base);
            packet->stream_index = aStream->index;
            av_interleaved_write_frame(fmtCtx, packet);
            av_packet_unref(packet);
        }

        av_packet_free(&packet);
    }

    bool writeAudio(const ep_sp<DecodedAudio>& audio) {
        if (wroteAudio) return false;
        if ((int)audio->sampleRate != aCodecCtx->sample_rate || (int)audio->channels != aCodecCtx->ch_layout.nb_channels) return false;

        const int frameSize = aCodecCtx->frame_size;
        for (uint64_t offset = 0; offset + frameSize * audio->channels <= audio->data.size(); offset += frameSize * audio->channels) {
            AVFrame* f = av_frame_alloc();
            f->format = aCodecCtx->sample_fmt;
            f->ch_layout = aCodecCtx->ch_layout;
            f->sample_rate = aCodecCtx->sample_rate;
            f->nb_samples = frameSize;
            f->data[0] = (uint8_t*)(audio->data.data() + offset);
            f->pts = aFramePts;
            aFramePts += frameSize;

            avcodec_send_frame(aCodecCtx, f);

            if (offset + frameSize * audio->channels > audio->data.size()) {
                avcodec_send_frame(aCodecCtx, nullptr);
            }

            flush();
            av_frame_free(&f);
        }

        return true;
    }

    bool writeVideoFrame(
        void* y, void* u, void* v,
        uint64_t lsy, uint64_t lsu, uint64_t lsv
    ) {
        writtenVideoFrameCount++;

        vSwFrame->pts = vFrameIdx++;
        vSwFrame->data[0] = (uint8_t*)y;
        vSwFrame->data[1] = (uint8_t*)u;
        vSwFrame->data[2] = (uint8_t*)v;
        vSwFrame->linesize[0] = lsy;
        vSwFrame->linesize[1] = lsu;
        vSwFrame->linesize[2] = lsv;

        if (avcodec_send_frame(vCodecCtx, vSwFrame) < 0) {
            std::cerr << "failed to send frame" << std::endl;
            return false;
        }

        flush();
        return true;
    }

    ~VideoCap() {
        if (isOpened) {
            avcodec_send_frame(vCodecCtx, nullptr);
            flush();

            av_write_trailer(fmtCtx);
            avio_closep(&fmtCtx->pb);
        }

        if (vSwFrame) av_frame_free(&vSwFrame);
        if (vCodecCtx) avcodec_free_context(&vCodecCtx);
        if (aCodecCtx) avcodec_free_context(&aCodecCtx);
        if (fmtCtx) avformat_free_context(fmtCtx);
    }
};

struct TelemetryDeckClient {
    static constexpr uint64_t version = 1;

    static void postSignal(const std::string& type, const JsonNode& rawPayload) {
        auto data = JsonNode::MakeArray({ JsonNode::MakeObject({
            { "appID", JsonNode::MakeString("9C828357-123C-42F6-AD02-62100B3B75A7") },
            { "clientUser", JsonNode::MakeString("anonymous") },
            { "type", JsonNode::MakeString(type) },
            { "payload", JsonNode::MakeObject({
                { "data", JsonNode::MakeString(rawPayload.toString()) }
            }) }
        }) });

        cpr::Response resp = cpr::Post(
            cpr::Url { "https://nom.telemetrydeck.com/v2/namespace/com.qaqfei/" },
            cpr::VerifySsl { false },
            cpr::Header {
                { "Content-Type", "application/json; charset=utf-8" }
            },
            cpr::Body { data.toString() }
        );
    }

    struct Performance {
        struct BaseInfo {
            struct CpuInfo {
                std::string vendor;
                std::string model;
                std::string arch;

                JsonNode toJson() const {
                    return JsonNode::MakeObject({
                        { "vendor", JsonNode::MakeString(vendor) },
                        { "model", JsonNode::MakeString(model) },
                    });
                }
            } cpuInfo;

            struct GpuInfo {
                std::string vendor;
                std::string model;

                JsonNode toJson() const {
                    return JsonNode::MakeObject({
                        { "vendor", JsonNode::MakeString(vendor) },
                        { "model", JsonNode::MakeString(model) },
                    });
                }
            } gpuInfo;

            struct SystemInfo {
                std::string os;
                std::string version;

                JsonNode toJson() const {
                    return JsonNode::MakeObject({
                        { "os", JsonNode::MakeString(os) },
                        { "version", JsonNode::MakeString(version) },
                    });
                }
            } systemInfo;

            struct BuildInfo {
                std::string shortCommitHash;
                double buildTime;
                bool isDebug;
                bool isDev;

                JsonNode toJson() const {
                    return JsonNode::MakeObject({
                        { "shortCommitHash", JsonNode::MakeString(shortCommitHash) },
                        { "buildTime", JsonNode::MakeNumber(buildTime) },
                        { "isDebug", JsonNode::MakeBool(isDebug) },
                        { "isDev", JsonNode::MakeBool(isDev) }
                    });
                }

                void fill() {
                    shortCommitHash = BUILD_SHORT_COMMIT_HASH;
                    buildTime = BUILD_TIME;
                    isDebug = BUILD_IS_DEBUG;
                    isDev = std::filesystem::exists("dev.flag");
                }
            } buildInfo;

            JsonNode toJson() const {
                return JsonNode::MakeObject({
                    { "cpuInfo", cpuInfo.toJson() },
                    { "gpuInfo", gpuInfo.toJson() },
                    { "systemInfo", systemInfo.toJson() },
                    { "buildInfo", buildInfo.toJson() },
                    { "version", JsonNode::MakeNumber(version) },
                    { "uploadTime", JsonNode::MakeNumber(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::utc_clock::now().time_since_epoch()).count()) }
                });
            }

            #ifdef _WIN32
            static BaseInfo make() {
                BaseInfo info;

                {
                    auto cpuid_gcc = [](int info[4], int func_id) {
                        unsigned int eax, ebx, ecx, edx;
                        __get_cpuid(func_id, &eax, &ebx, &ecx, &edx);
                        info[0] = (int)eax;
                        info[1] = (int)ebx;
                        info[2] = (int)ecx;
                        info[3] = (int)edx;
                    };

                    auto cpuid_gcc_ext = [](int info[4], unsigned int func_id) {
                        unsigned int eax, ebx, ecx, edx;
                        __get_cpuid(func_id, &eax, &ebx, &ecx, &edx);
                        info[0] = (int)eax;
                        info[1] = (int)ebx;
                        info[2] = (int)ecx;
                        info[3] = (int)edx;
                    };

                    int cpuInfo[4] = {};
                    char vendor[13] = {};
                    cpuid_gcc(cpuInfo, 0);
                    *(int*)&vendor[0] = cpuInfo[1];   // ebx
                    *(int*)&vendor[4] = cpuInfo[3];  // edx
                    *(int*)&vendor[8] = cpuInfo[2];  // ecx
                    info.cpuInfo.vendor = vendor;

                    cpuid_gcc_ext(cpuInfo, 0x80000000);
                    if ((unsigned)cpuInfo[0] >= 0x80000004) {
                        char brand[49] = {};
                        char* p = brand;
                        for (unsigned int i = 0x80000002; i <= 0x80000004; ++i) {
                            cpuid_gcc_ext(cpuInfo, i);
                            memcpy(p, cpuInfo, 16);
                            p += 16;
                        }
                        info.cpuInfo.model = brand;
                        size_t pos = info.cpuInfo.model.find_first_not_of(' ');
                        if (pos != std::string::npos) info.cpuInfo.model = info.cpuInfo.model.substr(pos);
                    }

                    SYSTEM_INFO si;
                    GetNativeSystemInfo(&si);
                    switch (si.wProcessorArchitecture) {
                        case PROCESSOR_ARCHITECTURE_AMD64: info.cpuInfo.arch = "x86_64"; break;
                        case PROCESSOR_ARCHITECTURE_INTEL: info.cpuInfo.arch = "x86"; break;
                        case PROCESSOR_ARCHITECTURE_ARM64: info.cpuInfo.arch = "arm64"; break;
                        case PROCESSOR_ARCHITECTURE_ARM: info.cpuInfo.arch = "arm"; break;
                        default: info.cpuInfo.arch = "unknown"; break;
                    }
                }

                {
                    DISPLAY_DEVICEA dd = { sizeof(dd) };
                    for (DWORD i = 0; EnumDisplayDevicesA(nullptr, i, &dd, 0); ++i) {
                        if (dd.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE) {
                            info.gpuInfo.model = dd.DeviceString;
                            break;
                        }
                    }

                    std::string lower = info.gpuInfo.model;
                    for (auto& c : lower) c = (char)tolower(c);
                    if (lower.find("nvidia") != std::string::npos) info.gpuInfo.vendor = "NVIDIA";
                    else if (lower.find("amd") != std::string::npos || lower.find("radeon") != std::string::npos) info.gpuInfo.vendor = "AMD";
                    else if (lower.find("intel") != std::string::npos) info.gpuInfo.vendor = "Intel";
                    else info.gpuInfo.vendor = "Unknown";
                }

                {
                    info.systemInfo.os = "Windows";

                    typedef LONG (WINAPI *RtlGetVersion_t)(void*);
                    auto rtlGetVersion = (RtlGetVersion_t)GetProcAddress(GetModuleHandleA("ntdll.dll"), "RtlGetVersion");
                    if (rtlGetVersion) {
                        struct { ULONG size, major, minor, build, platform; WCHAR csd[128]; } osvi = { sizeof(osvi) };
                        if (rtlGetVersion(&osvi) >= 0) {
                            info.systemInfo.version = std::to_string(osvi.major) + "." + std::to_string(osvi.minor) + "." + std::to_string(osvi.build);
                        }
                    }
                }

                info.buildInfo.fill();
                return info;
            }
            #endif
        };

        struct ChartPlayback {
            struct Completed {
                struct FrameInfo {
                    std::pair<double, double> screenSize;
                    std::pair<double, double> timeRange; // s
                    double calculationTook; // ms
                    double renderTook; // ms

                    JsonNode toJson() const {
                        return JsonNode::MakeObject({
                            { "screenSize", JsonNode::MakeArray({ JsonNode::MakeNumber(screenSize.first), JsonNode::MakeNumber(screenSize.second) }) },
                            { "timeRange", JsonNode::MakeArray({ JsonNode::MakeNumber(timeRange.first), JsonNode::MakeNumber(timeRange.second) }) },
                            { "calculationTook", JsonNode::MakeNumber(calculationTook) },
                            { "renderTook", JsonNode::MakeNumber(renderTook) },
                        });
                    }
                };

                BaseInfo baseInfo;
                uint64_t chartHash;
                double loadingTook; // s
                std::vector<FrameInfo> frames;

                JsonNode toJson() const {
                    return JsonNode::MakeObject({
                        { "baseInfo", baseInfo.toJson() },
                        { "chartHash", JsonNode::MakeString(std::to_string(chartHash)) },
                        { "loadingTook", JsonNode::MakeNumber(loadingTook) },
                        { "frames", [&]{
                            std::vector<JsonNode> arr;
                            arr.reserve(frames.size());
                            for (const auto& f : frames) arr.push_back(f.toJson());
                            return JsonNode::MakeArray(arr);
                        }() },
                    });
                }
            };

            static void completed(const Completed& payload) {
                postSignal("Performance.ChartPlayback.completed", payload.toJson());
            }
        };

        struct VideoRender {
            struct Completed {
                BaseInfo baseInfo;
                uint64_t chartHash;
                double loadingTook; // s
                std::pair<double, double> screenSize;
                double frameRate;
                uint64_t frameCount;
                double renderTotalTook; // s
                std::string encoderName;

                JsonNode toJson() const {
                    return JsonNode::MakeObject({
                        { "baseInfo", baseInfo.toJson() },
                        { "chartHash", JsonNode::MakeString(std::to_string(chartHash)) },
                        { "loadingTook", JsonNode::MakeNumber(loadingTook) },
                        { "screenSize", JsonNode::MakeArray({ JsonNode::MakeNumber(screenSize.first), JsonNode::MakeNumber(screenSize.second) }) },
                        { "frameRate", JsonNode::MakeNumber(frameRate) },
                        { "frameCount", JsonNode::MakeNumber(frameCount) },
                        { "renderTotalTook", JsonNode::MakeNumber(renderTotalTook) },
                        { "encoderName", JsonNode::MakeString(encoderName) }
                    });
                }
            };

            static void completed(const Completed& payload) {
                postSignal("Performance.VideoRender.completed", payload.toJson());
            }
        };
    };
};

struct WindowBase {
    GLFWwindow* window;
    int width, height;
    bool hidden;
    double frameBusyWaitPercentage = 0.8;
    bool fullscreen;
    bool vsync;
    std::string chartDir;
    double mouseX, mouseY;

    ep_sp<GL33Context> glCtx;

    std::function<void(ep_f64)> audioSeekTo;

    bool shouldClose() {
        if (glfwWindowShouldClose(window)) {
            glfwSetWindowShouldClose(window, false);
            return true;
        }

        return false;
    }

    void setHidden(bool newValue) {
        hidden = newValue;
        if (hidden) glfwHideWindow(window);
        else glfwShowWindow(window);
    }

    void setVSync(bool enabled) {
        vsync = enabled;
        glfwSwapInterval(vsync ? 1 : 0);
    }

    void busyWait(double frameSt, bool printInfo) {
        if (!vsync) return;

        double waitSt = globalTimer();
        auto* vm = (GLFWvidmode*)glfwGetVideoMode(glfwGetPrimaryMonitor());
        volatile int* dummy = nullptr;
        while ((globalTimer() - frameSt) < frameBusyWaitPercentage / vm->refreshRate) {
            dummy++;
        }

        if (printInfo) {
            std::cout << "wait took " << ((globalTimer() - waitSt) * 1000) << " ms" << std::endl;
        }
    }

    struct MainloopConfigBase {
        std::optional<double> time;
        bool isRenderingVideo;
    };

    template <typename TakeOverer, typename RenderConfig, typename RenderResultInfo>
    bool mainloopFrame(
        const MainloopConfigBase& config,
        const TakeOverer& renderer,
        std::function<RenderConfig(const RenderConfig&)> renderConfigurer = [](auto c) { return c; },
        std::function<void(RenderResultInfo&)> callback = [](auto i) {}
    ) {
        auto frameSt = globalTimer();

        if (shouldClose()) return false;

        if (!config.isRenderingVideo) {
            glfwGetFramebufferSize(window, &width, &height);
        }

        renderer->calcConfig.screenSize = { (double)width, (double)height };
        
        RenderResultInfo& resultInfo = renderer->render(renderConfigurer({
            .base = {
                .time = config.time,
                .disableHitsound = config.isRenderingVideo
            }
        }));

        callback(resultInfo);

        if (!config.isRenderingVideo) {
            std::cout << "calculate took: " << (resultInfo.base.calculatedTook * 1000) << " ms" << std::endl;
            std::cout << "gl operations took: " << (resultInfo.base.glOperationsTook * 1000) << " ms" << std::endl;

            glfwPollEvents();
            busyWait(frameSt, !config.isRenderingVideo);
            glfwSwapBuffers(window);
        }

        if (!config.isRenderingVideo) {
            std::cout << "frame took " << ((globalTimer() - frameSt) * 1000) << " ms" << std::endl;
            std::cout << "draw calls count: " << glCtx->drawCallsCount << std::endl;
            std::cout << std::string(80, '.') << std::endl;
        }

        glCtx->frameEnded();
        return true;
    }
};

void createGLfwWindow(WindowBase& wbase) {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4);

    if (wbase.hidden) {
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    }

    auto* vm = (GLFWvidmode*)glfwGetVideoMode(glfwGetPrimaryMonitor());
    wbase.width = vm->width * 0.6;
    wbase.height = vm->height * 0.6;

    if (wbase.fullscreen) {
        glfwWindowHint(GLFW_AUTO_ICONIFY, GLFW_FALSE);
        glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
        glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
        wbase.width = vm->width; wbase.height = vm->height;
    }

    wbase.width += wbase.width % 2;
    wbase.height += wbase.height % 2;

    wbase.window = glfwCreateWindow(wbase.width, wbase.height, "", nullptr, nullptr);

    glfwMakeContextCurrent(wbase.window);
    wbase.glCtx = GL33Context::Make(MakeGL33CoreInterface( [](const char* name) { return (void*)glfwGetProcAddress(name); } ));

    glfwSetWindowUserPointer(wbase.window, &wbase);

    glfwSetCursorPosCallback(wbase.window, [](GLFWwindow* glfwWindow, double x, double y) {
        auto* wbase = (WindowBase*)glfwGetWindowUserPointer(glfwWindow);
        wbase->mouseX = x;
        wbase->mouseY = y;
    });

    glfwSetMouseButtonCallback(wbase.window, [](GLFWwindow* glfwWindow, int button, int action, int mods) {
        auto* wbase = (WindowBase*)glfwGetWindowUserPointer(glfwWindow);

        if (action == GLFW_PRESS) {
            if (button == GLFW_MOUSE_BUTTON_RIGHT) {
                wbase->audioSeekTo(wbase->mouseX / wbase->width);
            }
        }
    });

    wbase.setVSync(wbase.vsync);
}

struct PhiWindow {
    WindowBase base;

    ep_sp<PhiTakeOverer> renderer;

    void init() {
        base.audioSeekTo = [this](ep_f64 p) {
            renderer->audioManager.seekBgm(renderer->audioManager.getBgmLength() * p);
        };

        createGLfwWindow(base);

        renderer = PhiTakeOverer::Make();
        renderer->noteTextureDataLoader = PhiStaticResourceHelpers::noteTextureDataLoader;
        renderer->hitEffectDataLoader = PhiStaticResourceHelpers::hitEffectDataLoader;
        renderer->hitsoundDataLoader = PhiStaticResourceHelpers::hitsoundDataLoader;

        renderer->storyboardDataLoader = [this](const std::string& name) -> Data {
            auto path = PhiStoryboardHelpers::textureNameToPath(base.chartDir, name);

            Data data;
            if (!Data::MakeFromFile(data, path)) {
                std::cout << "failed to read storyboard file: " << name << std::endl;
            }

            return data;
        };

        renderer->shaderDataLoader = [this](const std::string& name) -> std::string {
            Data shaderText {};

            if (!PhiStaticResourceHelpers::getBuiltinShader(name, shaderText)) {
                if (!Data::MakeFromFile(shaderText, std::filesystem::path(base.chartDir + "/" + name).lexically_normal().string())) {
                    std::cout << "failed to read shader file: " << name << std::endl;
                }
            }

            return shaderText.toString();
        };

        renderer->glCtx = base.glCtx;
        renderer->sharedComp.textureDecoder = decodeImage;
        renderer->textManager.renderer = PhiStaticResourceHelpers::createTextRenderer();
        renderer->audioManager.decoder = decodeAudioMiniaudio;
        renderer->audioManager.engine = makeAudioEngineMiniaudio();
        renderer->init();
    }

    auto loadChart(const std::string& path, const std::string& chartDir) {
        auto data = Data::MakeFromFile(path);

        base.chartDir = chartDir;

        auto resultInfo = renderer->loadChart(data, [this](auto& chart) {
            Data extraData;
            if (Data::MakeFromFile(extraData, base.chartDir + "extra.json")) {
                auto extraLoadResult = loadPhiExtraFromJsonData(extraData, chart.storyboardAssets);
                if (std::holds_alternative<PhiExtra>(extraLoadResult)) {
                    chart.extra = std::move(std::get<PhiExtra>(extraLoadResult));
                } else if (std::holds_alternative<std::string>(extraLoadResult)) {
                    std::cout << "failed to load extra: " << std::get<std::string>(extraLoadResult) << std::endl;
                }
            }

            chart.init();
        });

        std::cout << "create chart object took: " << resultInfo.createObjectTook << " s" << std::endl;
        std::cout << "init chart took: " << resultInfo.initTook << " s" << std::endl;

        resultInfo.checkAndThrow();
        return resultInfo;
    }

    struct MainloopConfig {
        WindowBase::MainloopConfigBase base;
        TelemetryDeckClient::Performance::ChartPlayback::Completed::FrameInfo* pccfi;
    };

    bool mainloopFrame(const MainloopConfig& config) {
        return base.mainloopFrame<
            decltype(renderer),
            decltype(renderer)::element_type::RenderConfig,
            decltype(renderer)::element_type::RenderResultInfo
        >(
            config.base,
            renderer,
            [](auto c) { return c; },
            [&](auto info) {
                if (config.pccfi) {
                    config.pccfi->calculationTook = info.base.calculatedTook * 1000;
                    config.pccfi->screenSize = { (double)base.width, (double)base.height };
                    config.pccfi->timeRange = renderer->calculatedFrame.frameTimeRange.toPair();
                    config.pccfi->renderTook = info.base.glOperationsTook * 1000;
                }
            }
        );
    }
};

struct MilWindow {
    WindowBase base;

    ep_sp<MilTakeOverer> renderer;

    void init() {
        base.audioSeekTo = [this](ep_f64 p) {
            renderer->audioManager.seekBgm(renderer->audioManager.getBgmLength() * p);
        };

        createGLfwWindow(base);

        renderer = MilTakeOverer::Make();
        renderer->lineHeadTextureLoader = MilStaticResourceHelpers::lineHeadTextureLoader;
        renderer->noteTextureDataLoader = MilStaticResourceHelpers::noteTextureDataLoader;
        renderer->hitsoundDataLoader = MilStaticResourceHelpers::hitsoundDataLoader;
        
        renderer->glCtx = base.glCtx;
        renderer->sharedComp.textureDecoder = decodeImage;
        renderer->textManager.renderer = MilStaticResourceHelpers::createTextRenderer();
        renderer->audioManager.decoder = decodeAudioMiniaudio;
        renderer->audioManager.engine = makeAudioEngineMiniaudio();
        renderer->init();
    }

    auto loadChart(const std::string& path, const std::string& chartDir) {
        auto data = Data::MakeFromFile(path);

        base.chartDir = chartDir;
        
        auto resultInfo = renderer->loadChart(data);

        std::cout << "create chart object took: " << resultInfo.createObjectTook << " s" << std::endl;
        std::cout << "init chart took: " << resultInfo.initTook << " s" << std::endl;

        resultInfo.checkAndThrow();
        return resultInfo;
    }

    struct MainloopConfig {
        WindowBase::MainloopConfigBase base;
    };
    
    bool mainloopFrame(const MainloopConfig& config) {
        return base.mainloopFrame<
            decltype(renderer),
            decltype(renderer)::element_type::RenderConfig,
            decltype(renderer)::element_type::RenderResultInfo
        >(
            config.base,
            renderer,
            [](auto c) { return c; },
            [](auto i) { }
        );
    }
};

template<typename T>
class ThreadSafeQueue {
public:
    ~ThreadSafeQueue() {
        shutdown();
    }
    
    void enqueue(T frame) {
        {
            std::lock_guard<std::mutex> lock(mtx);
            if (done) return;
            queue.push(std::move(frame));
        }
        cv.notify_one();
    }
    
    bool wait_dequeue(T& frame) {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [this] { return !queue.empty() || done; });
        if (queue.empty()) return false;
        frame = std::move(queue.front());
        queue.pop();
        return true;
    }
    
    void shutdown() {
        {
            std::lock_guard<std::mutex> lock(mtx);
            done = true;
        }
        cv.notify_all();
    }
    
    size_t size_approx() const {
        std::lock_guard<std::mutex> lock(mtx);
        return queue.size();
    }
    
private:
    mutable std::mutex mtx;
    std::condition_variable cv;
    std::queue<T> queue;
    bool done = false;
};

struct FPSCalc {
    double fps;

    FPSCalc() {
        lastTime = globalTimer();
    }

    void frame() {
        double t = globalTimer();
        count++;
        if (count >= maxCount) {
            if (t != lastTime) {
                fps = count / (t - lastTime);
                maxCount = fps / 10;
            } else {
                fps = std::numeric_limits<double>::infinity();
                maxCount *= 2;
            }

            maxCount = std::min(std::max(maxCount, 1.0), 50.0);
            count = 0;
            lastTime = t;
        }
    }

    private:
    uint64_t count;
    double maxCount = 12.0;
    double lastTime;
};
