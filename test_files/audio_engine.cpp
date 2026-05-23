#define EASY_PHI_MINIAUDIO_AUDIO_ENGINE
#define EASY_PHI_MINIAUDIO_AUDIO_ENGINE_NO_MINIAUDIO_IMPL
#include <easy_phi.hpp>

#include "./resources/inlined_resources.cpp"

using namespace easy_phi;

int main() {
    auto data = StaticResource::get("/test.mp3");

    auto decoded = decodeAudioMiniaudio(data);
    std::cout << "decoded audio: " << decoded.get() << std::endl;

    auto engine = makeAudioEngineMiniaudio();
    std::cout << "engine: " << engine.get() << std::endl;

    engine->createTask(decoded);
    std::cout << "task created" << std::endl;

    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }

    return 0;
}
