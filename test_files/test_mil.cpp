#include "test_shared.hpp"

int main(int argc, char** argv) {
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

    for (int i = 0; i < (int)chartNames.size(); ++i) {
        std::cout << (i + 1) << ". " << chartNames[i] << std::endl;
    }

    int choice;
    std::cout << ">> ";
    std::cin >> choice;
    choice -= 1;

    if (choice < 0 || choice >= (int)chartNames.size()) {
        std::cout << "Invalid choice" << std::endl;
        return 1;
    }

    chartPath = "./test_files/mil/" + chartNames[choice] + "/.json";
    imagePath = "./test_files/mil/" + chartNames[choice] + "/.png";
    audioPath = "./test_files/mil/" + chartNames[choice] + "/.wav";
    storyboardAssetsPath = "./test_files/mil/" + chartNames[choice];

    MilWindow window {};
    return 0;
}
