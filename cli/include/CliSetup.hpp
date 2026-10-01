#ifndef CLI_SETUP_HPP
#define CLI_SETUP_HPP

#include <string>
#include <vector>

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
        settings.mode = SetupMode::NEW_GAME;
        settings.playerCount = 2;
        settings.playerNames = {"Player_1", "Player_2"};
        return settings;
    }
};

#endif
