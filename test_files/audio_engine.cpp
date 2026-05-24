#define EASY_PHI_MINIAUDIO_AUDIO_ENGINE
#include <easy_phi.hpp>

#include "./resources/inlined_resources.cpp"

using namespace easy_phi;

int main() {
    auto data = StaticResource::get("/test.ogg");

    auto decoded = decodeAudioMiniaudio(data);
    std::cout << "decoded audio: " << decoded.get() << std::endl;

    auto engine = makeAudioEngineMiniaudio();
    std::cout << "engine: " << engine.get() << std::endl;

    auto task = engine->createTask(decoded);
    std::cout << "task created: " << task.get() << std::endl;

    while (!engine->getTaskEnded(task)) {
        std::cout << "time: " << engine->getTaskTime(task) << std::endl;
    }

    return 0;
}
