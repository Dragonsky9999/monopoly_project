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
    std::vector<std::string> playerNames = {"Player_1", "Player_2"};
    int startingMoney = 1500;
    bool enableTax = true;
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
        for (int i = 0; i < settings.playerCount; ++i) {
            std::cout << "Enter name for Player " << (i + 1) << " (e.g. Player_" << (i + 1) << " or AI_Bot_" << (i + 1) << "): ";
            if (std::getline(std::cin, line) && !line.empty()) {
                settings.playerNames.push_back(line);
            } else {
                settings.playerNames.push_back("Player_" + std::to_string(i + 1));
            }
        }

        std::cout << "Enter starting money per player in 万 (default 1500): ";
        if (std::getline(std::cin, line) && !line.empty()) {
            try {
                int money = std::stoi(line);
                if (money > 0) settings.startingMoney = money;
            } catch (...) {}
        }

        std::cout << "Enable Tax squares & GO bonus rules? (y/n, default y): ";
        if (std::getline(std::cin, line) && !line.empty()) {
            char c = std::tolower(line[0]);
            if (c == 'n') settings.enableTax = false;
        }

        std::cout << "\nSetup completed successfully! Starting game...\n" << std::endl;
        return settings;
    }
};

#endif
