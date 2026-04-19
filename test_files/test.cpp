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

        default: {
            std::cout << "Invalid choice" << std::endl;
            return 1;
        }
    }

    Window window {};
    window.init();
    window.vsync = !hasArg("--disable-vsync");
    if (hasArg("--extend-scale")) window.globalScale = 0.25;

    window.loadChart(chartPath, storyboardAssetsPath);

    if (window.chart.meta.title.empty()) {
        window.chart.meta.title = chartTitle;
        window.chart.meta.difficulty = chartDifficulty;
    }

    window.loadBgImage(imagePath);
    window.loadMainSound(audioPath);
    window.startMainSound();

    while (ma_sound_is_playing(window.mainSound)) {
        double t = getMaSoundPosition(window.mainSound);

        if (!window.mainloopFrame(t)) {
            break;
        }
    }

    return 0;
}
