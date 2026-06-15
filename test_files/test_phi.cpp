#include "test_shared.hpp"

int main(int argc, char** argv) {
    std::vector<std::string> args(argv, argv + argc);
    auto hasArg = [&](const std::string& arg) {
        return std::find(args.begin(), args.end(), arg) != args.end();
    };

    std::string chartPath, imagePath, audioPath, chartTitle, chartDifficulty, storyboardAssetsPath;
    
    std::cout << "1. fv1" << std::endl;
    std::cout << "3. fv3" << std::endl;
    std::cout << "4. rpe" << std::endl;
    std::cout << "5. rpe_2" << std::endl;
    std::cout << "6. rpe_3" << std::endl;
    std::cout << "7. rpe_4" << std::endl;
    std::cout << "8. rpe_5" << std::endl;
    std::cout << "9. pec" << std::endl;

    int choice;
    std::cout << ">> ";
    std::cin >> choice;

    switch (choice) {
        case 1: {
            chartPath = "./test_files/fv1/chart.json";
            imagePath = "./test_files/fv1/image.png";
            audioPath = "./test_files/fv1/audio.mp3";
            chartTitle = "Aleph-0";
            chartDifficulty = "Legacy  Lv.15";
            break;
        }

        case 3: {
            chartPath = "./test_files/fv3/chart.json";
            imagePath = "./test_files/fv3/image.png";
            audioPath = "./test_files/fv3/audio.mp3";
            chartTitle = "Alice in a xxxxxxxx";
            chartDifficulty = "IN  Lv.15";
            break;
        }

        case 4: {
            chartPath = "./test_files/rpe/chart.json";
            imagePath = "./test_files/rpe/image.png";
            audioPath = "./test_files/rpe/audio.mp3";
            storyboardAssetsPath = "./test_files/rpe";
            break;
        }

        case 5: {
            chartPath = "./test_files/rpe_2/chart.json";
            imagePath = "./test_files/rpe_2/image.png";
            audioPath = "./test_files/rpe_2/audio.mp3";
            storyboardAssetsPath = "./test_files/rpe_2";
            break;
        }

        case 6: {
            chartPath = "./test_files/rpe_3/chart.json";
            imagePath = "./test_files/rpe_3/image.png";
            audioPath = "./test_files/rpe_3/audio.mp3";
            storyboardAssetsPath = "./test_files/rpe_3";
            break;
        }

        case 7: {
            chartPath = "./test_files/rpe_4/chart.json";
            imagePath = "./test_files/rpe_4/image.png";
            audioPath = "./test_files/rpe_4/audio.mp3";
            storyboardAssetsPath = "./test_files/rpe_4";
            break;
        }

        case 8: {
            chartPath = "./test_files/rpe_5/chart.json";
            imagePath = "./test_files/rpe_5/image.png";
            audioPath = "./test_files/rpe_5/audio.mp3";
            storyboardAssetsPath = "./test_files/rpe_5";
            break;
        }

        case 9: {
            chartPath = "./test_files/pec/chart.pec";
            imagePath = "./test_files/pec/image.png";
            audioPath = "./test_files/pec/audio.mp3";
            storyboardAssetsPath = "./test_files/pec";
            break;
        }

        default: {
            std::cout << "Invalid choice" << std::endl;
            return 1;
        }
    }

    PhiWindow window {};
    window.base.fullscreen = hasArg("--fullscreen");
    window.base.setVSync(!hasArg("--disable-vsync"));
    window.init();

    window.loadChart(chartPath, storyboardAssetsPath);

    if (window.renderer->chart.meta.title.empty()) {
        window.renderer->chart.meta.title = chartTitle;
        window.renderer->chart.meta.difficulty = chartDifficulty;
    }

    window.renderer->loadIllustion(imagePath);
    window.renderer->audioManager.load(audioPath);

    if (hasArg("--bench")) {
        auto data = JsonNode::MakeArray();

        ep_f64 t = 0.0;
        while (t < window.renderer->audioManager.getBgmLength()) {
            auto& frameInfo = window.renderer->render({ .base = { .time = t } });
            data.getArray().push_back(JsonNode::MakeNumber(frameInfo.base.calculatedTook * 1000));
            t += 1.0 / 120.0;
        }

        data.print();

        return 0;
    }

    window.renderer->audioManager.startBgm();

    while (!window.renderer->audioManager.getBpmIsEnded()) {
        if (!window.mainloopFrame({})) {
            break;
        }
    }

    return 0;
}
