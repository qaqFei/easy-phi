#define EASY_PHI_TEXT_RENDERER
#define EASY_PHI_IMAGE_DECODER
#define EASY_PHI_MINIAUDIO_AUDIO_ENGINE
#define EASY_PHI_PHI_RESOURCE
#define EASY_PHI_MIL_RESOURCE
#include <easy_phi.hpp>

#include <GLFW/glfw3.h>
extern "C" {
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
}

using namespace easy_phi;
using namespace GL;

struct VideoCap {
    AVFormatContext* fmtCtx;

    AVStream* vStream;
    AVCodecContext* vCodecCtx;

    AVStream* aStream;
    AVCodecContext* aCodecCtx;

    AVFrame* vFrame;
    AVFrame* aFrame;
    AVPacket* packet;

    static constexpr uint64_t aSampleRate = 44100;
    static constexpr uint64_t aChannels = 2;

    void init(const std::string& path, int width, int height, double fps) {
        fmtCtx = avformat_alloc_context();
        fmtCtx->oformat = av_guess_format("mp4", nullptr, nullptr);

        const AVCodec* vCodec = avcodec_find_encoder(AV_CODEC_ID_H264);
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
        avcodec_parameters_from_context(vStream->codecpar, vCodecCtx);

        const AVCodec* aCodec = avcodec_find_encoder(AV_CODEC_ID_AAC);
        aStream = avformat_new_stream(fmtCtx, aCodec);
        aStream->time_base = {1, aSampleRate};
        aCodecCtx = avcodec_alloc_context3(aCodec);
        aCodecCtx->sample_fmt = AV_SAMPLE_FMT_S16;
        aCodecCtx->bit_rate = 192000;
        aCodecCtx->sample_rate = aSampleRate;
        aCodecCtx->ch_layout.nb_channels = aChannels;
        av_channel_layout_default(&aCodecCtx->ch_layout, aCodecCtx->ch_layout.nb_channels);
        aCodecCtx->time_base = {1, aSampleRate};

        AVDictionary* aopts = nullptr;
        avcodec_open2(aCodecCtx, aCodec, &aopts);
        av_dict_free(&aopts);
        avcodec_parameters_from_context(aStream->codecpar, aCodecCtx);

        vFrame = av_frame_alloc();
        vFrame->pts = 0;
        vFrame->format = AV_PIX_FMT_YUV420P;
        vFrame->width = width;
        vFrame->height = height;

        aFrame = av_frame_alloc();
        aFrame->pts = 0;
        aFrame->format = aCodecCtx->sample_fmt;
        aFrame->ch_layout = aCodecCtx->ch_layout;
        aFrame->sample_rate = aCodecCtx->sample_rate;
        aFrame->nb_samples = aCodecCtx->frame_size;

        packet = av_packet_alloc();

        avio_open(&fmtCtx->pb, path.c_str(), AVIO_FLAG_WRITE);
        avformat_write_header(fmtCtx, nullptr) == 0;
    }

    void writeAudio(ep_sp<DecodedAudio> audio) {
        audio = audio->copy();
        audio->resample(aCodecCtx->ch_layout.nb_channels, aCodecCtx->sample_rate);

        auto frameSamples = aCodecCtx->frame_size * audio->channels;

        for (uint64_t offset = 0; offset + frameSamples <= audio->data.size(); offset += frameSamples) {
            aFrame->data[0] = (uint8_t*)(audio->data.data() + offset);
            aFrame->pts += aCodecCtx->frame_size;

            if (avcodec_send_frame(aCodecCtx, aFrame) < 0) {
                std::cerr << "failed to send a frame" << std::endl;
            }

            flush();
        }
    }

    void writeVideoFrame(uint8_t* data[3], uint64_t linesize[3]) {
        vFrame->data[0] = data[0];
        vFrame->data[1] = data[1];
        vFrame->data[2] = data[2];
        vFrame->linesize[0] = linesize[0];
        vFrame->linesize[1] = linesize[1];
        vFrame->linesize[2] = linesize[2];
        vFrame->pts++;

        if (avcodec_send_frame(vCodecCtx, vFrame) < 0) {
            std::cerr << "failed to send v frame" << std::endl;
        }

        flush();
    }

    ~VideoCap() {
        avcodec_send_frame(vCodecCtx, nullptr);
        avcodec_send_frame(aCodecCtx, nullptr);
        flush();

        av_write_trailer(fmtCtx);
        avio_closep(&fmtCtx->pb);

        avformat_free_context(fmtCtx);
        avcodec_free_context(&vCodecCtx);
        avcodec_free_context(&aCodecCtx);
        av_frame_free(&vFrame);
        av_frame_free(&aFrame);
        av_packet_free(&packet);
    }

    private:
    void flushStream(AVStream* stream, AVCodecContext* codecCtx) {
        while (avcodec_receive_packet(codecCtx, packet) == 0) {
            av_packet_rescale_ts(packet, codecCtx->time_base, stream->time_base);
            packet->stream_index = stream->index;
            av_interleaved_write_frame(fmtCtx, packet);
            av_packet_unref(packet);
        }
    }

    void flush() {
        flushStream(vStream, vCodecCtx);
        flushStream(aStream, aCodecCtx);
    }
};

struct WindowBase {
    GLFWwindow* window;
    int width, height;
    double frameBusyWaitPercentage = 0.8;
    bool fullscreen;
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

    void setHidden(bool value) {
        if (value) glfwHideWindow(window);
        else glfwShowWindow(window);
    }

    void setVSync(bool value) {
        glfwSwapInterval((int)value);
        vsyncEnabled = value;
    }

    void busyWait(double frameSt, bool printInfo) {
        if (!vsyncEnabled) return;

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

    template <typename TakeOverer>
    bool mainloopFrame(
        const MainloopConfigBase& config,
        const TakeOverer& renderer,
        std::function<typename TakeOverer::element_type::RenderConfig(const typename TakeOverer::element_type::RenderConfig&)> renderConfigurer = [](auto c) { return c; },
        std::function<void(typename TakeOverer::element_type::RenderResultInfo&)> callback = [](auto i) {}
    ) {
        auto frameSt = globalTimer();

        if (shouldClose()) return false;

        if (!config.isRenderingVideo) {
            glfwGetFramebufferSize(window, &width, &height);
        }

        renderer->calcConfig.screenSize = { (double)width, (double)height };
        
        auto& resultInfo = renderer->render(renderConfigurer({
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

    private:
    bool vsyncEnabled = false;
};

void createGLfwWindow(WindowBase& wbase) {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4);

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

    wbase.setVSync(false);
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
            auto path = PhiStoryboardHelpers::nameToPath(base.chartDir, name);

            Data data;
            if (!Data::MakeFromFile(data, path)) {
                std::cout << "failed to read storyboard file: " << name << std::endl;
            }

            return data;
        };

        renderer->shaderDataLoader = [this](const std::string& name) -> std::string {
            Data shaderText {};

            if (!PhiStaticResourceHelpers::getBuiltinShader(name, shaderText)) {
                if (!Data::MakeFromFile(shaderText, PhiStoryboardHelpers::nameToPath(base.chartDir, name))) {
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
        base.chartDir = chartDir;
        auto data = Data::MakeFromFile(path);

        auto resultInfo = renderer->loadChart(data, [this](auto& chart) {
            Data extraData;

            if (Data::MakeFromFile(extraData, base.chartDir + "extra.json")) {
                try {
                    chart.extra = loadPhiExtraFromJsonData(extraData, chart.storyboardAssets);
                } catch (const std::exception& e) {
                    std::cout << "failed to load extra: " << e.what() << std::endl;
                }
            }

            chart.init();
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
        renderer->pauseButtonTextureDataLoader = MilStaticResourceHelpers::pauseButtonTextureDataLoader;
        
        renderer->glCtx = base.glCtx;
        renderer->sharedComp.textureDecoder = decodeImage;
        renderer->textManager.renderer = MilStaticResourceHelpers::createTextRenderer();
        renderer->audioManager.decoder = decodeAudioMiniaudio;
        renderer->audioManager.engine = makeAudioEngineMiniaudio();
        renderer->init();
    }

    auto loadChart(const std::string& path, const std::string& chartDir) {
        base.chartDir = chartDir;
        auto data = Data::MakeFromFile(path);
        
        auto resultInfo = renderer->loadChart(data);
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
