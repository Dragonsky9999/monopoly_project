#ifndef CLI_SETUP_HPP
#define CLI_SETUP_HPP

#include <iostream>
#include <string>
#include <vector>

enum class SetupMode {
    NEW_GAME,
    LOAD_LOG
};

struct GameSettings {
    SetupMode mode = SetupMode::NEW_GAME;
    std::string importFilename = "";

    int playerCount = 2;
    std::vector<std::string> playerNames;
    int startingMoney = 1500;
    int goBonus = 200;
    bool enableTax = true;
};

class CliSetup {
public:
    static GameSettings runWizard() {
        GameSettings config;
        std::string line;

        std::cout << "================================================================================" << std::endl;
        std::cout << "          Business Monopoly CLI - Game Setup Wizard" << std::endl;
        std::cout << "================================================================================" << std::endl;

        std::cout << "[Mode Select]\n  1: New Game\n  2: Load Log File (.log / Replay)\nChoose mode [Default: 1]: ";
        std::getline(std::cin, line);
        if (!line.empty() && line[0] == '2') {
            config.mode = SetupMode::LOAD_LOG;
            std::cout << "Enter log filename to load [Default: monopoly.log]: ";
            std::getline(std::cin, line);
            config.importFilename = line.empty() ? "monopoly.log" : line;
            return config;
        }

        std::cout << "\n[1/5] Number of players (2 - 4) [Default: 2]: ";
        std::getline(std::cin, line);
        if (!line.empty()) {
            try {
                int val = std::stoi(line);
                if (val >= 2 && val <= 4) config.playerCount = val;
            } catch (...) {}
        }

        std::cout << "\n[2/5] Player Names:" << std::endl;
        for (int i = 0; i < config.playerCount; ++i) {
            std::string defaultName = "Player_" + std::to_string(i + 1);
            std::cout << "      Player " << (i + 1) << " Name [Default: " << defaultName << "]: ";
            std::getline(std::cin, line);
            if (line.empty()) {
                config.playerNames.push_back(defaultName);
            } else {
                config.playerNames.push_back(line);
            }
        }

        std::cout << "\n[3/5] Starting money per player (万円) [Default: 1500]: ";
        std::getline(std::cin, line);
        if (!line.empty()) {
            try {
                int val = std::stoi(line);
                if (val > 0) config.startingMoney = val;
            } catch (...) {}
        }

        std::cout << "[4/5] GO Pass Bonus amount (万円) [Default: 200]: ";
        std::getline(std::cin, line);
        if (!line.empty()) {
            try {
                int val = std::stoi(line);
                if (val >= 0) config.goBonus = val;
            } catch (...) {}
        }

        std::cout << "[5/5] Enable Tax squares? (y/n) [Default: y]: ";
        std::getline(std::cin, line);
        if (!line.empty() && (line[0] == 'n' || line[0] == 'N')) {
            config.enableTax = false;
        }

        std::cout << "================================================================================" << std::endl;
        std::cout << "Setup Complete! Starting Business Monopoly...\n" << std::endl;

        return config;
    }
};

#endif
