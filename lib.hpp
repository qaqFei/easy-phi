namespace geasy_phi {
    using gdecoded_rgba_texture::DecodedRGBATexture;
    using gsp::gsp;
    using gdecoded_audio::DecodedAudio;
    using gdata::Data;
    using gaudio_engine::AudioEngine;
    using HashBucket = ghash_bucket::Bucket;
    namespace GL = gopengl::GL;
    using namespace gnumeric::types;
    using namespace ggeometry;
    using namespace gcolor;
    using namespace gobject_index;
    using namespace gstrutils;
    using namespace gjson;
    using namespace geasing;
    using namespace gskvcache;
    using namespace gtime_based_anim;

    namespace SharedCalculatedObjects {
        struct CalculatedText {
            std::string text;
            Vec2 position, scale = { 1.0, 1.0 }, anchor;
            float64 fontSize, rotation;
            Color color = Color::White();
        };

        struct CalculatedRect {
            Vec2 position, size;
            float64 rotation;
            Color color;
        };

        struct CalculatedPoly {
            Vec2 p1, p2, p3, p4;
            Color color;

            static CalculatedPoly Make(
                const Vec2& point,
                const Vec2& size,
                const Color& color,
                const Transform2D& transform = Transform2D()
            ) noexcept {
                return {
                    .p1 = transform.transformPoint(point),
                    .p2 = transform.transformPoint(point + Vec2 { size.x, 0.0 }),
                    .p3 = transform.transformPoint(point + size),
                    .p4 = transform.transformPoint(point + Vec2 { 0.0, size.y }),
                    .color = color
                };
            }
        };

        #define UsingSharedCalculatedObjects \
            using CalculatedText = SharedCalculatedObjects::CalculatedText; \
            using CalculatedRect = SharedCalculatedObjects::CalculatedRect; \
            using CalculatedPoly = SharedCalculatedObjects::CalculatedPoly; \
            static_assert(true, "")
        
        #define ListSharedCalculatedObjects \
            CalculatedText, CalculatedRect, CalculatedPoly

        template <typename T>
        bool sharedCulling(std::vector<T>& objects, const Rect& screenRect) noexcept {
            auto& obj = objects.back();

            if (std::holds_alternative<CalculatedText>(obj)) { }
            else if (std::holds_alternative<CalculatedRect>(obj)) {
                auto& rect = std::get<CalculatedRect>(obj);
                if (!quadStrictlyIntersectRect(makeQuadFromRectInfo({
                    .position = rect.position,
                    .size = rect.size,
                    .rotation = rect.rotation
                }).data(), screenRect)) objects.pop_back();
            } else if (std::holds_alternative<CalculatedPoly>(obj)) {
                auto& poly = std::get<CalculatedPoly>(obj);
                if (!quadStrictlyIntersectRect((Vec2[4]) {
                    poly.p1, poly.p2, poly.p3, poly.p4
                }, screenRect)) objects.pop_back();
            } else return false;

            return true;
        }
    };

    namespace TakeOvererComponents {
        struct SharedComp {
            using TextureDeocder = std::function<DecodedRGBATexture(const Data&)>;
            TextureDeocder textureDecoder;

            gsp<GL::TextureInfo> illustionTexture;

            void check() {
                gassert::assert(!!textureDecoder, "SharedComp: textureDecoder is not set");
            }
        };

        struct AudioManager {
            using Decoder = std::function<gsp<DecodedAudio>(const Data&)>;;

            Decoder decoder;
            gsp<AudioEngine> engine;
            uint64 maxSfxPlaying = 16;

            gsp<DecodedAudio> bgmAudio;

            void check() {
                gassert::assert(!!decoder, "AudioManager: decoder is not set");
                gassert::assert(!!engine, "AudioManager: engine is not set");
            }

            gsp<DecodedAudio> decodeAndCheck(const Data& data) {
                auto decoded = decoder(data);
                if (!decoded) throw std::runtime_error("failed to decode audio");
                return decoded;
            }

            void load(const Data& data) {
                bgmAudio = decodeAndCheck(data);
            }

            void load(const std::string& path) { load(Data::MakeFromFile(path)); }

            void startBgm() {
                if (!bgmAudio) throw std::runtime_error("bgm is not loaded");
                stopBgm();
                bgmAudioTask = engine->createTask(bgmAudio);
            }

            float64 getBgmTime() const {
                return bgmAudioTask ? engine->getTaskTime(bgmAudioTask) : 0.0;
            }

            bool getBpmIsEnded() {
                return bgmAudioTask && engine->getTaskEnded(bgmAudioTask);
            }

            void stopBgm() {
                if (bgmAudioTask) {
                    bgmAudioTask->stopped = true;
                    bgmAudioTask.reset();
                }
            }

            void setBgmVolume(float64 vol) {
                bgmVolume = vol;

                if (bgmAudioTask) {
                    bgmAudioTask->volume = bgmVolume;
                }
            }

            void setSfxVolume(float64 vol) {
                sfxVolume = vol;
            }

            void seekBgm(float64 t) {
                if (bgmAudioTask) {
                    engine->seekTask(bgmAudioTask, t);
                }
            }

            float64 getBgmLength() {
                return bgmAudio ? bgmAudio->getLengthInSeconds() : 0.0;
            }

            void playSfx(const gsp<DecodedAudio>& audio, uint64 count) {
                if (!maxSfxPlaying) return;

                while (playingSfxs.size() >= maxSfxPlaying) {
                    auto& task = playingSfxs.front();
                    task->stopped = true;
                    playingSfxs.erase(playingSfxs.begin());
                }

                auto task = engine->createTask(audio);
                task->volume = sfxVolume * count;
            }

            private:
            gsp<AudioEngine::Task> bgmAudioTask;
            float64 bgmVolume = 1.0, sfxVolume = 1.0;
            std::vector<gsp<AudioEngine::Task>> playingSfxs;
        };

        struct LoadChartResultInfo {
            bool success = true;
            std::string error;

            float64 createObjectTook;
            float64 initTook;

            void checkAndThrow() const {
                if (success) return;
                throw std::runtime_error(error);
            }

            float64 totalTook() const {
                return createObjectTook + initTook;
            }
        };

        struct RenderConfigBase {
            std::optional<float64> time;
            bool flushGl = true;
            bool disableHitsound = false;

            float64 getTime(const AudioManager& audioManager) const noexcept {
                return time.value_or(audioManager.getBgmTime());
            }
        };

        struct RenderResultInfoBase {
            float64 calculatedTook;
            float64 glOperationsTook;
        };

        template <typename T>
        bool renderSharedObject(
            const T& obj,
            const gsp<GL::GL33Context>& glCtx,
            GL::GL33Canvas& cvs,
            GL::TextManager& textManager
        ) noexcept {
            using namespace SharedCalculatedObjects;
            using namespace GL;

            if (std::holds_alternative<CalculatedText>(obj)) {
                auto& text = std::get<CalculatedText>(obj);

                TextManager::DrawTextConfig config {
                    .text = text.text,
                    .fontSize = text.fontSize,
                    .pos = text.position,
                    .anchor = text.anchor,
                    .rotation = text.rotation,
                    .scale = text.scale,
                    .color = text.color
                };

                textManager.drawText(cvs, config);
            } else if (std::holds_alternative<CalculatedRect>(obj)) {
                auto& rect = std::get<CalculatedRect>(obj);

                cvs.save();
                cvs.translate(rect.position);
                cvs.rotateDegrees(rect.rotation);
                cvs.drawRect({
                    .position = -rect.size / 2,
                    .size = rect.size,
                    .color = rect.color
                });
                cvs.restore();
            } else if (std::holds_alternative<CalculatedPoly>(obj)) {
                auto& poly = std::get<CalculatedPoly>(obj);

                auto mesh = glCtx->requestMesh(6);
                *mesh.vnext() = { poly.p1 }; *mesh.vnext() = { poly.p2 }; *mesh.vnext() = { poly.p4 };
                *mesh.vnext() = { poly.p4 }; *mesh.vnext() = { poly.p3 }; *mesh.vnext() = { poly.p2 };
                mesh.color = poly.color;
                cvs.drawMesh(mesh);
            } else return false;

            return true;
        }
    };

    static const float64 INF_TIME = 99999.0;
    static const Vec2 INF_TZ = { -INF_TIME, INF_TIME };
    static const float64 INF_EV = 1e9;

    enum class EnumPhiEventType : uint64 {
        PositionX, PositionY,
        SelfRotation, AxisRotation,
        MultiplyAlpha, AdditiveAlpha,
        Color,
        ScaleX, ScaleY,
        Speed, SpeedCoefficient,
        Text,
        PhiShaderUniform,
        MAX = PhiShaderUniform + 1
    };

    enum class EnumPhiNoteType {
        Tap, Drag, Flick, Hold
    };

    enum class EnumPhiLineAttachUI {
        Pause, Bar,
        ComboNumber, Combo, Score,
        Name, Level,
        None
    };

    bool phiEventTypeIsMultiply(EnumPhiEventType type) noexcept {
        return (
            type == EnumPhiEventType::MultiplyAlpha ||
            type == EnumPhiEventType::ScaleX ||
            type == EnumPhiEventType::ScaleY ||
            type == EnumPhiEventType::SpeedCoefficient
        );
    }

    struct PhiNoteTypeHelper {
        static EnumPhiNoteType FromOfficial(uint64 n) {
            if (n == 1) return EnumPhiNoteType::Tap;
            if (n == 2) return EnumPhiNoteType::Drag;
            if (n == 3) return EnumPhiNoteType::Hold;
            if (n == 4) return EnumPhiNoteType::Flick;
            return EnumPhiNoteType::Tap;
        }

        static EnumPhiNoteType FromRPE(uint64 n) {
            if (n == 1) return EnumPhiNoteType::Tap;
            if (n == 2) return EnumPhiNoteType::Hold;
            if (n == 3) return EnumPhiNoteType::Flick;
            if (n == 4) return EnumPhiNoteType::Drag;
            return EnumPhiNoteType::Tap;
        }

        static EnumPhiNoteType FromPEC(const std::string& s) {
            if (s == "n1") return EnumPhiNoteType::Tap;
            if (s == "n2") return EnumPhiNoteType::Hold;
            if (s == "n3") return EnumPhiNoteType::Flick;
            if (s == "n4") return EnumPhiNoteType::Drag;
            return EnumPhiNoteType::Tap;
        } 
    };

    struct PhiLineAttachUIHelper {
        static EnumPhiLineAttachUI FromString(const std::string& s) {
            if (s == "pause") return EnumPhiLineAttachUI::Pause;
            if (s == "bar") return EnumPhiLineAttachUI::Bar;
            if (s == "combo") return EnumPhiLineAttachUI::Combo;
            if (s == "combonumber") return EnumPhiLineAttachUI::ComboNumber;
            if (s == "score") return EnumPhiLineAttachUI::Score;
            if (s == "name") return EnumPhiLineAttachUI::Name;
            if (s == "level") return EnumPhiLineAttachUI::Level;
            return EnumPhiLineAttachUI::None;
        }
    };

    struct PhiMeta {
        float64 offset;
        std::string title;
        std::string composer;
        std::string artist;
        std::string charter;
        std::string difficulty;

        uint64 rpeVersion = 0;

        bool isHoldCoverAtHead;
        bool isZeroLengthHoldHidden;
        bool isHighNoteHidden;
        bool isRegLineAlphaNoteHidden;
        Vec2 lineWidthUnit, lineHeightUnit;

        float64 coverEllipsis = 1e-5;
        float64 maxViewRatio = (float64)16 / 9;
        Vec2 worldOrigin, worldViewport;
        float64 speedUnit = 1.0;
    };

    struct PhiBPMEvent {
        float64 time;
        float64 bpm;

        static void SortBpmEvents(std::vector<PhiBPMEvent>& events) {
            std::sort(events.begin(), events.end(), [](const auto& a, const auto& b) {
                return a.time < b.time;
            });
        }
    };

    struct PhiEventLayerIndexs {
        static constexpr uint64 RPE_MAX = 5;
        static constexpr uint64 UNIT = 1000000;

        static constexpr uint64 NOTE_ATTRS = UNIT * 1;
        static constexpr uint64 NOTE_ATTRS_2 = UNIT * 2;
        static constexpr uint64 LINE_DEFAULT = UNIT * 3;
        static constexpr uint64 SHADER_UNIFORM_DEFAULT = UNIT * 4;
    };

    struct PhiEvent {
        Vec2 timeZone;
        Vec2 valueZone;
        EnumPhiEventType type;

        float64 (* easingFunc)(void*, float64);
        float64 (* easingIntFunc)(void*, float64);
        void* easingFuncContext;
        Vec2 easingZone = { 0.0, 1.0 };

        uint64 layerIndex;

        float64 cumulativeValueAtStart;

        float64 getProgressAtTime(float64 t) noexcept {
            // if (t < timeZone.x) return 0.0;
            return std::clamp((t - timeZone.x) / (timeZone.y - timeZone.x), 0.0, 1.0);
        }

        float64 valueAtTime(float64 t) noexcept {
            auto p = getProgressAtTime(t);
            
            if (hasValueEasing()) {
                if (easingZone == Vec2 { 0.0, 1.0 }) {
                    p = easingFunc(easingFuncContext, p);
                } else if (easingZone.x < easingZone.y) {
                    float64 s = easingFunc(easingFuncContext, easingZone.x);
                    float64 e = easingFunc(easingFuncContext, easingZone.y);

                    if (e != s) {
                        p = (easingZone.y - easingZone.x) * p + easingZone.x;
                        p = (easingFunc(easingFuncContext, p) - s) / (e - s);
                    }
                }
            }

            return valueZone.x + p * (valueZone.y - valueZone.x);
        }

        static float64 getDefaultValue(EnumPhiEventType type) noexcept {
            return phiEventTypeIsMultiply(type) ? 1.0 : 0.0;
        }

        float64 getIntegralValue(float64 t) noexcept {
            auto p = getProgressAtTime(t);
            float64 iv = p * p / 2.0;

            if (hasAllEasing()) {
                if (easingZone == Vec2 { 0.0, 1.0 } ) {
                    iv = easingIntFunc(easingFuncContext, p);
                } else if (easingZone.x < easingZone.y) {
                    float64 is = easingIntFunc(easingFuncContext, easingZone.x);
                    float64 tp = (easingZone.y - easingZone.x) * p + easingZone.x;
                    float64 tiv = easingIntFunc(easingFuncContext, tp);
                    float64 s = easingFunc(easingFuncContext, easingZone.x);
                    iv = (tiv - is - s * (tp - easingZone.x)) / (1.0 - s) / (1.0 - (easingZone.y - easingZone.x));
                }
            }

            float64 res = (timeZone.y - timeZone.x) * (valueZone.x * p + (valueZone.y - valueZone.x) * iv);
            if (t > timeZone.y) res += valueZone.y * (t - timeZone.y);
            if (t < timeZone.x) res -= valueZone.x * (timeZone.x - t);
            return res;
        }

        private:
        bool hasValueEasing() const noexcept { return easingFunc != nullptr; }
        bool hasAllEasing() const noexcept { return easingFunc != nullptr && easingIntFunc != nullptr; }
    };

    struct PhiAnimLayer {
        std::vector<PhiEvent> events[(uint64)EnumPhiEventType::MAX];

        void addEvent(const PhiEvent& e) { events[(uint64)e.type].push_back(e); }

        void init() {
            std::ranges::fill(lastUpdatedTimes, -std::numeric_limits<float64>::infinity());

            for (auto& typedEvents : events) {
                std::sort(typedEvents.begin(), typedEvents.end(), [](const auto& a, const auto& b) {
                    return a.timeZone.x < b.timeZone.x;
                });
            }

            initSpeedCumul();
        }

        void updateType(uint64 type, float64 t) noexcept {
            auto& typedEvents = events[type];
            if (typedEvents.empty()) return;

            if (lastUpdatedTimes[type] == t) return;
            if (lastUpdatedTimes[type] > t) rewindTo(type, t);

            while (shouldAdvanceToNext(type, t)) currentIndexs[type]++;

            auto& e = typedEvents[currentIndexs[type]];

            if (type == (uint64)EnumPhiEventType::Speed) {
                currentValues[type] = e.cumulativeValueAtStart + e.getIntegralValue(t);
            } else {
                currentValues[type] = e.valueAtTime(t);
            }

            currentValueZones[type] = e.valueZone;
            lastUpdatedTimes[type] = t;
        }

        void updateType(EnumPhiEventType type, float64 t) noexcept {
            updateType((uint64)type, t);
        }

        void update(float64 t) noexcept {
            for (uint64 type = 0; type < (uint64)EnumPhiEventType::MAX; type++) {
                updateType(type, t);
            }
        }

        float64 get(EnumPhiEventType type) noexcept {
            if (events[(uint64)type].empty()) return PhiEvent::getDefaultValue(type);
            return currentValues[(uint64)type];
        }

        std::optional<float64> getAlwaysValue(EnumPhiEventType type) noexcept {
            auto& typedEvents = events[(uint64)type];
            if (typedEvents.empty()) return PhiEvent::getDefaultValue(type);

            if (type == EnumPhiEventType::Speed) {
                if (typedEvents.size() == 1 && typedEvents[0].valueZone.isZeroZone() && typedEvents[0].timeZone.x <= -INF_TIME / 2) {
                    return typedEvents[0].valueZone.x;
                }

                return std::nullopt;
            }

            float64 fixedValue = typedEvents[0].valueZone.x;
            for (auto& e : typedEvents) {
                if (!e.valueZone.isZeroZone() || fixedValue != e.valueZone.x) {
                    return std::nullopt;
                }
            }

            return fixedValue;
        }

        std::optional<Vec2> get_zone(EnumPhiEventType type) noexcept {
            return currentValueZones[(uint64)type];
        }

        private:
        float64 lastUpdatedTimes[(uint64)EnumPhiEventType::MAX];
        uint64 currentIndexs[(uint64)EnumPhiEventType::MAX];
        float64 currentValues[(uint64)EnumPhiEventType::MAX];
        std::optional<Vec2> currentValueZones[(uint64)EnumPhiEventType::MAX];

        void initSpeedCumul() {
            auto& speedEvents = events[(uint64)EnumPhiEventType::Speed];
            float64 cumulativeValue = 0.0;

            for (uint64 i = 0; i < speedEvents.size(); i++) {
                auto& e = speedEvents[i];
                e.cumulativeValueAtStart = cumulativeValue;

                if (i < speedEvents.size() - 1) {
                    cumulativeValue += e.getIntegralValue(speedEvents[i + 1].timeZone.x);
                }
            }
        }

        bool shouldAdvanceToNext(uint64 type, float64 t) const noexcept {
            return (
                currentIndexs[type] < events[type].size() - 1
                && events[type][currentIndexs[type] + 1].timeZone.x <= t
            );
        }

        void rewindTo(uint64 type, float64 t) noexcept {
            while (!shouldAdvanceToNext(type, t) && currentIndexs[type] > 0) {
                currentIndexs[type]--;
            }
        }
    };

    struct PhiAnimGroup {
        std::unordered_map<uint64, uint64> layerIndexMap;
        std::vector<PhiAnimLayer> layers;

        void addEvent(const PhiEvent& e) {
            if (!layerIndexMap.contains(e.layerIndex)) {
                layerIndexMap[e.layerIndex] = layers.size();
                layers.push_back({});
            }

            layers[layerIndexMap[e.layerIndex]].addEvent(e);
        }

        void init() {
            for (auto& layer : layers) {
                layer.init();
            }
        }

        void updateType(EnumPhiEventType type, float64 t) noexcept {
            for (auto& layer : layers) {
                layer.updateType(type, t);
            }
        }

        void update(float64 t) noexcept {
            for (auto& layer : layers) {
                layer.update(t);
            }
        }

        float64 get_based(EnumPhiEventType type, float64 baseValue) noexcept {
            float64 value = baseValue;

            for (auto& layer : layers) {
                if (phiEventTypeIsMultiply(type)) value *= layer.get(type);
                else value += layer.get(type);
            }

            return value;
        }

        Vec2 get_zone(EnumPhiEventType type) noexcept {
            for (auto& layer : layers) {
                auto z = layer.get_zone(type);
                if (z.has_value()) return z.value();
            }

            return {};
        }

        std::optional<float64> getAlwaysHashValue(EnumPhiEventType type) {
            float64 result = PhiEvent::getDefaultValue(type);

            for (auto& layer : layers) {
                auto v = layer.getAlwaysValue(type);
                if (!v.has_value()) return std::nullopt;
                if (phiEventTypeIsMultiply(type)) result *= v.value();
                else result += v.value();
            }

            return result;
        }
    };

    struct PhiAnimator {
        std::unordered_map<uint64, PhiAnimGroup> groups;

        PhiAnimGroup& requestGroup(uint64 index) {
            return groups.try_emplace(index, PhiAnimGroup {}).first->second;
        }

        template <typename T>
        PhiAnimGroup& requestGroup(T& obj) {
            return requestGroup(obj.indexer.get());
        }

        template <typename T>
        void addEvent(T& obj, const PhiEvent& e) {
            requestGroup(obj).addEvent(e);
        }

        void init() {
            for (auto& [_, group] : groups) {
                group.init();
            }
        }

        float64 get_based(uint64 index, float64 t, EnumPhiEventType type, float64 baseValue) noexcept {
            auto group_it = groups.find(index);
            if (group_it == groups.end()) return baseValue;

            auto& group = group_it->second;
            group.updateType(type, t);
            return group.get_based(type, baseValue);
        }

        template <typename T>
        float64 get_based(T& obj, float64 t, EnumPhiEventType type, float64 baseValue) noexcept {
            return get_based(obj.indexer.get(), t, type, baseValue);
        }

        float64 get(uint64 index, float64 t, EnumPhiEventType type) noexcept {
            return get_based(index, t, type, PhiEvent::getDefaultValue(type));
        }

        template <typename T>
        float64 get(T& obj, float64 t, EnumPhiEventType type) noexcept {
            return get(obj.indexer.get(), t, type);
        }

        Vec2 get_zone(uint64 index, float64 t, EnumPhiEventType type) noexcept {
            auto group_it = groups.find(index);
            if (group_it == groups.end()) return {};

            auto& group = group_it->second;
            group.updateType(type, t);
            return group.get_zone(type);
        }

        template <typename T>
        Vec2 get_zone(T& obj, float64 t, EnumPhiEventType type) noexcept {
            return get_zone(obj.indexer.get(), t, type);
        }

        template <typename T>
        std::optional<uint64> get_note_group_hash(T& note) {
            HashBucket hash;

            auto group_it = groups.find(note.indexer.get());

            for (const auto type : {
                EnumPhiEventType::PositionY,
                EnumPhiEventType::SelfRotation,
                EnumPhiEventType::AxisRotation,
                EnumPhiEventType::ScaleY,
                EnumPhiEventType::Speed,
                EnumPhiEventType::SpeedCoefficient
            }) {
                if (group_it == groups.end()) {
                    hash.submitNumber(PhiEvent::getDefaultValue(type));
                } else {
                    auto v = group_it->second.getAlwaysHashValue(type);
                    if (!v.has_value()) return std::nullopt;
                    hash.submitNumber(v.value());
                }
            }

            return hash.hash;
        }

        float64 get_alpha(uint64 index, float64 t, float64 additionalDefault) noexcept {
            return get(index, t, EnumPhiEventType::MultiplyAlpha) * get_based(index, t, EnumPhiEventType::AdditiveAlpha, additionalDefault);
        }

        template <typename T>
        float64 get_alpha(T& obj, float64 t, float64 additionalDefault) noexcept {
            return get_alpha(obj.indexer.get(), t, additionalDefault);
        }
    };

    struct PhiNote {
        ObjectIndexer indexer;

        struct State {
            float64 lastUpdateTime;
            bool playedHitsound;

            void timeUpdated(const PhiNote& note, float64 t) noexcept {
                if (lastUpdateTime > t) {
                    playedHitsound = note.time < t;
                }

                lastUpdateTime = t;
            }

            bool onPlayHitsound() noexcept {
                if (!playedHitsound) {
                    playedHitsound = true;
                    return true;
                }

                return false;
            }
        };

        EnumPhiNoteType type;
        float64 time, holdTime;
        bool isFake;

        uint64 lineIndex;
        Vec2 floorPosition;
        std::optional<float64> fixedHoldSpeed;
        bool isSimul;
        bool isReversedCover;

        State state;

        void init(PhiAnimator& animator) {
            floorPosition = { getFloorPositionAt(time, animator), getFloorPositionAt(time + holdTime, animator) };
        }

        float64 getFloorPositionAt(float64 t, PhiAnimator& animator) noexcept {
            if (t > time && fixedHoldSpeed.has_value()) {
                return getFloorPositionAt(time, animator) + (t - time) * fixedHoldSpeed.value();
            }

            return animator.get(lineIndex, t, EnumPhiEventType::Speed) + animator.get(*this, t, EnumPhiEventType::Speed);
        }

        bool isHold() noexcept {
            return holdTime > 0.0 || type == EnumPhiNoteType::Hold;
        }

        void reverseCover() {
            isReversedCover = !isReversedCover;
        }
    };

    struct PhiNoteGroup {
        struct State {
            float64 lastUpdateTime;
            uint64 firstNoteIndex;

            void timeUpdated(float64 t) noexcept {
                if (lastUpdateTime > t) {
                    firstNoteIndex = 0;
                }

                lastUpdateTime = t;
            }

            void passedNoteIndex(uint64 index) noexcept {
                if (firstNoteIndex == index) {
                    firstNoteIndex++;
                }
            }
        };
        
        std::vector<uint64> indexs;
        bool breakable = true;

        State state;
    };

    struct PhiLine {
        ObjectIndexer indexer;

        std::vector<PhiBPMEvent> bpms;
        std::vector<PhiNote> notes;

        std::optional<uint64> fatherLineIndex;
        float64 zOrder;
        bool enableCover;
        Vec2 anchor = { 0.5, 0.5 };

        std::optional<std::string> textureName;
        std::optional<EnumPhiLineAttachUI> attachUI;

        std::vector<PhiNoteGroup> noteGroups;

        void init(PhiAnimator& animator) {
            std::stable_sort(notes.begin(), notes.end(), [](const auto& a, const auto& b) {
                return a.time < b.time;
            });

            noteGroups.emplace_back().breakable = false;
            std::unordered_map<uint64, uint64> noteGroupMap;

            for (uint64 i = 0; i < notes.size(); i++) {
                auto& note = notes[i];
                note.lineIndex = indexer.get();
                note.init(animator);

                auto hash = animator.get_note_group_hash(note);
                if (hash.has_value()) {
                    if (!noteGroupMap.contains(hash.value())) {
                        noteGroups.emplace_back();
                        noteGroupMap[hash.value()] = noteGroups.size() - 1;
                    }

                    auto& group = noteGroups[noteGroupMap[hash.value()]];
                    group.indexs.push_back(i);
                } else noteGroups[0].indexs.push_back(i);
            }
        }

        float64 beat2sec(float64 beat) const {
            if (bpms.size() == 1) return beat * (60.0 / bpms[0].bpm);

            float64 t = 0.0;

            for (uint64 i = 0; i < bpms.size(); i++) {
                auto& e = bpms[i];
                float64 spb = 60.0 / e.bpm;

                if (i != bpms.size() - 1) {
                    float64 et_beat = bpms[i + 1].time - e.time;

                    if (beat >= et_beat) {
                        t += et_beat * spb;
                        beat -= et_beat;
                    } else {
                        t += beat * spb;
                        break;
                    }
                } else {
                    t += beat * spb;
                }
            }

            return t;
        }

        float64 sec2beat(float64 t) const {
            if (bpms.size() == 1) return t / (60.0 / bpms[0].bpm);

            float64 beat = 0.0;

            for (uint64 i = 0; i < bpms.size(); i++) {
                auto& e = bpms[i];
                float64 spb = 60.0 / e.bpm;

                if (i != bpms.size() - 1) {
                    float64 et_beat = bpms[i + 1].time - e.time;
                    float64 et_sec = et_beat * spb;

                    if (t >= et_sec) {
                        beat += et_beat;
                        t -= et_sec;
                    } else {
                        beat += t / spb;
                        break;
                    }
                } else {
                    beat += t / spb;
                }
            }

            return beat;
        }

        float64 getBpmAtSecond(float64 t) const noexcept {
            if (bpms.size() == 1) return bpms[0].bpm;

            for (uint64 i = 0; i < bpms.size(); i++) {
                auto& e = bpms[i];

                if (i != bpms.size() - 1) {
                    float64 et_beat = bpms[i + 1].time - e.time;
                    float64 et_sec = et_beat * (60.0 / e.bpm);

                    if (t >= et_sec) {
                        t -= et_sec;
                    } else {
                        return e.bpm;
                    }
                } else {
                    return e.bpm;
                }
            }

            return 120.0;
        }
    };

    struct PhiExtraEffectItem {
        Vec2 timeZone;
        std::optional<uint64> targetLine;
        uint64 order;
        bool isGlobal;
        std::string shaderName;
        uint64 shaderId;
        std::unordered_map<std::string, PhiAnimLayer> uniforms;
    };

    struct PhiExtra {
        std::vector<PhiExtraEffectItem> effects;
        std::vector<uint64> zOrderSortedEffects;

        void init() {
            initZOrderSortedEffects();
        }

        private:
        void initZOrderSortedEffects() {
            zOrderSortedEffects.clear();
            for (uint64 i = 0; i < effects.size(); i++) zOrderSortedEffects.push_back(i);

            std::stable_sort(zOrderSortedEffects.begin(), zOrderSortedEffects.end(), [&](uint64 a, uint64 b){
                return effects[a].order < effects[b].order;
            });
        }
    };

    struct PhiShaderUniform {
        uint8 used;
        float64 value[4];

        PhiShaderUniform(float64 v0, float64 v1, float64 v2, float64 v3) : used(4), value{ v0, v1, v2, v3 } {}
        PhiShaderUniform(float64 v0, float64 v1, float64 v2) : used(3), value{ v0, v1, v2, 0.0 } {}
        PhiShaderUniform(float64 v0, float64 v1) : used(2), value{ v0, v1, 0.0, 0.0 } {}
        PhiShaderUniform(float64 v0) : used(1), value{ v0, 0.0, 0.0, 0.0 } {}
        PhiShaderUniform() : used(0) {}

        PhiShaderUniform(const std::vector<float64>& v) : used(v.size()), value{} {
            for (uint8 i = 0; i < v.size(); i++) value[i] = v[i];
        }

        static PhiShaderUniform Interpolate(const PhiShaderUniform& a, const PhiShaderUniform& b, float64 t) noexcept {
            PhiShaderUniform result;
            result.used = std::max(a.used, b.used);
            for (uint8 i = 0; i < 4; i++) result.value[i] = a.value[i] + (b.value[i] - a.value[i]) * t;
            return result;
        }

        bool operator==(const PhiShaderUniform& other) const {
            if (used != other.used) return false;
            for (uint8 i = 0; i < 4; i++) if (value[i] != other.value[i]) return false;
            return true;
        }

        bool operator!=(const PhiShaderUniform& other) const { return !(*this == other); }

        void setToGlLocation(GL::ProgramInfo::Location loc) const noexcept {
            if (used == 1) loc.setf(value[0]);
            else if (used == 2) loc.setf(value[0], value[1]);
            else if (used == 3) loc.setf(value[0], value[1], value[2]);
            else if (used == 4) loc.setf(value[0], value[1], value[2], value[3]);
        }
    };

    struct PhiStoryboardAssets {
        // 用于区分是否到达了第一个
        static constexpr float64 kTextIndexOffset = 1;
        static constexpr float64 kColorIndexOffset = 1;
        static constexpr float64 kShaderUniformIndexOffset = 1;

        std::vector<std::string> texts;
        std::unordered_map<std::string, std::pair<uint64, Vec2>> textures; // name, (id, size)
        std::vector<Color> colors;
        std::vector<PhiShaderUniform> shaderUniforms;

        std::unordered_map<uint64, std::string> shaderNameMap;

        std::function<std::optional<std::pair<uint64, Vec2>>(std::string)> textureLoader;
        std::function<void(uint64)> textureDestroyer;
        std::function<void(std::string, uint64)> shaderPreloader;

        Vec2 requestTextPair(const std::string& start, const std::string& end) {
            Vec2 valueZone;
            if (texts.empty() || texts[texts.size() - 1] != start) texts.push_back(start);
            valueZone.x = texts.size() - 1;
            if (texts.empty() || texts[texts.size() - 1] != end) texts.push_back(end);
            valueZone.y = texts.size() - 1;
            return valueZone + kTextIndexOffset;
        }

        Vec2 requestColorPair(const Color& start, const Color& end) {
            Vec2 valueZone;
            if (colors.empty() || colors[colors.size() - 1] != start) colors.push_back(start);
            valueZone.x = colors.size() - 1;
            if (colors.empty() || colors[colors.size() - 1] != end) colors.push_back(end);
            valueZone.y = colors.size() - 1;
            return valueZone + kColorIndexOffset;
        }

        Vec2 requestShaderUniformPair(const PhiShaderUniform& start, const PhiShaderUniform& end) {
            Vec2 valueZone;
            if (shaderUniforms.empty() || shaderUniforms[shaderUniforms.size() - 1] != start) shaderUniforms.push_back(start);
            valueZone.x = shaderUniforms.size() - 1;
            if (shaderUniforms.empty() || shaderUniforms[shaderUniforms.size() - 1] != end) shaderUniforms.push_back(end);
            valueZone.y = shaderUniforms.size() - 1;
            return valueZone + kShaderUniformIndexOffset;
        }

        Vec2 requestShaderUniformPair(float64 start, float64 end) {
            return requestShaderUniformPair(PhiShaderUniform(start), PhiShaderUniform(end));
        }

        static std::string textInterplate(const std::string& s, const std::string& e, float64 p) {
            auto sps = s.find("%P%");
            auto eps = e.find("%P%");
            
            if (sps != std::string::npos && eps != std::string::npos) {
                float64 sv = 0.0, ev = 0.0;
                try { sv = std::stod(replaceStringWith(s, "%P%", "")); } catch (...) {}
                try { ev = std::stod(replaceStringWith(e, "%P%", "")); } catch (...) {}
                auto v = (ev - sv) * p + sv;

                if (std::fmod(sv, 1.0) == 0.0 && std::fmod(ev, 1.0) == 0.0) {
                    return std::format("{:.0f}", v);
                } else {
                    return std::format("{:.3f}", v);
                }
            } else if (s.empty() && e.empty()) return "";
            else if (e.empty()) return textInterplate(e, replaceStringWith(s, "%P%", ""), 1.0 - p);
            else if (s.empty()) return stringSliceProgress(e, p);
            else {
                int64 ml = std::min(s.size(), e.size());
                if (s.substr(0, ml) == e.substr(0, ml)) {
                    auto take = (int64)std::round((float64)((e.size() - s.size()) * p)) + s.size();
                    return s + stringSliceProgress(e.substr(ml, e.size() - ml), p);
                } else return replaceStringWith(s, "%P%", "");
            }
        }

        std::optional<std::string> getText(float64 index, const Vec2& valueZone) noexcept {
            if (valueZone.x < kTextIndexOffset) return std::nullopt;

            auto start = texts[(uint64)valueZone.x - kTextIndexOffset];
            auto end = texts[(uint64)valueZone.y - kTextIndexOffset];
            auto p = index - valueZone.x;

            return textInterplate(start, end, p);
        }

        Color getColor(float64 index, const Color& defaultValue, const Vec2& valueZone) noexcept {
            if (valueZone.x < kColorIndexOffset) return defaultValue;

            auto start = colors[(uint64)valueZone.x - kColorIndexOffset];
            auto end = colors[(uint64)valueZone.y - kColorIndexOffset];
            auto p = index - valueZone.x;
            return start * (1.0 - p) + end * p;
        }

        PhiShaderUniform getShaderUniform(float64 index, const PhiShaderUniform& defaultValue, const Vec2& valueZone) noexcept {
            if (valueZone.x < kShaderUniformIndexOffset) return defaultValue;

            auto start = shaderUniforms[(uint64)valueZone.x - kShaderUniformIndexOffset];
            auto end = shaderUniforms[(uint64)valueZone.y - kShaderUniformIndexOffset];
            auto p = index - valueZone.x;
            return PhiShaderUniform::Interpolate(start, end, p);
        }

        bool requestLoadTexture(const std::string& name) {
            if (textures.contains(name)) return true;
            if (!textureLoader) return false;

            auto id = textureLoader(name);

            if (id.has_value()) {
                textures[name] = id.value();
                return true;
            }

            return false;
        }

        bool isTextureLoaded(const std::string& name) noexcept {
            return textures.contains(name);
        }

        std::pair<uint64, Vec2>& getTexture(const std::string& name) noexcept {
            return textures[name];
        }

        void clearTextures() {
            for (auto& [_, texture] : textures) {
                if (textureDestroyer) {
                    textureDestroyer(texture.first);
                }
            }

            textures.clear();
        }

        uint64 requestShaderName(const std::string& name) {
            uint64 id = shaderNameMap.size();
            shaderNameMap[id] = name;
            return id;
        }

        std::string getShaderName(uint64 id) noexcept {
            return shaderNameMap[id];
        }

        ~PhiStoryboardAssets() {
            clearTextures();
        }
    };

    struct PhiHitEffectItem {
        struct Particle {
            float64 dt, rotation, size;
        };

        float64 time;
        uint64 lineIndex, noteIndex;
        std::vector<Particle> particles;
    };

    struct PhiChart {
        struct State {
            float64 lastUpdateTime;
            uint64 firstHitEffectIndex;

            void timeUpdated(float64 t) noexcept {
                if (lastUpdateTime > t) {
                    firstHitEffectIndex = 0;
                }

                lastUpdateTime = t;
            }

            void passedHitEffectIndex(uint64 index) noexcept {
                if (firstHitEffectIndex == index) {
                    firstHitEffectIndex++;
                }
            }
        };

        struct UserOptions {
            float64 noteScaling = 1.0;

            float64 unsafeBackgroundDim = 0.8;
            float64 backgroundDim = 0.6;
            float64 backgroundTextureBlurRadius = (float64)1 / 20;

            Color lineDefaultColor = { (float64)0xff / 0xff, (float64)0xec / 0xff, (float64)0x9f / 0xff, 1.0 };

            std::pair<Color, Color> progressBarDefaultColor = {
                { (float64)145 / 255, (float64)145 / 255, (float64)145 / 255, 0.85 },
                { 1.0, 1.0, 1.0, 0.9 }
            };

            Vec2 storyboardTextBaseSize = { 0.028125, 0.0 };

            enum class EnumStoryboardTextureSclaingBehavior {
                AboutWidth,
                AboutHeight,
                Stretch
            };
            EnumStoryboardTextureSclaingBehavior storyboardTextureSclaingBehavior = EnumStoryboardTextureSclaingBehavior::AboutWidth;
            Vec2 storyboardTextureScaling = { 1.0, 1.0 };

            float64 hitEffectDuration = 0.5;
            float64 hitEffectAlpha = (float64)0xe1 / 0xff;
            float64 hitEffectTextureScaling = 1.54;
            float64 hitEffectParticleSize = 1.0;
            float64 hitEffectParticleDistance = 1.0;
        };

        PhiMeta meta;
        std::vector<PhiLine> lines;
        PhiAnimator animator;
        PhiStoryboardAssets storyboardAssets;
        PhiExtra extra;

        std::vector<PhiHitEffectItem> hitEffects;
        std::vector<float64> comboTimes;
        std::vector<uint64> zOrderSortedLines;
        uint64 rawHash;

        UserOptions options;

        State state;

        void init() {
            animator.init();

            for (auto& line : lines) {
                if (line.textureName.has_value()) {
                    storyboardAssets.requestLoadTexture(line.textureName.value());
                }

                line.init(animator);
            }

            std::unordered_set<std::string> shaderNames;
            for (auto& effect : extra.effects) {
                shaderNames.insert(effect.shaderName);

                for (auto& [_, layer] : effect.uniforms) {
                    layer.init();
                }
            }
            
            if (storyboardAssets.shaderPreloader) {
                std::unordered_map<std::string, uint64> shaderNameMapInv;

                for (auto& name : shaderNames) {
                    auto id = storyboardAssets.requestShaderName(name);
                    storyboardAssets.shaderPreloader(name, id);
                    shaderNameMapInv[name] = id;
                }

                for (auto& effect : extra.effects) {
                    effect.shaderId = shaderNameMapInv[effect.shaderName];
                }
            }

            extra.init();
            
            initSimulNote();
            initHitEffects();
            initPlayemntInfo();
            initZOrderSortedLines();
        }

        Vec2 getLinePositionRaw(float64 t, PhiLine& line) noexcept {
            return {
                animator.get(line, t, EnumPhiEventType::PositionX),
                animator.get(line, t, EnumPhiEventType::PositionY)
            };
        }

        Vec2 getLinePositionRelOrigin(float64 t, PhiLine& line, const Vec2& screenSize) noexcept {
            Vec2 pos = getLinePositionRaw(t, line);
            pos = pos / meta.worldViewport * screenSize;

            if (line.fatherLineIndex.has_value()) {
                auto fatherLineIndex = line.fatherLineIndex.value();
                if (0 <= fatherLineIndex && fatherLineIndex < lines.size()) {
                    auto& fatherLine = lines[fatherLineIndex];
                    auto fatherLinePosition = getLinePositionRelOrigin(t, fatherLine, screenSize);
                    auto fatherLineRotation = animator.get(fatherLine, t, EnumPhiEventType::SelfRotation);

                    pos = fatherLinePosition.rotateDegrees(
                        fatherLineRotation + std::atan2(pos.y, pos.x) * 180.0 / std::numbers::pi,
                        pos.length()
                    );
                }
            }

            return pos;
        }

        Vec2 getLinePosition(float64 t, PhiLine& line, const Vec2& screenSize) noexcept {
            Vec2 ori = getLinePositionRelOrigin(t, line, screenSize);
            return ori - meta.worldOrigin / meta.worldViewport * screenSize;
        }

        struct NoteFrameInfo {
            Vec2 headPosition, tailPosition;
            bool isArrived;
            float64 lineRotation, textureRotation, speedVectorRotation;
            Color color;
            Vec2 scale;

            void setHidden() noexcept {
                color.a = 0.0;
            }
        };

        NoteFrameInfo getNoteFrameInfo(
            PhiLine& line, PhiNote& note,
            float64 time, const Vec2& screenSize
        ) noexcept {
            NoteFrameInfo info {};

            auto linePosition = getLinePosition(time, line, screenSize);
            auto lineRotation = animator.get(line, time, EnumPhiEventType::SelfRotation);
            auto lineSpeedCoefficient = animator.get(line, time, EnumPhiEventType::SpeedCoefficient);
            auto lineAlpha = animator.get_alpha(line, time, 0.0);
            auto noteRotation = animator.get(note, time, EnumPhiEventType::SelfRotation);
            auto noteAxisRotation = animator.get(note, time, EnumPhiEventType::AxisRotation);
            auto noteColorIndex = animator.get(note, time, EnumPhiEventType::Color);
            auto noteColorIndexZone = animator.get_zone(note, time, EnumPhiEventType::Color);
            auto noteColor = storyboardAssets.getColor(noteColorIndex, { 1.0, 1.0, 1.0, 1.0 }, noteColorIndexZone);
            auto noteAlpha = animator.get_alpha(note, time, 1.0);
            auto noteScaling = Vec2 {
                animator.get(note, time, EnumPhiEventType::ScaleX),
                animator.get(note, time, EnumPhiEventType::ScaleY)
            };
            
            Transform2D lineTransform {};
            lineTransform.translate(linePosition);
            lineTransform.rotateDegrees(lineRotation);
            lineTransform.rotateDegrees(noteAxisRotation);
            lineTransform.scale(screenSize / meta.worldViewport.abs());
            lineTransform.scale(1.0, -1.0);

            info.isArrived = time >= note.time;
            auto finalSpeedCoefficient = lineSpeedCoefficient * animator.get(note, time, EnumPhiEventType::SpeedCoefficient);
            auto noteFloorPosition = (note.floorPosition - note.getFloorPositionAt(time, animator)) * finalSpeedCoefficient * meta.speedUnit;
            Vec2 noteBasePosition = { animator.get(note, time, EnumPhiEventType::PositionX), animator.get(note, time, EnumPhiEventType::PositionY) };

            auto noteRelPositionHead = noteBasePosition + Vec2 { 0.0, info.isArrived ? 0.0 : noteFloorPosition.x },
                noteRelPositionTail = noteBasePosition + Vec2 { 0.0, noteFloorPosition.y };
            
            info.color = noteColor.applyAlpha(noteAlpha);

            if (line.enableCover && !info.isArrived) {
                if (meta.isHoldCoverAtHead && noteRelPositionHead.y * (note.isReversedCover ? -1.0 : 1.0) < -meta.coverEllipsis) info.setHidden();
                if (!meta.isHoldCoverAtHead && noteRelPositionTail.y * (note.isReversedCover ? -1.0 : 1.0) < -meta.coverEllipsis) info.setHidden();
            }

            if (note.isHold() && meta.isZeroLengthHoldHidden && note.floorPosition.xyDiff() == 0) info.setHidden();
            if (noteRelPositionHead.y > 2.0 && meta.isHighNoteHidden) info.setHidden();
            if (meta.isRegLineAlphaNoteHidden && lineAlpha < 0.0) info.setHidden();

            info.headPosition = lineTransform.transformPoint(noteRelPositionHead);
            info.tailPosition = lineTransform.transformPoint(noteRelPositionTail);
            info.lineRotation = lineRotation;
            info.textureRotation = lineRotation + noteRotation + noteAxisRotation;
            info.speedVectorRotation = lineRotation + noteAxisRotation - 90.0;
            if (finalSpeedCoefficient < 0) info.speedVectorRotation += 180.0;
            info.scale = noteScaling;

            return info;
        }

        uint64 getCombo(float64 t) const noexcept {
            if (comboTimes.empty() || comboTimes[0] > t) return 0;

            uint64 left = 0, right = comboTimes.size() - 1;
            uint64 ans = 1;

            while (left <= right) {
                uint64 mid = left + (right - left) / 2;
                if (comboTimes[mid] <= t) {
                    ans = mid + 1;
                    left = mid + 1;
                } else {
                    right = mid - 1;
                }
            }

            return ans;
        }

        private:
        void initSimulNote() {
            std::unordered_map<float64, uint64> noteTimes;

            for (auto& line : lines) {
                for (auto& note : line.notes) {
                    noteTimes[note.time]++;
                }
            }

            for (auto& line : lines) {
                for (auto& note : line.notes) {
                    note.isSimul = noteTimes[note.time] > 1;
                }
            }
        }

        void initHitEffects() {
            std::mt19937 rng { std::random_device {} () };
            std::uniform_real_distribution<float64> rng_dist { 0.0, 1.0 };
            auto uniform = [&](float64 a, float64 b) { return a + (b - a) * rng_dist(rng); };

            hitEffects.clear();

            for (auto& line : lines) {
                for (auto& note : line.notes) {
                    if (note.isFake) continue;

                    float64 t = note.time;
                    while (t <= note.time + note.holdTime) {
                        auto& item = hitEffects.emplace_back();
                        item.time = t;
                        item.lineIndex = &line - lines.data();
                        item.noteIndex = &note - line.notes.data();

                        for (uint64 i = 0; i < 4; i++) {
                            auto& particle = item.particles.emplace_back();
                            particle.rotation = uniform(0.0, 360.0);
                            particle.size = uniform(185.0, 265.0);
                        }

                        t += 30.0 / line.getBpmAtSecond(t);
                    }
                }
            }

            std::stable_sort(hitEffects.begin(), hitEffects.end(), [](const auto& a, const auto& b) {
                return a.time < b.time;
            });
        }

        void initPlayemntInfo() {
            for (auto& line : lines) {
                for (auto& note : line.notes) {
                    if (note.isFake) continue;
                    comboTimes.push_back(note.time + std::max(0.0, note.holdTime - 0.2));
                }
            }

            std::sort(comboTimes.begin(), comboTimes.end());
        }

        void initZOrderSortedLines() {
            zOrderSortedLines.clear();
            for (uint64 i = 0; i < lines.size(); i++) zOrderSortedLines.push_back(i);

            std::stable_sort(zOrderSortedLines.begin(), zOrderSortedLines.end(), [&](uint64 a, uint64 b){
                return lines[a].zOrder < lines[b].zOrder;
            });
        }
    };

    PhiChart loadPhiChartFromOfficialJson(const Data& data) {
        auto failed = [](const std::string& msg) {
            throw std::runtime_error(std::format("official: {}", msg));
        };

        auto jsonRoot = JsonNode::Parse(data);

        PhiChart chart {};

        if (!jsonRoot.isObject()) failed("root is not an object");

        if (!jsonRoot.hasKey("formatVersion")) failed("missing formatVersion field");
        if (!jsonRoot["formatVersion"].isNumber()) failed("formatVersion is not a number");
        uint64 formatVersion = jsonRoot["formatVersion"].getNumber();

        chart.meta.isHoldCoverAtHead = true;
        chart.meta.isZeroLengthHoldHidden = true;
        chart.meta.isHighNoteHidden = true;
        chart.meta.isRegLineAlphaNoteHidden = false;
        chart.meta.lineWidthUnit = { 0.0, 5.76 };
        chart.meta.lineHeightUnit = { 0.0, 0.0075 };
        chart.meta.worldOrigin = { 0.0, 1.0 };
        chart.meta.worldViewport = { 1.0, -1.0 };

        if (1 <= formatVersion && formatVersion <= 3) {
            if (!jsonRoot.hasKey("offset")) failed("missing offset field");
            if (!jsonRoot["offset"].isNumber()) failed("offset is not a number");
            chart.meta.offset = jsonRoot["offset"].getNumber();

            if (!jsonRoot.hasKey("judgeLineList")) failed("missing judgeLineList field");
            if (!jsonRoot["judgeLineList"].isArray()) failed("judgeLineList is not an array");
            auto& judgeLineListNode = jsonRoot["judgeLineList"];

            for (auto& judgeLineNode : judgeLineListNode.getArray()) {
                if (!judgeLineNode.isObject()) failed("judgeLineList item is not an object");
                
                auto& line = chart.lines.emplace_back();
                line.enableCover = true;

                if (!judgeLineNode.hasKey("bpm")) failed("missing bpm field");
                if (!judgeLineNode["bpm"].isNumber()) failed("bpm is not a number");
                float64 bpm = judgeLineNode["bpm"].getNumber();
                float64 timeFactor = 1.875 / bpm;
                line.bpms = { { 0, bpm } };

                if (!judgeLineNode.hasKey("notesAbove")) failed("missing notesAbove field");
                if (!judgeLineNode["notesAbove"].isArray()) failed("notesAbove is not an array");
                if (!judgeLineNode.hasKey("notesBelow")) failed("missing notesBelow field");
                if (!judgeLineNode["notesBelow"].isArray()) failed("notesBelow is not an array");

                auto& notesAboveNode = judgeLineNode["notesAbove"];
                auto& notesBelowNode = judgeLineNode["notesBelow"];
                std::vector<std::pair<JsonNode*, bool>> noteGroups = {
                    { &notesAboveNode, true },
                    { &notesBelowNode, false }
                };

                for (auto& [ notesNodePtr, isAbove ] : noteGroups) {
                    auto& notesNode = *notesNodePtr;

                    for (auto& noteNode : notesNode.getArray()) {
                        if (!noteNode.isObject()) failed("notesAbove/notesBelow item is not an object");

                        if (!noteNode.hasKey("type")) failed("missing type field");
                        if (!noteNode["type"].isNumber()) failed("type is not a number");
                        auto type = PhiNoteTypeHelper::FromOfficial(noteNode["type"].getNumber());

                        if (!noteNode.hasKey("time")) failed("missing time field");
                        if (!noteNode["time"].isNumber()) failed("time is not a number");
                        auto time = noteNode["time"].getNumber() * timeFactor;

                        if (!noteNode.hasKey("holdTime")) failed("missing holdTime field");
                        if (!noteNode["holdTime"].isNumber()) failed("holdTime is not a number");
                        auto holdTime = noteNode["holdTime"].getNumber() * timeFactor;

                        if (!noteNode.hasKey("positionX")) failed("missing positionX field");
                        if (!noteNode["positionX"].isNumber()) failed("positionX is not a number");
                        auto positionX = noteNode["positionX"].getNumber() * 0.05625;

                        std::string speedKey = "speed";
                        if (!noteNode.hasKey(speedKey)) failed(std::string("missing ") + speedKey + " field");
                        if (!noteNode[speedKey].isNumber()) failed(speedKey + " is not a number");
                        auto speed = noteNode[speedKey].getNumber();

                        auto& note = line.notes.emplace_back();
                        note.type = type;
                        note.time = time;
                        note.holdTime = holdTime;
                        note.isFake = false;

                        chart.animator.addEvent(note, PhiEvent {
                            .timeZone = INF_TZ,
                            .valueZone = { positionX, positionX },
                            .type = EnumPhiEventType::PositionX,
                            .layerIndex = PhiEventLayerIndexs::NOTE_ATTRS
                        });

                        if (type == EnumPhiNoteType::Hold) {
                            note.fixedHoldSpeed = speed * 0.6;
                        } else {
                            if (speed != 1.0) {
                                chart.animator.addEvent(note, PhiEvent {
                                    .timeZone = INF_TZ,
                                    .valueZone = { speed, speed },
                                    .type = EnumPhiEventType::SpeedCoefficient,
                                    .layerIndex = PhiEventLayerIndexs::NOTE_ATTRS
                                });
                            }
                        }

                        if (!isAbove) {
                            chart.animator.addEvent(note, PhiEvent {
                                .timeZone = INF_TZ,
                                .valueZone = { -1.0, -1.0 },
                                .type = EnumPhiEventType::SpeedCoefficient,
                                .layerIndex = PhiEventLayerIndexs::NOTE_ATTRS_2
                            });

                            chart.animator.addEvent(note, PhiEvent {
                                .timeZone = INF_TZ,
                                .valueZone = { 180.0, 180.0 },
                                .type = EnumPhiEventType::SelfRotation,
                                .layerIndex = PhiEventLayerIndexs::NOTE_ATTRS_2
                            });

                            note.reverseCover();
                        }
                    }
                }

                if (!judgeLineNode.hasKey("speedEvents")) failed("missing speedEvents field");
                if (!judgeLineNode["speedEvents"].isArray()) failed("speedEvents is not an array");
                if (!judgeLineNode.hasKey("judgeLineMoveEvents")) failed("missing judgeLineMoveEvents field");
                if (!judgeLineNode["judgeLineMoveEvents"].isArray()) failed("judgeLineMoveEvents is not an array");
                if (!judgeLineNode.hasKey("judgeLineRotateEvents")) failed("missing judgeLineRotateEvents field");
                if (!judgeLineNode["judgeLineRotateEvents"].isArray()) failed("judgeLineRotateEvents is not an array");
                if (!judgeLineNode.hasKey("judgeLineDisappearEvents")) failed("missing judgeLineDisappearEvents field");
                if (!judgeLineNode["judgeLineDisappearEvents"].isArray()) failed("judgeLineDisappearEvents is not an array");

                auto& speedEventsNode = judgeLineNode["speedEvents"];
                auto& judgeLineMoveEventsNode = judgeLineNode["judgeLineMoveEvents"];
                auto& judgeLineRotateEventsNode = judgeLineNode["judgeLineRotateEvents"];
                auto& judgeLineDisappearEventsNode = judgeLineNode["judgeLineDisappearEvents"];

                // events, startKey, endKey, easeTypeKey, type, converter
                std::vector<std::tuple<JsonNode*, std::string, std::string, std::string, EnumPhiEventType, std::function<float64(float64)>>> eventGroups;
                
                if (formatVersion == 1) {
                    eventGroups = {
                        { &speedEventsNode, "value", "value", "", EnumPhiEventType::Speed, [](float64 v) { return v * 0.6; } },
                        { &judgeLineMoveEventsNode, "start", "end", "", EnumPhiEventType::PositionX, [](float64 v) { return std::floor(v / 1000.0) / 880.0; } },
                        { &judgeLineMoveEventsNode, "start", "end", "", EnumPhiEventType::PositionY, [](float64 v) { return std::fmod(v, 1000.0) / 520.0; } },
                        { &judgeLineRotateEventsNode, "start", "end", "", EnumPhiEventType::SelfRotation, [](float64 v) { return -v; } },
                        { &judgeLineDisappearEventsNode, "start", "end", "", EnumPhiEventType::AdditiveAlpha, [](float64 v) { return v; } }
                    };
                } else if (formatVersion == 3) {
                    eventGroups = {
                        { &speedEventsNode, "value", "value", "", EnumPhiEventType::Speed, [](float64 v) { return v * 0.6; } },
                        { &judgeLineMoveEventsNode, "start", "end", "", EnumPhiEventType::PositionX, [](float64 v) { return v; } },
                        { &judgeLineMoveEventsNode, "start2", "end2", "", EnumPhiEventType::PositionY, [](float64 v) { return v; } },
                        { &judgeLineRotateEventsNode, "start", "end", "", EnumPhiEventType::SelfRotation, [](float64 v) { return -v; } },
                        { &judgeLineDisappearEventsNode, "start", "end", "", EnumPhiEventType::AdditiveAlpha, [](float64 v) { return v; } }
                    };
                }

                for (auto& [eventsNode, startKey, endKey, easeTypeKey, type, converter] : eventGroups) {
                    if (!eventsNode->isArray()) failed("XXXEvents is not an array");

                    for (auto& eventNode : eventsNode->getArray()) {
                        if (!eventNode.isObject()) failed("XXXEvents item is not an object");

                        if (!eventNode.hasKey("startTime")) failed("missing startTime field");
                        if (!eventNode["startTime"].isNumber()) failed("startTime is not a number");
                        auto startTime = eventNode["startTime"].getNumber() * timeFactor;

                        if (!eventNode.hasKey("endTime")) failed("missing endTime field");
                        if (!eventNode["endTime"].isNumber()) failed("endTime is not a number");
                        auto endTime = eventNode["endTime"].getNumber() * timeFactor;

                        if (!eventNode.hasKey(startKey)) failed(std::string("missing ") + startKey + " field");
                        if (!eventNode[startKey].isNumber()) failed(std::string(startKey) + " is not a number");
                        auto startValue = converter(eventNode[startKey].getNumber());

                        if (!eventNode.hasKey(endKey)) failed(std::string("missing ") + endKey + " field");
                        if (!eventNode[endKey].isNumber()) failed(std::string(endKey) + " is not a number");
                        auto endValue = converter(eventNode[endKey].getNumber());

                        chart.animator.addEvent(line, PhiEvent {
                            .timeZone = { startTime, endTime },
                            .valueZone = { startValue, endValue },
                            .type = type,
                            .layerIndex = PhiEventLayerIndexs::LINE_DEFAULT
                        });
                    }
                }
            }
        } else {
            failed(std::string("unsupported formatVersion: ") + std::to_string(formatVersion));
        }

        chart.rawHash = data.getHash();
        return chart;
    }

    PhiChart loadPhiChartFromRpeJson(const Data& data) {
        auto failed = [](const std::string& msg) {
            throw std::runtime_error(std::format("rpe: {}", msg));
        };

        auto jsonRoot = JsonNode::Parse(data);
        
        PhiChart chart {};

        chart.meta.isHoldCoverAtHead = false;
        chart.meta.isZeroLengthHoldHidden = false;
        chart.meta.isHighNoteHidden = false;
        chart.meta.isRegLineAlphaNoteHidden = true;
        chart.meta.lineWidthUnit = { (float64)4000 / 1350, 0.0 };
        chart.meta.lineHeightUnit = { 0.0, (float64)1 / 180 };
        chart.meta.worldOrigin = { (float64)-1350 / 2, (float64)900 / 2 };
        chart.meta.worldViewport = { 1350, -900 };
        chart.meta.speedUnit = 120.0;

        if (!jsonRoot.isObject()) failed("root is not an object");

        if (!jsonRoot.hasKey("META")) failed("missing META field");
        if (!jsonRoot["META"].isObject()) failed("META is not an object");
        auto& metaNode = jsonRoot["META"];

        if (!metaNode.hasKey("RPEVersion")) failed("missing RPEVersion field");
        if (!metaNode["RPEVersion"].isNumber()) failed("RPEVersion is not a number");
        chart.meta.rpeVersion = metaNode["RPEVersion"].getNumber();

        if (!metaNode.hasKey("charter")) failed("missing charter field");
        if (!metaNode["charter"].isString()) failed("charter is not a string");
        chart.meta.charter = metaNode["charter"].getString();

        if (!metaNode.hasKey("composer")) failed("missing composer field");
        if (!metaNode["composer"].isString()) failed("composer is not a string");
        chart.meta.composer = metaNode["composer"].getString();

        if (!metaNode.hasKey("name")) failed("missing name field");
        if (!metaNode["name"].isString()) failed("name is not a string");
        chart.meta.title = metaNode["name"].getString();

        if (!metaNode.hasKey("level")) failed("missing level field");
        if (!metaNode["level"].isString()) failed("level is not a string");
        chart.meta.difficulty = metaNode["level"].getString();

        if (!metaNode.hasKey("offset")) failed("missing offset field");
        if (!metaNode["offset"].isNumber()) failed("offset is not a number");
        chart.meta.offset = metaNode["offset"].getNumber() / 1000;

        auto parseTimeTuple = [](const JsonNode& node, float64* dst) {
            if (!node.isArray()) return false;
            if (node.getArray().size() != 3) return false;

            const auto& arr = node.getArray();
            if (!arr[0].isNumber()) return false;
            if (!arr[1].isNumber()) return false;
            if (!arr[2].isNumber()) return false;

            float64 n1 = arr[0].getNumber(),
                n2 = arr[1].getNumber(),
                n3 = arr[2].getNumber();

            *dst = n1 + n2 / n3;
            return true;
        };

        std::vector<PhiBPMEvent> sharedBpmEvents;
        
        if (!jsonRoot.hasKey("BPMList")) failed("missing BPMList field");
        if (!jsonRoot["BPMList"].isArray()) failed("BPMList is not an array");
        auto& bpmListNode = jsonRoot["BPMList"];

        for (auto& bpmEventNode : bpmListNode.getArray()) {
            if (!bpmEventNode.isObject()) failed("BPMList item is not an object");

            if (!bpmEventNode.hasKey("startTime")) failed("missing startTime field");
            float64 startTime;
            if (!parseTimeTuple(bpmEventNode["startTime"], &startTime)) failed("startTime is not a valid time tuple");

            if (!bpmEventNode.hasKey("bpm")) failed("missing bpm field");
            if (!bpmEventNode["bpm"].isNumber()) failed("bpm is not a number");
            float64 bpm = bpmEventNode["bpm"].getNumber();

            sharedBpmEvents.push_back({
                .time = startTime,
                .bpm = bpm
            });
        }

        PhiBPMEvent::SortBpmEvents(sharedBpmEvents);

        if (!jsonRoot.hasKey("judgeLineList")) failed("missing judgeLineList field");
        if (!jsonRoot["judgeLineList"].isArray()) failed("judgeLineList is not an array");
        auto& judgeLineListNode = jsonRoot["judgeLineList"];

        for (auto& judgeLineNode : judgeLineListNode.getArray()) {
            if (!judgeLineNode.isObject()) failed("judgeLineList item is not an object");

            auto& line = chart.lines.emplace_back();
            line.bpms = sharedBpmEvents;

            if (judgeLineNode.hasKey("bpmfactor")) {
                if (!judgeLineNode["bpmfactor"].isNumber()) failed("bpmfactor is not a number");
                auto factor = judgeLineNode["bpmfactor"].getNumber();

                for (auto& e : line.bpms) {
                    e.bpm /= factor;
                }
            }

            if (!judgeLineNode.hasKey("eventLayers")) failed("missing eventLayers field");
            if (!judgeLineNode["eventLayers"].isArray()) failed("eventLayers is not an array");
            auto& eventLayersNode = judgeLineNode["eventLayers"];

            uint64 eventLayerIndex = 0;
            // events, type, converter
            using EventGroupType = std::tuple<JsonNode*, EnumPhiEventType, std::function<float64(float64)>>;

            auto progressEventGroup = [&](EventGroupType group) -> std::pair<bool, std::string> {
                auto& [eventsNode, type, converter] = group;
                if (!eventsNode->isArray()) return { false, "XXXEvents is not an array" };
                
                auto& arr =  eventsNode->getArray();
                if (arr.empty()) return { true, "" };

                float64 earliestTime = INF_TIME;

                for (auto& eventNode : arr) {
                    if (!eventNode.isObject()) return { false, "XXXEvents item is not an object" };

                    if (!eventNode.hasKey("startTime")) return { false, "missing startTime field" };
                    float64 startTime;
                    if (!parseTimeTuple(eventNode["startTime"], &startTime)) return { false, "startTime is not a valid time tuple" };

                    if (!eventNode.hasKey("endTime")) return { false, "missing endTime field" };
                    float64 endTime;
                    if (!parseTimeTuple(eventNode["endTime"], &endTime)) return { false, "endTime is not a valid time tuple" };

                    float64 start, end;

                    if (!eventNode.hasKey("start")) return { false, "missing start field" };
                    if (!eventNode.hasKey("end")) return { false, "missing end field" };

                    if (type == EnumPhiEventType::Text) {
                        if (!eventNode["start"].isString()) return { false, "start is not a string" };
                        if (!eventNode["end"].isString()) return { false, "end is not a string" };

                        auto valueZone = chart.storyboardAssets.requestTextPair(eventNode["start"].getString(), eventNode["end"].getString());
                        start = valueZone.x;
                        end = valueZone.y;
                    } else if (type == EnumPhiEventType::Color) {
                        if (!eventNode["start"].isArray()) return { false, "start is not an array" };
                        if (!eventNode["end"].isArray()) return { false, "end is not an array" };

                        auto& startArr = eventNode["start"].getArray();
                        auto& endArr = eventNode["end"].getArray();

                        if (startArr.size() < 3) return { false, "start array size is less than 3" };
                        if (endArr.size() < 3) return { false, "end array size is less than 3" };

                        if (!startArr[0].isNumber()) return { false, "start array item is not a number" };
                        if (!startArr[1].isNumber()) return { false, "start array item is not a number" };
                        if (!startArr[2].isNumber()) return { false, "start array item is not a number" };

                        if (!endArr[0].isNumber()) return { false, "end array item is not a number" };
                        if (!endArr[1].isNumber()) return { false, "end array item is not a number" };
                        if (!endArr[2].isNumber()) return { false, "end array item is not a number" };

                        auto startColor = Color {
                            startArr[0].getNumber() / 255,
                            startArr[1].getNumber() / 255,
                            startArr[2].getNumber() / 255,
                            1.0
                        };

                        auto endColor = Color {
                            endArr[0].getNumber() / 255,
                            endArr[1].getNumber() / 255,
                            endArr[2].getNumber() / 255,
                            1.0
                        };

                        auto valueZone = chart.storyboardAssets.requestColorPair(startColor, endColor);
                        start = valueZone.x;
                        end = valueZone.y;
                    } else {
                        if (!eventNode["start"].isNumber()) return { false, "start is not a number" };
                        start = eventNode["start"].getNumber();

                        if (!eventNode["end"].isNumber()) return { false, "end is not a number" };
                        end = eventNode["end"].getNumber();
                    }

                    start = converter(start);
                    end = converter(end);

                    startTime = line.beat2sec(startTime);
                    endTime = line.beat2sec(endTime);
                    earliestTime = std::min(earliestTime, startTime);

                    PhiEvent e {};
                    e.timeZone = { startTime, endTime };
                    e.valueZone = { start, end };
                    e.type = type;
                    e.layerIndex = PhiEventLayerIndexs::LINE_DEFAULT + eventLayerIndex;

                    if (eventNode.hasKey("easingType") && eventNode["easingType"].isNumber()) {
                        e.easingFuncContext = (void*)(uint64)eventNode["easingType"].getNumber();
                    }

                    if ((uint64)e.easingFuncContext > 1) {
                        e.easingFunc = [](void* ctx, float64 p) { return EaseSet::Phigros::RePhiEdit::easing((uint64)ctx, p); };
                        e.easingIntFunc = [](void* ctx, float64 p) { return EaseSet::Phigros::RePhiEdit::easing_int((uint64)ctx, p); };
                    }

                    if (eventNode.hasKey("easingLeft")) {
                        if (!eventNode["easingLeft"].isNumber()) return { false, "easingLeft is not a number" };
                        e.easingZone.x = eventNode["easingLeft"].getNumber();
                    }

                    if (eventNode.hasKey("easingRight")) {
                        if (!eventNode["easingRight"].isNumber()) return { false, "easingRight is not a number" };
                        e.easingZone.y = eventNode["easingRight"].getNumber();
                    }

                    chart.animator.addEvent(line, e);

                    if (&eventNode == &arr.front()) {
                        if (type != EnumPhiEventType::Text && type != EnumPhiEventType::Color) {
                            if (start != end) {
                                PhiEvent e {};
                                e.timeZone = { -INF_TIME, startTime };
                                e.valueZone = { start - (end - start) * (startTime + INF_TIME), start };
                                e.type = type;
                                e.layerIndex = PhiEventLayerIndexs::LINE_DEFAULT + eventLayerIndex;
                                chart.animator.addEvent(line, e);
                            }
                        }
                    }
                }

                if (type == EnumPhiEventType::Text) {
                    PhiEvent e {};
                    e.timeZone = { -INF_TIME, earliestTime };
                    e.valueZone = chart.storyboardAssets.requestTextPair("", "");
                    e.type = type;
                    e.layerIndex = PhiEventLayerIndexs::LINE_DEFAULT + eventLayerIndex;
                    chart.animator.addEvent(line, e);
                }

                return { true, "" };
            };

            for (auto& eventLayerNode : eventLayersNode.getArray()) {
                if (eventLayerNode.isNull()) continue;
                if (!eventLayerNode.isObject()) failed("eventLayers item is not an object");

                std::vector<EventGroupType> groups;

                if (eventLayerNode.hasKey("alphaEvents")) groups.push_back({ &eventLayerNode["alphaEvents"], EnumPhiEventType::AdditiveAlpha, [](float64 v) { return v / 255; } });
                if (eventLayerNode.hasKey("moveXEvents")) groups.push_back({ &eventLayerNode["moveXEvents"], EnumPhiEventType::PositionX, [](float64 v) { return v; } });
                if (eventLayerNode.hasKey("moveYEvents")) groups.push_back({ &eventLayerNode["moveYEvents"], EnumPhiEventType::PositionY, [](float64 v) { return v; } });
                if (eventLayerNode.hasKey("rotateEvents")) groups.push_back({ &eventLayerNode["rotateEvents"], EnumPhiEventType::SelfRotation, [](float64 v) { return v; } });
                if (eventLayerNode.hasKey("speedEvents")) groups.push_back({ &eventLayerNode["speedEvents"], EnumPhiEventType::Speed, [](float64 v) { return v; } });

                for (auto& group : groups) {
                    auto [success, msg] = progressEventGroup(group);
                    if (!success) failed(msg);
                }

                eventLayerIndex++;
            }

            if (judgeLineNode.hasKey("extended")) {
                auto& extendedNode = judgeLineNode["extended"];
                if (!extendedNode.isObject()) failed("extended is not an object");

                std::vector<EventGroupType> groups;

                if (extendedNode.hasKey("textEvents")) groups.push_back({ &extendedNode["textEvents"], EnumPhiEventType::Text, [](float64 v) { return v; } });
                if (extendedNode.hasKey("scaleXEvents")) groups.push_back({ &extendedNode["scaleXEvents"], EnumPhiEventType::ScaleX, [](float64 v) { return v; } });
                if (extendedNode.hasKey("scaleYEvents")) groups.push_back({ &extendedNode["scaleYEvents"], EnumPhiEventType::ScaleY, [](float64 v) { return v; } });
                if (extendedNode.hasKey("colorEvents")) groups.push_back({ &extendedNode["colorEvents"], EnumPhiEventType::Color, [](float64 v) { return v; } });

                for (auto& group : groups) {
                    auto [success, msg] = progressEventGroup(group);
                    if (!success) failed(msg);
                }

                eventLayerIndex++;
            }

            if (judgeLineNode.hasKey("notes")) {
                auto& notesNode = judgeLineNode["notes"];
                if (!notesNode.isArray()) failed("notes is not an array");

                for (auto& noteNode : notesNode.getArray()) {
                    if (!noteNode.isObject()) failed("notes item is not an object");

                    auto& note = line.notes.emplace_back();

                    if (!noteNode.hasKey("startTime")) failed("missing startTime field");
                    float64 startTime;
                    if (!parseTimeTuple(noteNode["startTime"], &startTime)) failed("startTime is not a valid time tuple");

                    if (!noteNode.hasKey("endTime")) failed("missing endTime field");
                    float64 endTime;
                    if (!parseTimeTuple(noteNode["endTime"], &endTime)) failed("endTime is not a valid time tuple");

                    startTime = line.beat2sec(startTime);
                    endTime = line.beat2sec(endTime);

                    if (!noteNode.hasKey("above")) failed("missing above field");
                    bool isAbove;
                    if (noteNode["above"].isBool()) isAbove = noteNode["above"].getBool();
                    else if (noteNode["above"].isNumber()) isAbove = noteNode["above"].getNumber() == 1;
                    else failed("above is not a boolean or number");

                    if (!noteNode.hasKey("type")) failed("missing type field");
                    if (!noteNode["type"].isNumber()) failed("type is not a number");
                    auto type = PhiNoteTypeHelper::FromRPE(noteNode["type"].getNumber());

                    if (!noteNode.hasKey("speed")) failed("missing speed field");
                    if (!noteNode["speed"].isNumber()) failed("speed is not a number");
                    float64 speed = noteNode["speed"].getNumber();

                    if (!noteNode.hasKey("isFake")) failed("missing isFake field");
                    bool isFake;
                    if (noteNode["isFake"].isBool()) isFake = noteNode["isFake"].getBool();
                    else if (noteNode["isFake"].isNumber()) isFake = noteNode["isFake"].getNumber() == 1;
                    else failed("isFake is not a boolean or number");

                    if (!noteNode.hasKey("positionX")) failed("missing positionX field");
                    if (!noteNode["positionX"].isNumber()) failed("positionX is not a number");
                    float64 positionX = noteNode["positionX"].getNumber();
                    
                    if (noteNode.hasKey("yOffset")) {
                        if (!noteNode["yOffset"].isNumber()) failed("yOffset is not a number");
                        auto yOffset = noteNode["yOffset"].getNumber() * speed;
                        if (!isAbove) yOffset *= -1;
                        
                        if (yOffset != 0.0) {
                            chart.animator.addEvent(note, PhiEvent {
                                .timeZone = INF_TZ,
                                .valueZone = { yOffset, yOffset },
                                .type = EnumPhiEventType::PositionY,
                                .layerIndex = PhiEventLayerIndexs::NOTE_ATTRS
                            });
                        }
                    }

                    if (noteNode.hasKey("visibleTime")) {
                        if (!noteNode["visibleTime"].isNumber()) failed("visibleTime is not a number");
                        auto visibleTime = noteNode["visibleTime"].getNumber();

                        if (visibleTime < 999999.0) {
                            chart.animator.addEvent(note, PhiEvent {
                                .timeZone = { -INF_TIME, startTime - visibleTime },
                                .valueZone = { 0.0, 0.0 },
                                .type = EnumPhiEventType::MultiplyAlpha,
                                .layerIndex = PhiEventLayerIndexs::NOTE_ATTRS
                            });

                            chart.animator.addEvent(note, PhiEvent {
                                .timeZone = { startTime - visibleTime, INF_TIME },
                                .valueZone = { 1.0, 1.0 },
                                .type = EnumPhiEventType::MultiplyAlpha,
                                .layerIndex = PhiEventLayerIndexs::NOTE_ATTRS
                            });
                        }
                    }

                    if (noteNode.hasKey("size")) {
                        if (!noteNode["size"].isNumber()) failed("size is not a number");
                        auto size = noteNode["size"].getNumber();

                        if (size != 1.0) {
                            chart.animator.addEvent(note, PhiEvent {
                                .timeZone = INF_TZ,
                                .valueZone = { size, size },
                                .type = EnumPhiEventType::ScaleX,
                                .layerIndex = PhiEventLayerIndexs::NOTE_ATTRS
                            });
                        }
                    }

                    if (noteNode.hasKey("alpha")) {
                        if (!noteNode["alpha"].isNumber()) failed("alpha is not a number");
                        auto alpha = noteNode["alpha"].getNumber() / 255;

                        if (alpha != 1.0) {
                            chart.animator.addEvent(note, PhiEvent {
                                .timeZone = INF_TZ,
                                .valueZone = { alpha, alpha },
                                .type = EnumPhiEventType::MultiplyAlpha,
                                .layerIndex = PhiEventLayerIndexs::NOTE_ATTRS_2
                            });
                        }
                    }

                    if (noteNode.hasKey("tint")) {
                        if (!noteNode["tint"].isArray()) failed("tint is not an array");

                        auto& arr = noteNode["tint"].getArray();
                        if (arr.size() < 3) failed("tint array is too small");

                        auto& n1 = arr[0];
                        auto& n2 = arr[1];
                        auto& n3 = arr[2];

                        if (!n1.isNumber()) failed("tint[0] is not a number");
                        if (!n2.isNumber()) failed("tint[1] is not a number");
                        if (!n3.isNumber()) failed("tint[2] is not a number");

                        auto color = Color {
                            n1.getNumber() / 255,
                            n2.getNumber() / 255,
                            n3.getNumber() / 255,
                            1.0
                        };

                        chart.animator.addEvent(note, PhiEvent {
                            .timeZone = INF_TZ,
                            .valueZone = chart.storyboardAssets.requestColorPair(color, color),
                            .type = EnumPhiEventType::Color,
                            .layerIndex = PhiEventLayerIndexs::NOTE_ATTRS
                        });
                    }

                    note.type = type;
                    note.time = startTime;
                    note.holdTime = endTime - startTime;
                    note.isFake = isFake;

                    chart.animator.addEvent(note, PhiEvent {
                        .timeZone = INF_TZ,
                        .valueZone = { positionX, positionX },
                        .type = EnumPhiEventType::PositionX,
                        .layerIndex = PhiEventLayerIndexs::NOTE_ATTRS
                    });

                    if (speed != 1.0) {
                        chart.animator.addEvent(note, PhiEvent {
                            .timeZone = INF_TZ,
                            .valueZone = { speed, speed },
                            .type = EnumPhiEventType::SpeedCoefficient,
                            .layerIndex = PhiEventLayerIndexs::NOTE_ATTRS
                        });
                    }

                    if (!isAbove) {
                        chart.animator.addEvent(note, PhiEvent {
                            .timeZone = INF_TZ,
                            .valueZone = { -1.0, -1.0 },
                            .type = EnumPhiEventType::SpeedCoefficient,
                            .layerIndex = PhiEventLayerIndexs::NOTE_ATTRS_2
                        });

                        chart.animator.addEvent(note, PhiEvent {
                            .timeZone = INF_TZ,
                            .valueZone = { 180.0, 180.0 },
                            .type = EnumPhiEventType::SelfRotation,
                            .layerIndex = PhiEventLayerIndexs::NOTE_ATTRS_2
                        });

                        note.reverseCover();
                    }
                }
            }

            if (judgeLineNode.hasKey("attachUI")) {
                if (!judgeLineNode["attachUI"].isString()) failed("attachUI is not a string");
                line.attachUI = PhiLineAttachUIHelper::FromString(judgeLineNode["attachUI"].getString());
            }

            if (judgeLineNode.hasKey("Texture")) {
                if (!judgeLineNode["Texture"].isString()) failed("Texture is not a string");
                auto textureName = judgeLineNode["Texture"].getString();

                if (textureName != "line.png") {
                    line.textureName = textureName;
                }
            }

            if (judgeLineNode.hasKey("father")) {
                if (!judgeLineNode["father"].isNumber()) failed("father is not a number");
                int64 fatherLineIndex = judgeLineNode["father"].getNumber();
                if (fatherLineIndex >= 0) {
                    line.fatherLineIndex = fatherLineIndex;
                }
            }

            bool enableCover = true;
            if (judgeLineNode.hasKey("isCover")) {
                if (judgeLineNode["isCover"].isNumber()) enableCover = judgeLineNode["isCover"].getNumber() == 1;
                else if (judgeLineNode["isCover"].isBool()) enableCover = judgeLineNode["isCover"].getBool();
                else failed("isCover is not a boolean or number");
            }
            line.enableCover = enableCover;

            if (judgeLineNode.hasKey("zOrder")) {
                if (!judgeLineNode["zOrder"].isNumber()) failed("zOrder is not a number");
                line.zOrder = judgeLineNode["zOrder"].getNumber();
            }

            if (judgeLineNode.hasKey("anchor")) {
                if (!judgeLineNode["anchor"].isArray()) failed("anchor is not an array");

                auto& anchorArr = judgeLineNode["anchor"].getArray();
                if (anchorArr.size() < 2) failed("anchor array size is less than 2");

                if (!anchorArr[0].isNumber() || !anchorArr[1].isNumber()) failed("anchor array element is not a number");
                line.anchor = { anchorArr[0].getNumber(), anchorArr[1].getNumber() };
            }
        }

        chart.rawHash = data.getHash();
        return chart;
    }

    PhiChart loadPhiChartFromPec(const Data& data) {
        auto failed = [](const std::string& msg) {
            throw std::runtime_error(std::format("pec: {}", msg));
        };

        struct TokenReader {
            std::string str;
            uint64 pos = 0;

            TokenReader(const std::string& str) : str(str) {}

            bool nextToken(std::string& dst) {
                jumpToNextNonWhiteSpace();
                if (pos >= str.size()) return false;
                uint64 start = pos;
                jumpToNextWhiteSpace();
                if (pos == start) return false;
                dst = str.substr(start, pos - start);
                jumpToNextNonWhiteSpace();
                return true;
            }

            private:
            bool currentIsWhiteSpace() const {
                return str[pos] == ' ' || str[pos] == '\t' || str[pos] == '\n' || str[pos] == '\r' || str[pos] == '\f' || str[pos] == '\v';
            }

            void jumpToNextWhiteSpace() {
                while (pos < str.size() && !currentIsWhiteSpace()) pos++;
            }

            void jumpToNextNonWhiteSpace() {
                while (pos < str.size() && currentIsWhiteSpace()) pos++;
            }
        };

        TokenReader reader(std::string((char*)data.data.data(), data.data.size()));
        std::string token;

        auto readNumber = [&](float64* dst) {
            if (!reader.nextToken(token)) return false;
            char* end;
            *dst = std::strtod(token.c_str(), &end);
            return end != token.c_str();
        };

        auto readBool = [&](bool* dst) {
            float64 num;
            if (!readNumber(&num)) return false;
            *dst = num == 1.0;
            return true;
        };

        PhiChart chart {};
        chart.meta.isHoldCoverAtHead = true;
        chart.meta.isZeroLengthHoldHidden = false;
        chart.meta.isHighNoteHidden = false;
        chart.meta.isRegLineAlphaNoteHidden = true;
        chart.meta.lineWidthUnit = { (float64)4000 / 1350, 0.0 };
        chart.meta.lineHeightUnit = { 0.0, (float64)1 / 180 };
        chart.meta.worldOrigin = { 0.0, 1400 };
        chart.meta.worldViewport = { 2048, -1400 };
        chart.meta.speedUnit = 120.0;

        float64 offset;
        if (!readNumber(&offset)) failed("failed to read offset");
        chart.meta.offset = (offset - 150.0) / 1000.0;

        struct Commands {
            struct Bpm {
                float64 startTime, bpm;
            };

            struct Note {
                int64 lineIndex;
                PhiNote note;
                bool isAbove;
                float64 speed = 1.0, size = 1.0, positionX;
            };

            struct Event {
                Vec2 timeZone;
                float64 value;
                bool useFront = false;
                uint64 easingType = 1;
            };
        };

        std::vector<Commands::Bpm> bpmCommands;
        std::vector<Commands::Note> noteCommands;
        std::unordered_map<int64, std::unordered_map<EnumPhiEventType, std::vector<Commands::Event>>> eventCommands;

        while (reader.nextToken(token)) {
            if (token == "bp") {
                float64 startTime, bpm;
                if (!readNumber(&startTime)) failed("failed to read startTime (bp)");
                if (!readNumber(&bpm)) failed("failed to read bpm (bp)");

                bpmCommands.push_back({
                    .startTime = startTime,
                    .bpm = bpm
                });
            } else if (token == "n1" || token == "n2" || token == "n3" || token == "n4") {
                auto type = PhiNoteTypeHelper::FromPEC(token);

                float64 lineIndex;
                if (!readNumber(&lineIndex)) failed("failed to read lineIndex (nx)");

                float64 startTime, endTime;
                if (!readNumber(&startTime)) failed("failed to read startTime (nx)");
                if (type == EnumPhiNoteType::Hold) {
                    if (!readNumber(&endTime)) failed("failed to read endTime (nx)");
                } else endTime = startTime;

                float64 positionX;
                bool isAbove, isFake;

                if (!readNumber(&positionX)) failed("failed to read positionX (nx)");
                if (!readBool(&isAbove)) failed("failed to read isAbove (nx)");
                if (!readBool(&isFake)) failed("failed to read isFake (nx)");

                noteCommands.push_back(Commands::Note {
                    .lineIndex = (int64)lineIndex,
                    .note = PhiNote {
                        .type = type,
                        .time = startTime,
                        .holdTime = endTime - startTime,
                        .isFake = isFake
                    },
                    .isAbove = isAbove,
                    .positionX = positionX
                });
            } else if (token == "#") {
                float64 speed;
                if (!readNumber(&speed)) failed("failed to read speed (#)");
                if (!noteCommands.empty()) noteCommands.back().speed = speed;
            } else if (token == "&") {
                float64 size;
                if (!readNumber(&size)) failed("failed to read size (&)");
                if (!noteCommands.empty()) noteCommands.back().size = size;
            } else if (token == "cp") {
                float64 lineIndex;
                if (!readNumber(&lineIndex)) failed("failed to read lineIndex (cp)");

                float64 time;
                if (!readNumber(&time)) failed("failed to read time (cp)");

                float64 x, y;
                if (!readNumber(&x)) failed("failed to read x (cp)");
                if (!readNumber(&y)) failed("failed to read y (cp)");

                eventCommands[lineIndex][EnumPhiEventType::PositionX].push_back(Commands::Event {
                    .timeZone = { time, time }, .value = x
                });

                eventCommands[lineIndex][EnumPhiEventType::PositionY].push_back(Commands::Event {
                    .timeZone = { time, time }, .value = y
                });
            } else if (token == "cd") {
                float64 lineIndex;
                if (!readNumber(&lineIndex)) failed("failed to read lineIndex (cd)");

                float64 time;
                if (!readNumber(&time)) failed("failed to read time (cd)");

                float64 r;
                if (!readNumber(&r)) failed("failed to read y (cd)");

                eventCommands[lineIndex][EnumPhiEventType::SelfRotation].push_back(Commands::Event {
                    .timeZone = { time, time }, .value = r
                });
            } else if (token == "ca") {
                float64 lineIndex;
                if (!readNumber(&lineIndex)) failed("failed to read lineIndex (ca)");

                float64 time;
                if (!readNumber(&time)) failed("failed to read time (ca)");

                float64 a;
                if (!readNumber(&a)) failed("failed to read a (ca)");

                eventCommands[lineIndex][EnumPhiEventType::AdditiveAlpha].push_back(Commands::Event {
                    .timeZone = { time, time }, .value = a
                });
            } else if (token == "cv") {
                float64 lineIndex;
                if (!readNumber(&lineIndex)) failed("failed to read lineIndex (cv)");

                float64 time;
                if (!readNumber(&time)) failed("failed to read time (cv)");

                float64 v;
                if (!readNumber(&v)) failed("failed to read v (cv)");

                eventCommands[lineIndex][EnumPhiEventType::Speed].push_back(Commands::Event {
                    .timeZone = { time, time }, .value = v
                });
            } else if (token == "cm") {
                float64 lineIndex;
                if (!readNumber(&lineIndex)) failed("failed to read lineIndex (cm)");

                float64 startTime, endTime;
                if (!readNumber(&startTime)) failed("failed to read startTime (cm)");
                if (!readNumber(&endTime)) failed("failed to read endTime (cm)");

                float64 x, y;
                if (!readNumber(&x)) failed("failed to read x (cm)");
                if (!readNumber(&y)) failed("failed to read y (cm)");

                float64 easingType;
                if (!readNumber(&easingType)) failed("failed to read easingType (cm)");

                eventCommands[lineIndex][EnumPhiEventType::PositionX].push_back(Commands::Event {
                    .timeZone = { startTime, endTime }, .value = x,
                    .useFront = true, .easingType = (uint64)easingType
                });

                eventCommands[lineIndex][EnumPhiEventType::PositionY].push_back(Commands::Event {
                    .timeZone = { startTime, endTime }, .value = y,
                    .useFront = true, .easingType = (uint64)easingType
                });
            } else if (token == "cr") {
                float64 lineIndex;
                if (!readNumber(&lineIndex)) failed("failed to read lineIndex (cr)");

                float64 startTime, endTime;
                if (!readNumber(&startTime)) failed("failed to read startTime (cr)");
                if (!readNumber(&endTime)) failed("failed to read endTime (cr)");

                float64 r;
                if (!readNumber(&r)) failed("failed to read r (cr)");

                float64 easingType;
                if (!readNumber(&easingType)) failed("failed to read easingType (cr)");

                eventCommands[lineIndex][EnumPhiEventType::SelfRotation].push_back(Commands::Event {
                    .timeZone = { startTime, endTime }, .value = r,
                    .useFront = true, .easingType = (uint64)easingType
                });
            } else if (token == "cf") {
                float64 lineIndex;
                if (!readNumber(&lineIndex)) failed("failed to read lineIndex (cf)");

                float64 startTime, endTime;
                if (!readNumber(&startTime)) failed("failed to read startTime (cf)");
                if (!readNumber(&endTime)) failed("failed to read endTime (cf)");

                float64 a;
                if (!readNumber(&a)) failed("failed to read a (cf)");

                eventCommands[lineIndex][EnumPhiEventType::AdditiveAlpha].push_back(Commands::Event {
                    .timeZone = { startTime, endTime }, .value = a,
                    .useFront = true
                });
            }
        }

        std::sort(bpmCommands.begin(), bpmCommands.end(), [](const auto& a, const auto& b) { return a.startTime < b.startTime; });
        std::vector<PhiBPMEvent> sharedBpmEvents;
        for (auto& cmd : bpmCommands) {
            sharedBpmEvents.push_back(PhiBPMEvent {
                .time = cmd.startTime,
                .bpm = cmd.bpm
            });
        }

        std::unordered_map<int64, uint64> lineIndexMap;
        auto getLineByIndex = [&](int64 index) -> PhiLine& {
            if (lineIndexMap.contains(index)) return chart.lines[lineIndexMap[index]];
            PhiLine line {};
            line.bpms = sharedBpmEvents;
            chart.lines.push_back(line);
            lineIndexMap[index] = chart.lines.size() - 1;
            return chart.lines.back();
        };

        auto toSeconds = [&](int64 lineIndex, float64 beatTime) {
            auto& line = getLineByIndex(lineIndex);
            return line.beat2sec(beatTime);
        };

        for (auto& cmd : noteCommands) {
            auto& line = getLineByIndex(cmd.lineIndex);
            auto& note = line.notes.emplace_back(std::move(cmd.note));

            auto time = toSeconds(cmd.lineIndex, note.time);
            auto holdTime = toSeconds(cmd.lineIndex, note.time + note.holdTime) - time;
            note.time = time;
            note.holdTime = holdTime;

            if (!cmd.isAbove) {
                chart.animator.addEvent(note, PhiEvent {
                    .timeZone = INF_TZ,
                    .valueZone = { -1.0, -1.0 },
                    .type = EnumPhiEventType::SpeedCoefficient,
                    .layerIndex = PhiEventLayerIndexs::NOTE_ATTRS_2
                });

                chart.animator.addEvent(note, PhiEvent {
                    .timeZone = INF_TZ,
                    .valueZone = { 180.0, 180.0 },
                    .type = EnumPhiEventType::SelfRotation,
                    .layerIndex = PhiEventLayerIndexs::NOTE_ATTRS_2
                });

                note.reverseCover();
            }

            if (cmd.positionX != 0.0) {
                chart.animator.addEvent(note, PhiEvent {
                    .timeZone = INF_TZ,
                    .valueZone = { cmd.positionX, cmd.positionX },
                    .type = EnumPhiEventType::PositionX,
                    .layerIndex = PhiEventLayerIndexs::NOTE_ATTRS
                });
            }

            if (cmd.speed != 1.0) {
                chart.animator.addEvent(note, PhiEvent {
                    .timeZone = INF_TZ,
                    .valueZone = { cmd.speed, cmd.speed },
                    .type = EnumPhiEventType::SpeedCoefficient,
                    .layerIndex = PhiEventLayerIndexs::NOTE_ATTRS
                });
            }

            if (cmd.size != 1.0) {
                chart.animator.addEvent(note, PhiEvent {
                    .timeZone = INF_TZ,
                    .valueZone = { cmd.size, cmd.size },
                    .type = EnumPhiEventType::ScaleX,
                    .layerIndex = PhiEventLayerIndexs::NOTE_ATTRS
                });
            }
        }

        for (auto& [lineIndex, events] : eventCommands) {
            for (auto& [type, typedEvents] : events) {
                std::sort(typedEvents.begin(), typedEvents.end(), [](const auto& a, const auto& b) {
                    if (a.timeZone.x != b.timeZone.x) return a.timeZone.x < b.timeZone.x;
                    if (a.timeZone.y != b.timeZone.y) return a.timeZone.y < b.timeZone.y;
                    return b.useFront;
                });

                for (auto& cmd : typedEvents) {
                    cmd.timeZone.x = toSeconds(lineIndex, cmd.timeZone.x);
                    cmd.timeZone.y = toSeconds(lineIndex, cmd.timeZone.y);

                    if (type == EnumPhiEventType::AdditiveAlpha) cmd.value /= 255.0;
                }

                for (uint64 i = 0; i < typedEvents.size(); i++) {
                    auto& cmd = typedEvents[i];
                    auto& line = getLineByIndex(lineIndex);

                    float64 startValue;
                    if (cmd.useFront && i - 1 >= 0) startValue = typedEvents[i - 1].value;
                    else startValue = cmd.value;

                    if (i < typedEvents.size() - 1) {
                        auto& next = typedEvents[i + 1];
                        if (cmd.timeZone.isZeroZone() && next.timeZone.x == cmd.timeZone.y) continue;
                    }
                    
                    if (cmd.timeZone.isZeroZone()) cmd.timeZone.y += 1.0;

                    PhiEvent e {};
                    e.timeZone = cmd.timeZone;
                    e.valueZone = { startValue, cmd.value };
                    e.type = type;
                    e.layerIndex = PhiEventLayerIndexs::LINE_DEFAULT;

                    if (cmd.easingType > 1) {
                        e.easingFuncContext = (void*)cmd.easingType;
                        e.easingFunc = [](void* ctx, float64 p) { return EaseSet::Phigros::RePhiEdit::easing((uint64)ctx, p); };
                        e.easingIntFunc = [](void* ctx, float64 p) { return EaseSet::Phigros::RePhiEdit::easing_int((uint64)ctx, p); };
                    }

                    chart.animator.addEvent(line, e);
                }
            }
        }

        chart.rawHash = data.getHash();
        return chart;
    }

    PhiChart loadPhiChartFromData(const Data& data) {
        std::vector<std::string> msgs;

        #define try_(func) \
            try { return func(data); } \
            catch (const std::exception& err) { msgs.push_back(err.what()); }
        
        try_(loadPhiChartFromOfficialJson);
        try_(loadPhiChartFromRpeJson);
        try_(loadPhiChartFromPec);

        std::string msg = "failures: \n";
        for (auto& m : msgs) msg += m + "\n";
        return {};

        #undef try
    }

    PhiExtra loadPhiExtraFromJsonData(const Data& data, PhiStoryboardAssets& assets) {
        auto failed = [](const std::string& msg) {
            throw std::runtime_error(msg);
        };

        auto jsonRoot = JsonNode::Parse(data);

        if (!jsonRoot.isObject()) failed("root is not an object");

        PhiExtra extra {};
        
        auto parseTimeTuple = [](const JsonNode& node, float64* dst) {
            if (!node.isArray()) return false;
            if (node.getArray().size() != 3) return false;

            const auto& arr = node.getArray();
            if (!arr[0].isNumber()) return false;
            if (!arr[1].isNumber()) return false;
            if (!arr[2].isNumber()) return false;

            float64 n1 = arr[0].getNumber(),
                n2 = arr[1].getNumber(),
                n3 = arr[2].getNumber();

            *dst = n1 + n2 / n3;
            return true;
        };

        std::vector<PhiBPMEvent> bpmEvents;

        if (!jsonRoot.hasKey("bpm")) failed("missing bpm field");
        if (!jsonRoot["bpm"].isArray()) failed("bpm is not an array");

        auto& bpmArr = jsonRoot["bpm"].getArray();
        for (auto& bpmEventNode : bpmArr) {
            if (!bpmEventNode.isObject()) failed("bpm item is not an object");

            if (!bpmEventNode.hasKey("time")) failed("missing time field");
            float64 time;
            if (!parseTimeTuple(bpmEventNode["time"], &time)) failed("time is not a valid time tuple");

            if (!bpmEventNode.hasKey("bpm")) failed("missing bpm field");
            if (!bpmEventNode["bpm"].isNumber()) failed("bpm is not a number");
            float64 bpm = bpmEventNode["bpm"].getNumber();

            bpmEvents.push_back({
                .time = time,
                .bpm = bpm
            });
        }

        PhiBPMEvent::SortBpmEvents(bpmEvents);

        PhiLine tempLine {};
        tempLine.bpms = bpmEvents;

        auto parseTimeTupleToSecond = [&](const JsonNode& node, float64* dst) {
            if (!parseTimeTuple(node, dst)) return false;
            *dst = tempLine.beat2sec(*dst);
            return true;
        };

        auto parseVectorUniform = [&](JsonNode& node, PhiShaderUniform* dst) {
            if (!node.isArray()) return false;
            auto& arr = node.getArray();
            for (auto& i : arr) {
                if (!i.isNumber()) return false;
            }

            if (!(2 <= arr.size() && arr.size() <= 4)) return false;

            dst->used = arr.size();

            for (uint8 i = 0; i < dst->used; i++) {
                dst->value[i] = arr[i].getNumber();
                if (dst->used >= 3) dst->value[i] /= 255.0;
            }

            return true;
        };

        if (!jsonRoot.hasKey("effects")) failed("missing effects field");
        if (!jsonRoot["effects"].isArray()) failed("effects is not an array");
        auto& effectsNode = jsonRoot["effects"].getArray();

        for (auto& effectNode : effectsNode) {
            if (!effectNode.isObject()) failed("effects item is not an object");

            if (!effectNode.hasKey("start")) failed("missing start field");
            if (!effectNode.hasKey("end")) failed("missing end field");
            
            float64 startTime, endTime;
            if (!parseTimeTupleToSecond(effectNode["start"], &startTime)) failed("start is not a valid time tuple");
            if (!parseTimeTupleToSecond(effectNode["end"], &endTime)) failed("end is not a valid time tuple");

            bool isGlobal = false;
            if (effectNode.hasKey("global")) {
                if (!effectNode["global"].isBool()) failed("global is not a bool");
                isGlobal = effectNode["global"].getBool();
            }

            std::optional<uint64> targetLine;
            if (effectNode.hasKey("line")) {
                if (!effectNode["line"].isNumber()) failed("line is not a number");
                targetLine = effectNode["line"].getNumber();
            }

            uint64 order = 0;
            if (effectNode.hasKey("order")) {
                if (!effectNode["order"].isNumber()) failed("order is not a number");
                order = effectNode["order"].getNumber();
            }

            if (!effectNode.hasKey("shader")) failed("missing shader field");
            if (!effectNode["shader"].isString()) failed("shader is not a string");
            auto shaderName = effectNode["shader"].getString();

            auto& item = extra.effects.emplace_back();
            item.timeZone = { startTime, endTime };
            item.targetLine = targetLine;
            item.order = order;
            item.isGlobal = isGlobal;
            item.shaderName = shaderName;

            if (effectNode.hasKey("vars")) {
                if (!effectNode["vars"].isObject()) failed("vars is not an object");
                auto& varsNode = effectNode["vars"].getObject();

                for (auto& [uniformName, eventsNode] : varsNode) {
                    auto& layer = item.uniforms[uniformName];

                    if (eventsNode.isArray()) {
                        auto& eventsArr = eventsNode.getArray();
                        if (eventsArr.empty()) failed("events array is empty");

                        JsonNode::EnumType eventItemNodeType = eventsArr[0].type;
                        for (auto& node : eventsArr) {
                            if (node.type != eventItemNodeType) failed("events array contains different types of nodes");
                        }

                        if (eventItemNodeType == JsonNode::EnumType::Object) {
                            for (auto& eventNode : eventsArr) {
                                if (!eventNode.hasKey("startTime")) failed("missing startTime field");
                                if (!eventNode.hasKey("endTime")) failed("missing endTime field");

                                float64 startTime, endTime;
                                if (!parseTimeTupleToSecond(eventNode["startTime"], &startTime)) failed("startTime is not a valid time tuple");
                                if (!parseTimeTupleToSecond(eventNode["endTime"], &endTime)) failed("endTime is not a valid time tuple");

                                if (!eventNode.hasKey("start")) failed("missing start field");
                                if (!eventNode.hasKey("end")) failed("missing end field");
                                if (eventNode["start"].type != eventNode["end"].type) failed("start and end are not the same type");

                                Vec2 valueZone;
                                
                                if (eventNode["start"].isNumber()) {
                                    valueZone = assets.requestShaderUniformPair(eventNode["start"].getNumber(), eventNode["end"].getNumber());
                                } else if (eventNode["start"].isArray()) {
                                    PhiShaderUniform startUniform, endUniform;
                                    if (!parseVectorUniform(eventNode["start"], &startUniform)) failed("start is not a valid vector uniform");
                                    if (!parseVectorUniform(eventNode["end"], &endUniform)) failed("end is not a valid vector uniform");
                                    valueZone = assets.requestShaderUniformPair(startUniform, endUniform);
                                } else failed("start and end are not a number or array");

                                uint64 easingType = 1;
                                if (eventNode.hasKey("easingType")) {
                                    if (!eventNode["easingType"].isNumber()) failed("easingType is not a number");
                                    easingType = eventNode["easingType"].getNumber();
                                }

                                PhiEvent e {};
                                e.timeZone = { startTime, endTime };
                                e.valueZone = valueZone;
                                e.type = EnumPhiEventType::PhiShaderUniform;
                                e.layerIndex = PhiEventLayerIndexs::SHADER_UNIFORM_DEFAULT;

                                if (easingType > 1) {
                                    e.easingFuncContext = (void*)easingType;
                                    e.easingFunc = [](void* ctx, float64 p) { return EaseSet::Phigros::RePhiEdit::easing((uint64)ctx, p); };
                                }

                                layer.addEvent(e);
                            }
                        } else if (eventItemNodeType == JsonNode::EnumType::Number) {
                            PhiShaderUniform uniform;
                            if (!parseVectorUniform(eventsNode, &uniform)) failed("events item is not a valid vector uniform");
                            layer.addEvent({
                                .timeZone = INF_TZ,
                                .valueZone = assets.requestShaderUniformPair(uniform, uniform),
                                .type = EnumPhiEventType::PhiShaderUniform,
                                .layerIndex = PhiEventLayerIndexs::SHADER_UNIFORM_DEFAULT
                            });
                        } else failed("events array item is not an object or number");
                    } else if (eventsNode.isNumber()) {
                        layer.addEvent({
                            .timeZone = INF_TZ,
                            .valueZone = assets.requestShaderUniformPair(eventsNode.getNumber(), eventsNode.getNumber()),
                            .type = EnumPhiEventType::PhiShaderUniform,
                            .layerIndex = PhiEventLayerIndexs::SHADER_UNIFORM_DEFAULT
                        });
                    } else failed("event(s) is not an array or number");
                }
            }
        }

        return extra;
    }

    struct PhiStoryboardHelpers {
        static std::string nameToPath(const std::string& dir, const std::string& name) {
            return std::filesystem::path(std::format("{}/{}", dir, name))
                .lexically_normal().string();
        }

        static std::unordered_map<std::string, PhiShaderUniform> parseDefaultShaderUniforms(
            const std::string& code
        ) {
            std::vector<std::string> lines;
            splitString(code, lines);

            std::unordered_map<std::string, PhiShaderUniform> result;

            for (auto& line : lines) {
                stripString(line);
                if (!stringIsStartsWith(line, "uniform ")) continue;

                auto s = line.find('%');
                if (s == std::string::npos) continue;

                auto e = line.find('%', s);
                if (e == std::string::npos) continue;

                auto default_str = line.substr(s + 1, e - s - 1);

                std::vector<std::string> value_strs;
                splitString(default_str, value_strs, ',');

                std::vector<float64> values;
                for (auto& value_str : value_strs) {
                    stripString(value_str);
                    try { values.push_back(std::stod(value_str)); }
                    catch (...) { values.push_back(0); }
                }

                if (values.empty() || values.size() > 4) continue;

                s = line.find(';');
                if (s == std::string::npos) continue;
                e = s;
                while (e > 0 && line[e - 1] != ' ') e--;

                result[line.substr(e, s - e)] = PhiShaderUniform(values);
            }

            return result;
        }
    };

    struct ParsedRPEChartInfo {
        std::string name;
        std::string path;
        std::string song;
        std::string picture;
        std::string chart;
        std::string level;
        std::string composer;
        std::string lastEditTime;
        std::string length;
        std::string editTime;
        std::string group;

        static std::vector<ParsedRPEChartInfo> parse(const Data& data) {
            std::vector<ParsedRPEChartInfo> infos;

            auto str = data.toString();
            str.erase(std::remove(str.begin(), str.end(), '\r'), str.end());

            std::vector<std::string> lines;
            splitString(str, lines);

            ParsedRPEChartInfo info {};
            uint64 vaildLineCount = 0;

            for (auto& line : lines) {
                stripString(line);

                if (line.empty()) continue;
                if (line[0] == '#') {
                    if (vaildLineCount) {
                        infos.push_back(info);
                        info = {};
                        vaildLineCount = 0;
                    }
                    continue;
                }

                auto split = line.find(": ");
                if (split == std::string::npos) continue;

                auto key = line.substr(0, split);
                auto value = line.substr(split + 2);

                if (key == "Name") info.name = value;
                else if (key == "Path") info.path = value;
                else if (key == "Song") info.song = value;
                else if (key == "Picture") info.picture = value;
                else if (key == "Chart") info.chart = value;
                else if (key == "Level") info.level = value;
                else if (key == "Composer") info.composer = value;
                else if (key == "LastEditTime") info.lastEditTime = value;
                else if (key == "Length") info.length = value;
                else if (key == "EditTime") info.editTime = value;
                else if (key == "Group") info.group = value;
                vaildLineCount++;
            }

            if (vaildLineCount) {
                infos.push_back(info);
            }

            return infos;
        }
    };

    struct PhiCalculateFrameConfig {
        struct NoteTextureInfo {
            struct Item {
                Vec2 textureSize;
                Vec2 cutPadding;
                Vec2 scaling = { 1.0, 1.0 };
            };

            Item single;
            Item simul;
        };

        Vec2 screenSize;
        Vec2 backgroundTextureSize;
        std::unordered_map<EnumPhiNoteType, NoteTextureInfo> noteTextureInfos;
        float64 songLength;
        float64 maxNoteBodyLength = 8192.0;
    };

    struct PhiCalculatedFrame {
        UsingSharedCalculatedObjects;

        struct CalculatedNote {
            Vec2 position;
            float64 rotation;
            float64 width, head, body, tail;
            EnumPhiNoteType type;
            bool isSimul;
            Color color;
        };

        struct CalculatedStoryboardTexture {
            uint64 texture;
            Vec2 position, size, scale, anchor;
            float64 rotation;
            Color color;
        };

        struct CalculatedHitEffectTexture {
            Vec2 position, size;
            float64 progress, rotation;
            Color color;
        };

        struct CalculatedShader {
            uint64 id;
            std::unordered_map<std::string, PhiShaderUniform> uniforms;
        };

        using CalculatedObject = std::variant<
            ListSharedCalculatedObjects,

            CalculatedNote,
            CalculatedStoryboardTexture,
            CalculatedHitEffectTexture,
            CalculatedShader
        >;

        float64 backgroundImageBlurRadius;
        Rect unsafeBackgroundRect, backgroundRect;
        float64 unsafeAreaDim, backgroundDim;
        Rect objectsClipRect;
        std::vector<CalculatedObject> objects;
        std::unordered_map<EnumPhiNoteType, uint64> hitsounds;

        void culling(std::vector<CalculatedObject>& objects, const Rect& screenRect) noexcept {
            if (SharedCalculatedObjects::sharedCulling(objects, screenRect)) return;

            auto& obj = objects.back();

            if (std::holds_alternative<CalculatedStoryboardTexture>(obj)) {
                auto& tex = std::get<CalculatedStoryboardTexture>(obj);
                if (!quadStrictlyIntersectRect(makeQuadFromRectInfo({
                    .position = tex.position,
                    .size = tex.size * tex.scale,
                    .rotation = tex.rotation,
                    .anchor = tex.anchor
                }).data(), screenRect)) objects.pop_back();
            } else if (std::holds_alternative<CalculatedHitEffectTexture>(obj)) {
                auto& tex = std::get<CalculatedHitEffectTexture>(obj);
                if (!quadStrictlyIntersectRect(makeQuadFromRectInfo({
                    .position = tex.position,
                    .size = tex.size,
                    .rotation = tex.rotation
                }).data(), screenRect)) objects.pop_back();
            }
        }

        void addObject(std::vector<CalculatedObject>& objects, const CalculatedObject& obj, const Rect& screenRect) noexcept {
            objects.push_back(obj);
            culling(objects, screenRect);
        }

        struct Cache {
            struct AttachUIData {
                Vec2 position, scale = { 1.0, 1.0 };
                float64 rotation;
                Color color = { 1.0, 1.0, 1.0, 1.0 };
            };

            std::unordered_map<EnumPhiLineAttachUI, AttachUIData> attachUIDatas;
            std::unordered_map<EnumPhiNoteType, std::vector<CalculatedObject>> noteObjects;

            void clear() {
                attachUIDatas.clear();
                for (auto& [_, v] : noteObjects) v.clear();
            }
        };

        Cache cache;
    };

    void calculatePhiFrame(
        PhiChart& chart, float64 time,
        const PhiCalculateFrameConfig& config,
        PhiCalculatedFrame& frame
    ) {
        frame.objects.clear();
        frame.hitsounds.clear();
        frame.cache.clear();

        float64 screenRatio = config.screenSize.x / config.screenSize.y;
        Rect safeArea = screenRatio > chart.meta.maxViewRatio ? getCoveredOrContainRect(
            { 0.0, 0.0, config.screenSize.x, config.screenSize.y },
            { chart.meta.maxViewRatio, 1.0 }, false
        ) : Rect { 0.0, 0.0, config.screenSize.x, config.screenSize.y };

        auto safeAreaPosition = safeArea.position();
        auto safeAreaSize = safeArea.size();
        auto toScreen = [&](Vec2 pos) { return pos + safeAreaPosition; };

        frame.backgroundImageBlurRadius = config.backgroundTextureSize.sum() * chart.options.backgroundTextureBlurRadius;

        frame.unsafeBackgroundRect = getCoveredOrContainRect(
            { 0.0, 0.0, config.screenSize.x, config.screenSize.y },
            config.backgroundTextureSize, true
        );
        frame.unsafeAreaDim = chart.options.unsafeBackgroundDim;

        frame.backgroundRect = getCoveredOrContainRect(safeArea, config.backgroundTextureSize, true);
        frame.backgroundDim = chart.options.backgroundDim;

        frame.objectsClipRect = safeArea;

        auto processAttachUIText = [&](PhiCalculatedFrame::CalculatedText rawText, EnumPhiLineAttachUI attachUIType) {
            auto& data = frame.cache.attachUIDatas[attachUIType];
            rawText.position += data.position;
            rawText.scale *= data.scale;
            rawText.rotation += data.rotation;
            rawText.color *= data.color;
            return rawText;
        };

        time -= chart.meta.offset;
        
        auto lineWidth = (chart.meta.lineWidthUnit * safeAreaSize).sum();
        auto lineHeight = (chart.meta.lineHeightUnit * safeAreaSize).sum();
        auto standardNoteWidth = safeAreaSize.x * 0.1234375 * chart.options.noteScaling;

        struct NoteTextureSizeInfo {
            float64 width;
            float64 head, body, tail;

            void scale(const Vec2& v) {
                width *= v.x;
                head *= v.y;
                body *= v.y;
                tail *= v.y;
            }

            float64 getHeadHalfDiagonal() const {
                return Vec2 { width, head }.length() / 2;
            }
        };

        auto getNoteTextureSizeInfo = [&](EnumPhiNoteType type, bool isSimul, bool hideHead) {
            const auto& texInfo = (
                isSimul
                ? config.noteTextureInfos.at(type).simul
                : config.noteTextureInfos.at(type).single
            );

            auto width = standardNoteWidth;
            auto totalHeight = width / texInfo.textureSize.x * texInfo.textureSize.y;
            width *= texInfo.scaling.x; totalHeight *= texInfo.scaling.y;
            auto cutPadding = texInfo.cutPadding / texInfo.textureSize.y;
            auto head = hideHead ? 0.0 : cutPadding.x * totalHeight;
            auto tail = cutPadding.y * totalHeight;

            return NoteTextureSizeInfo {
                .width = width,
                .head = head,
                .tail = tail
            };
        };

        float64 maxHalfNoteHeadDiagonal = 0.0;

        for (auto& [type, _] : config.noteTextureInfos) {
            maxHalfNoteHeadDiagonal = std::max({
                maxHalfNoteHeadDiagonal,
                getNoteTextureSizeInfo(type, false, false).getHeadHalfDiagonal(),
                getNoteTextureSizeInfo(type, true, false).getHeadHalfDiagonal()
            });
        }

        chart.state.timeUpdated(time);

        for (auto& lineIndex : chart.zOrderSortedLines) {
            auto& line = chart.lines[lineIndex];

            auto linePosition = chart.getLinePosition(time, line, safeAreaSize);
            auto lineScreenPosition = toScreen(linePosition);
            auto linePositionRelOrigin = chart.getLinePositionRelOrigin(time, line, safeAreaSize);
            auto lineRotation = chart.animator.get(line, time, EnumPhiEventType::SelfRotation);
            auto lineAlpha = chart.animator.get_alpha(line, time, 0.0);
            auto lineTextIndex = chart.animator.get(line, time, EnumPhiEventType::Text);
            auto lineTextIndexZone = chart.animator.get_zone(line, time, EnumPhiEventType::Text);
            auto lineText = chart.storyboardAssets.getText(lineTextIndex, lineTextIndexZone);
            auto lineColorIndex = chart.animator.get(line, time, EnumPhiEventType::Color);
            auto lineColorIndexZone = chart.animator.get_zone(line, time, EnumPhiEventType::Color);
            auto lineColor = chart.storyboardAssets.getColor(
                lineColorIndex,
                (line.attachUI.has_value() || lineText.has_value() || line.textureName.has_value())
                    ? Color::White()
                    : chart.options.lineDefaultColor,
                lineColorIndexZone
            );
            auto lineScale = Vec2 {
                chart.animator.get(line, time, EnumPhiEventType::ScaleX),
                chart.animator.get(line, time, EnumPhiEventType::ScaleY)
            };

            if (line.attachUI.has_value()) {
                frame.cache.attachUIDatas[line.attachUI.value()] = {
                    .position = linePositionRelOrigin,
                    .scale = lineScale,
                    .rotation = lineRotation,
                    .color = lineColor.applyAlpha(lineAlpha)
                };
            }

            if (lineAlpha * lineColor.a > 0) {
                if (lineText.has_value()) {
                    frame.objects.push_back(PhiCalculatedFrame::CalculatedText {
                        .text = lineText.value(),
                        .position = lineScreenPosition,
                        .scale = lineScale,
                        .anchor = line.anchor,
                        .fontSize = (chart.options.storyboardTextBaseSize * safeAreaSize).sum(),
                        .rotation = lineRotation,
                        .color = lineColor.applyAlpha(lineAlpha)
                    });
                } else if (!line.attachUI.has_value()) {
                    if (line.textureName.has_value()) {
                        auto& textureName = line.textureName.value();

                        if (chart.storyboardAssets.isTextureLoaded(textureName)) {
                            auto& texture = chart.storyboardAssets.getTexture(textureName);
                            float64 textureWidth, textureHeight;

                            if (chart.options.storyboardTextureSclaingBehavior == PhiChart::UserOptions::EnumStoryboardTextureSclaingBehavior::AboutWidth) {
                                textureWidth = texture.second.x / std::abs(chart.meta.worldViewport.x) * safeAreaSize.x;
                                textureHeight = textureWidth / texture.second.x * texture.second.y;
                            } else if (chart.options.storyboardTextureSclaingBehavior == PhiChart::UserOptions::EnumStoryboardTextureSclaingBehavior::AboutHeight) {
                                textureHeight = texture.second.y / std::abs(chart.meta.worldViewport.y) * safeAreaSize.y;
                                textureWidth = textureHeight / texture.second.y * texture.second.x;
                            } else if (chart.options.storyboardTextureSclaingBehavior == PhiChart::UserOptions::EnumStoryboardTextureSclaingBehavior::Stretch) {
                                textureWidth = texture.second.x / std::abs(chart.meta.worldViewport.x) * safeAreaSize.x;
                                textureHeight = texture.second.y / std::abs(chart.meta.worldViewport.y) * safeAreaSize.y;
                            } else textureWidth = textureHeight = 0;

                            textureWidth *= chart.options.storyboardTextureScaling.x;
                            textureHeight *= chart.options.storyboardTextureScaling.y;

                            frame.addObject(frame.objects, PhiCalculatedFrame::CalculatedStoryboardTexture {
                                .texture = texture.first,
                                .position = lineScreenPosition,
                                .size = Vec2 { textureWidth, textureHeight },
                                .scale = lineScale,
                                .anchor = line.anchor,
                                .rotation = lineRotation,
                                .color = lineColor.applyAlpha(lineAlpha)
                            }, safeArea);
                        }
                    } else {
                        frame.addObject(frame.objects, PhiCalculatedFrame::CalculatedPoly::Make(
                            Vec2 { -lineWidth, -lineHeight } * line.anchor * lineScale,
                            Vec2 { lineWidth, lineHeight } * lineScale,
                            lineColor.applyAlpha(lineAlpha),
                            Transform2D()
                                .translate(lineScreenPosition)
                                .rotateDegrees(lineRotation)
                        ), safeArea);
                    }
                }
            }

            for (auto& noteGroup : line.noteGroups) {
                noteGroup.state.timeUpdated(time);

                for (uint64 note_ii = noteGroup.state.firstNoteIndex; note_ii < noteGroup.indexs.size(); note_ii++) {
                    auto note_i = noteGroup.indexs[note_ii];
                    auto& note = line.notes[note_i];
                    note.state.timeUpdated(note, time);

                    auto frameInfo = chart.getNoteFrameInfo(line, note, time, safeAreaSize);

                    if (frameInfo.isArrived && note.state.onPlayHitsound()) {
                        if (!note.isFake) {
                            frame.hitsounds[note.type]++;
                        }
                    }

                    if (note.time + note.holdTime < time) {
                        noteGroup.state.passedNoteIndex(note_ii);
                        continue;
                    }

                    auto noteScreenHeadPosition = toScreen(frameInfo.headPosition);
                    auto noteScreenTailPosition = toScreen(frameInfo.tailPosition);

                    auto sizeInfo = getNoteTextureSizeInfo(note.type, note.isSimul, frameInfo.isArrived);
                    sizeInfo.body = std::min(config.maxNoteBodyLength, (noteScreenHeadPosition - noteScreenTailPosition).length());
                    sizeInfo.scale(frameInfo.scale);

                    Transform2D noteTransform;
                    noteTransform.translate(noteScreenHeadPosition);
                    noteTransform.rotateDegrees(frameInfo.textureRotation);
                    noteTransform.scale(1.0, -1.0);

                    Vec2 noteQuad[4] = {
                        noteTransform.transformPoint({ -sizeInfo.width / 2, -sizeInfo.head }),
                        noteTransform.transformPoint({ sizeInfo.width / 2, -sizeInfo.head }),
                        noteTransform.transformPoint({ sizeInfo.width / 2, sizeInfo.body + sizeInfo.tail }),
                        noteTransform.transformPoint({ -sizeInfo.width / 2, sizeInfo.body + sizeInfo.tail })
                    };

                    // 只 hide 不用考虑 maxHalfNoteHeadDiagonal, 但是这里 break 优化也要用
                    auto extendedSafeArea = safeArea.extend(maxHalfNoteHeadDiagonal * frameInfo.scale.max());
                    bool noteInsideScreen = quadStrictlyIntersectRect(noteQuad, extendedSafeArea);

                    if (noteInsideScreen) {
                        if (frameInfo.color.a > 0.0) {
                            frame.cache.noteObjects[note.type].push_back(PhiCalculatedFrame::CalculatedNote {
                                .position = noteScreenHeadPosition,
                                .rotation = frameInfo.textureRotation,
                                .width = sizeInfo.width,
                                .head = sizeInfo.head,
                                .body = sizeInfo.body,
                                .tail = sizeInfo.tail,
                                .type = note.type,
                                .isSimul = note.isSimul,
                                .color = frameInfo.color
                            });
                        }
                    } else {
                        if (noteGroup.breakable) {
                            if (lineIsLeavingScreen(
                                noteScreenHeadPosition,
                                frameInfo.speedVectorRotation + 90.0,
                                extendedSafeArea
                            ) && lineIsLeavingScreen(
                                noteScreenHeadPosition,
                                frameInfo.textureRotation,
                                extendedSafeArea
                            )) break;
                        }
                    }
                }
            }
        }

        for (const auto type : {
            EnumPhiNoteType::Hold,
            EnumPhiNoteType::Drag,
            EnumPhiNoteType::Tap,
            EnumPhiNoteType::Flick
        }) {
            auto& noteObjects = frame.cache.noteObjects[type];
            frame.objects.insert(frame.objects.end(), noteObjects.begin(), noteObjects.end());
        }

        const float64 hitEffectTextureSize = standardNoteWidth * chart.options.hitEffectTextureScaling;

        for (uint64 i = chart.state.firstHitEffectIndex; i < chart.hitEffects.size(); i++) {
            auto& hitEffect = chart.hitEffects[i];
            if (hitEffect.time > time) break;

            auto& line = chart.lines[hitEffect.lineIndex];
            auto& note = line.notes[hitEffect.noteIndex];

            auto info = chart.getNoteFrameInfo(line, note, hitEffect.time, safeAreaSize);
            auto endTime = hitEffect.time + std::max(chart.options.hitEffectDuration, hitEffect.particles.size() ? (hitEffect.particles.back().dt + chart.options.hitEffectDuration) : 0.0);

            if (endTime < time) {
                chart.state.passedHitEffectIndex(i);
                continue;
            }

            auto effectScreenPosition = toScreen(info.headPosition);
            auto progress = (time - hitEffect.time) / chart.options.hitEffectDuration;

            if (progress <= 1.0) {
                frame.addObject(frame.objects, PhiCalculatedFrame::CalculatedHitEffectTexture {
                    .position = effectScreenPosition,
                    .size = { hitEffectTextureSize, hitEffectTextureSize },
                    .progress = progress,
                    .rotation = 0.0,
                    .color = chart.options.lineDefaultColor.applyAlpha(chart.options.hitEffectAlpha)
                }, safeArea);
            }

            for (auto& particle : hitEffect.particles) {
                auto particleTime = hitEffect.time + particle.dt;
                if (particleTime > time) break;
                if (particleTime + chart.options.hitEffectDuration < time) continue;

                auto info = chart.getNoteFrameInfo(line, note, particleTime, safeAreaSize);
                auto effectScreenHeadPosition = toScreen(info.headPosition);
                auto progress = std::clamp((time - particleTime) / chart.options.hitEffectDuration, 0.0, 1.0);
                auto size = standardNoteWidth / 5.3 * chart.options.hitEffectParticleSize * (((0.20783014 * progress - 1.65243926) * progress + 1.6398785) * progress + 0.49884492);
                auto distance = standardNoteWidth / 180 * chart.options.hitEffectParticleDistance * particle.size * (((850.3997391752 * progress + 6236.3848902154) * progress + 80.3542231806) * progress / ((6570.5817658876 * progress + 495.7977913926) * progress + 1.0));

                auto particlePosition = toScreen(info.headPosition.rotateDegrees(particle.rotation, distance));
                frame.addObject(frame.objects, PhiCalculatedFrame::CalculatedRect {
                    .position = particlePosition,
                    .size = { size, size },
                    .rotation = 0.0,
                    .color = chart.options.lineDefaultColor.applyAlpha(chart.options.hitEffectAlpha * (1.0 - progress))
                }, safeArea);
            }
        }

        auto calculateExtra = [&](bool isGlobal) {
            for (auto& effectIndex : chart.extra.zOrderSortedEffects) {
                auto& effect = chart.extra.effects[effectIndex];
                if (effect.isGlobal != isGlobal) continue;
                if (!effect.timeZone.include(time)) continue;

                PhiCalculatedFrame::CalculatedShader shader { .id = effect.shaderId };

                for (auto& [uniformName, layer] : effect.uniforms) {
                    layer.updateType(EnumPhiEventType::PhiShaderUniform, time);
                    auto uniformIndex = layer.get(EnumPhiEventType::PhiShaderUniform);
                    auto uniformIndexZone = layer.get_zone(EnumPhiEventType::PhiShaderUniform).value_or(Vec2 {});
                    auto uniformValue = chart.storyboardAssets.getShaderUniform(uniformIndex, PhiShaderUniform(), uniformIndexZone);
                    shader.uniforms[uniformName] = uniformValue;
                }

                frame.objects.push_back(shader);
            }
        };

        calculateExtra(false);

        auto combo = chart.getCombo(time);

        time += chart.meta.offset;
        float64 songPorgress = time / config.songLength;

        float64 progressBarHeight = safeAreaSize.x * 0.005921;
        float64 progressBarWidth = safeAreaSize.x * songPorgress;
        float64 progressBarPointWidth = safeAreaSize.x * 0.00175;

        auto& progressBarAttachUIData = frame.cache.attachUIDatas[EnumPhiLineAttachUI::Bar];

        frame.objects.push_back(PhiCalculatedFrame::CalculatedPoly::Make(
            { 0.0, 0.0 },
            { progressBarWidth, progressBarHeight },
            chart.options.progressBarDefaultColor.first * progressBarAttachUIData.color,
            Transform2D()
                .translate(safeAreaPosition)
                .translate(progressBarAttachUIData.position)
                .scale(progressBarAttachUIData.scale)
                .rotateDegrees(progressBarAttachUIData.rotation)
        ));

        frame.objects.push_back(PhiCalculatedFrame::CalculatedPoly::Make(
            { progressBarWidth - progressBarPointWidth, 0.0 },
            { progressBarPointWidth, progressBarHeight },
            chart.options.progressBarDefaultColor.second * progressBarAttachUIData.color,
            Transform2D()
                .translate(safeAreaPosition)
                .translate(progressBarAttachUIData.position)
                .scale(progressBarAttachUIData.scale)
                .rotateDegrees(progressBarAttachUIData.rotation)
        ));

        auto pauseButtonPosition = Vec2 { 3.16669, 3.6065 } * progressBarHeight;
        auto pauseButtonSize = Vec2 { safeAreaSize.x * 32 / 1920, safeAreaSize.x * 37.48 / 1920 };
        float64 pauseButtonItemWidth = pauseButtonSize.x * 0.323;

        auto& pauseButtonAttachUIData = frame.cache.attachUIDatas[EnumPhiLineAttachUI::Pause];

        frame.objects.push_back(PhiCalculatedFrame::CalculatedPoly::Make(
            { 0.0, 0.0 },
            { pauseButtonItemWidth, pauseButtonSize.y },
            pauseButtonAttachUIData.color,
            Transform2D()
                .translate(safeAreaPosition)
                .translate(pauseButtonPosition)
                .translate(pauseButtonAttachUIData.position)
                .scale(pauseButtonAttachUIData.scale)
                .rotateDegrees(pauseButtonAttachUIData.rotation)
        ));

        frame.objects.push_back(PhiCalculatedFrame::CalculatedPoly::Make(
            { pauseButtonSize.x - pauseButtonItemWidth, 0.0 },
            { pauseButtonItemWidth, pauseButtonSize.y },
            pauseButtonAttachUIData.color,
            Transform2D()
                .translate(safeAreaPosition)
                .translate(pauseButtonPosition)
                .translate(pauseButtonAttachUIData.position)
                .scale(pauseButtonAttachUIData.scale)
                .rotateDegrees(pauseButtonAttachUIData.rotation)
        ));

        if (combo >= 3) {
            frame.objects.push_back(processAttachUIText(PhiCalculatedFrame::CalculatedText {
                .text = std::to_string(combo),
                .position = toScreen({ safeAreaSize.x / 2, safeAreaSize.x * 0.027083 }),
                .anchor = { 0.5, 0.5 },
                .fontSize = safeAreaSize.x * 0.0393081
            }, EnumPhiLineAttachUI::ComboNumber));

            frame.objects.push_back(processAttachUIText(PhiCalculatedFrame::CalculatedText {
                .text = "AUTOPLAY",
                .position = toScreen({ safeAreaSize.x / 2, safeAreaSize.x * 0.0478125 }),
                .anchor = { 0.5, 0.0 },
                .fontSize = safeAreaSize.x * 0.0130208
            }, EnumPhiLineAttachUI::Combo));
        }

        uint64 score = chart.comboTimes.size() ? std::clamp<float64>(std::ceil((float64)1000000 / chart.comboTimes.size() * combo), 0, 1000000) : 1000000;
        frame.objects.push_back(processAttachUIText(PhiCalculatedFrame::CalculatedText {
            .text = std::format("{:07}", score),
            .position = toScreen({ safeAreaSize.x * (1 - ((float64)40 / 1920)), safeAreaSize.x * 0.01614583 }),
            .anchor = { 1.0, 0.0 },
            .fontSize = safeAreaSize.x * 0.0277778
        }, EnumPhiLineAttachUI::Score));
        
        frame.objects.push_back(processAttachUIText(PhiCalculatedFrame::CalculatedText {
            .text = chart.meta.title,
            .position = toScreen({ safeAreaSize.x * 0.0225, safeAreaSize.y - safeAreaSize.x * 0.0196875 }),
            .anchor = { 0.0, 1.0 },
            .fontSize = safeAreaSize.x * 0.018115942
        }, EnumPhiLineAttachUI::Name));
        
        frame.objects.push_back(processAttachUIText(PhiCalculatedFrame::CalculatedText {
            .text = chart.meta.difficulty,
            .position = toScreen({ safeAreaSize.x * 0.9775, safeAreaSize.y - safeAreaSize.x * 0.0196875 }),
            .anchor = { 1.0, 1.0 },
            .fontSize = safeAreaSize.x * 0.018115942
        }, EnumPhiLineAttachUI::Level));

        calculateExtra(true);
    }

    struct PhiTakeOverer {
        PhiTakeOverer() = default;
        PhiTakeOverer(const PhiTakeOverer&) = delete;
        PhiTakeOverer(PhiTakeOverer&&) = delete;
        PhiTakeOverer& operator=(const PhiTakeOverer&) = delete;
        PhiTakeOverer& operator=(PhiTakeOverer&&) = delete;

        static gsp<PhiTakeOverer> Make() {
            auto* tor = new PhiTakeOverer();
            return gsp<PhiTakeOverer>(tor);
        }

        struct NoteTextureDataLoaderConfig {
            EnumPhiNoteType type;
            bool isSimul;
        };

        struct NoteTextureDataLoaderResult {
            Data encoded;
            Vec2 cutPadding;
            bool cutPaddingIsPixel = true;
            bool ignoreCutPadding;
            Vec2 scaling = { 1.0, 1.0 };
        };

        using NoteTextureDataLoader = std::function<NoteTextureDataLoaderResult(const NoteTextureDataLoaderConfig&)>;
        NoteTextureDataLoader noteTextureDataLoader;

        using HitEffectDataLoader = std::function<std::vector<Data>()>;
        HitEffectDataLoader hitEffectDataLoader;

        using HitsoundDataLoader = std::function<Data(EnumPhiNoteType)>;
        HitsoundDataLoader hitsoundDataLoader;

        using StoryboardDataLoader = std::function<Data(const std::string&)>;
        StoryboardDataLoader storyboardDataLoader;

        using ShaderDataLoader = std::function<std::string(const std::string&)>;
        ShaderDataLoader shaderDataLoader;

        gsp<GL::GL33Context> glCtx;
        TakeOvererComponents::SharedComp sharedComp;
        GL::TextManager textManager;
        TakeOvererComponents::AudioManager audioManager;

        // 这个放在后面会因为析构顺序相反崩溃, 即先析构这个再析构 chart, chart 回来再用这个就炸了
        private:
        std::unordered_map<uint64, gsp<GL::TextureInfo>> storyboardTextures;
        public:

        PhiCalculateFrameConfig calcConfig;
        PhiChart chart;
        PhiCalculatedFrame calculatedFrame;

        void init() {
            gassert::assert(!!noteTextureDataLoader, "PhiTakeOverer: noteTextureDataLoader is not set");
            gassert::assert(!!hitEffectDataLoader, "PhiTakeOverer: hitEffectDataLoader is not set");
            gassert::assert(!!hitsoundDataLoader, "PhiTakeOverer: hitsoundDataLoader is not set");
            gassert::assert(!!storyboardDataLoader, "PhiTakeOverer: storyboardDataLoader is not set");
            gassert::assert(!!shaderDataLoader, "PhiTakeOverer: shaderDataLoader is not set");
            gassert::assert(!!glCtx, "PhiTakeOverer: glCtx is not set");

            textManager.glCtx = glCtx;

            sharedComp.check();
            textManager.check();
            audioManager.check();

            loadResources();
        }

        void loadIllustion(const Data& data) {
            auto decoded = sharedComp.textureDecoder(data);
            sharedComp.illustionTexture = glCtx->createTextureFromDecoded(decoded, true);
            bluredIllustionCache.key = -1.0;
        }

        void loadIllustion(const std::string& path) { loadIllustion(Data::MakeFromFile(path)); }

        struct MixBgmConfig {
            float64 musicVol = 1.0, sfxVol = 1.0;
            bool sfxRandshake = false;
        };

        gsp<DecodedAudio> mixFinalBgm(const PhiChart& chart, const MixBgmConfig& config) {
            if (!audioManager.bgmAudio) throw std::runtime_error("bgm is not loaded");

            auto result = audioManager.bgmAudio->copy();
            result->applyVolume(config.musicVol);
            
            std::mt19937 rng { std::random_device {} () };
            std::uniform_real_distribution<float64> sfxRandshakeDist { 0.0, 0.02 };

            for (const auto& line : chart.lines) {
                for (const auto& note : line.notes) {
                    if (note.isFake) continue;

                    float64 t = note.time + chart.meta.offset;
                    if (config.sfxRandshake) t += sfxRandshakeDist(rng);

                    auto sfx = hitsoundAudios.at(note.type);
                    result->overlapSecond(sfx, t, config.sfxVol);
                }
            }

            return result;
        }

        using ChartIniter = std::function<void(PhiChart&)>;

        struct LoadChartConfig {
            Data data;
            ChartIniter initer = [](PhiChart& chart) { chart.init(); };
            std::optional<Data> extraData;
        };

        TakeOvererComponents::LoadChartResultInfo loadChart(const LoadChartConfig& config) {
            TakeOvererComponents::LoadChartResultInfo resultInfo {};

            {
                gtime::Timer timer;

                try {
                    chart = loadPhiChartFromData(config.data);
                } catch (const std::exception& e) {
                    resultInfo.success = false;
                    resultInfo.error = e.what();
                    return resultInfo;
                }

                resultInfo.createObjectTook = timer.elapsed();
            }

            uint64 storyboardTextureId = 0;

            chart.storyboardAssets.clearTextures();

            chart.storyboardAssets.textureLoader = [&, this](const std::string& name) {
                auto data = storyboardDataLoader(name);
                auto decoded = sharedComp.textureDecoder(data);
                auto tex = glCtx->createTextureFromDecoded(decoded, true);
                auto id = storyboardTextureId++;
                storyboardTextures[id] = tex;
                return std::make_pair(id, Vec2 { (float64)decoded.width, (float64)decoded.height });
            };

            chart.storyboardAssets.textureDestroyer = [this](uint64 id) {
                storyboardTextures.erase(id);
            };

            chart.storyboardAssets.shaderPreloader = [this](const std::string& name, uint64 id) {
                auto shaderString = shaderDataLoader(name);

                if (shaderString.empty()) {
                    throw std::runtime_error("shader string is empty: " + name);
                }

                try {
                    auto prog = glCtx->createConfiguredProgram(GL::GL33Context::CreateProgramConfig {
                        .vertCode = R"(
#version 100

attribute vec2 inPosition;
attribute vec2 inTexCoord;

varying vec2 uv;

void main() {
    gl_Position = vec4(inPosition, 0.0, 1.0);
    uv = inTexCoord;
}
)",
                        .fragCode = shaderString,
                        .vertConfigurer = [](GL::ProgramInfo* prog, GL::VertexArrayInfo* vao, GL::BufferInfo* vbo) {
                            using namespace GL;
                            auto vaoGuard = vao->use();
                            auto vboGuard = vbo->use();
                            auto inPosition = prog->getAttribLocationPosition("inPosition");
                            auto inTexCoord = prog->getAttribLocationPosition("inTexCoord");
                            vaoGuard.enable(inPosition);
                            vaoGuard.enable(inTexCoord);
                            vaoGuard.pointer(inPosition, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));
                            vaoGuard.pointer(inTexCoord, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texCoord));
                        }
                    });

                    prog->fragConfig.textureUniformName = "screenTexture";
                    prog->fragConfig.colorUniformName = std::nullopt;
                    shaders[id] = prog;
                } catch (const std::exception& e) {
                    throw std::runtime_error("failed to load shader: " + name + "\n" + e.what());
                }

                auto defaultUnfiroms = PhiStoryboardHelpers::parseDefaultShaderUniforms(shaderString);
                shadersDefaultUniforms[id] = defaultUnfiroms;
            };

            shaders.clear();
            shadersDefaultUniforms.clear();

            if (config.extraData.has_value()) {
                chart.extra = loadPhiExtraFromJsonData(config.extraData.value(), chart.storyboardAssets);
            }

            {
                gtime::Timer timer;
                config.initer(chart);
                resultInfo.initTook = timer.elapsed();
            }

            return resultInfo;
        }

        struct RenderConfig {
            TakeOvererComponents::RenderConfigBase base;
        };

        struct RenderResultInfo {
            TakeOvererComponents::RenderResultInfoBase base;
        };

        RenderResultInfo& render(const RenderConfig& renderConfig) {
            calcConfig.songLength = audioManager.getBgmLength();
            calcConfig.backgroundTextureSize = { sharedComp.illustionTexture->width, sharedComp.illustionTexture->height };

            auto t = renderConfig.base.getTime(audioManager);

            {
                gtime::Timer timer;
                calculatePhiFrame(chart, t, calcConfig, calculatedFrame);
                renderResultInfoCache.base.calculatedTook = timer.elapsed();
            }

            gtime::Timer glOpsTimer;

            using namespace GL;

            auto illuTex = bluredIllustionCache.get(calculatedFrame.backgroundImageBlurRadius, [&](float64 radius) {
                auto tex = glCtx->createTexture();
                glCtx->copyTexture(sharedComp.illustionTexture.get(), tex.get());
                glCtx->gaussianBlurToTexture(tex.get(), radius);
                return tex;
            });

            glCtx->setViewport(calcConfig.screenSize.x, calcConfig.screenSize.y);
            glCtx->gl.glClearColor(0.0, 0.0, 0.0, 0.0);
            glCtx->gl.glClear(GL_COLOR_BUFFER_BIT);

            auto cvs = GL33Canvas::Make(glCtx.get());

            cvs.drawRect({
                .position = { calculatedFrame.unsafeBackgroundRect.x, calculatedFrame.unsafeBackgroundRect.y },
                .size = { calculatedFrame.unsafeBackgroundRect.w, calculatedFrame.unsafeBackgroundRect.h },
                .color = GLvec4::Gray(1.0 - calculatedFrame.unsafeAreaDim),
                .texture = illuTex.get()
            });

            glCtx->setViewport(
                calculatedFrame.objectsClipRect.x, calculatedFrame.objectsClipRect.y,
                calculatedFrame.objectsClipRect.w, calculatedFrame.objectsClipRect.h
            );

            cvs.drawRect({
                .position = { calculatedFrame.backgroundRect.x, calculatedFrame.backgroundRect.y },
                .size = { calculatedFrame.backgroundRect.w, calculatedFrame.backgroundRect.h },
                .color = GLvec4::Gray(1.0 - calculatedFrame.backgroundDim),
                .texture = illuTex.get()
            });

            for (auto& obj : calculatedFrame.objects) {
                if (TakeOvererComponents::renderSharedObject(obj, glCtx, cvs, textManager)) {
                    continue;
                }
                    
                if (std::holds_alternative<PhiCalculatedFrame::CalculatedNote>(obj)) {
                    auto& note = std::get<PhiCalculatedFrame::CalculatedNote>(obj);
                    auto& img = note.isSimul ? noteTextures[note.type].second : noteTextures[note.type].first;
                    auto& imgInfo = note.isSimul ? calcConfig.noteTextureInfos[note.type].simul : calcConfig.noteTextureInfos[note.type].single;

                    cvs.save();
                    cvs.translate(note.position);
                    cvs.rotateDegrees(note.rotation);

                    auto mesh = glCtx->requestMesh(6 * 3);
                    mesh.color = note.color;
                    mesh.texture = img.get();

                    mesh.addRect(
                        { -note.width / 2, 0.0 }, { note.width, note.head },
                        GLvec2 { 0.0, imgInfo.textureSize.y - imgInfo.cutPadding.x } / imgInfo.textureSize,
                        GLvec2 { imgInfo.textureSize.x, imgInfo.cutPadding.x } / imgInfo.textureSize
                    );

                    mesh.addRect(
                        { -note.width / 2, -note.body },
                        { note.width, note.body },
                        GLvec2 { 0.0, imgInfo.cutPadding.y } / imgInfo.textureSize,
                        GLvec2 { imgInfo.textureSize.x, imgInfo.textureSize.y - imgInfo.cutPadding.sum() } / imgInfo.textureSize
                    );

                    mesh.addRect(
                        { -note.width / 2, -note.body - note.tail },
                        { note.width, note.tail },
                        GLvec2 { 0.0, 0.0 },
                        GLvec2 { imgInfo.textureSize.x, imgInfo.cutPadding.y } / imgInfo.textureSize
                    );

                    cvs.drawMesh(mesh);
                    cvs.restore();
                } else if (std::holds_alternative<PhiCalculatedFrame::CalculatedStoryboardTexture>(obj)) {
                    auto& sbTexture = std::get<PhiCalculatedFrame::CalculatedStoryboardTexture>(obj);
                    auto& img = storyboardTextures[sbTexture.texture];

                    cvs.save();
                    cvs.translate(sbTexture.position);
                    cvs.rotateDegrees(sbTexture.rotation);
                    cvs.scale(sbTexture.scale);
                    cvs.drawRect({
                        .position = -sbTexture.size * sbTexture.anchor,
                        .size = sbTexture.size,
                        .color = sbTexture.color,
                        .texture = img.get()
                    });
                    cvs.restore();
                } else if (std::holds_alternative<PhiCalculatedFrame::CalculatedHitEffectTexture>(obj)) {
                    auto& effect = std::get<PhiCalculatedFrame::CalculatedHitEffectTexture>(obj);
                    auto& img = hitEffectTextures[std::clamp<uint64>(effect.progress * hitEffectTextures.size(), 0, hitEffectTextures.size() - 1)];

                    cvs.save();
                    cvs.translate(effect.position);
                    cvs.rotateDegrees(effect.rotation);
                    cvs.drawRect({
                        .position = -effect.size / 2,
                        .size = effect.size,
                        .color = effect.color,
                        .texture = img.get()
                    });
                    cvs.restore();
                } else if (std::holds_alternative<PhiCalculatedFrame::CalculatedShader>(obj)) {
                    auto& shader = std::get<PhiCalculatedFrame::CalculatedShader>(obj);
                    auto& prog = shaders[shader.id];
                    if (!prog) continue;

                    {
                        auto guard = prog->use();

                        for (auto& [k, v] : shadersDefaultUniforms[shader.id]) v.setToGlLocation(prog->getUniformLocation(k));
                        for (auto& [k, v] : shader.uniforms) v.setToGlLocation(prog->getUniformLocation(k));

                        prog->getUniformLocation("screenSize").setf(calcConfig.screenSize.x, calcConfig.screenSize.y);
                        prog->getUniformLocation("time").setf(t);
                    }

                    auto mesh = glCtx->requestMesh(6);
                    mesh.program = prog.get();
                    mesh.color = GLvec4::White();
                    glCtx->renderToDrawFbo(calcConfig.screenSize.x, calcConfig.screenSize.y, mesh);
                }
            }

            if (renderConfig.base.flushGl) {
                glCtx->gl.glFlush();
            }

            renderResultInfoCache.base.glOperationsTook = glOpsTimer.elapsed();

            if (!renderConfig.base.disableHitsound) {
                for (auto& [type, count] : calculatedFrame.hitsounds) {
                    audioManager.playSfx(hitsoundAudios.at(type), count);
                }
            }

            return renderResultInfoCache;
        }

        private:
        SKVCache<float64, gsp<GL::TextureInfo>> bluredIllustionCache;
        std::unordered_map<EnumPhiNoteType, std::pair<gsp<GL::TextureInfo>, gsp<GL::TextureInfo>>> noteTextures;
        std::vector<gsp<GL::TextureInfo>> hitEffectTextures;
        std::unordered_map<EnumPhiNoteType, gsp<DecodedAudio>> hitsoundAudios;
        RenderResultInfo renderResultInfoCache;
        std::unordered_map<uint64, gsp<GL::ProgramInfo>> shaders;
        std::unordered_map<uint64, std::unordered_map<std::string, PhiShaderUniform>> shadersDefaultUniforms;

        void loadResources() {
            noteTextures.clear();
            hitEffectTextures.clear();
            storyboardTextures.clear();

            for (const auto type : {
                EnumPhiNoteType::Tap, EnumPhiNoteType::Drag,
                EnumPhiNoteType::Flick, EnumPhiNoteType::Hold
            }) {
                noteTextures[type] = { nullptr, nullptr };
                calcConfig.noteTextureInfos[type] = {};

                for (const auto isSimul : { false, true }) {
                    auto loadResult = noteTextureDataLoader(NoteTextureDataLoaderConfig {
                        .type = type,
                        .isSimul = isSimul
                    });

                    auto decoded = sharedComp.textureDecoder(loadResult.encoded);
                    auto tex = glCtx->createTextureFromDecoded(decoded, true);
                    if (!loadResult.cutPaddingIsPixel) loadResult.cutPadding *= decoded.height;
                    if (loadResult.ignoreCutPadding) loadResult.cutPadding = Vec2 { (float64)decoded.height, (float64)decoded.height } / 2;

                    if (!isSimul) noteTextures[type].first = tex;
                    else noteTextures[type].second = tex;

                    PhiCalculateFrameConfig::NoteTextureInfo::Item item {
                        .textureSize = Vec2 { (float64)decoded.width, (float64)decoded.height },
                        .cutPadding = loadResult.cutPadding,
                        .scaling = loadResult.scaling
                    };

                    if (!isSimul) calcConfig.noteTextureInfos[type].single = item;
                    else calcConfig.noteTextureInfos[type].simul = item;
                }

                auto& info = calcConfig.noteTextureInfos[type];
                auto simulScale = (float64)info.simul.textureSize.x / info.single.textureSize.x;
                info.simul.scaling = { simulScale, simulScale };
            }

            auto hitEffectDatas = hitEffectDataLoader();
            for (const auto& data : hitEffectDatas) {
                auto decoded = sharedComp.textureDecoder(data);
                auto tex = glCtx->createTextureFromDecoded(decoded, true);
                hitEffectTextures.push_back(tex);
            }

            for (const auto type : {
                EnumPhiNoteType::Tap, EnumPhiNoteType::Drag,
                EnumPhiNoteType::Flick, EnumPhiNoteType::Hold
            }) {
                auto data = hitsoundDataLoader(type);
                hitsoundAudios[type] = audioManager.decodeAndCheck(data);
            }
        }
    };

    enum class EnumMilEventType : uint64 {
        PositionX, PositionY,
        Transparency, Size, Rotation,
        FlowSpeed,
        RelativeX, RelativeY,
        LineBodyTransparency, LineHeadTransparency,
        StoryBoardWidth, StoryBoardHeight,
        Speed,
        WholeTransparency,
        StoryBoardLeftBottomX, StoryBoardLeftBottomY,
        StoryBoardRightBottomX, StoryBoardRightBottomY,
        StoryBoardLeftTopX, StoryBoardLeftTopY,
        StoryBoardRightTopX, StoryBoardRightTopY,
        Color,
        VisibleArea,
        MAX = VisibleArea + 1
    };

    enum class EnumMilObjectType : uint64 {
        Line, Note, Storyboard,
        MAX = Storyboard + 1
    };

    enum class EnumMilNoteType {
        Hit, Drag
    };

    enum class EnumMilFinalNoteType {
        Tap, Hold, Drag
    };

    enum class EnumMilStoryboardType {
        Picture, Text
    };

    enum class EnumMilStoryboardLayer {
        Background, Normal, Foreground
    };

    struct MilEventTypeHelper {
        static EnumMilEventType FromInt(uint64 type) {
            if (type == 0) return EnumMilEventType::PositionX;
            if (type == 1) return EnumMilEventType::PositionY;
            if (type == 2) return EnumMilEventType::Transparency;
            if (type == 3) return EnumMilEventType::Size;
            if (type == 4) return EnumMilEventType::Rotation;
            if (type == 5) return EnumMilEventType::FlowSpeed;
            if (type == 6) return EnumMilEventType::RelativeX;
            if (type == 7) return EnumMilEventType::RelativeY;
            if (type == 8) return EnumMilEventType::LineBodyTransparency;
            if (type == 9) return EnumMilEventType::LineHeadTransparency;
            if (type == 10) return EnumMilEventType::StoryBoardWidth;
            if (type == 11) return EnumMilEventType::StoryBoardHeight;
            if (type == 12) return EnumMilEventType::Speed;
            if (type == 13) return EnumMilEventType::WholeTransparency;
            if (type == 14) return EnumMilEventType::StoryBoardLeftBottomX;
            if (type == 15) return EnumMilEventType::StoryBoardLeftBottomY;
            if (type == 16) return EnumMilEventType::StoryBoardRightBottomX;
            if (type == 17) return EnumMilEventType::StoryBoardRightBottomY;
            if (type == 18) return EnumMilEventType::StoryBoardLeftTopX;
            if (type == 19) return EnumMilEventType::StoryBoardLeftTopY;
            if (type == 20) return EnumMilEventType::StoryBoardRightTopX;
            if (type == 21) return EnumMilEventType::StoryBoardRightTopY;
            if (type == 22) return EnumMilEventType::Color;
            if (type == 23) return EnumMilEventType::VisibleArea;
            return EnumMilEventType::PositionX;
        }
    };

    struct MilObjectTypeHelper {
        static EnumMilObjectType FromInt(uint64 type) {
            if (type == 0) return EnumMilObjectType::Line;
            if (type == 1) return EnumMilObjectType::Note;
            if (type == 2) return EnumMilObjectType::Storyboard;
            return EnumMilObjectType::Line;
        }
    };

    struct MilNoteTypeHelper {
        static EnumMilNoteType FromInt(uint64 type) {
            if (type == 0) return EnumMilNoteType::Hit;
            if (type == 1) return EnumMilNoteType::Drag;
            return EnumMilNoteType::Hit;
        }
    };

    struct MilStoryboardTypeHelper {
        static EnumMilStoryboardType FromInt(uint64 type) {
            if (type == 0) return EnumMilStoryboardType::Picture;
            if (type == 1) return EnumMilStoryboardType::Text;
            return EnumMilStoryboardType::Picture;
        }
    };

    struct MilStoryboardLayerHelper {
        static EnumMilStoryboardLayer FromInt(uint64 type) {
            if (type == 0) return EnumMilStoryboardLayer::Background;
            if (type == 1) return EnumMilStoryboardLayer::Normal;
            if (type == 2) return EnumMilStoryboardLayer::Foreground;
            return EnumMilStoryboardLayer::Background;
        }
    };

    struct MilMeta {
        std::string title;
        std::string composer;
        std::string artist;
        std::string charter;
        std::string difficultyName;
        float64 difficultyValue;

        enum class NoteFlowSpeedBehavior {
            Override, Multiply, Add
        };

        NoteFlowSpeedBehavior noteFlowSpeedBehavior;

        Vec2 worldOrigin, worldViewport;
        float64 speedUnit;
        float64 holdDisappearTime = 0.2;

        std::string getFinalDifficultyString() {
            auto ret = difficultyName + " ";
            ret += std::to_string((int64)difficultyValue);
            if (std::fmod(difficultyValue, 1.0) != 0.0) ret += "+";
            return ret;
        }
    };

    struct MilBPMEvent {
        float64 time;
        float64 bpm;
    };

    struct MilEventLayerIndexs {
        static constexpr uint64 UNIT = 1000000;

        static constexpr uint64 DEFAULT = UNIT * 1;
    };

    struct MilEvent {
        Vec2 timeZone;
        Vec2 valueZone;
        EnumMilEventType type;

        float64 (* easingFunc)(void*, float64);
        float64 (* easingIntFunc)(void*, float64);
        void* easingFuncContext;
        uint64 index;

        float64 cumulativeValueAtStart;

        static float64 getDefaultValue(EnumMilObjectType objType, EnumMilEventType eventType) {
            static std::unordered_map<EnumMilObjectType, std::unordered_map<EnumMilEventType, float64>> defaultValues = {
                { EnumMilObjectType::Line, {
                    { EnumMilEventType::PositionY, -350 },
                    { EnumMilEventType::Transparency, 1 },
                    { EnumMilEventType::Size, 1 },
                    { EnumMilEventType::Rotation, 90 },
                    { EnumMilEventType::FlowSpeed, 1 },
                    { EnumMilEventType::LineBodyTransparency, 1 },
                    { EnumMilEventType::LineHeadTransparency, 1 },
                    { EnumMilEventType::Speed, 1 },
                    { EnumMilEventType::WholeTransparency, 1 },
                    { EnumMilEventType::VisibleArea, (float64)2500 / 1080 }
                } },
                { EnumMilObjectType::Note, {
                    { EnumMilEventType::Transparency, 1 },
                    { EnumMilEventType::Size, 1 },
                    { EnumMilEventType::FlowSpeed, 1 },
                } },
                { EnumMilObjectType::Storyboard, {
                    { EnumMilEventType::Size, 1 },
                    { EnumMilEventType::StoryBoardWidth, 1 },
                    { EnumMilEventType::StoryBoardHeight, 1 },
                    { EnumMilEventType::StoryBoardLeftBottomX, -0.5 },
                    { EnumMilEventType::StoryBoardLeftBottomY, -0.5 },
                    { EnumMilEventType::StoryBoardRightBottomX, 0.5 },
                    { EnumMilEventType::StoryBoardRightBottomY, -0.5 },
                    { EnumMilEventType::StoryBoardLeftTopX, -0.5 },
                    { EnumMilEventType::StoryBoardLeftTopY, 0.5 },
                    { EnumMilEventType::StoryBoardRightTopX, 0.5 },
                    { EnumMilEventType::StoryBoardRightTopY, 0.5 },
                } }
            };

            return defaultValues[objType][eventType];
        }

        float64 getProgressAtTime(float64 t) noexcept {
            if (timeZone.isZeroZone()) return 1.0;
            return std::clamp((t - timeZone.x) / (timeZone.y - timeZone.x), 0.0, 1.0);
        }

        float64 valueAtTime(float64 t) noexcept {
            auto p = getProgressAtTime(t);

            if (hasValueEasing()) {
                p = easingFunc(easingFuncContext, p);
            }

            return valueZone.x + p * (valueZone.y - valueZone.x);
        }

        float64 getIntegralValue(float64 t) noexcept {
            auto p = getProgressAtTime(t);
            float64 iv = p * p / 2.0;

            if (hasIntEasing()) {
                iv = easingIntFunc(easingFuncContext, p);
            }

            float64 res = (timeZone.y - timeZone.x) * (valueZone.x * p + (valueZone.y - valueZone.x) * iv);
            if (t > timeZone.y) res += valueZone.y * (t - timeZone.y);
            if (t < timeZone.x) res -= valueZone.x * (timeZone.x - t);
            return res;
        }

        private:
        bool hasValueEasing() const noexcept { return easingFunc != nullptr; }
        bool hasIntEasing() const noexcept { return easingIntFunc != nullptr; }
    };

    struct MilAnimGroup {
        std::vector<MilEvent> events[(uint64)EnumMilEventType::MAX];
        EnumMilObjectType objType;

        void addEvent(const MilEvent& e) { events[(uint64)e.type].push_back(e); }

        void init() {
            std::ranges::fill(lastUpdatedTimes, -std::numeric_limits<float64>::infinity());

            for (uint64 i = 0; i < (uint64)EnumMilEventType::MAX; i++) {
                auto& typedEvents = events[i];

                std::sort(typedEvents.begin(), typedEvents.end(), [](const auto& a, const auto& b) {
                    if (a.timeZone.x != b.timeZone.x) return a.timeZone.x < b.timeZone.x;
                    if (a.timeZone.y != b.timeZone.y) return a.timeZone.y < b.timeZone.y;
                    return a.index < b.index;
                });

                if (typedEvents.empty()) {
                    auto value = MilEvent::getDefaultValue(objType, (EnumMilEventType)i);
                    currentValues[i] = value;
                    currentValueZones[i] = Vec2(value);
                }
            }

            initSpeedCumul();
        }

        void updateType(uint64 type, float64 t) noexcept {
            auto& typedEvents = events[type];

            if (typedEvents.empty()) {
                if (type == (uint64)EnumMilEventType::Speed) {
                    currentValues[type] = MilEvent::getDefaultValue(objType, (EnumMilEventType)type) * t;
                }

                return;
            }

            if (lastUpdatedTimes[type] == t) return;
            if (lastUpdatedTimes[type] > t) rewindTo(type, t);
            
            while (shouldAdvanceToNext(type, t)) currentIndexs[type]++;

            auto& e = typedEvents[currentIndexs[type]];

            if (type == (uint64)EnumMilEventType::Speed) {
                currentValues[type] = e.cumulativeValueAtStart + e.getIntegralValue(t);
            } else {
                currentValues[type] = e.valueAtTime(t);
            }

            currentValueZones[type] = e.valueZone;
            lastUpdatedTimes[type] = t;
        }

        float64 get(EnumMilEventType type) const noexcept {
            return currentValues[(uint64)type];
        }

        std::optional<float64> getAlwaysValue(EnumMilEventType type) noexcept {
            auto& typedEvents = events[(uint64)type];
            if (typedEvents.empty()) return MilEvent::getDefaultValue(objType, type);

            if (type == EnumMilEventType::Speed) {
                if (typedEvents.size() == 1 && typedEvents[0].valueZone.isZeroZone()) {
                    return typedEvents[0].valueZone.x;
                }

                return std::nullopt;
            }

            float64 fixedValue = typedEvents[0].valueZone.x;
            for (auto& e : typedEvents) {
                if (e.timeZone.isZeroZone()) {
                    if (e.valueZone.y != fixedValue) {
                        return std::nullopt;
                    }

                    continue;
                }

                if (!e.valueZone.isZeroZone() || fixedValue != e.valueZone.x) {
                    return std::nullopt;
                }
            }

            return fixedValue;
        }

        bool has(EnumMilEventType type) const noexcept {
            return !events[(uint64)type].empty();
        }

        Vec2 get_zone(EnumMilEventType type) const noexcept {
            return currentValueZones[(uint64)type];
        }

        private:
        float64 lastUpdatedTimes[(uint64)EnumMilEventType::MAX];
        uint64 currentIndexs[(uint64)EnumMilEventType::MAX];
        float64 currentValues[(uint64)EnumMilEventType::MAX];
        Vec2 currentValueZones[(uint64)EnumMilEventType::MAX];

        void initSpeedCumul() {
            auto& speedEvents = events[(uint64)EnumMilEventType::Speed];
            if (speedEvents.empty()) return;

            float64 cumulativeValue = speedEvents[0].timeZone.x * speedEvents[0].valueZone.x;

            for (uint64 i = 0; i < speedEvents.size(); i++) {
                auto& e = speedEvents[i];
                e.cumulativeValueAtStart = cumulativeValue;

                if (i < speedEvents.size() - 1) {
                    cumulativeValue += e.getIntegralValue(speedEvents[i + 1].timeZone.x);
                }
            }
        }

        bool shouldAdvanceToNext(uint64 type, float64 t) const noexcept {
            return (
                currentIndexs[type] < events[type].size() - 1
                && events[type][currentIndexs[type]].timeZone.y <= t
                && events[type][currentIndexs[type] + 1].timeZone.x <= t
            );
        }

        void rewindTo(uint64 type, float64 t) noexcept {
            while (!shouldAdvanceToNext(type, t) && currentIndexs[type] > 0) {
                currentIndexs[type]--;
            }
        }
    };

    struct MilAnimator {
        using ObjDesc = std::pair<EnumMilObjectType, uint64>;

        ObjectIndexGenerator<ObjDesc> indexGen;
        std::unordered_map<uint64, MilAnimGroup> groups;

        MilAnimGroup& requestGroup(const ObjDesc& obj) {
            auto& ret = groups.try_emplace(indexGen.get(obj), MilAnimGroup { }).first->second;
            ret.objType = obj.first;
            return ret;
        }

        void addEvent(const ObjDesc& obj, const MilEvent& e) {
            requestGroup(obj).addEvent(e);
        }

        void init() {
            for (auto& [_, group] : groups) {
                group.init();
            }
        }

        float64 get(const ObjDesc& obj, float64 t, EnumMilEventType type) noexcept {
            auto group_it = groups.find(obj.second);

            if (group_it == groups.end()) {
                auto value = MilEvent::getDefaultValue(obj.first, type);
                if (type == EnumMilEventType::Speed) value *= t;
                return value;
            }

            auto& group = group_it->second;
            group.updateType((uint64)type, t);
            return group.get(type);
        }

        template <typename T>
        float64 get(T& obj, float64 t, EnumMilEventType type) noexcept {
            return get({ T::ObjType, obj.indexer.get() }, t, type);
        }

        template <typename T>
        std::optional<float64> getNoteAnimHash(T& obj) {
            HashBucket hash;

            auto group_it = groups.find(obj.indexer.get());

            for (const auto type : {
                EnumMilEventType::PositionX,
                EnumMilEventType::PositionY,
                EnumMilEventType::Size,
                EnumMilEventType::Rotation,
                EnumMilEventType::FlowSpeed,
                EnumMilEventType::RelativeX,
                EnumMilEventType::RelativeY,
                EnumMilEventType::Speed
            }) {
                if (group_it == groups.end()) {
                    hash.submitNumber(MilEvent::getDefaultValue(T::ObjType, type));
                } else {
                    auto v = group_it->second.getAlwaysValue(type);
                    if (!v.has_value()) return std::nullopt;
                    hash.submitNumber(v.value());
                }
            }

            return hash.hash;
        }

        bool has(const ObjDesc& obj, EnumMilEventType type) const noexcept {
            auto group_it = groups.find(obj.second);
            if (group_it == groups.end()) return false;

            return group_it->second.has(type);
        }

        template <typename T>
        bool has(T& obj, EnumMilEventType type) const noexcept {
            return has({ T::ObjType, obj.indexer.get() }, type);
        }

        Vec2 get_zone(const ObjDesc& obj, float64 t, EnumMilEventType type) noexcept {
            auto group_it = groups.find(obj.second);
            if (group_it == groups.end()) return Vec2(MilEvent::getDefaultValue(obj.first, type));

            auto& group = group_it->second;
            group.updateType((uint64)type, t);
            return group.get_zone(type);
        }

        template <typename T>
        Vec2 get_zone(T& obj, float64 t, EnumMilEventType type) noexcept {
            return get_zone({ T::ObjType, obj.indexer.get() }, t, type);
        }
    };

    // type, isHold, isSimul, isAlwaysPerfect
    using MilNoteTextureDesc = std::tuple<EnumMilNoteType, bool, bool, bool>;
    static constexpr uint64 MilNoteTextureDescAttrType = 0;
    static constexpr uint64 MilNoteTextureDescAttrIsHold = 1;
    static constexpr uint64 MilNoteTextureDescAttrIsSimul = 2;
    static constexpr uint64 MilNoteTextureDescAttrIsAlwaysPerfect = 3;

    bool fallbackMilNoteTextureDesc(MilNoteTextureDesc& desc) {
        if (std::get<MilNoteTextureDescAttrIsAlwaysPerfect>(desc)) {
            std::get<MilNoteTextureDescAttrIsAlwaysPerfect>(desc) = false;
            return true;
        }

        if (std::get<MilNoteTextureDescAttrIsHold>(desc)) {
            std::get<MilNoteTextureDescAttrIsHold>(desc) = false;
            return true;
        }

        if (std::get<MilNoteTextureDescAttrIsSimul>(desc)) {
            std::get<MilNoteTextureDescAttrIsSimul>(desc) = false;
            return true;
        }

        return false;
    }

    struct MilNote {
        ObjectIndexer indexer;
        static constexpr auto ObjType = EnumMilObjectType::Note;

        struct State {
            float64 lastUpdateTime;
            bool playedHitsound;

            void timeUpdated(const MilNote& note, float64 t) noexcept {
                if (lastUpdateTime > t) {
                    playedHitsound = note.timeZone.x < t;
                }

                lastUpdateTime = t;
            }

            bool onPlayHitsound() noexcept {
                if (!playedHitsound) {
                    playedHitsound = true;
                    return true;
                }

                return false;
            }
        };

        EnumMilNoteType type;
        Vec2 timeZone;
        bool isFake, isAlwaysPerfect;

        uint64 lineIndex;
        Vec2 floorPosition;
        bool isSimul;
        MilNoteTextureDesc textureDesc;
        EnumMilFinalNoteType finalType;

        State state;

        void init(MilAnimator& animator) {
            floorPosition = { getFloorPositionAt(timeZone.x, animator), getFloorPositionAt(timeZone.y, animator) };
            textureDesc = MilNoteTextureDesc(type, isHold(), isSimul, isAlwaysPerfect);

            if (type == EnumMilNoteType::Hit) {
                finalType = isHold() ? EnumMilFinalNoteType::Hold : EnumMilFinalNoteType::Tap;
            } else {
                finalType = EnumMilFinalNoteType::Drag;
            }
        }

        float64 getFloorPositionAt(float64 t, MilAnimator& animator) noexcept {
            return animator.get({ EnumMilObjectType::Line, lineIndex }, t, EnumMilEventType::Speed) + animator.get(*this, t, EnumMilEventType::Speed);
        }

        bool isHold() noexcept {
            return !timeZone.isZeroZone() && type == EnumMilNoteType::Hit;
        }
    };

    struct MilNoteGroup {
        struct State {
            float64 lastUpdateTime;
            uint64 firstNoteIndex;

            void timeUpdated(float64 t) noexcept {
                if (lastUpdateTime > t) {
                    firstNoteIndex = 0;
                }

                lastUpdateTime = t;
            }

            void passedNoteIndex(uint64 index) noexcept {
                if (firstNoteIndex == index) {
                    firstNoteIndex++;
                }
            }
        };

        std::vector<uint64> indexs;
        bool breakable = true;

        State state;
    };

    struct MilLine {
        ObjectIndexer indexer;
        static constexpr auto ObjType = EnumMilObjectType::Line;

        std::vector<MilNote> notes;

        std::vector<MilNoteGroup> noteGroups;

        void init(MilAnimator& animator) {
            std::stable_sort(notes.begin(), notes.end(), [](const auto& a, const auto& b) {
                return a.timeZone.x < b.timeZone.x;
            });

            noteGroups.emplace_back().breakable = false;
            std::unordered_map<uint64, uint64> noteGroupMap;

            for (uint64 i = 0; i < notes.size(); i++) {
                auto& note = notes[i];
                note.lineIndex = indexer.get();
                note.init(animator);
                
                auto hash = animator.getNoteAnimHash(note);
                if (hash.has_value()) {
                    if (!noteGroupMap.contains(hash.value())) {
                        noteGroups.emplace_back();
                        noteGroupMap[hash.value()] = noteGroups.size() - 1;
                    }

                    auto& group = noteGroups[noteGroupMap[hash.value()]];
                    group.indexs.push_back(i);
                } else noteGroups[0].indexs.push_back(i);
            }
        }
    };

    struct MilStoryboardObject {
        ObjectIndexer indexer;
        static constexpr auto ObjType = EnumMilObjectType::Storyboard;

        EnumMilStoryboardType type;
        std::string data;
        EnumMilStoryboardLayer layer;
    };

    struct MilStoryboardAssets {
        static constexpr uint64 kColorIndexOffset = 1;

        std::vector<Color> colors;

        Vec2 requestColorPair(const Color& start, const Color& end) {
            Vec2 valueZone;
            if (colors.empty() || colors[colors.size() - 1] != start) colors.push_back(start);
            valueZone.x = colors.size() - 1;
            if (colors.empty() || colors[colors.size() - 1] != end) colors.push_back(end);
            valueZone.y = colors.size() - 1;
            return valueZone + kColorIndexOffset;
        }

        Color getColor(float64 index, const Vec2& valueZone) noexcept {
            if (valueZone.x < kColorIndexOffset) return Color::White();

            auto start = colors[(uint64)valueZone.x - kColorIndexOffset];
            auto end = colors[(uint64)valueZone.y - kColorIndexOffset];
            auto p = index - valueZone.x;
            return start * (1.0 - p) + end * p;
        }
    };

    struct MilHitEffectItem {
        struct Particle {
            float64 dt;
            float64 rotate;
            float64 initialSpeed;
            float64 initialSize;
            Vec2 scale;
            float64 gravCoeff;

            float64 getRadius(float64 p) const noexcept {
                return p * initialSpeed * initialSpeed * (p * p / 3.0 - p + 1);
            }

            float64 getDeltaY(float64 p) const noexcept {
                return p * p * gravCoeff * 0.025;
            }

            Vec2 getScale(float64 p) const noexcept {
                return Vec2 {
                    std::pow(p + 1, -scale.x) * 1.34,
                    std::pow(p + 1, -scale.y) * 0.25
                } / (p + 1);
            }
        };

        float64 time;
        float64 texRotation;
        uint64 lineIndex, noteIndex;
        std::vector<Particle> particles;

        float64 getEndTime(float64 duration) const noexcept {
            return (particles.empty() ? time : time + particles.back().dt) + duration;
        }
    };

    struct MilChart {
        struct State {
            float64 lastUpdateTime;
            uint64 firstHitEffectIndex;

            TimeBasedAnim scoreAnim = { 
                .duration = 0.15
            };

            TimeBasedAnim comboScaleAnim = {
                .duration = 0.15,
                .easing = [](auto, float64 t) { return 1.0 + 0.07 * std::sin(t * std::numbers::pi); }
            };

            void timeUpdated(float64 t) noexcept {
                if (lastUpdateTime > t) {
                    firstHitEffectIndex = 0;
                    scoreAnim.reset();
                }

                lastUpdateTime = t;
            }

            void passedHitEffectIndex(uint64 index) noexcept {
                if (firstHitEffectIndex == index) {
                    firstHitEffectIndex++;
                }
            }
        };

        struct UserOptions {
            float64 noteScaling = 1.0;
            float64 flowSpeed = 1.66;

            float64 backgroundDim = 0.8;

            float64 hitEffectDuration = 0.5;

            ColorLink particleRGBColor = {
                .steps = {
                    { 0.0,  Color { 142, 197, 252 } / 255 },
                    { 0.75, Color { 162, 66,  255 } / 255 }
                }
            };

            ColorLink particleAlphaColor = {
                .steps = {
                    { 0.0,   Color { 0, 0, 0, 0 } },
                    { 0.128, Color { 0, 0, 0, 1 } },
                    { 0.805, Color { 0, 0, 0, 1 } },
                    { 1.0,   Color { 0, 0, 0, 0 } }
                }
            };
        };

        MilMeta meta;
        std::vector<MilLine> lines;
        std::vector<MilStoryboardObject> storyboardObjects;
        MilAnimator animator;
        MilStoryboardAssets storyboardAssets;
        
        std::vector<MilHitEffectItem> hitEffects;
        std::vector<float64> comboTimes;
        uint64 rawHash;

        UserOptions options;

        State state;

        void init() {
            animator.init();

            initSimulNote();

            for (auto& line : lines) {
                line.init(animator);
            }

            initHitEffects();
            initPlayemntInfo();
        }

        template <typename T>
        Vec2 getObjectPosition(float64 t, T& obj, const Vec2& screenSize) {
            Vec2 pos = {
                animator.get(obj, t, EnumMilEventType::PositionX) + animator.get(obj, t, EnumMilEventType::RelativeX),
                animator.get(obj, t, EnumMilEventType::PositionY) + animator.get(obj, t, EnumMilEventType::RelativeY)
            };

            return (pos - meta.worldOrigin) / meta.worldViewport * screenSize;
        }

        struct NoteFrameInfo {
            Vec2 headPosition, tailPosition;
            bool isArrived;
            float64 textureRotation, speedVectorRotation;
            Vec2 scale;
            Color color;

            void setHidden() noexcept {
                color.a = 0.0;
            }
        };

        NoteFrameInfo getNoteFrameInfo(
            MilLine& line, MilNote& note,
            float64 time, const Vec2& screenSize
        ) noexcept {
            NoteFrameInfo info {};

            auto linePosition = getObjectPosition(time, line, screenSize);
            auto lineRotation = animator.get(line, time, EnumMilEventType::Rotation);
            auto lineFlowSpeed = animator.get(line, time, EnumMilEventType::FlowSpeed);
            auto lineSize = animator.get(line, time, EnumMilEventType::Size);
            auto lineWholeAlpha = animator.get(line, time, EnumMilEventType::WholeTransparency);
            auto lineVisibleArea = animator.get(line, time, EnumMilEventType::VisibleArea);
            auto noteSize = animator.get(note, time, EnumMilEventType::Size);
            auto noteAlpha = animator.get(note, time, EnumMilEventType::Transparency);
            auto noteRotation = animator.get(note, time, EnumMilEventType::Rotation);
            auto noteColorIndex = animator.get(note, time, EnumMilEventType::Color);
            auto noteColorIndexZone = animator.get_zone(note, time, EnumMilEventType::Color);
            auto noteColor = storyboardAssets.getColor(noteColorIndex, noteColorIndexZone);
            
            Transform2D lineTransform {};
            lineTransform.translate(linePosition);
            lineTransform.rotateDegrees(90 - lineRotation);
            lineTransform.scale(screenSize / meta.worldViewport.abs());
            lineTransform.scale(lineSize);
            lineTransform.scale(1.0, -1.0);

            info.isArrived = time >= note.timeZone.x;

            auto finalFlowSpeed = lineFlowSpeed;
            if (meta.noteFlowSpeedBehavior == MilMeta::NoteFlowSpeedBehavior::Override) {
                if (animator.has(note, EnumMilEventType::FlowSpeed)) {
                    finalFlowSpeed = animator.get(note, time, EnumMilEventType::FlowSpeed);
                }
            } else if (meta.noteFlowSpeedBehavior == MilMeta::NoteFlowSpeedBehavior::Multiply) {
                finalFlowSpeed *= animator.get(note, time, EnumMilEventType::FlowSpeed);
            } else if (meta.noteFlowSpeedBehavior == MilMeta::NoteFlowSpeedBehavior::Add) {
                finalFlowSpeed += animator.get(note, time, EnumMilEventType::FlowSpeed);
            }

            auto noteFloorPosition = (note.floorPosition - note.getFloorPositionAt(std::min(time, note.timeZone.y), animator)) * finalFlowSpeed * meta.speedUnit * options.flowSpeed;

            if (time >= note.timeZone.x) {
                noteFloorPosition.x = 0.0;
            }
            
            if (noteFloorPosition.x > lineVisibleArea) {
                info.setHidden();
            }

            if (animator.has(note, EnumMilEventType::PositionY)) {
                noteFloorPosition -= noteFloorPosition.x;
                noteFloorPosition += animator.get(note, time, EnumMilEventType::PositionY);
            }

            Vec2 noteBasePosition = {
                animator.get(note, time, EnumMilEventType::RelativeX) + animator.get(note, time, EnumMilEventType::PositionX),
                animator.get(note, time, EnumMilEventType::RelativeY)
            };

            auto noteRelPositionHead = noteBasePosition + Vec2 { 0.0, noteFloorPosition.x },
                noteRelPositionTail = noteBasePosition + Vec2 { 0.0, noteFloorPosition.y };
            
            info.headPosition = lineTransform.transformPoint(noteRelPositionHead);
            info.tailPosition = lineTransform.transformPoint(noteRelPositionTail);
            info.textureRotation = -lineRotation - noteRotation;
            info.speedVectorRotation = -lineRotation;
            if (finalFlowSpeed < 0) info.speedVectorRotation += 180.0;
            info.scale = Vec2(lineSize * noteSize);
            info.color = noteColor.applyAlpha(lineWholeAlpha * noteAlpha);

            return info;
        }

        uint64 getCombo(float64 t) const noexcept {
            if (comboTimes.empty() || comboTimes[0] > t) return 0;

            uint64 left = 0, right = comboTimes.size() - 1;
            uint64 ans = 1;

            while (left <= right) {
                uint64 mid = left + (right - left) / 2;
                if (comboTimes[mid] <= t) {
                    ans = mid + 1;
                    left = mid + 1;
                } else {
                    right = mid - 1;
                }
            }

            return ans;
        }

        private:
        void initSimulNote() {
            std::unordered_map<float64, uint64> noteTimes;

            for (auto& line : lines) {
                for (auto& note : line.notes) {
                    noteTimes[note.timeZone.x]++;
                }
            }

            for (auto& line : lines) {
                for (auto& note : line.notes) {
                    note.isSimul = noteTimes[note.timeZone.x] > 1;
                }
            }
        }

        void initHitEffects() {
            std::mt19937 rng { std::random_device {} () };
            std::uniform_real_distribution<float64> rng_dist { 0.0, 1.0 };
            auto uniform = [&](float64 a, float64 b) { return a + (b - a) * rng_dist(rng); };

            auto makeParticle = [&](MilHitEffectItem& item, float64 dt = 0.0) {
                auto& particle = item.particles.emplace_back();
                particle.dt = dt;
                particle.rotate = uniform(0.0, 360.0);
                particle.initialSpeed = uniform(0.3, 0.72);
                particle.initialSize = std::pow(particle.initialSpeed, 0.22) * uniform(0.6, 0.7) / 42;
                particle.scale = { uniform(1.5, 2.1), uniform(-0.5, 0.5) };
                particle.gravCoeff = uniform(0.9, 1.3);
            };

            hitEffects.clear();

            for (uint64 i = 0; i < lines.size(); i++) {
                auto& line = lines[i];
                for (uint64 j = 0; j < line.notes.size(); j++) {
                    auto& note = line.notes[j];
                    if (note.isFake) continue;

                    auto& item = hitEffects.emplace_back();
                    item.time = note.timeZone.x;
                    item.texRotation = uniform(0.0, 360.0);
                    item.lineIndex = i;
                    item.noteIndex = j;

                    if (!note.isHold()) {
                        for (uint64 i = 0; i < 10; i++) {
                            makeParticle(item);
                        }
                    } else {
                        float64 dt = 0;
                        while (true) {
                            if (note.timeZone.x + dt >= note.timeZone.y) break;
                            makeParticle(item, dt);
                            dt += 0.01;
                        }
                    }
                }
            }

            std::stable_sort(hitEffects.begin(), hitEffects.end(), [](const auto& a, const auto& b) {
                return a.time < b.time;
            });
        }

        void initPlayemntInfo() {
            for (auto& line : lines) {
                for (auto& note : line.notes) {
                    if (note.isFake) continue;
                    comboTimes.push_back(note.timeZone.x);
                    if (note.isHold()) comboTimes.push_back(note.timeZone.y);
                }
            }

            std::sort(comboTimes.begin(), comboTimes.end());
        }
    };

    MilChart loadMilChartFromDevJson(const Data& data) {
        auto failed = [](const std::string& msg) {
            throw std::runtime_error(std::format("dev: {}", msg));
        };

        auto jsonRoot = JsonNode::Parse(data);

        MilChart chart {};
        chart.meta.noteFlowSpeedBehavior = MilMeta::NoteFlowSpeedBehavior::Override;
        chart.meta.worldOrigin = { (float64)-1920 / 2, (float64)1080 / 2 };
        chart.meta.worldViewport = { 1920, -1080 };
        chart.meta.speedUnit = 108.0;

        if (!jsonRoot.isObject()) failed("root is not an object");

        if (!jsonRoot.hasKey("meta")) failed("missing meta field");
        if (!jsonRoot["meta"].isObject()) failed("meta is not an object");

        auto& metaNode = jsonRoot["meta"];

        if (!metaNode.hasKey("Title")) failed("missing Title field");
        if (!metaNode["Title"].isString()) failed("Title is not a string");
        chart.meta.title = metaNode["Title"].getString();

        if (!metaNode.hasKey("Composer")) failed("missing Composer field");
        if (!metaNode["Composer"].isString()) failed("Composer is not a string");
        chart.meta.composer = metaNode["Composer"].getString();

        if (!metaNode.hasKey("Illustrator")) failed("missing Illustrator field");
        if (!metaNode["Illustrator"].isString()) failed("Illustrator is not a string");
        chart.meta.artist = metaNode["Illustrator"].getString();

        if (!metaNode.hasKey("Beatmapper")) failed("missing Beatmapper field");
        if (!metaNode["Beatmapper"].isString()) failed("Beatmapper is not a string");
        chart.meta.charter = metaNode["Beatmapper"].getString();

        if (!metaNode.hasKey("Difficulty")) failed("missing Difficulty field");
        if (!metaNode["Difficulty"].isString()) failed("Difficulty is not a string");
        chart.meta.difficultyName = metaNode["Difficulty"].getString();

        if (!metaNode.hasKey("DifficultyValue")) failed("missing DifficultyValue field");
        if (!metaNode["DifficultyValue"].isNumber()) failed("DifficultyValue is not a number");
        chart.meta.difficultyValue = metaNode["DifficultyValue"].getNumber();

        std::vector<MilBPMEvent> bpms;

        if (!jsonRoot.hasKey("bpms")) failed("missing bpms field");
        if (!jsonRoot["bpms"].isArray()) failed("bpms is not an array");

        for (auto& bpmNode : jsonRoot["bpms"].getArray()) {
            if (!bpmNode.isObject()) failed("bpm is not an object");

            if (!bpmNode.hasKey("start")) failed("missing start field");
            if (!bpmNode["start"].isNumber()) failed("start is not a number");
            float64 start = bpmNode["start"].getNumber();

            if (!bpmNode.hasKey("bpm")) failed("missing bpm field");
            if (!bpmNode["bpm"].isNumber()) failed("bpm is not a number");
            float64 bpm = bpmNode["bpm"].getNumber();

            bpms.push_back({ .time = start, .bpm = bpm });
        }

        auto cvtTime = [&](JsonNode& node, const std::string& key, uint64 bpm, float64* dst) {
            if (!node.hasKey(key)) return false;
            auto& timeNode = node[key];

            if (timeNode.isNumber()) {
                *dst = timeNode.getNumber();
                return true;
            }

            if (timeNode.isArray()) {
                auto& timeArray = timeNode.getArray();
                if (timeArray.size() != 3 && timeArray.size() != 4) return false;

                if (!timeArray[0].isNumber()) return false;
                if (!timeArray[1].isNumber()) return false;
                if (!timeArray[2].isNumber()) return false;
                if (timeArray.size() == 4 && !timeArray[3].isNumber()) return false;

                float64 beatTime = timeArray[0].getNumber() + timeArray[1].getNumber() / timeArray[2].getNumber();
                auto& bpmEvent = bpms[timeArray.size() == 3 ? bpm : (uint64)timeArray[3].getNumber()];

                *dst = bpmEvent.time + beatTime * (60.0 / bpmEvent.bpm);
                return true;
            }

            return false;
        };

        if (!jsonRoot.hasKey("lines")) failed("missing lines field");
        if (!jsonRoot["lines"].isArray()) failed("lines is not an array");

        uint64 lineIndex = 0;
        for (auto& lineNode : jsonRoot["lines"].getArray()) {
            if (!lineNode.isObject()) failed("line is not an object");

            auto& line = chart.lines.emplace_back();
            line.indexer.set(chart.animator.indexGen.get({ EnumMilObjectType::Line, lineIndex++ }));

            if (!lineNode.hasKey("notes")) failed("missing notes field");
            if (!lineNode["notes"].isArray()) failed("notes is not an array");

            for (auto& noteNode : lineNode["notes"].getArray()) {
                if (!noteNode.isObject()) failed("note is not an object");

                auto& note = line.notes.emplace_back();

                if (!noteNode.hasKey("bpm")) failed("missing bpm field");
                if (!noteNode["bpm"].isNumber()) failed("bpm is not a number");
                uint64 bpm = noteNode["bpm"].getNumber();

                if (!cvtTime(noteNode, "startTime", bpm, &note.timeZone.x)) failed("invalid startTime");
                if (!cvtTime(noteNode, "endTime", bpm, &note.timeZone.y)) failed("invalid endTime");

                if (!noteNode.hasKey("type")) failed("missing type field");
                if (!noteNode["type"].isNumber()) failed("type is not a number");
                note.type = MilNoteTypeHelper::FromInt(noteNode["type"].getNumber());

                if (!noteNode.hasKey("isFake")) failed("missing isFake field");
                if (!noteNode["isFake"].isBool()) failed("isFake is not a bool");
                note.isFake = noteNode["isFake"].getBool();

                if (!noteNode.hasKey("isAlwaysPerfect")) failed("missing isAlwaysPerfect field");
                if (!noteNode["isAlwaysPerfect"].isBool()) failed("isAlwaysPerfect is not a bool");
                note.isAlwaysPerfect = noteNode["isAlwaysPerfect"].getBool();

                if (!noteNode.hasKey("index")) failed("missing index field");
                if (!noteNode["index"].isNumber()) failed("index is not a number");
                uint64 noteIndex = noteNode["index"].getNumber();

                note.indexer.set(chart.animator.indexGen.get({ EnumMilObjectType::Note, noteIndex }));
            }
        }

        if (!jsonRoot.hasKey("storyboardObjects")) failed("missing storyboardObjects field");
        if (!jsonRoot["storyboardObjects"].isArray()) failed("storyboardObjects is not an array");

        uint64 sbIndex = 0;
        for (auto& sbNode : jsonRoot["storyboardObjects"].getArray()) {
            if (!sbNode.isObject()) failed("storyboardObject is not an object");

            auto& sb = chart.storyboardObjects.emplace_back();
            sb.indexer.set(chart.animator.indexGen.get({ EnumMilObjectType::Storyboard, sbIndex++ }));

            if (!sbNode.hasKey("type")) failed("missing type field");
            if (!sbNode["type"].isNumber()) failed("type is not a number");
            sb.type = MilStoryboardTypeHelper::FromInt(sbNode["type"].getNumber());

            if (!sbNode.hasKey("data")) failed("missing data field");
            if (!sbNode["data"].isString()) failed("data is not an object");
            sb.data = sbNode["data"].getString();

            if (!sbNode.hasKey("layer")) failed("missing layer field");
            if (!sbNode["layer"].isNumber()) failed("layer is not a number");
            sb.layer = MilStoryboardLayerHelper::FromInt(sbNode["layer"].getNumber());
        }

        auto cvtAnimVal = [](JsonNode& node, const std::string& key, float64* dst) {
            if (!node.hasKey(key)) return false;

            auto& valNode = node[key];

            if (valNode.isNumber()) {
                *dst = valNode.getNumber();
                return true;
            } else if (valNode.isString()) {
                auto& str = valNode.getString();

                if (str.empty()) {
                    *dst = 0.0;
                    return true;
                }

                try { *dst = std::stod(str); }
                catch (...) { return false; }
                return true;
            }

            return false;
        };

        auto cvtColorAnimVal = [](JsonNode& node, const std::string& key, Color* dst) {
            if (!node.hasKey(key)) return false;

            auto& valNode = node[key];

            if (valNode.isString()) {
                try { *dst = Color::FromCss(valNode.getString()); }
                catch (...) { return false; }
                return true;
            }

            return false;
        };

        if (!jsonRoot.hasKey("animations")) failed("missing animations field");
        if (!jsonRoot["animations"].isArray()) failed("animations is not an array");

        uint64 eventIndex = 0;
        for (auto& animNode : jsonRoot["animations"].getArray()) {
            if (!animNode.isObject()) failed("animation is not an object");

            MilEvent e {};
            e.index = eventIndex++;

            if (!animNode.hasKey("bpmId")) failed("missing bpmId field");
            if (!animNode["bpmId"].isNumber()) failed("bpmId is not a number");
            uint64 bpm = animNode["bpmId"].getNumber();

            if (!cvtTime(animNode, "fromBeat", bpm, &e.timeZone.x)) failed("invalid fromBeat");
            if (!cvtTime(animNode, "toBeat", bpm, &e.timeZone.y)) failed("invalid toBeat");

            if (!animNode.hasKey("key")) failed("missing key field");
            if (!animNode["key"].isNumber()) failed("key is not a string");
            e.type = MilEventTypeHelper::FromInt(animNode["key"].getNumber());

            if (e.type == EnumMilEventType::Color) {
                Color fv, tv;
                if (!cvtColorAnimVal(animNode, "fv", &fv)) failed("invalid fv");
                if (!cvtColorAnimVal(animNode, "tv", &tv)) failed("invalid tv");
                e.valueZone = chart.storyboardAssets.requestColorPair(fv, tv);
            } else {
                if (!cvtAnimVal(animNode, "fv", &e.valueZone.x)) failed("invalid fv");
                if (!cvtAnimVal(animNode, "tv", &e.valueZone.y)) failed("invalid tv");
            }

            if (!animNode.hasKey("data")) failed("missing data field");
            if (!animNode["data"].isNumber()) failed("data is not a number");
            auto objType = MilObjectTypeHelper::FromInt(animNode["data"].getNumber());

            if (!animNode.hasKey("i1")) failed("missing i1 field");
            if (!animNode["i1"].isNumber()) failed("i1 is not a number");
            uint64 objIndex = animNode["i1"].getNumber();

            if (!animNode.hasKey("ease")) failed("missing ease field");
            if (!animNode["ease"].isNumber()) failed("ease is not a number");
            uint64 ease = animNode["ease"].getNumber();

            if (!animNode.hasKey("press")) failed("missing press field");
            if (!animNode["press"].isNumber()) failed("press is not a number");
            uint64 press = animNode["press"].getNumber();

            if (press != 0) {
                e.easingFuncContext = (void*)EaseSet::Milthm::pack(ease, press);

                e.easingFunc = [](void* ctx, float64 p) {
                    auto [ease, press] = EaseSet::Milthm::unpack((uint64)ctx);
                    return EaseSet::Milthm::easing(ease, press, p);
                };

                e.easingIntFunc = [](void* ctx, float64 p) {
                    auto [ease, press] = EaseSet::Milthm::unpack((uint64)ctx);
                    return EaseSet::Milthm::easing_int(ease, press, p);
                };
            }

            chart.animator.addEvent({ objType, objIndex }, e);
        }

        chart.rawHash = data.getHash();
        return chart;
    }

    MilChart loadMilChartFromData(const Data& data) {
        std::vector<std::string> msgs;

        #define try_(func) \
            try { return func(data); } \
            catch (const std::exception& err) { msgs.push_back(err.what()); }
        
        try_(loadMilChartFromDevJson);

        std::string msg = "failures: \n";
        for (auto& m : msgs) msg += m + "\n";
        return {};
        
        #undef try_
    }

    struct MilCalculateFrameConfig {
        struct NoteTextureInfo {
            Vec2 textureSize;
            Vec2 cutPadding;
            Vec2 scaling = { 1.0, 1.0 };
        };

        Vec2 screenSize;
        Vec2 backgroundTextureSize;
        std::map<MilNoteTextureDesc, NoteTextureInfo> noteTextureInfos;
        float64 songLength;
        float64 lineHeadScale = 1.0;
        float64 lineHeadConnectPoint;
        float64 maxNoteBodyLength = 8192.0;
    };

    struct MilCalculatedFrame {
        UsingSharedCalculatedObjects;

        struct CalculatedLineHead {
            Vec2 position, scale;
            float64 size;
            float64 rotation;
            Color color;
        };

        struct CalculatedNote {
            Vec2 position;
            float64 rotation;
            float64 width, head, body, tail;
            MilNoteTextureDesc textureDesc;
            Color color;
        };

        struct CalculatedPauseButton {
            Vec2 position, scale = { 1.0, 1.0 };
            float64 size;
            float64 rotation;
            Color color;
        };

        struct CalculatedParticles {
            struct Item {
                Vec2 position, radius;
                float64 rotation;
                Color color;
            };

            std::vector<Item> items;
        };

        struct CalculatedHitEffectTexture {
            Vec2 position, size;
            float64 progress, rotation;
            Color color;
        };

        using CalculatedObject = std::variant<
            ListSharedCalculatedObjects,

            CalculatedLineHead,
            CalculatedNote,
            CalculatedPauseButton,
            CalculatedParticles,
            CalculatedHitEffectTexture
        >;

        Rect backgroundRect;
        Rect progressbarRect;
        std::vector<CalculatedObject> objects;
        std::unordered_map<EnumMilNoteType, uint64> hitsounds;

        void culling(std::vector<CalculatedObject>& objects, const Rect& screenRect) noexcept {
            if (SharedCalculatedObjects::sharedCulling(objects, screenRect)) return;

            auto& obj = objects.back();

            if (std::holds_alternative<CalculatedLineHead>(obj)) {
                auto& head = std::get<CalculatedLineHead>(obj);
                if (!quadStrictlyIntersectRect(makeQuadFromRectInfo({
                    .position = head.position,
                    .size = head.scale * head.size,
                    .rotation = head.rotation
                }).data(), screenRect)) objects.pop_back();
            } else if (std::holds_alternative<CalculatedHitEffectTexture>(obj)) {
                auto& effect = std::get<CalculatedHitEffectTexture>(obj);
                if (!quadStrictlyIntersectRect(makeQuadFromRectInfo({
                    .position = effect.position,
                    .size = effect.size,
                    .rotation = effect.rotation
                }).data(), screenRect)) objects.pop_back();
            }
        }

        void addObject(std::vector<CalculatedObject>& objects, const CalculatedObject& obj, const Rect& screenRect) noexcept {
            objects.push_back(obj);
            culling(objects, screenRect);
        }

        struct Cache {
            std::vector<CalculatedObject> trackObjects, hitEffectCircs;
            std::unordered_map<EnumMilFinalNoteType, std::vector<CalculatedObject>> noteObjects;
            std::vector<CalculatedParticles::Item> hitEffectParticles;

            void clear() {
                trackObjects.clear();
                hitEffectCircs.clear();
                for (auto& [_, v] : noteObjects) v.clear();
                hitEffectParticles.clear();
            }
        };

        Cache cache;
    };

    void calculateMilFrame(
        MilChart& chart, float64 time,
        const MilCalculateFrameConfig& config,
        MilCalculatedFrame& frame
    ) {
        frame.objects.clear();
        frame.hitsounds.clear();
        frame.cache.clear();

        frame.backgroundRect = getCoveredOrContainRect(
            { 0.0, 0.0, config.screenSize.x, config.screenSize.y },
            config.backgroundTextureSize, true
        );

        Rect screenArea = { 0.0, 0.0, config.screenSize.x, config.screenSize.y };

        struct NoteTextureSizeInfo {
            bool isValid = false;
            MilNoteTextureDesc desc;
            float64 width;
            float64 head, body, tail;

            void scale(const Vec2& v) {
                width *= v.x;
                head *= v.y;
                body *= v.y;
                tail *= v.y;
            }

            float64 getHeadHalfDiagonal() const {
                return Vec2 { width, head }.length() / 2;
            }
        };

        float64 lineHeadBase = config.screenSize.sum() * 0.0223;

        auto fallbackNoteTextureDesc = [&](MilNoteTextureDesc& desc) {
            while (!config.noteTextureInfos.contains(desc)) {
                if (!fallbackMilNoteTextureDesc(desc)) {
                    return false;
                }
            }

            return true;
        };

        auto getNoteTextureSizeInfo = [&](MilNoteTextureDesc desc) -> NoteTextureSizeInfo {
            if (!fallbackNoteTextureDesc(desc)) return {};

            auto& texInfo = config.noteTextureInfos.at(desc);

            auto width = lineHeadBase;
            auto totalHeight = width / texInfo.textureSize.y * texInfo.textureSize.x;
            width *= texInfo.scaling.x; totalHeight *= texInfo.scaling.y;
            auto cutPadding = texInfo.cutPadding / texInfo.textureSize.x;
            auto head = cutPadding.x * totalHeight;
            auto tail = cutPadding.y * totalHeight;

            return NoteTextureSizeInfo {
                .isValid = true,
                .desc = desc,
                .width = width,
                .head = head,
                .tail = tail
            };
        };

        float64 maxHalfNoteHeadDiagonal = 0.0;

        for (auto& [desc, _] : config.noteTextureInfos) {
            maxHalfNoteHeadDiagonal = std::max(
                maxHalfNoteHeadDiagonal,
                getNoteTextureSizeInfo(desc).getHeadHalfDiagonal()
            );
        }

        chart.state.timeUpdated(time);

        auto calculateStoryboards = [&](EnumMilStoryboardLayer targetLayer) {
            for (auto& sb : chart.storyboardObjects) {
                if (sb.layer != targetLayer) continue;

                if (sb.type == EnumMilStoryboardType::Picture) {

                } else if (sb.type == EnumMilStoryboardType::Text) {
                    auto sbPosition = chart.getObjectPosition(time, sb, config.screenSize);
                    auto sbAlpha = chart.animator.get(sb, time, EnumMilEventType::Transparency);
                    auto sbSize = chart.animator.get(sb, time, EnumMilEventType::Size);
                    auto sbRotation = chart.animator.get(sb, time, EnumMilEventType::Rotation);
                    auto sbWidth = chart.animator.get(sb, time, EnumMilEventType::StoryBoardWidth);
                    auto sbHeight = chart.animator.get(sb, time, EnumMilEventType::StoryBoardHeight);
                    auto sbColorIndex = chart.animator.get(sb, time, EnumMilEventType::Color);
                    auto sbColorIndexZone = chart.animator.get_zone(sb, time, EnumMilEventType::Color);
                    auto sbColor = chart.storyboardAssets.getColor(sbColorIndex, sbColorIndexZone);

                    if (sbColor.a * sbAlpha > 0.0) {
                        frame.objects.push_back(MilCalculatedFrame::CalculatedText {
                            .text = sb.data,
                            .position = sbPosition,
                            .scale = { sbWidth, sbHeight },
                            .anchor = { 0.5, 0.5 },
                            .fontSize = config.screenSize.sum() * 0.025 * sbSize,
                            .rotation = -sbRotation,
                            .color = sbColor.applyAlpha(sbAlpha),
                        });
                    }
                }
            }
        };

        calculateStoryboards(EnumMilStoryboardLayer::Background);

        frame.objects.push_back(MilCalculatedFrame::CalculatedRect {
            .position = config.screenSize / 2.0,
            .size = config.screenSize,
            .color = Color::Black().applyAlpha(chart.options.backgroundDim)
        });

        for (auto& line : chart.lines) {
            auto linePosition = chart.getObjectPosition(time, line, config.screenSize);
            auto lineRotation = chart.animator.get(line, time, EnumMilEventType::Rotation);
            auto lineAlpha = chart.animator.get(line, time, EnumMilEventType::Transparency);
            auto lineHeadAlpha = chart.animator.get(line, time, EnumMilEventType::LineHeadTransparency);
            auto lineBodyAlpha = chart.animator.get(line, time, EnumMilEventType::LineBodyTransparency);
            auto lineSize = chart.animator.get(line, time, EnumMilEventType::Size);
            auto lineColorIndex = chart.animator.get(line, time, EnumMilEventType::Color);
            auto lineColorIndexZone = chart.animator.get_zone(line, time, EnumMilEventType::Color);
            auto lineColor = chart.storyboardAssets.getColor(lineColorIndex, lineColorIndexZone);

            lineHeadAlpha *= lineAlpha;
            lineBodyAlpha *= lineAlpha;

            if (lineHeadAlpha > 0.0) {
                frame.addObject(frame.cache.trackObjects, MilCalculatedFrame::CalculatedLineHead {
                    .position = linePosition,
                    .scale = { lineSize, lineSize },
                    .size = lineHeadBase * config.lineHeadScale,
                    .rotation = 180 - lineRotation,
                    .color = lineColor.applyAlpha(lineHeadAlpha)
                }, screenArea);
            }

            if (lineBodyAlpha > 0.0) {
                auto connectRadius = config.lineHeadConnectPoint * lineHeadBase * config.lineHeadScale;
                auto lineWidth = lineHeadBase * 0.096774;
                frame.addObject(frame.cache.trackObjects, MilCalculatedFrame::CalculatedPoly::Make(
                    { connectRadius, -lineWidth / 2 },
                    { config.screenSize.y * 2.5, lineWidth },
                    lineColor.applyAlpha(lineBodyAlpha),
                    Transform2D()
                        .translate(linePosition)
                        .rotateDegrees(-lineRotation)
                        .scale(lineSize)
                ), screenArea);
            }

            for (auto& noteGroup : line.noteGroups) {
                noteGroup.state.timeUpdated(time);

                for (uint64 note_ii = noteGroup.state.firstNoteIndex; note_ii < noteGroup.indexs.size(); note_ii++) {
                    auto note_i = noteGroup.indexs[note_ii];
                    auto& note = line.notes[note_i];
                    note.state.timeUpdated(note, time);

                    auto frameInfo = chart.getNoteFrameInfo(line, note, time, config.screenSize);

                    if (frameInfo.isArrived && note.state.onPlayHitsound()) {
                        if (!note.isFake) {
                            frame.hitsounds[note.type]++;
                        }
                    }

                    if (note.timeZone.y < time) {
                        if (!note.isHold() || note.timeZone.y + chart.meta.holdDisappearTime <= time) {
                            noteGroup.state.passedNoteIndex(note_ii);
                            continue;
                        }
                    }

                    if (note.isHold()) {
                        frameInfo.color.a *= 1.0 - std::clamp((time - note.timeZone.y) / chart.meta.holdDisappearTime, 0.0, 1.0);
                    }

                    auto sizeInfo = getNoteTextureSizeInfo(note.textureDesc);
                    sizeInfo.body = std::min(config.maxNoteBodyLength, (frameInfo.headPosition - frameInfo.tailPosition).length());
                    sizeInfo.scale(frameInfo.scale);

                    Transform2D noteTransform;
                    noteTransform.translate(frameInfo.headPosition);
                    noteTransform.rotateDegrees(frameInfo.textureRotation);
                    noteTransform.scale(1.0, -1.0);

                    Vec2 noteQuad[4] = {
                        noteTransform.transformPoint({ -sizeInfo.head, -sizeInfo.width / 2 }),
                        noteTransform.transformPoint({ -sizeInfo.head, sizeInfo.width / 2 }),
                        noteTransform.transformPoint({ sizeInfo.body + sizeInfo.tail, sizeInfo.width / 2 }),
                        noteTransform.transformPoint({ sizeInfo.body + sizeInfo.tail, -sizeInfo.width / 2 })
                    };

                    auto extendedSafeArea = screenArea.extend(maxHalfNoteHeadDiagonal * frameInfo.scale.max());
                    bool noteInsideScreen = quadStrictlyIntersectRect(noteQuad, extendedSafeArea);

                    if (noteInsideScreen) {
                        if (sizeInfo.isValid && frameInfo.color.a > 0.0) {
                            frame.cache.noteObjects[note.finalType].push_back(MilCalculatedFrame::CalculatedNote {
                                .position = frameInfo.headPosition,
                                .rotation = frameInfo.textureRotation,
                                .width = sizeInfo.width,
                                .head = sizeInfo.head,
                                .body = sizeInfo.body,
                                .tail = sizeInfo.tail,
                                .textureDesc = sizeInfo.desc,
                                .color = frameInfo.color
                            });
                        }
                    } else {
                        if (noteGroup.breakable) {
                            if (lineIsLeavingScreen(
                                frameInfo.headPosition,
                                frameInfo.speedVectorRotation + 90.0,
                                extendedSafeArea
                            ) && lineIsLeavingScreen(
                                frameInfo.headPosition,
                                frameInfo.textureRotation + 90.0,
                                extendedSafeArea
                            )) break;
                        }
                    }
                }
            }
        }

        for (uint64 i = chart.state.firstHitEffectIndex; i < chart.hitEffects.size(); i++) {
            auto& hitEffect = chart.hitEffects[i];
            if (hitEffect.time > time) break;
            
            auto& line = chart.lines[hitEffect.lineIndex];
            auto& note = line.notes[hitEffect.noteIndex];

            auto info = chart.getNoteFrameInfo(line, note, time, config.screenSize);

            if (hitEffect.getEndTime(chart.options.hitEffectDuration) < time) {
                chart.state.passedHitEffectIndex(i);
                continue;
            }

            auto progress = (time - hitEffect.time) / chart.options.hitEffectDuration;

            if (progress <= 1.0) {
                frame.addObject(frame.cache.hitEffectCircs, MilCalculatedFrame::CalculatedHitEffectTexture {
                    .position = info.headPosition,
                    .size = Vec2(lineHeadBase * 4.632 * (1.0 - std::pow(1.0 - progress, 3.0))) * info.scale.max(),
                    .progress = progress,
                    .rotation = hitEffect.texRotation,
                    .color = Color { 150, 144, 253, 255 } / 255
                }, screenArea);
            }

            for (auto& particle : hitEffect.particles) {
                auto particleTime = hitEffect.time + particle.dt;
                if (particleTime > time) break;
                if (particleTime + chart.options.hitEffectDuration < time) continue;

                auto progress = std::clamp((time - particleTime) / chart.options.hitEffectDuration, 0.0, 1.0);
                auto size = particle.initialSize * config.screenSize.sum();
                auto r = particle.getRadius(progress) * config.screenSize.sum();
                auto rotate = particle.rotate;
                auto color = chart.options.particleRGBColor.get(progress);
                color.a = chart.options.particleAlphaColor.get(progress).a;

                auto particlePos = info.headPosition.rotateDegrees(rotate, r * info.scale.max());
                particlePos.y += particle.getDeltaY(progress) * config.screenSize.sum() * info.scale.max();

                rotate += (rotate - (particlePos - info.headPosition).atanDegrees()) * 2;

                auto& item = frame.cache.hitEffectParticles.emplace_back();
                item.position = particlePos;
                item.radius = particle.getScale(progress) * size * info.scale.max();
                item.rotation = rotate;
                item.color = color;

                if (!quadStrictlyIntersectRect(makeQuadFromRectInfo({
                    .position = item.position,
                    .size = item.radius * 2.0,
                    .rotation = item.rotation
                }).data(), screenArea)) frame.cache.hitEffectParticles.pop_back();
            }
        }

        calculateStoryboards(EnumMilStoryboardLayer::Normal);
        frame.objects.insert(frame.objects.end(), frame.cache.trackObjects.begin(), frame.cache.trackObjects.end());
        frame.objects.insert(frame.objects.end(), frame.cache.hitEffectCircs.begin(), frame.cache.hitEffectCircs.end());

        for (const auto type : {
            EnumMilFinalNoteType::Hold,
            EnumMilFinalNoteType::Tap,
            EnumMilFinalNoteType::Drag
        }) {
            auto& objs = frame.cache.noteObjects[type];
            frame.objects.insert(frame.objects.end(), objs.begin(), objs.end());
        }

        frame.objects.push_back(MilCalculatedFrame::CalculatedParticles {
            .items = frame.cache.hitEffectParticles
        });

        calculateStoryboards(EnumMilStoryboardLayer::Foreground);

        auto combo = chart.getCombo(time);

        frame.progressbarRect = {
            0.0, 0.0,
            time / config.songLength * config.screenSize.x,
            config.screenSize.x * 0.0046875
        };

        frame.objects.push_back(MilCalculatedFrame::CalculatedPauseButton {
            .position = Vec2(config.screenSize.x) * Vec2 { 0.0494792, 0.0489583 },
            .size = config.screenSize.x * 0.040625,
            .color = Color::White().applyAlpha(0.67)
        });

        frame.objects.push_back(MilCalculatedFrame::CalculatedText {
            .text = chart.meta.title,
            .position = Vec2(config.screenSize.x) * Vec2 { 0.0994791, 0.0397208 },
            .anchor = { 0.0, 0.5 },
            .fontSize = config.screenSize.x * 0.0201352
        });

        frame.objects.push_back(MilCalculatedFrame::CalculatedText {
            .text = chart.meta.getFinalDifficultyString(),
            .position = Vec2(config.screenSize.x) * Vec2 { 0.0994791, 0.0604583 },
            .anchor = { 0.0, 0.5 },
            .fontSize = config.screenSize.x * 0.0151472,
            .color = Color::White().applyAlpha(0.75)
        });

        float64 targetScore = chart.comboTimes.size() ? std::clamp<float64>(std::ceil((float64)1010000 / chart.comboTimes.size() * combo), 0, 1010000) : 1010000;
        frame.objects.push_back(MilCalculatedFrame::CalculatedText {
            .text = std::format("{:07}", (uint64)chart.state.scoreAnim.weakSet(time, targetScore).get(time)),
            .position = Vec2(config.screenSize.x) * Vec2 { 0.9752375, 0.0395833 },
            .anchor = { 1.0, 0.5 },
            .fontSize = config.screenSize.x * 0.0268352,
        });

        frame.objects.push_back(MilCalculatedFrame::CalculatedText {
            .text = "100.00%",
            .position = Vec2(config.screenSize.x) * Vec2 { 0.9752375, 0.06684375 },
            .anchor = { 1.0, 0.5 },
            .fontSize = config.screenSize.x * 0.0201352,
            .color = Color::White().applyAlpha(0.75)
        });
        
        frame.objects.push_back(MilCalculatedFrame::CalculatedText {
            .text = "ALL PERFECT",
            .position = Vec2(config.screenSize.x) * Vec2 { 0.5, 0.0359375 },
            .anchor = { 0.5, 0.5 },
            .fontSize = config.screenSize.x * 0.0201352,
        });
        
        frame.objects.push_back(MilCalculatedFrame::CalculatedText {
            .text = std::to_string(combo),
            .position = Vec2(config.screenSize.x) * Vec2 { 0.5, 0.0677083 },
            .anchor = { 0.5, 0.5 },
            .fontSize = config.screenSize.x * 0.0263352 * chart.state.comboScaleAnim.weakSet(time, combo).get(time)
        });
    }

    DecodedRGBATexture spwanMilBackgroundMask() {
        auto tex = DecodedRGBATexture::Make(2, 128);
        const float64 start = 0.2;
        const float64 alphaExp = 0.8;

        for (uint64 i = 0; i < tex.width; i++) {
            for (uint64 j = 0; j < tex.height; j++) {
                float64 p = (float64)j / (tex.height - 1);
                p = p * (start + 1) - start;
                if (p <= 0.0) continue;
                tex.data[tex.getIndexBase(i, j) + 3] = gnumeric::utils::clamp<uint8, float64>(std::pow(p, alphaExp) * 270);
            }
        }

        return tex;
    }

    DecodedRGBATexture spwanMilProgressbar() {
        auto tex = DecodedRGBATexture::Make(128, 2);
        std::fill(tex.data.begin(), tex.data.end(), 255);

        for (uint64 i = 0; i < tex.width; i++) {
            for (uint64 j = 0; j < tex.height; j++) {
                float64 p = (float64)i / (tex.width - 1);
                p = 1.0 - std::pow(1.0 - p, 2.2);
                tex.data[tex.getIndexBase(i, j) + 3] = gnumeric::utils::clamp<uint8, float64>(p * 255);
            }
        }

        return tex;
    }

    struct MilTakeOverer {
        MilTakeOverer() = default;
        MilTakeOverer(const MilTakeOverer&) = delete;
        MilTakeOverer& operator=(const MilTakeOverer&) = delete;
        MilTakeOverer(MilTakeOverer&&) = default;
        MilTakeOverer& operator=(MilTakeOverer&&) = default;

        static gsp<MilTakeOverer> Make() {
            auto* tor = new MilTakeOverer();
            return gsp<MilTakeOverer>(tor);
        }
        
        struct LineHeadTextureLoaderResult {
            Data encoded;
            float64 scale = 1.0, connectPoint;
            bool connectPointIsPixel = true;
        };

        using LineHeadTextureLoader = std::function<LineHeadTextureLoaderResult()>;
        LineHeadTextureLoader lineHeadTextureLoader;

        struct NoteTextureDataLoaderResult {
            Data encoded;
            Vec2 cutPadding;
            bool cutPaddingIsPixel = true;
            bool ignoreCutPadding;
            Vec2 scaling = { 1.0, 1.0 };
        };

        using NoteTextureDataLoader = std::function<NoteTextureDataLoaderResult(const MilNoteTextureDesc&)>;
        NoteTextureDataLoader noteTextureDataLoader;

        using HitsoundDataLoader = std::function<Data(EnumMilNoteType)>;
        HitsoundDataLoader hitsoundDataLoader;
        
        using PauseButtonTextureDataLoader = std::function<Data()>;
        PauseButtonTextureDataLoader pauseButtonTextureDataLoader;

        gsp<GL::GL33Context> glCtx;
        TakeOvererComponents::SharedComp sharedComp;
        GL::TextManager textManager;
        TakeOvererComponents::AudioManager audioManager;

        MilCalculateFrameConfig calcConfig;
        MilChart chart;
        MilCalculatedFrame calculatedFrame;

        uint64 circHitEffectTexSize = 512;
        uint64 circHitEffectTexsCount = 60;

        void init() {
            gassert::assert(!!lineHeadTextureLoader, "MilTakeOverer: lineHeadTextureLoader is not set");
            gassert::assert(!!noteTextureDataLoader, "MilTakeOverer: noteTextureDataLoader is not set");
            gassert::assert(!!hitsoundDataLoader, "MilTakeOverer: hitsoundDataLoader is not set");
            gassert::assert(!!pauseButtonTextureDataLoader, "MilTakeOverer: pauseButtonTextureDataLoader is not set");
            gassert::assert(!!glCtx, "MilTakeOverer: glCtx is not set");

            textManager.glCtx = glCtx;

            sharedComp.check();
            textManager.check();
            audioManager.check();

            loadResources();
        }

        void loadIllustion(const Data& data) {
            auto decoded = sharedComp.textureDecoder(data);
            sharedComp.illustionTexture = glCtx->createTextureFromDecoded(decoded);
        }

        void loadIllustion(const std::string& path) { loadIllustion(Data::MakeFromFile(path)); }

        struct MixBgmConfig {
            float64 musicVol = 1.0, sfxVol = 1.0;
        };

        gsp<DecodedAudio> mixFinalBgm(const MilChart& chart, const MixBgmConfig& config) {
            if (!audioManager.bgmAudio) throw std::runtime_error("bgm is not loaded");

            auto result = audioManager.bgmAudio->copy();
            result->applyVolume(config.musicVol);
            
            for (const auto& line : chart.lines) {
                for (const auto& note : line.notes) {
                    if (note.isFake) continue;

                    auto sfx = hitsoundAudios.at(note.type);
                    result->overlapSecond(sfx, note.timeZone.x, config.sfxVol);
                }
            }

            return result;
        }

        using ChartIniter = std::function<void(MilChart&)>;

        struct LoadChartConfig {
            Data data;
            ChartIniter initer = [](MilChart& chart) { chart.init(); };
        };

        TakeOvererComponents::LoadChartResultInfo loadChart(const LoadChartConfig& config) {
            TakeOvererComponents::LoadChartResultInfo resultInfo {};

            {
                gtime::Timer timer;

                try {
                    chart = loadMilChartFromData(config.data);
                } catch (const std::exception& e) {
                    resultInfo.success = false;
                    resultInfo.error = e.what();
                    return resultInfo;
                }

                resultInfo.createObjectTook = timer.elapsed();
            }

            {
                gtime::Timer timer;
                config.initer(chart);
                resultInfo.initTook = timer.elapsed();
            }

            return resultInfo;
        }

        struct RenderConfig {
            TakeOvererComponents::RenderConfigBase base;
        };

        struct RenderResultInfo {
            TakeOvererComponents::RenderResultInfoBase base;
        };

        RenderResultInfo& render(const RenderConfig& renderConfig) {
            calcConfig.songLength = audioManager.getBgmLength();
            calcConfig.backgroundTextureSize = { sharedComp.illustionTexture->width, sharedComp.illustionTexture->height };

            auto t = renderConfig.base.getTime(audioManager);

            {
                gtime::Timer timer;
                calculateMilFrame(chart, t, calcConfig, calculatedFrame);
                renderResultInfoCache.base.calculatedTook = timer.elapsed();
            }

            gtime::Timer glOpsTimer;

            using namespace GL;

            glCtx->setViewport(calcConfig.screenSize.x, calcConfig.screenSize.y);
            glCtx->gl.glClearColor(0.0, 0.0, 0.0, 0.0);
            glCtx->gl.glClear(GL_COLOR_BUFFER_BIT);

            auto cvs = GL33Canvas::Make(glCtx.get());

            cvs.drawRect({
                .position = { calculatedFrame.backgroundRect.x, calculatedFrame.backgroundRect.y },
                .size = { calculatedFrame.backgroundRect.w, calculatedFrame.backgroundRect.h },
                .texture = sharedComp.illustionTexture.get()
            });

            cvs.drawRect({
                .position = { calculatedFrame.backgroundRect.x, calculatedFrame.backgroundRect.y },
                .size = { calculatedFrame.backgroundRect.w, calculatedFrame.backgroundRect.h },
                .texture = backgroundMask.get()
            });

            for (auto& obj : calculatedFrame.objects) {
                if (TakeOvererComponents::renderSharedObject(obj, glCtx, cvs, textManager)) {
                    continue;
                }

                if (std::holds_alternative<MilCalculatedFrame::CalculatedLineHead>(obj)) {
                    auto& lineHead = std::get<MilCalculatedFrame::CalculatedLineHead>(obj);

                    cvs.save();
                    cvs.translate(lineHead.position);
                    cvs.rotateDegrees(lineHead.rotation);
                    cvs.scale(lineHead.scale);
                    cvs.drawRect({
                        .position = -Vec2 { lineHead.size, lineHead.size } / 2,
                        .size = { lineHead.size, lineHead.size },
                        .color = lineHead.color,
                        .texture = lineHeadTex.get()
                    });
                    cvs.restore();
                } else if (std::holds_alternative<MilCalculatedFrame::CalculatedNote>(obj)) {
                    auto& note = std::get<MilCalculatedFrame::CalculatedNote>(obj);
                    auto& img = noteTextures.at(note.textureDesc);
                    auto& imgInfo = calcConfig.noteTextureInfos.at(note.textureDesc);

                    cvs.save();
                    cvs.translate(note.position);
                    cvs.rotateDegrees(note.rotation);

                    auto mesh = glCtx->requestMesh(6 * 3);
                    mesh.color = note.color;
                    mesh.texture = img.get();

                    mesh.addRect(
                        { -note.head, -note.width / 2 }, { note.head, note.width },
                        GLvec2 { 0.0, 0.0 },
                        GLvec2 { imgInfo.cutPadding.x, imgInfo.textureSize.y } / imgInfo.textureSize
                    );

                    mesh.addRect(
                        { 0.0, -note.width / 2 }, { note.body, note.width },
                        GLvec2 { imgInfo.cutPadding.x, 0.0 } / imgInfo.textureSize,
                        GLvec2 { imgInfo.textureSize.x - imgInfo.cutPadding.sum(), imgInfo.textureSize.y } / imgInfo.textureSize
                    );

                    mesh.addRect(
                        { note.body, -note.width / 2 }, { note.tail, note.width },
                        GLvec2 { imgInfo.textureSize.x - imgInfo.cutPadding.y, 0.0 } / imgInfo.textureSize,
                        GLvec2 { imgInfo.cutPadding.y, imgInfo.textureSize.y } / imgInfo.textureSize
                    );

                    cvs.drawMesh(mesh);
                    cvs.restore();
                } else if (std::holds_alternative<MilCalculatedFrame::CalculatedPauseButton>(obj)) {
                    auto& btn = std::get<MilCalculatedFrame::CalculatedPauseButton>(obj);

                    cvs.save();
                    cvs.translate(btn.position);
                    cvs.rotateDegrees(btn.rotation);
                    cvs.scale(btn.scale);
                    cvs.drawRect({
                        .position = -Vec2 { btn.size, btn.size } / 2,
                        .size = { btn.size, btn.size },
                        .color = btn.color,
                        .texture = pauseButtonTex.get()
                    });
                    cvs.restore();
                } else if (std::holds_alternative<MilCalculatedFrame::CalculatedParticles>(obj)) {
                    auto& particles = std::get<MilCalculatedFrame::CalculatedParticles>(obj);

                    auto mesh = glCtx->requestMesh(6 * particles.items.size());
                    mesh.program = glCtx->preloadedPrograms.circle.get();
                    mesh.color = GLvec4::White();

                    for (auto& it : particles.items) {
                        Transform2D transform;
                        transform.translate(it.position);
                        transform.rotateDegrees(it.rotation);
                        mesh.addRectCentered({}, it.radius, { 0.5, 0.5 }, { 0.5, 0.5 }, it.color);
                        mesh.vertices.transformBack(transform, 6);
                    }

                    cvs.drawMesh(mesh);
                } else if (std::holds_alternative<MilCalculatedFrame::CalculatedHitEffectTexture>(obj)) {
                    auto& effect = std::get<MilCalculatedFrame::CalculatedHitEffectTexture>(obj);
                    auto& img = circHitEffectTexs[std::clamp<uint64>(effect.progress * circHitEffectTexs.size(), 0, circHitEffectTexs.size() - 1)];
                    
                    cvs.save();
                    cvs.translate(effect.position);
                    cvs.rotateDegrees(effect.rotation);
                    cvs.drawRect({
                        .position = -effect.size / 2,
                        .size = effect.size,
                        .color = effect.color,
                        .texture = img.get()
                    });
                    cvs.restore();
                }
            }

            cvs.drawRect({
                .position = { calculatedFrame.progressbarRect.x, calculatedFrame.progressbarRect.y },
                .size = { calculatedFrame.progressbarRect.w, calculatedFrame.progressbarRect.h },
                .texture = progressbarTex.get()
            });

            if (renderConfig.base.flushGl) {
                glCtx->gl.glFlush();
            }

            renderResultInfoCache.base.glOperationsTook = glOpsTimer.elapsed();

            if (!renderConfig.base.disableHitsound) {
                for (auto& [type, count] : calculatedFrame.hitsounds) {
                    audioManager.playSfx(hitsoundAudios.at(type), count);
                }
            }

            return renderResultInfoCache;
        }

        private:
        RenderResultInfo renderResultInfoCache;
        gsp<GL::TextureInfo> backgroundMask;
        gsp<GL::TextureInfo> progressbarTex;
        gsp<GL::TextureInfo> lineHeadTex;
        std::unordered_map<EnumMilNoteType, gsp<DecodedAudio>> hitsoundAudios;
        std::map<MilNoteTextureDesc, gsp<GL::TextureInfo>> noteTextures;
        gsp<GL::TextureInfo> pauseButtonTex;
        gsp<GL::ProgramInfo> circHitEffectProg;
        std::vector<gsp<GL::TextureInfo>> circHitEffectTexs;

        void loadResources() {
            using namespace GL;

            backgroundMask = glCtx->createTextureFromDecoded(spwanMilBackgroundMask());
            progressbarTex = glCtx->createTextureFromDecoded(spwanMilProgressbar());

            {
                auto lineHead = lineHeadTextureLoader();
                auto decoded = sharedComp.textureDecoder(lineHead.encoded);
                lineHeadTex = glCtx->createTextureFromDecoded(decoded, true);

                calcConfig.lineHeadScale = lineHead.scale;
                if (lineHead.connectPointIsPixel) lineHead.connectPoint /= decoded.height;
                calcConfig.lineHeadConnectPoint = lineHead.connectPoint;
            }

            for (const auto type : {
                EnumMilNoteType::Hit,
                EnumMilNoteType::Drag
            }) {
                for (const auto isHold : { false, true }) {
                    for (const auto isSimul : { false, true }) {
                        for (const auto isAlwaysPerfect : { false, true }) {
                            auto desc = MilNoteTextureDesc(type, isHold, isSimul, isAlwaysPerfect);
                            auto loadResult = noteTextureDataLoader(desc);
                            if (loadResult.encoded.empty()) continue;

                            auto decoded = sharedComp.textureDecoder(loadResult.encoded);
                            auto tex = glCtx->createTextureFromDecoded(decoded, true);
                            if (!loadResult.cutPaddingIsPixel) loadResult.cutPadding *= decoded.width;
                            if (loadResult.ignoreCutPadding) loadResult.cutPadding = Vec2 { (float64)decoded.width, (float64)decoded.width } / 2;

                            calcConfig.noteTextureInfos[desc] = {
                                .textureSize = { (float64)decoded.width, (float64)decoded.height },
                                .cutPadding = loadResult.cutPadding,
                                .scaling = loadResult.scaling
                            };

                            noteTextures[desc] = tex;
                        }
                }
                }
            }

            for (const auto type : {
                EnumMilNoteType::Hit,
                EnumMilNoteType::Drag
            }) {
                auto data = hitsoundDataLoader(type);
                hitsoundAudios[type] = audioManager.decodeAndCheck(data);
            }

            {
                auto btn = pauseButtonTextureDataLoader();
                auto decoded = sharedComp.textureDecoder(btn);
                pauseButtonTex = glCtx->createTextureFromDecoded(decoded, true);
            }

            circHitEffectProg = glCtx->createConfiguredProgram(GL33Context::CreateProgramConfig { .fragCode = R"(
#version 330 core

in vec2 fragTexCoord;

uniform float uProgress;
uniform float uSeed;

out vec4 outColor;

float rand(vec2 n) { 
    return fract(sin(dot(n, vec2(12.9898, 78.233))) * 43758.5453);
}

float noise(vec2 p) {
    vec2 ip = floor(p);
    vec2 fp = fract(p);
    
    float a = rand(ip);
    float b = rand(ip + vec2(1.0, 0.0));
    float c = rand(ip + vec2(0.0, 1.0));
    float d = rand(ip + vec2(1.0, 1.0));
    
    vec2 u = fp * fp * (3.0 - 2.0 * fp);
    return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

float circularNoise(vec2 uv, float density, float seed) {
    vec2 center = uv - 0.5;
    float radius = length(center) * density;
    float angle = abs(atan(center.y, center.x));

    if (uv.y > 0.5) {
        angle += sin(angle) * 2.;
    }

    vec2 seedOffset = vec2(seed * 100.0, seed * 100.0);
    vec2 polarCoord = vec2(radius, angle) + seedOffset;
    
    float n = 0.0;
    n += noise(polarCoord) * 0.7;
    n += noise(polarCoord * 2.0) * 0.3;
    n += noise(polarCoord * 4.0) * 0.1;
    
    return n;
}

void main() {
    outColor = vec4(1.0);
    float l = length(fragTexCoord - 0.5);

    if (0.43 <= l && l <= 0.5) {
        float n = circularNoise(fragTexCoord, 50.0, uSeed);
        outColor.a *= (n < uProgress) ? 0.0 : 1.0;
    } else {
        outColor.a = 0.0;
    }
}
)" }.defaultColorVert());

            circHitEffectProg->fragConfig.textureUniformName = std::nullopt;
            circHitEffectProg->fragConfig.colorUniformName = std::nullopt;

            {
                static std::mt19937 rng { std::random_device {} () };
                std::uniform_real_distribution<float64> rng_dist { 0.0, 1.0 };

                auto mesh = glCtx->requestMesh(6);
                mesh.program = circHitEffectProg.get();
                mesh.color = GLvec4::White();

                {
                    auto progGuard = mesh.program->use();
                    mesh.program->getUniformLocation("uSeed").setf(rng_dist(rng));
                }

                for (uint64 i = 0; i < circHitEffectTexsCount; i++) {
                    auto p = (float64)i / (circHitEffectTexsCount - 1);
                    auto tex = glCtx->createTexture();
                    tex->use().image2D(circHitEffectTexSize, circHitEffectTexSize, nullptr);

                    {
                        auto progGuard = mesh.program->use();
                        mesh.program->getUniformLocation("uProgress").setf(p);
                    }

                    glCtx->renderIntoTexture(tex.get(), mesh);

                    tex->use().generateMipmap();
                    circHitEffectTexs.push_back(tex);
                }
            }
        }
    };

    enum class EnumRizNoteType {
        Tap,
        Drag,
        Hold
    };

    struct RizTheme {
        Color bgColor;
        Color noteColor;
        Color uiColor;
    };

    struct RizChallengeTime {
        float64 checkPoint;
        Vec2 timeZone;
        float64 transTime;
    };

    struct RizEase {
        float64 (* func)(void*, float64);
        float64 (* intFunc)(void*, float64);
        void* context;
    };

    struct RizBpmEvent {
        float64 time, bpm;
        RizEase ease;
    };

    struct RizLinePoint {
        float64 time;
        float64 xPosition;
        Color color;
        RizEase ease;
        uint64 canvasIndex;
    };

    struct RizNote {
        EnumRizNoteType type;
        Vec2 timeZone;
        uint64 tailCanvasIndex;
    };

    struct RizColorPoint {
        float64 time;
        Color start, end;
    };

    struct RizKeyPoint {
        float64 time, value;
        RizEase ease;
    };

    struct RizCanvasMove {
        uint64 index;
        std::vector<RizKeyPoint> xPositions;
        std::vector<RizKeyPoint> speeds;
    };

    struct RizCameraMove {
        std::vector<RizKeyPoint> scales;
        std::vector<RizKeyPoint> xPositions;
    };

    struct RizLine {
        std::vector<RizLinePoint> linePoints;
        std::vector<RizNote> notes;
        std::vector<RizColorPoint> ringColors;
        std::vector<RizColorPoint> lineColors;
    };

    struct RizChart {
        struct UserOptions {
            float64 lineRingY = 0.68;
        };

        float64 offset;
        std::vector<RizTheme> themes;
        std::vector<RizChallengeTime> challengeTimes;
        std::vector<RizBpmEvent> bpmEvents;
        std::vector<RizCanvasMove> canvasMoves;
        std::vector<RizLine> lines;
        RizCameraMove cameraMove;

        UserOptions options;

        void init() {

        }
    };

    RizChart loadRizChartFromOfficialJson(const Data& data) {
        auto failed = [](const std::string& msg) {
            throw std::runtime_error(std::format("official: {}", msg));
        };

        auto jsonRoot = JsonNode::Parse(data);

        RizChart chart {};

        return chart;
    }

    RizChart loadRizChartFromData(const Data& data) {
        std::vector<std::string> msgs;

        #define try_(func) \
            try { return func(data); } \
            catch (const std::exception& err) { msgs.push_back(err.what()); }
        
        try_(loadRizChartFromOfficialJson);

        std::string msg = "failures: \n";
        for (auto& m : msgs) msg += m + "\n";
        return {};
        
        #undef try_
    }

    struct RizCalculateFrameConfig {
        Vec2 screenSize;
    };

    struct RizCalculatedFrame {
        UsingSharedCalculatedObjects;

        using CalculatedObject = std::variant<
            ListSharedCalculatedObjects
        >;

        std::vector<CalculatedObject> objects;
    };

    void calculateRizFrame(
        RizChart& chart, float64 time,
        const RizCalculateFrameConfig& config,
        RizCalculatedFrame& frame
    ) {
        frame.objects.clear();
    }

    struct RizTakeOverer {
        RizTakeOverer() = default;
        RizTakeOverer(const RizTakeOverer&) = delete;
        RizTakeOverer& operator=(const RizTakeOverer&) = delete;
        RizTakeOverer(RizTakeOverer&&) = default;
        RizTakeOverer& operator=(RizTakeOverer&&) = default;

        static gsp<RizTakeOverer> Make() {
            auto* tor = new RizTakeOverer();
            return gsp<RizTakeOverer>(tor);
        }

        gsp<GL::GL33Context> glCtx;
        TakeOvererComponents::SharedComp sharedComp;
        GL::TextManager textManager;
        TakeOvererComponents::AudioManager audioManager;

        RizCalculateFrameConfig calcConfig;
        RizChart chart;
        RizCalculatedFrame calculatedFrame;

        void init() {
            gassert::assert(!!glCtx, "MilTakeOverer: glCtx is not set");

            textManager.glCtx = glCtx;

            sharedComp.check();
            textManager.check();
            audioManager.check();

            loadResources();
        }

        void loadIllustion(const Data& data) {
            auto decoded = sharedComp.textureDecoder(data);
            sharedComp.illustionTexture = glCtx->createTextureFromDecoded(decoded);
        }

        void loadIllustion(const std::string& path) { loadIllustion(Data::MakeFromFile(path)); }

        struct MixBgmConfig {
            float64 musicVol = 1.0, sfxVol = 1.0;
        };

        gsp<DecodedAudio> mixFinalBgm(const RizChart& chart, const MixBgmConfig& config) {
            if (!audioManager.bgmAudio) throw std::runtime_error("bgm is not loaded");

            auto result = audioManager.bgmAudio->copy();
            result->applyVolume(config.musicVol);
            
            // ...

            return result;
        }

        using ChartIniter = std::function<void(RizChart&)>;

        struct LoadChartConfig {
            Data data;
            ChartIniter initer = [](RizChart& chart) { chart.init(); };
        };

        TakeOvererComponents::LoadChartResultInfo loadChart(const LoadChartConfig& config) {
            TakeOvererComponents::LoadChartResultInfo resultInfo {};

            {
                gtime::Timer timer;

                try {
                    chart = loadRizChartFromData(config.data);
                } catch (const std::exception& e) {
                    resultInfo.success = false;
                    resultInfo.error = e.what();
                    return resultInfo;
                }

                resultInfo.createObjectTook = timer.elapsed();
            }

            {
                gtime::Timer timer;
                config.initer(chart);
                resultInfo.initTook = timer.elapsed();
            }

            return resultInfo;
        }

        struct RenderConfig {
            TakeOvererComponents::RenderConfigBase base;
        };

        struct RenderResultInfo {
            TakeOvererComponents::RenderResultInfoBase base;
        };

        RenderResultInfo& render(const RenderConfig& renderConfig) {
            auto t = renderConfig.base.getTime(audioManager);

            {
                gtime::Timer timer;
                calculateRizFrame(chart, t, calcConfig, calculatedFrame);
                renderResultInfoCache.base.calculatedTook = timer.elapsed();
            }
            
            gtime::Timer glOpsTimer;

            using namespace GL;

            glCtx->setViewport(calcConfig.screenSize.x, calcConfig.screenSize.y);
            glCtx->gl.glClearColor(0.0, 0.0, 0.0, 0.0);
            glCtx->gl.glClear(GL_COLOR_BUFFER_BIT);

            auto cvs = GL33Canvas::Make(glCtx.get());

            if (renderConfig.base.flushGl) {
                glCtx->gl.glFlush();
            }

            renderResultInfoCache.base.glOperationsTook = glOpsTimer.elapsed();

            if (!renderConfig.base.disableHitsound) {
                // for (auto& [type, count] : calculatedFrame.hitsounds) {
                //     audioManager.playSfx(hitsoundAudios.at(type), count);
                // }
            }

            return renderResultInfoCache;
        }

        private:
        RenderResultInfo renderResultInfoCache;

        void loadResources() {
            using namespace GL;
        }
    };

    #undef UsingSharedCalculatedObjects
    #undef ListSharedCalculatedObjects

    struct PhiStaticResourceHelpers {
        static PhiTakeOverer::NoteTextureDataLoaderResult noteTextureDataLoader(const PhiTakeOverer::NoteTextureDataLoaderConfig& config) {
            std::unordered_map<EnumPhiNoteType, std::string> nameMap = {
                { EnumPhiNoteType::Tap, "click" },
                { EnumPhiNoteType::Drag, "drag" },
                { EnumPhiNoteType::Flick, "flick" },
                { EnumPhiNoteType::Hold, "hold" }
            };

            auto name = nameMap.at(config.type);
            auto key = std::string("/notes/") + name + (config.isSimul ? "_mh.png" : ".png");
            auto data = Data::MakeFromGrain("geasy_phi/phigros" + key);

            float64 cutPadding = config.isSimul ? 100.0 : 50.0;

            return {
                .encoded = std::move(data),
                .cutPadding = Vec2 { cutPadding, cutPadding },
                .ignoreCutPadding = config.type != EnumPhiNoteType::Hold
            };
        }

        static std::vector<Data> hitEffectDataLoader() {
            std::vector<Data> result;

            for (uint64 i = 0; i < 60; i++) {
                auto key = std::string("/hittexs/") + std::to_string(i + 1) + ".png";
                result.push_back(Data::MakeFromGrain("geasy_phi/phigros" + key));
            }

            return result;
        }

        static Data hitsoundDataLoader(EnumPhiNoteType type) {
            std::unordered_map<EnumPhiNoteType, std::string> nameMap = {
                { EnumPhiNoteType::Tap, "click" },
                { EnumPhiNoteType::Drag, "drag" },
                { EnumPhiNoteType::Flick, "flick" },
                { EnumPhiNoteType::Hold, "click" }
            };

            auto name = nameMap.at(type);
            auto key = std::string("/hitsounds/") + name + ".wav";
            return Data::MakeFromGrain("geasy_phi/phigros" + key);
        }

        static bool getBuiltinShader(const std::string& name, Data& dst) {
            std::unordered_set<std::string> builtinShaders = {
                "chromatic", "circleBlur", "fisheye",
                "glitch", "grayscale", "noise",
                "pixel", "radialBlur", "shockwave", "vignette"
            };

            if (builtinShaders.contains(name)) {
                auto key = std::string("/shaders/") + name + ".glsl";
                dst = Data::MakeFromGrain("geasy_phi/phigros" + key);
                return true;
            }

            return false;
        }

        static PhiTakeOverer::ShaderDataLoader createShaderDataLoaderFromChartDir(const std::function<std::string()>& chartDirProvider) {
            return [=](const std::string& name) -> std::string {
                Data shaderText;

                if (!getBuiltinShader(name, shaderText)) {
                    auto path = PhiStoryboardHelpers::nameToPath(chartDirProvider(), name);
                    if (!Data::MakeFromFile(shaderText, path)) {
                        throw std::runtime_error(std::format("failed to read shader: {}", path));
                    }
                }

                return shaderText.toString();
            };
        }

        static PhiTakeOverer::StoryboardDataLoader createStoryboardDataLoaderFromChartDir(const std::function<std::string()>& chartDirProvider) {
            return [=](const std::string& name) -> Data {
                auto path = PhiStoryboardHelpers::nameToPath(chartDirProvider(), name);
                
                Data data;
                if (!Data::MakeFromFile(data, path)) {
                    throw std::runtime_error(std::format("failed to read storyboard: {}", path));
                }

                return data;
            };
        }

        static Data getFontData() {
            return Data::MakeFromGrain("geasy_phi/phigros/font.ttf");
        }
    };
    
    struct MilStaticResourceHelpers {
        static MilTakeOverer::LineHeadTextureLoaderResult lineHeadTextureLoader() {
            return {
                .encoded = Data::MakeFromGrain("geasy_phi/milthm/line_head.png"),
                .connectPoint = 334.0
            };
        }

        static MilTakeOverer::NoteTextureDataLoaderResult noteTextureDataLoader(const MilNoteTextureDesc& config) {
            auto type = std::get<MilNoteTextureDescAttrType>(config);
            auto isHold = std::get<MilNoteTextureDescAttrIsHold>(config);
            auto isSimul = std::get<MilNoteTextureDescAttrIsSimul>(config);
            auto isAlwaysPerfect = std::get<MilNoteTextureDescAttrIsAlwaysPerfect>(config);

            std::string name = isAlwaysPerfect ? "ex" : "";

            if (type == EnumMilNoteType::Hit) name += isHold ? "hold" : "tap";
            else name += "drag";

            if (isSimul) name += "_double";

            auto key = std::string("/notes/") + name + ".png";
            Data data;

            try {
                data = Data::MakeFromGrain("geasy_phi/milthm" + key);
            } catch (...) {}

            return {
                .encoded = std::move(data),
                .cutPadding = Vec2(668.0),
                .ignoreCutPadding = !isHold,
                .scaling = Vec2(1.77)
            };
        }

        static Data hitsoundDataLoader(EnumMilNoteType type) {
            std::unordered_map<EnumMilNoteType, std::string> nameMap = {
                { EnumMilNoteType::Hit, "hit" },
                { EnumMilNoteType::Drag, "drag" },
            };

            auto name = nameMap.at(type);
            auto key = std::string("/hitsounds/") + name + ".ogg";
            return Data::MakeFromGrain("geasy_phi/milthm" + key);
        }

        static Data getFontData() {
            return Data::MakeFromGrain("geasy_phi/milthm/font.ttf");
        }

        static Data pauseButtonTextureDataLoader() {
            return Data::MakeFromGrain("geasy_phi/milthm/pause.png");
        }
    };
}
