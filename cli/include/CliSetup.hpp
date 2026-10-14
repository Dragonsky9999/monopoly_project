#ifndef CLI_SETUP_HPP
#define CLI_SETUP_HPP

#include "GameState.hpp"
#include <algorithm>
#include <cctype>
#include <iostream>
#include <string>
#include <vector>

enum class SetupMode { NEW_GAME, LOAD_LOG };

struct GameSettings {
    SetupMode mode = SetupMode::NEW_GAME;
    std::vector<std::string> playerNames;
    Settings rules;
    std::string importFilename;
};

class CliSetup {
public:
    static std::string trim(std::string s) {
        auto ns = [](unsigned char ch) { return !std::isspace(ch); };
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), ns));
        s.erase(std::find_if(s.rbegin(), s.rend(), ns).base(), s.end());
        return s;
    }

    static GameSettings runWizard() {
        GameSettings settings;
        std::string line;
        std::cout << "================================================================================\n";
        std::cout << "                  MONOPOLY CLI - SETUP WIZARD\n";
        std::cout << "================================================================================\n";
        std::cout << " [1] New Game\n";
        std::cout << " [2] Load & Replay Log File\n";
        std::cout << "Select Mode [1-2] (default 1): ";
        if (std::getline(std::cin, line) && trim(line) == "2") {
            settings.mode = SetupMode::LOAD_LOG;
            std::cout << "Enter log filename to load: ";
            std::getline(std::cin, settings.importFilename);
            settings.importFilename = trim(settings.importFilename);
            // ドラッグ&ドロップ時の "..." を外す
            if (settings.importFilename.size() >= 2 && settings.importFilename.front() == '"' &&
                settings.importFilename.back() == '"')
                settings.importFilename = settings.importFilename.substr(1, settings.importFilename.size() - 2);
            return settings;
        }

        int playerCount = 2;
        std::cout << "\nEnter number of players (2 - 10, default 2): ";
        if (std::getline(std::cin, line) && !trim(line).empty()) {
            try {
                int c = std::stoi(line);
                if (c >= 2 && c <= 10) playerCount = c;
            } catch (...) {}
        }

        int aiCount = 0;
        for (int i = 0; i < playerCount; ++i) {
            std::cout << "Enter name for Player " << i << " (blank for AI): ";
            std::string name;
            if (std::getline(std::cin, line)) name = trim(line);
            for (char& ch : name)
                if (std::isspace(static_cast<unsigned char>(ch)) || ch == '=') ch = '_';  // ログ形式・コマンド解析のため
            if (name.empty()) name = "AI_" + std::to_string(++aiCount);
            settings.playerNames.push_back(name);
        }

        std::cout << "Starting money per player in man (default 1500): ";
        if (std::getline(std::cin, line) && !trim(line).empty()) {
            try {
                int m = std::stoi(line);
                if (m > 0) settings.rules.startMoney = m;
            } catch (...) {}
        }

        std::cout << "GO bonus amount (0 = off, default 200): ";
        if (std::getline(std::cin, line) && !trim(line).empty()) {
            try {
                int m = std::stoi(line);
                if (m >= 0) settings.rules.goBonus = m;
            } catch (...) {}
        }

        std::cout << "Enable Tax squares? (y/n, default y): ";
        if (std::getline(std::cin, line) && !trim(line).empty() && std::tolower(static_cast<unsigned char>(trim(line)[0])) == 'n')
            settings.rules.taxEnabled = false;

        std::cout << "\nSetup completed. Players whose name starts with \"AI_\" are played by the computer.\n";
        std::cout << "Type 'help' for commands.\n" << std::endl;
        return settings;
    }
};

#endif
