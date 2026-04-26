#include <easy_phi.hpp>

#include <miniaudio/miniaudio.h>
#define GLFW_INCLUDE_NONE
#include <glad/glad.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <skia/gpu/GrDirectContext.h>
#include <skia/gpu/gl/GrGLInterface.h>
#include <skia/gpu/ganesh/gl/GrGLDirectContext.h>
#include <skia/gpu/ganesh/SkSurfaceGanesh.h>
#include <skia/gpu/GpuTypes.h>
#include <skia/gpu/GrBackendSurface.h>
#include <skia/gpu/ganesh/gl/GrGLBackendSurface.h>
#include <skia/core/SkColorType.h>
#include <skia/core/SkColorSpace.h>
#include <skia/gpu/GrTypes.h>
#include <skia/core/SkCanvas.h>
#include <skia/core/SkColor.h>
#include <skia/effects/SkGradientShader.h>
#include <skia/effects/SkImageFilters.h>
#include <skia/core/SkStream.h>
#include <skia/core/SkData.h>
#include <skia/core/SkImage.h>
#include <skia/core/SkFont.h>
#include <skia/core/SkTypeface.h>
#include <skia/core/SkFontMgr.h>
#include <skia/ports/SkTypeface_win.h>
#include <skia/core/SkTextBlob.h>
#include <skia/core/SkPathEffect.h>
#include <skia/effects/SkDashPathEffect.h>
#include <skia/core/SkColorFilter.h>
#include <skia/effects/SkRuntimeEffect.h>
#include <skia/core/SkM44.h>
#include <skia/core/SkMaskFilter.h>
#include <skia/core/SkBlurTypes.h>
#include <skia/core/SkString.h>
#include <stb/stb_vorbis.c>
extern "C" {
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libavutil/imgutils.h>
}

#include "./resources/inlined_resources.cpp"

#include <condition_variable>
#include <queue>
#include <mutex>
#include <thread>
#include <set>
#include <cctype>
#include <sstream>
#include <stdexcept>
#include <unordered_set>
#include <string_view>
#include <utility>
#include <regex>
#include <map>
#include <charconv>

#define WAV_HEADER_SIZE 44

uint8_t* pcm16ToWav(int16_t* pcm, uint64_t pcm_size, int16_t chn, int32_t rate) {
    uint64_t wav_size = pcm_size + WAV_HEADER_SIZE;
    auto* wav = (uint8_t*)malloc(wav_size);

    *(char*)&wav[0] = 'R';
    *(char*)&wav[1] = 'I';
    *(char*)&wav[2] = 'F';
    *(char*)&wav[3] = 'F';
    *(int32_t*)&wav[4] = wav_size - 8;
    *(char*)&wav[8] = 'W';
    *(char*)&wav[9] = 'A';
    *(char*)&wav[10] = 'V';
    *(char*)&wav[11] = 'E';
    
    *(char*)&wav[12] = 'f';
    *(char*)&wav[13] = 'm';
    *(char*)&wav[14] = 't';
    *(char*)&wav[15] = ' ';
    *(int32_t*)&wav[16] = 0x10;
    *(int16_t*)&wav[20] = 1; // PCM
    *(int16_t*)&wav[22] = chn;
    *(int32_t*)&wav[24] = rate;
    *(int32_t*)&wav[28] = rate * chn * sizeof(int16_t);
    *(int16_t*)&wav[32] = chn * sizeof(int16_t);
    *(int16_t*)&wav[34] = 2 * 8;

    *(char*)&wav[36] = 'd';
    *(char*)&wav[37] = 'a';
    *(char*)&wav[38] = 't';
    *(char*)&wav[39] = 'a';
    *(int32_t*)&wav[40] = pcm_size;
    std::memcpy((void*)((uint8_t*)wav + WAV_HEADER_SIZE), pcm, pcm_size);

    return wav;
}

bool oggToWav(easy_phi::Data& data) {
    int chn, rate;
    int16_t* pcm;
    uint64_t frames = stb_vorbis_decode_memory(data.data.data(), data.data.size(), &chn, &rate, &pcm);
    if (frames <= 0) return false;

    uint32_t pcm_size = frames * chn * sizeof(int16_t);
    auto* wav = pcm16ToWav(pcm, pcm_size, chn, rate);
    free(pcm);
    data.data = std::vector<uint8_t>(wav, wav + pcm_size + WAV_HEADER_SIZE);
    return true;
}

bool dataIsStartsWith(const easy_phi::Data& data, const std::string& prefix) {
    return data.data.size() >= prefix.size() && std::memcmp(data.data.data(), prefix.data(), prefix.size()) == 0;
}

struct sound_ex {
    ma_sound sound;
    ma_decoder decoder;
    uint64_t encoded_size;
};

ma_sound* loadMaSoundFromMemory(ma_engine* engine, easy_phi::Data& data) {
    if (dataIsStartsWith(data, "OggS")) {
        if (!oggToWav(data)) return nullptr;
    }

    auto* ex = (sound_ex*)malloc(sizeof(sound_ex) + data.data.size());
    ex->encoded_size = data.data.size();
    memcpy(ex + 1, data.data.data(), data.data.size());
    if (ma_decoder_init_memory((void*)(ex + 1), data.data.size(), NULL, &ex->decoder) != MA_SUCCESS) {
        free(ex);
        return nullptr;
    }
    if (ma_sound_init_from_data_source(engine, &ex->decoder, 0, NULL, &ex->sound) != MA_SUCCESS) {
        ma_decoder_uninit(&ex->decoder);
        free(ex);
        return nullptr;
    }

    return (ma_sound*)ex;
}

ma_uint32 getMaSoundSampleRate(ma_sound* snd) {
    ma_uint32 sr = 0;
    ma_data_source *pSrc = ma_sound_get_data_source(snd);
    ma_data_source_get_data_format(pSrc, nullptr, nullptr, &sr, nullptr, 0);
    return sr;
}

double getMaSoundPosition(ma_sound* snd) {
    ma_uint64 cframes = 0;
    ma_sound_get_cursor_in_pcm_frames(snd, &cframes);
    return (double)cframes / getMaSoundSampleRate(snd);
}

double getMaSoundDuration(ma_sound* snd) {
    ma_uint64 frames = 0;
    ma_sound_get_length_in_pcm_frames(snd, &frames);
    return (double)frames / getMaSoundSampleRate(snd);
}

auto loadImage(const easy_phi::Data& data) {
    sk_sp<SkData> illuSkData = SkData::MakeWithCopy(data.data.data(), data.data.size());
    return SkImages::DeferredFromEncodedData(std::move(illuSkData));
}

SkRect cvtRect(const easy_phi::Rect& rect) {
    return SkRect::MakeXYWH(rect.x, rect.y, rect.w, rect.h);
}

SkColor4f cvtColor(const easy_phi::Color& color) {
    return SkColor4f(color.r, color.g, color.b, color.a);
}

SkPoint cvtVec2(const easy_phi::Vec2& vec) {
    return SkPoint::Make(vec.x, vec.y);
}

double globalTimer() {
    return std::chrono::duration<double>(
        std::chrono::system_clock::now()
        .time_since_epoch()
    ).count();
}

uint64_t rotl64(uint64_t x, uint64_t n) {
    return (x << n) | (x >> (64 - n));
}

struct FontInstance {
    sk_sp<SkTypeface> skTypeface;
    uint64_t cacheMaxSize = 65536;

    sk_sp<SkTextBlob> getTextBlob(void* text, uint64_t textLen, float fontsize) {
        uint64_t key = getBlobKey(text, textLen, fontsize);
        if (textBlobs.find(key) != textBlobs.end()) {
            return textBlobs[key];
        }

        SkFont font(skTypeface, fontsize);
        sk_sp<SkTextBlob> blob = SkTextBlob::MakeFromText((const void*)text, textLen, font);
        while (textBlobs.size() >= cacheMaxSize) {
            textBlobs.erase(textBlobs.begin());
        }
        textBlobs[key] = blob;
        return blob;
    }

    sk_sp<SkTextBlob> getTextBlob(const char* text, float fontsize) {
        return getTextBlob((void*)text, strlen(text), fontsize);
    }

    sk_sp<SkTextBlob> getTextBlob(std::string text, float fontsize) {
        return getTextBlob((void*)text.c_str(), text.size(), fontsize);
    }

    SkRect getTextMeasure(void* text, uint64_t textLen, float fontsize) {
        uint64_t key = getBlobKey(text, textLen, fontsize);
        if (textMeasures.find(key) != textMeasures.end()) {
            return textMeasures[key];
        }

        SkFont font(skTypeface, fontsize);
        SkRect rect;
        font.measureText((const void*)text, textLen, SkTextEncoding::kUTF8, &rect);
        while (textMeasures.size() >= cacheMaxSize) {
            textMeasures.erase(textMeasures.begin());
        }
        textMeasures[key] = rect;
        return rect;
    }

    SkRect getTextMeasure(const char* text, float fontsize) {
        return getTextMeasure((void*)text, strlen(text), fontsize);
    }

    SkRect getTextMeasure(std::string text, float fontsize) {
        return getTextMeasure((void*)text.c_str(), text.size(), fontsize);
    }

    private:
    std::map<uint64_t, sk_sp<SkTextBlob>> textBlobs;
    std::map<uint64_t, SkRect> textMeasures;

    uint64_t getBlobKey(void* text, uint64_t textLen, float fontsize) {
        uint64_t hash = 0x5bd1e995;
        uint8_t* p = (uint8_t*)text;

        for (uint64_t i = 0; i < 8; i++) {
            hash = rotl64(hash, 24) * 0x51ed2875;
            hash += (textLen >> (i * 8)) & 0xFF;
        }

        for (uint64_t i = 0; i < textLen; ++i) {
            hash = rotl64(hash, 24) * 0x51ed2875;
            hash += p[i];
        }

        uint32_t fbits;
        memcpy(&fbits, &fontsize, sizeof(float));
        hash = rotl64(hash, 24) * 0x51ed2875;
        hash += fbits;
        hash = rotl64(hash, 24) * 0x51ed2875;
        hash += fbits >> 16;
        return hash;
    }
};

struct FontMgr {
    sk_sp<SkFontMgr> skFontMgr;

    static FontMgr Make() {
        FontMgr m;
        m.skFontMgr = SkFontMgr_New_GDI();
        return m;
    }

    FontInstance makeFontFromData(const easy_phi::Data& data) {
        FontInstance ins;
        ins.skTypeface = skFontMgr->makeFromData(SkData::MakeWithCopy(data.data.data(), data.data.size()));
        return ins;
    }
};

using StoryboardTexturesType = std::unordered_map<easy_phi::ep_u64, sk_sp<SkImage>>;
void attachTextureLoader(
    easy_phi::PhiChart& chart,
    const std::string& assetsPath,
    StoryboardTexturesType& loadedStoryboardTextures
) {
    static easy_phi::ep_u64 currentStoryboardTextureId = 0;
    StoryboardTexturesType* loadedStoryboardTexturesPtr = &loadedStoryboardTextures;

    easy_phi::StoryboardHelpers::attachTextureLoader(
        chart.storyboardAssets,
        assetsPath,
        [=](std::string path) -> std::optional<std::pair<easy_phi::ep_u64, easy_phi::Vec2>> {
            easy_phi::Data imageData;
            easy_phi::Data::FromFile(&imageData, path);
            auto image = loadImage(imageData);
            if (!image) return std::nullopt;

            easy_phi::ep_u64 id = currentStoryboardTextureId++;
            (*loadedStoryboardTexturesPtr)[id] = image;
            return std::make_pair(id, easy_phi::Vec2 { (double)image->width(), (double)image->height() });
        },
        [=](easy_phi::ep_u64 texture) {
            loadedStoryboardTexturesPtr->erase(texture);
        }
    );
}

using NoteImagesType = std::unordered_map<easy_phi::EnumPhiNoteType, std::pair<sk_sp<SkImage>, sk_sp<SkImage>>>;
NoteImagesType loadNoteImages(easy_phi::CalculateFrameConfig& config) {
    NoteImagesType noteImages;

    for (const auto& [ type, name ] : std::vector<std::pair<easy_phi::EnumPhiNoteType, std::string>> {
        { easy_phi::EnumPhiNoteType::Tap, "click" },
        { easy_phi::EnumPhiNoteType::Drag, "drag" },
        { easy_phi::EnumPhiNoteType::Flick, "flick" },
        { easy_phi::EnumPhiNoteType::Hold, "hold" },
    }) {
        auto single_data = StaticResource::get(std::string("/") + name + ".png");
        auto simul_data = StaticResource::get(std::string("/") + name + "_mh.png");
        noteImages[type] = { loadImage(single_data), loadImage(simul_data) };

        auto simulScale = (double)noteImages[type].second->width() / noteImages[type].first->width();

        config.noteTextureInfos[type] = easy_phi::CalculateFrameConfig::NoteTextureInfo {
            .single = easy_phi::CalculateFrameConfig::NoteTextureInfo::Item {
                .textureSize = easy_phi::Vec2 { (double)noteImages[type].first->width(), (double)noteImages[type].first->height() },
                .cutPadding = name == "hold" ? easy_phi::Vec2 { 50.0, 50.0 } : easy_phi::Vec2 { (double)noteImages[type].first->height() / 2, (double)noteImages[type].first->height() / 2 }
            },
            .simul = easy_phi::CalculateFrameConfig::NoteTextureInfo::Item {
                .textureSize = easy_phi::Vec2 { (double)noteImages[type].second->width(), (double)noteImages[type].second->height() },
                .cutPadding = name == "hold" ? easy_phi::Vec2 { 100.0, 100.0 } : easy_phi::Vec2 { (double)noteImages[type].second->height() / 2, (double)noteImages[type].second->height() / 2 },
                .scaling = easy_phi::Vec2 { simulScale, simulScale }
            },
        };
    }

    return noteImages;
}

static const int hitsoundsBufferSize = 3;

using NoteHitsoundsType = std::unordered_map<easy_phi::EnumPhiNoteType, std::vector<ma_sound*>>;
NoteHitsoundsType loadNoteHitsounds(ma_engine& eng) {
    NoteHitsoundsType noteHitsounds;
    for (const auto& [ type, name ] : std::vector<std::pair<easy_phi::EnumPhiNoteType, std::string>> {
        { easy_phi::EnumPhiNoteType::Tap, "click" },
        { easy_phi::EnumPhiNoteType::Drag, "drag" },
        { easy_phi::EnumPhiNoteType::Flick, "flick" },
        { easy_phi::EnumPhiNoteType::Hold, "click" },
    }) {
        auto data = StaticResource::get(std::string("/") + name + ".wav");
        for (int i = 0; i < hitsoundsBufferSize; i++) {
            noteHitsounds[type].push_back(loadMaSoundFromMemory(&eng, data));
        }
    }

    return noteHitsounds;
}

using HitEffectImagesType = std::vector<sk_sp<SkImage>>;
HitEffectImagesType loadHitEffectImages() {
    HitEffectImagesType hitEffectImages;
    for (int i = 0; i < 60; i++) {
        auto data = StaticResource::get(std::string("/") + "hit_fx_" + std::to_string(i + 1) + ".png");
        hitEffectImages.push_back(loadImage(data));
    }

    return hitEffectImages;
}

struct Pcm16 {
    std::vector<int16_t> pcm;
    uint16_t channels;
    uint32_t sampleRate;

    uint64_t GetPcmByteSize() {
        return pcm.size() * sizeof(int16_t);
    }

    uint64_t GetPcmSampleSize() {
        return pcm.size();
    }
};

#define PCM_FIXED_SAMPLE_RATE 44100
#define PCM_FIXED_CHANNELS 2

std::pair<Pcm16, bool> decodePcm16FromMaSound(ma_sound* snd) {
    auto* ex = (sound_ex*)snd;
    ma_decoder decoder{};
    ma_decoder_config decoderCfg{};
    decoderCfg.format = ma_format_s16;
    decoderCfg.channels = PCM_FIXED_CHANNELS;
    decoderCfg.sampleRate = PCM_FIXED_SAMPLE_RATE;

    if (ma_decoder_init_memory((void*)(ex + 1), ex->encoded_size, &decoderCfg, &decoder) != MA_SUCCESS) {
        return { {}, false };
    }

    uint32_t channels = decoder.outputChannels;
    uint32_t sampleRate = decoder.outputSampleRate;
    ma_uint64 totalFrames = 0;
    if (ma_data_source_get_length_in_pcm_frames(&decoder, &totalFrames) != MA_SUCCESS) {
        ma_decoder_uninit(&decoder);
        return { {}, false };
    }

    if (channels != PCM_FIXED_CHANNELS) {
        std::cerr << "pcm channels mismatch\n";
        ma_decoder_uninit(&decoder);
        return { {}, false };
    }

    if (sampleRate != PCM_FIXED_SAMPLE_RATE) {
        std::cerr << "pcm sample rate mismatch\n";
        ma_decoder_uninit(&decoder);
        return { {}, false };
    }

    Pcm16 pcm {};
    pcm.pcm = std::vector<int16_t>(totalFrames * channels);
    pcm.channels = channels;
    pcm.sampleRate = sampleRate;

    ma_uint64 framesDecoded = 0;
    if (ma_decoder_read_pcm_frames(&decoder, pcm.pcm.data(), totalFrames, &framesDecoded) != MA_SUCCESS) {
        ma_decoder_uninit(&decoder);
        return { {}, false };
    }

    ma_decoder_uninit(&decoder);

    return { pcm, true };
}

void interleave_uv_16(uint8_t* dst, uint8_t* u, uint8_t* v) {
    __m128i u16 = _mm_loadu_si128((const __m128i*)u);
    __m128i v16 = _mm_loadu_si128((const __m128i*)v);
    __m128i lo = _mm_unpacklo_epi8(u16, v16);
    __m128i hi = _mm_unpackhi_epi8(u16, v16);
    _mm_storeu_si128((__m128i*)(dst +  0), lo);
    _mm_storeu_si128((__m128i*)(dst + 16), hi);
}

void interleave_uv(uint8_t* dst, uint8_t* u, uint8_t* v, uint64_t count) {
    uint64_t i = 0;
    for (; i + 15 < count; i += 16) {
        interleave_uv_16(dst + i * 2, u + i, v + i);
    }
    for (; i < count; ++i) {
        dst[i * 2 + 0] = u[i];
        dst[i * 2 + 1] = v[i];
    }
}

std::string av_error_string(int errnum) {
    std::array<char, AV_ERROR_MAX_STRING_SIZE> buf{};
    av_make_error_string(buf.data(), buf.size(), errnum);
    return std::string(buf.data());
}

static bool DISABLE_H264_QSV = false;
struct VideoCap {
    const char* path;
    int width; int height; double fps;
    bool isOpened = false;

    AVFormatContext* fmtCtx;

    enum class VCodecType {
        LIBX264,
        H264_QSV
    };

    AVCodecContext* vCodecCtx = nullptr;
    AVStream* vStream = nullptr;
    int vFrameIdx = 0;
    uint64_t writtenVideoFrameCount = 0;
    VCodecType vCodecType;

    AVBufferRef* hw_device_ctx = nullptr;
    AVBufferRef* hw_frames_ctx = nullptr;
    AVFrame* vSwFrame = nullptr;

    AVStream* aStream = nullptr;
    AVCodecContext* aCodecCtx = nullptr;
    int aFramePts = 0;
    bool wroteAudio = false;

    VideoCap(const char* path, int width, int height, double fps) {
        int err;
        this->path = path;
        this->width = width; this->height = height; this->fps = fps;

        auto init = [&]() {
            fmtCtx = avformat_alloc_context();
            fmtCtx->oformat = av_guess_format("mp4", nullptr, nullptr);
        };

        AVCodec* vCodec = nullptr;        
        if (!DISABLE_H264_QSV && !vCodec && (vCodec = (AVCodec*)avcodec_find_encoder_by_name("h264_qsv"))) {
            init();
            vStream = avformat_new_stream(fmtCtx, vCodec);
            vCodecCtx = avcodec_alloc_context3(vCodec);
            vCodecCtx->width = width;
            vCodecCtx->height = height;
            vCodecCtx->time_base = {1, (int)fps};
            vCodecCtx->framerate = {(int)fps, 1};
            vCodecCtx->pix_fmt = AV_PIX_FMT_QSV;
            vCodecCtx->gop_size = std::max(10, (int)fps / 4);
            vCodecCtx->max_b_frames = 1;

            AVBufferRef* qsv_hw_dev = nullptr;
            AVDictionary* qsv_opts = nullptr;
            av_dict_set(&qsv_opts, "child_device_type", "dxva2", 0);
            err = av_hwdevice_ctx_create(&qsv_hw_dev, AV_HWDEVICE_TYPE_QSV, nullptr, qsv_opts, 0);
            av_dict_free(&qsv_opts);
            if (err < 0) {
                std::cerr << "failed to create qsv device context: " << av_error_string(err) << std::endl;
                goto h264_qsv_failed;
            }
            hw_device_ctx = qsv_hw_dev;
            vCodecCtx->hw_device_ctx = av_buffer_ref(hw_device_ctx);

            hw_frames_ctx = av_hwframe_ctx_alloc(hw_device_ctx);
            AVHWFramesContext* hwfc = (AVHWFramesContext*)hw_frames_ctx->data;
            hwfc->format = vCodecCtx->pix_fmt;
            hwfc->sw_format = AV_PIX_FMT_NV12;
            hwfc->width = vCodecCtx->width;
            hwfc->height = vCodecCtx->height;
            hwfc->initial_pool_size = 32;
            hwfc->device_ctx = (AVHWDeviceContext*)hw_device_ctx->data;
            err = av_hwframe_ctx_init(hw_frames_ctx);
            if (err < 0) {
                std::cerr << "failed to init hw frame context: " << av_error_string(err) << std::endl;
                goto h264_qsv_failed;
            }
            vCodecCtx->hw_frames_ctx = av_buffer_ref(hw_frames_ctx);

            AVDictionary* vopts = nullptr;
            av_dict_set_int(&vopts, "global_quality", 25, 0);
            av_dict_set(&vopts, "async_depth", "4", 0);
            err = avcodec_open2(vCodecCtx, vCodec, &vopts);
            av_dict_free(&vopts);
            if (err < 0) {
                std::cerr << "failed to open h264_qsv encoder: " << av_error_string(err) << std::endl;
                goto h264_qsv_failed;
            }

            vSwFrame = av_frame_alloc();
            vSwFrame->format = AV_PIX_FMT_NV12;
            vSwFrame->width = width;
            vSwFrame->height = height;
            av_frame_get_buffer(vSwFrame, 0);
            
            vCodecType = VCodecType::H264_QSV;
        }
        goto h264_qsv_success;
        h264_qsv_failed:
        std::cout << "h264_qsv failed, fallback to libx264\n";
        vCodec = nullptr;
        FreeResource();
        h264_qsv_success:
        
        if (!vCodec && (vCodec = (AVCodec*)avcodec_find_encoder(AV_CODEC_ID_H264))) {
            init();
            vStream = avformat_new_stream(fmtCtx, vCodec);
            vCodecCtx = avcodec_alloc_context3(vCodec);
            vCodecCtx->width = width;
            vCodecCtx->height = height;
            vCodecCtx->time_base = {1, (int)fps};
            vCodecCtx->framerate = {(int)fps, 1};
            vCodecCtx->pix_fmt = AV_PIX_FMT_YUV420P;
            vCodecCtx->gop_size = std::max(10, (int)fps / 4);
            vCodecCtx->max_b_frames = 1;

            AVDictionary* vopts = nullptr;
            av_dict_set(&vopts, "preset", "ultrafast", 0);
            av_dict_set(&vopts, "tune", "zerolatency", 0);
            err = avcodec_open2(vCodecCtx, vCodec, &vopts);
            av_dict_free(&vopts);
            if (err < 0) {
                std::cerr << "failed to open libx264 vcodec: " << av_error_string(err) << std::endl;
                return;
            }

            vSwFrame = av_frame_alloc();
            vSwFrame->format = AV_PIX_FMT_YUV420P;
            vSwFrame->width = width;
            vSwFrame->height = height;
            
            vCodecType = VCodecType::LIBX264;
        }
        goto libx264_success;
        libx264_failed:
        std::cout << "libx264 failed\n";
        vCodec = nullptr;
        FreeResource();
        libx264_success:

        if (!vCodec) {
            std::cerr << "no encoder found\n";
            return;
        }

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
        err = avcodec_open2(aCodecCtx, aCodec, &aopts);
        av_dict_free(&aopts);
        if (err < 0) {
            std::cerr << "failed to open acodec: " << av_error_string(err) << std::endl;
            return;
        }

        avcodec_parameters_from_context(aStream->codecpar, aCodecCtx);

        err = avio_open(&fmtCtx->pb, path, AVIO_FLAG_WRITE);
        if (err < 0) {
            std::cerr << "failed to open file: " << av_error_string(err) << std::endl;
            return;
        }

        err = avformat_write_header(fmtCtx, nullptr);
        if (err < 0) {
            std::cerr << "failed to write header: " << av_error_string(err) << std::endl;
            return;
        }

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

    bool writeAudio(const Pcm16& pcm) {
        if (wroteAudio) return false;
        if (pcm.sampleRate != aCodecCtx->sample_rate || pcm.channels != aCodecCtx->ch_layout.nb_channels) return false;

        const int frameSize = aCodecCtx->frame_size;
        for (uint64_t offset = 0; offset + frameSize * pcm.channels <= pcm.pcm.size(); offset += frameSize * pcm.channels) {
            AVFrame* f = av_frame_alloc();
            f->format = aCodecCtx->sample_fmt;
            f->ch_layout = aCodecCtx->ch_layout;
            f->sample_rate = aCodecCtx->sample_rate;
            f->nb_samples = frameSize;
            f->data[0] = (uint8_t*)(pcm.pcm.data() + offset);
            f->pts = aFramePts;
            aFramePts += frameSize;

            avcodec_send_frame(aCodecCtx, f);

            if (offset + frameSize * pcm.channels > pcm.pcm.size()) {
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

        if (vCodecType == VCodecType::LIBX264) {
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
        } else if (vCodecType == VCodecType::H264_QSV) {
            uint64_t pts = vFrameIdx++;

            vSwFrame->pts = pts;

            memcpy(vSwFrame->data[0], y, lsy * vCodecCtx->height);
            interleave_uv(
                vSwFrame->data[1],
                (uint8_t*)u, (uint8_t*)v,
                vCodecCtx->width * vCodecCtx->height / 4
            );

            // 不能复用 hw 的 AVFrame, 会申请buffer失败/画面抖动
            AVFrame* hw = av_frame_alloc();
            hw->width = vCodecCtx->width;
            hw->height = vCodecCtx->height;
            hw->format = vCodecCtx->pix_fmt;
            hw->pts = pts;
            hw->hw_frames_ctx = vCodecCtx->hw_frames_ctx;

            if (av_hwframe_get_buffer(vCodecCtx->hw_frames_ctx, hw, 0) < 0) {
                std::cerr << "failed to get hw buffer\n";
                av_frame_free(&hw);
                return false;
            }

            if (av_hwframe_transfer_data(hw, vSwFrame, 0) < 0) {
                std::cerr << "failed to transfer data\n";
                av_frame_free(&hw);
                return false;
            }

            if (avcodec_send_frame(vCodecCtx, hw) < 0) {
                std::cerr << "failed to send frame\n";
                av_frame_free(&hw);
                return false;
            }

            flush();
            av_frame_free(&hw);
        } else {
            std::cerr << "unsupported pixel format" << std::endl;
            return false;
        }

        return true;
    }

    const wchar_t* getCodecName() {
        if (vCodecType == VCodecType::LIBX264) {
            return L"libx264";
        } else if (vCodecType == VCodecType::H264_QSV) {
            return L"h264_qsv";
        } else {
            return L"unknown";
        }
    }

    ~VideoCap() {
        if (isOpened) {
            avcodec_send_frame(vCodecCtx, nullptr);
            flush();

            av_write_trailer(fmtCtx);
            avio_closep(&fmtCtx->pb);
        }

        FreeResource();
    }

    private:
    void FreeResource() {
        if (hw_frames_ctx) av_buffer_unref(&hw_frames_ctx);
        if (hw_device_ctx) av_buffer_unref(&hw_device_ctx);
        if (vSwFrame) av_frame_free(&vSwFrame);
        if (vCodecCtx) avcodec_free_context(&vCodecCtx);
        if (aCodecCtx) avcodec_free_context(&aCodecCtx);
        if (fmtCtx) avformat_free_context(fmtCtx);
    }
};

struct Window {
    GLFWwindow* window;
    sk_sp<GrDirectContext> skGrCtx;
    GrGLFramebufferInfo skFbi;
    sk_sp<SkColorSpace> skSurfaceColorSpace;
    GrBackendRenderTarget skTarget;
    sk_sp<SkSurface> skSurface;
    SkSamplingOptions kImSO;
    SkCanvas* skCanvas;
    bool vsync;
    ma_engine maeng;
    easy_phi::CalculateFrameConfig calculateFrameConfig;
    ma_sound* mainSound;
    NoteImagesType noteImages;
    NoteHitsoundsType noteHitsounds;
    HitEffectImagesType hitEffectImages;
    std::vector<ma_sound*> playingHitsounds;
    FontMgr fontMgr;
    FontInstance font;
    std::pair<double, sk_sp<SkImage>> cachedBluredImage;
    sk_sp<SkImage> bgImage;
    sk_sp<SkSurface> bluredImageTempSurface;
    double globalScale;
    easy_phi::CalculatedFrame calculatedFrame;
    easy_phi::PhiChart chart;
    StoryboardTexturesType loadedStoryboardTextures;
    int width, height;
    bool hidden;

    void init() {
        glfwInit();
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        if (hidden) glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        auto* vm = (GLFWvidmode*)glfwGetVideoMode(glfwGetPrimaryMonitor());
        int32_t width = vm->width * 0.6;
        int32_t height = vm->height * 0.6;
        window = glfwCreateWindow(width, height, "", nullptr, nullptr);
        glfwMakeContextCurrent(window);
        gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
        skGrCtx = GrDirectContexts::MakeGL();
        skGrCtx->setResourceCacheLimit(std::numeric_limits<size_t>::max());
        skFbi.fFBOID = 0;
        skFbi.fFormat = GL_RGBA8;
        kImSO = {SkFilterMode::kLinear, SkMipmapMode::kLinear};
        skSurfaceColorSpace = SkColorSpace::MakeSRGB();
        vsync = false;
        resetSurface();

        ma_engine_init(NULL, &maeng);

        noteImages = loadNoteImages(calculateFrameConfig);
        noteHitsounds = loadNoteHitsounds(maeng);
        hitEffectImages = loadHitEffectImages();

        fontMgr = FontMgr::Make();
        font = fontMgr.makeFontFromData(StaticResource::get("/font.ttf"));

        cachedBluredImage = { -1, nullptr };
        globalScale = 1.0;
    }

    void setHidden(bool newValue) {
        hidden = newValue;
        if (hidden) glfwHideWindow(window);
        else glfwShowWindow(window);
    }

    std::optional<std::string> loadMainSound(const std::string& path) {
        easy_phi::Data data;
        if (!easy_phi::Data::FromFile(&data, path)) return "failed to read file";
        mainSound = loadMaSoundFromMemory(&maeng, data);
        if (!mainSound) return "failed to load";
        calculateFrameConfig.songLength = getMaSoundDuration(mainSound);
        return std::nullopt;
    }

    void startMainSound() {
        ma_sound_start(mainSound);
        while (!ma_sound_is_playing(mainSound)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }
    }

    std::optional<std::string> loadBgImage(const std::string& path) {
        easy_phi::Data data;
        if (!easy_phi::Data::FromFile(&data, path)) return "failed to read file";
        bgImage = loadImage(data);
        if (!bgImage) return "failed to load";
        bluredImageTempSurface = skSurface->makeSurface(bgImage->width(), bgImage->height());
        calculateFrameConfig.backgroundTextureSize = { (double)bgImage->width(), (double)bgImage->height() };
        return std::nullopt;
    }

    std::optional<std::string> loadChart(const std::string& path, const std::string& chartDir) {
        easy_phi::Data data;
        if (!easy_phi::Data::FromFile(&data, path)) return "failed to read chart file";

        double load_st = globalTimer();
        auto chartLoadResult = easy_phi::loadChartFromData(data);
        std::cout << "loadChartFromData took: " << globalTimer() - load_st << " s" << std::endl;
        std::cout << "chartLoadResult.success: " << chartLoadResult.success << std::endl;
        std::cout << "chartLoadResult.erros: " << std::endl;
        for (const auto& e : chartLoadResult.errors) std::cout << e << std::endl;
        if (!chartLoadResult.success) return "failed to load chart";
        chart = std::move(chartLoadResult.chart);
        
        attachTextureLoader(chart, chartDir, loadedStoryboardTextures);

        easy_phi::Data extraData;
        if (easy_phi::Data::FromFile(&extraData, chartDir + "extra.json")) {
            auto extraLoadResult = easy_phi::loadExtraFromJsonData(extraData, chart.storyboardAssets);
            if (std::holds_alternative<easy_phi::PhiExtra>(extraLoadResult)) {
                chart.extra = std::move(std::get<easy_phi::PhiExtra>(extraLoadResult));
                std::cout << "loaded extra" << std::endl;
            } else if (std::holds_alternative<std::string>(extraLoadResult)) {
                std::cout << "failed to load extra: " << std::get<std::string>(extraLoadResult) << std::endl;
            }
        }

        chart.storyboardAssets.shaderPreloader = [&](const std::string& name) {
            std::cout << std::string(80, '-') << std::endl;
            std::unordered_set<std::string> builtinShaders = {
                "chromatic", "circleBlur", "fisheye",
                "glitch", "grayscale", "noise",
                "pixel", "radialBlur", "shockwave",
                "trigrid", "vignette"
            };

            easy_phi::Data shaderText;
            if (builtinShaders.contains(name)) {
                shaderText = StaticResource::get(std::string("/shaders/") + name + ".glsl");
            } else {
                if (!easy_phi::Data::FromFile(&shaderText, std::filesystem::path(chartDir + "/" + name).lexically_normal().string())) {
                    std::cout << "failed to read shader file: " << name << std::endl;
                    return;
                }
            }

            // TODO: glsl2sksl
            std::string sksl = "";

            std::cout << "preloading shader: " << name << std::endl;

            auto shaderResult = SkRuntimeEffect::MakeForShader(SkString(sksl));
            if (!shaderResult.effect) {
                std::cout << "failed to compile shader: " << name << std::endl;
                std::cout << shaderResult.errorText.c_str() << std::endl;
                return;
            }

            std::cout << "shader compiled: " << shaderResult.effect.get() << std::endl;
        };

        chart.init();

        for (auto& [_, img] : loadedStoryboardTextures) skCanvas->drawImage(img, 0, 0);
        skGrCtx->flush();
        skGrCtx->submit(GrSyncCpu::kYes);
        return std::nullopt;
    }

    void resetSurface() {
        skTarget = GrBackendRenderTargets::MakeGL(width, height, 0, 8, skFbi);
        skSurface = SkSurfaces::WrapBackendRenderTarget(
            skGrCtx.get(), skTarget,
            GrSurfaceOrigin::kBottomLeft_GrSurfaceOrigin,
            SkColorType::kRGBA_8888_SkColorType, skSurfaceColorSpace,
            nullptr, nullptr, nullptr
        );
        skCanvas = skSurface->getCanvas();

        if (vsync) glfwSwapInterval(1);
        else glfwSwapInterval(0);
        calculateFrameConfig.screenSize = { (double)width, (double)height };
    }

    void drawTextBase(std::string text, float x, float y, const SkPaint& paint, float fontsize, easy_phi::EnumTextAlign align, easy_phi::EnumTextBaseline baseline) {
        auto blob = font.getTextBlob(text, fontsize);
        SkRect measure = font.getTextMeasure(text, fontsize);
        double ry = y;
        x -= measure.fLeft;
        y -= measure.fTop;
        float dw = measure.width();
        float dh = measure.height();

        switch (align) {
            case easy_phi::EnumTextAlign::Left: break;
            case easy_phi::EnumTextAlign::Center: x -= dw / 2; break;
            case easy_phi::EnumTextAlign::Right: x -= dw; break;
            default: break;
        }

        switch (baseline) {
            case easy_phi::EnumTextBaseline::Top: break;
            case easy_phi::EnumTextBaseline::Middle: y -= dh / 2; break;
            case easy_phi::EnumTextBaseline::Bottom: y -= dh; break;
            default: break;
        }

        skCanvas->drawTextBlob(blob, x, y, paint);
    }

    void drawText(std::string text, float x, float y, SkColor color, float fontsize, easy_phi::EnumTextAlign align, easy_phi::EnumTextBaseline baseline) {
        SkPaint paint;
        paint.setColor(color);
        drawTextBase(text, x, y, paint, fontsize, align, baseline);
    }

    void drawEPText(const easy_phi::CalculatedFrame::CalculatedText& text) {
        skCanvas->save();
        skCanvas->translate(text.position.x, text.position.y);
        skCanvas->rotate(text.rotation);
        skCanvas->scale(text.scale.x, text.scale.y);
        drawText(text.text, 0.0, 0.0, cvtColor(text.color).toSkColor(), text.fontSize, text.align, text.baseline);
        skCanvas->restore();
    }

    bool mainloopFrame(double t, bool isRenderingVideo = false) {
        double frame_st = globalTimer();
        if (!isRenderingVideo && glfwWindowShouldClose(window)) return false;

        if (!isRenderingVideo) {
            int32_t last_w = width, last_h = height;
            glfwGetFramebufferSize(window, &width, &height);
            if (width != last_w || height != last_h) resetSurface();
        }
        
        {
            double st = globalTimer();
            easy_phi::calculateFrame(chart, t, calculateFrameConfig, calculatedFrame);
            std::cout << "calculateFrame took " << ((globalTimer() - st) * 1000) << " ms" << std::endl;
        }
        
        if (cachedBluredImage.first != calculatedFrame.backgroundImageBlurRadius) {
            static SkPaint pt;
            pt.setImageFilter(SkImageFilters::Blur(
                calculatedFrame.backgroundImageBlurRadius, calculatedFrame.backgroundImageBlurRadius,
                SkTileMode::kClamp, nullptr
            ));

            auto bluredImageTempCanvas = bluredImageTempSurface->getCanvas();
            bluredImageTempCanvas->clear(0);
            bluredImageTempCanvas->drawImage(bgImage, 0.0, 0.0, kImSO, &pt);
            cachedBluredImage = { calculatedFrame.backgroundImageBlurRadius, bluredImageTempSurface->makeImageSnapshot() };
        }

        skCanvas->clear(0);

        skCanvas->drawImageRect(
            cachedBluredImage.second,
            cvtRect(calculatedFrame.unsafeBackgroundRect),
            kImSO
        );

        {
            static SkPaint pt;
            pt.setColor(0xff000000);
            pt.setStyle(SkPaint::kFill_Style);
            pt.setAlphaf(calculatedFrame.unsafeAreaDim);
            skCanvas->drawRect(cvtRect(calculatedFrame.unsafeBackgroundRect), pt);
        }

        skCanvas->save();

        if (globalScale == 1.0) {
            skCanvas->clipRect(cvtRect(calculatedFrame.objectsClipRect));
        } else {
            skCanvas->translate(width / 2, height / 2);
            skCanvas->scale(globalScale, globalScale);
            skCanvas->translate(-width / 2, -height / 2);
        }

        skCanvas->drawImageRect(
            cachedBluredImage.second,
            cvtRect(calculatedFrame.backgroundRect),
            kImSO
        );

        {
            static SkPaint pt;
            pt.setColor(0xff000000);
            pt.setStyle(SkPaint::kFill_Style);
            pt.setAlphaf(calculatedFrame.backgroundDim);
            skCanvas->drawRect(cvtRect(calculatedFrame.backgroundRect), pt);
        }

        for (auto& obj : calculatedFrame.objects) {
            if (std::holds_alternative<easy_phi::CalculatedFrame::CalculatedNote>(obj)) {
                auto& note = std::get<easy_phi::CalculatedFrame::CalculatedNote>(obj);
                
                auto& img = note.isSimul ? noteImages[note.type].second : noteImages[note.type].first;
                auto& imgInfo = note.isSimul ? calculateFrameConfig.noteTextureInfos[note.type].simul : calculateFrameConfig.noteTextureInfos[note.type].single;

                static SkPaint pt;
                pt.setColor(cvtColor(note.color));

                auto constraint = SkCanvas::SrcRectConstraint::kStrict_SrcRectConstraint;

                skCanvas->save();
                skCanvas->translate(note.position.x, note.position.y);
                skCanvas->rotate(note.rotation);

                skCanvas->drawImageRect(
                    img, SkRect::MakeXYWH(0, imgInfo.textureSize.y - imgInfo.cutPadding.x, imgInfo.textureSize.x, imgInfo.cutPadding.x),
                    SkRect::MakeXYWH(
                        -note.width / 2, 0,
                        note.width, note.head
                    ), kImSO, &pt, constraint
                );

                skCanvas->drawImageRect(
                    img, SkRect::MakeXYWH(0, imgInfo.cutPadding.y, imgInfo.textureSize.x, imgInfo.textureSize.y - imgInfo.cutPadding.sum()),
                    SkRect::MakeXYWH(
                        -note.width / 2, -note.body,
                        note.width, note.body
                    ), kImSO, &pt, constraint
                );

                skCanvas->drawImageRect(
                    img, SkRect::MakeXYWH(0, 0, imgInfo.textureSize.x, imgInfo.cutPadding.y),
                    SkRect::MakeXYWH(
                        -note.width / 2, -note.body - note.tail,
                        note.width, note.tail
                    ), kImSO, &pt, constraint
                );

                skCanvas->restore();
            } else if (std::holds_alternative<easy_phi::CalculatedFrame::CalculatedText>(obj)) {
                auto& text = std::get<easy_phi::CalculatedFrame::CalculatedText>(obj);

                drawEPText(text);
            } else if (std::holds_alternative<easy_phi::CalculatedFrame::CalculatedStoryboardTexture>(obj)) {
                auto& sbTexture = std::get<easy_phi::CalculatedFrame::CalculatedStoryboardTexture>(obj);

                auto& img = loadedStoryboardTextures[sbTexture.texture];

                static SkPaint pt;
                pt.setColorFilter(SkColorFilters::Blend(cvtColor(sbTexture.color).toSkColor(), SkBlendMode::kModulate));

                skCanvas->save();
                skCanvas->translate(sbTexture.position.x, sbTexture.position.y);
                skCanvas->rotate(sbTexture.rotation);
                skCanvas->scale(sbTexture.scale.x, sbTexture.scale.y);
                skCanvas->drawImageRect(
                    img,
                    SkRect::MakeXYWH(
                        -sbTexture.size.x * sbTexture.anchor.x,
                        -sbTexture.size.y * sbTexture.anchor.y,
                        sbTexture.size.x, sbTexture.size.y
                    ),
                    kImSO, &pt
                );
                skCanvas->restore();
            } else if (std::holds_alternative<easy_phi::CalculatedFrame::CalculatedHitEffectTexture>(obj)) {
                auto& effect = std::get<easy_phi::CalculatedFrame::CalculatedHitEffectTexture>(obj);
                
                auto& img = hitEffectImages[std::min<int>((int)(effect.progress * hitEffectImages.size()), hitEffectImages.size() - 1)];

                static SkPaint pt;
                pt.setColorFilter(SkColorFilters::Blend(cvtColor(effect.color).toSkColor(), SkBlendMode::kModulate));

                skCanvas->save();
                skCanvas->translate(effect.position.x, effect.position.y);
                skCanvas->rotate(effect.rotation);
                skCanvas->drawImageRect(
                    img,
                    SkRect::MakeXYWH(-effect.size.x / 2, -effect.size.y / 2, effect.size.x, effect.size.y),
                    kImSO, &pt
                );
                skCanvas->restore();
            } else if (std::holds_alternative<easy_phi::CalculatedFrame::CalculatedHitEffectParticle>(obj)) {
                auto& effect = std::get<easy_phi::CalculatedFrame::CalculatedHitEffectParticle>(obj);
                
                static SkPaint pt;
                pt.setStyle(SkPaint::kFill_Style);
                pt.setColor(cvtColor(effect.color));
                pt.setAntiAlias(true);

                skCanvas->save();
                skCanvas->translate(effect.position.x, effect.position.y);
                skCanvas->rotate(effect.rotation);
                skCanvas->drawRect(SkRect::MakeXYWH(-effect.size.x / 2, -effect.size.y / 2, effect.size.x, effect.size.y), pt);
                skCanvas->restore();
            } else if (std::holds_alternative<easy_phi::CalculatedFrame::CalculatedPoly>(obj)) {
                auto& poly = std::get<easy_phi::CalculatedFrame::CalculatedPoly>(obj);

                static SkPaint pt;
                pt.setColor(cvtColor(poly.color));
                pt.setStyle(SkPaint::kFill_Style);
                pt.setAntiAlias(true);

                SkPath path;
                path.moveTo(cvtVec2(poly.p1));
                path.lineTo(cvtVec2(poly.p2));
                path.lineTo(cvtVec2(poly.p3));
                path.lineTo(cvtVec2(poly.p4));
                path.close();

                skCanvas->drawPath(path, pt);
            } else if (std::holds_alternative<easy_phi::CalculatedFrame::CalculatedShader>(obj)) {
                auto& shader = std::get<easy_phi::CalculatedFrame::CalculatedShader>(obj);
            }
        }

        if (!isRenderingVideo) {
            playingHitsounds.erase(
                std::remove_if(
                    playingHitsounds.begin(), playingHitsounds.end(),
                    [](ma_sound* sound) {
                        return !ma_sound_is_playing(sound);
                    }
                ), playingHitsounds.end()
            );

            for (int i = std::max<int>(0, calculatedFrame.hitsounds.size() - hitsoundsBufferSize); i < calculatedFrame.hitsounds.size(); i++) {
                auto& type = calculatedFrame.hitsounds[i].first;
                
                while (playingHitsounds.size() >= hitsoundsBufferSize) {
                    ma_sound_stop(playingHitsounds.front());
                    playingHitsounds.erase(playingHitsounds.begin());
                }

                for (auto* sound : noteHitsounds[type]) {
                    if (std::find(playingHitsounds.begin(), playingHitsounds.end(), sound) == playingHitsounds.end()) {
                        ma_sound_start(sound);
                        ma_sound_seek_to_pcm_frame(sound, 0);
                        playingHitsounds.push_back(sound);
                    }
                }
            }
        }

        skCanvas->restore();

        std::cout << "frame took (without glfw operation) " << ((globalTimer() - frame_st) * 1000) << " ms" << std::endl;

        if (!isRenderingVideo) {
            skGrCtx->flushAndSubmit();

            glfwSwapBuffers(window);
            glfwPollEvents();
        }

        std::cout << "frame took " << ((globalTimer() - frame_st) * 1000) << " ms" << std::endl;

        {
            size_t resourceBytes;
            skGrCtx->getResourceCacheUsage(nullptr, &resourceBytes);
            std::cout << "gpu resources size: " << easy_phi::formatToStdString("%.2f MB", resourceBytes / 1024.0 / 1024.0) << std::endl;
        }

        std::cout << std::string(80, '.') << std::endl;

        return true;
    }

    std::optional<std::wstring> renderHitsounds(Pcm16& dst) {
        std::unordered_map<easy_phi::EnumPhiNoteType, Pcm16> noteHitsoundsPcm;

        for (auto& [type, sound] : noteHitsounds) {
            auto pcm = decodePcm16FromMaSound(sound[0]);
            if (!pcm.second) return L"解码打击音效失败";
            noteHitsoundsPcm[type] = std::move(pcm.first);
        }

        for (auto& line : chart.lines) {
            for (auto& note : line.notes) {
                if (note.isFake) continue;

                double t = note.time + chart.meta.offset;
                int64_t start = std::max<int64_t>(0, (int64_t)(t * PCM_FIXED_SAMPLE_RATE) * PCM_FIXED_CHANNELS);
                if (start >= dst.pcm.size()) continue;

                auto& hitsoundPcm = noteHitsoundsPcm[note.type];
                int64_t end = std::min<int64_t>(dst.pcm.size(), start + hitsoundPcm.pcm.size());

                for (int64_t i = start; i < end; i++) {
                    dst.pcm[i] = (int16_t)std::clamp<int32_t>((int32_t)dst.pcm[i] + (int32_t)hitsoundPcm.pcm[i - start], -32768, 32767);
                }
            }
        }

        return std::nullopt;
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
