#ifndef CLI_SETUP_HPP
#define CLI_SETUP_HPP

#include <string>
#include <vector>
#include <iostream>
#include <algorithm>

enum class SetupMode {
    NEW_GAME,
    LOAD_LOG
};

struct GameSettings {
    SetupMode mode = SetupMode::NEW_GAME;
    int playerCount = 2;
    std::vector<std::string> playerNames = {"AI_1", "AI_2"};
    int startingMoney = 1500;
    bool enableTax = true;
    bool enableGoBonus = true;
    std::string importFilename = "";
};

class CliSetup {
public:
    static GameSettings runWizard() {
        GameSettings settings;
        std::cout << "================================================================================" << std::endl;
        std::cout << "                  MONOPOLY CLI - SETUP WIZARD                                   " << std::endl;
        std::cout << "================================================================================" << std::endl;
        std::cout << " [1] New Game" << std::endl;
        std::cout << " [2] Load & Replay Log File" << std::endl;
        std::cout << "Select Mode [1-2] (default 1): ";

        std::string line;
        if (std::getline(std::cin, line) && !line.empty()) {
            if (line == "2") {
                settings.mode = SetupMode::LOAD_LOG;
                std::cout << "Enter log filename to load: ";
                std::getline(std::cin, settings.importFilename);
                return settings;
            }
        }

        std::cout << "\nEnter number of players (2 - 10, default 2): ";
        if (std::getline(std::cin, line) && !line.empty()) {
            try {
                int count = std::stoi(line);
                if (count >= 2 && count <= 10) {
                    settings.playerCount = count;
                }
            } catch (...) {}
        }

        settings.playerNames.clear();
        int aiCount = 0;

        for (int i = 0; i < settings.playerCount; ++i) {
            std::cout << "Enter name for Player " << (i + 1) << " (blank for AI): ";
            
            if (std::getline(std::cin, line)) {
                // トリム
                line.erase(line.begin(), std::find_if(line.begin(), line.end(), [](unsigned char ch) { return !std::isspace(ch); }));
                line.erase(std::find_if(line.rbegin(), line.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(), line.end());

                if (line.empty()) {
                    // 0番目を含め、空白の場合はすべて AI_連番 とする
                    aiCount++;
                    settings.playerNames.push_back("AI_" + std::to_string(aiCount));
                } else {
                    settings.playerNames.push_back(line);
                }
            } else {
                aiCount++;
                settings.playerNames.push_back("AI_" + std::to_string(aiCount));
            }
        }

        std::cout << "Enter starting money per player in man (default 1500): ";
        if (std::getline(std::cin, line) && !line.empty()) {
            try {
                int money = std::stoi(line);
                if (money > 0) settings.startingMoney = money;
            } catch (...) {}
        }

        std::cout << "Enable GO Bonus (pass GO -> +200)? (y/n, default y): ";
        if (std::getline(std::cin, line) && !line.empty()) {
            char c = std::tolower(line[0]);
            if (c == 'n') settings.enableGoBonus = false;
        }

        std::cout << "Enable Tax squares? (y/n, default y): ";
        if (std::getline(std::cin, line) && !line.empty()) {
            char c = std::tolower(line[0]);
            if (c == 'n') settings.enableTax = false;
        }

        std::cout << "\nSetup completed successfully! Starting game...\n" << std::endl;
        return settings;
    }
};

#endif
