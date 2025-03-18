#include "KeyBindings.h"
#include <fstream>
#include <sstream>
#include "Logger/Log.h"


    std::unordered_map<KeyAction, int> KeyBindings::s_keyBindings;

    void KeyBindings::init(const std::string& configFilePath)
    {
        // Load defaults first
        loadDefaults();
        
        // Try to open the config file
        std::ifstream configFile(configFilePath);
        if (!configFile.is_open())
        {
            Rapture::GE_WARN("Could not open keybindings config file: {0}. Using defaults.", configFilePath);
            return;
        }
        
        // Parse the config file
        std::string line;
        while (std::getline(configFile, line))
        {
            // Skip comments and empty lines
            if (line.empty() || line[0] == '#' || line[0] == ';')
                continue;
                
            std::istringstream iss(line);
            std::string actionStr;
            int keyCode;
            
            if (iss >> actionStr >> keyCode)
            {
                if (actionStr == "MOVE_FORWARD")
                    s_keyBindings[KeyAction::MoveForward] = keyCode;
                else if (actionStr == "MOVE_BACKWARD")
                    s_keyBindings[KeyAction::MoveBackward] = keyCode;
                else if (actionStr == "MOVE_LEFT")
                    s_keyBindings[KeyAction::MoveLeft] = keyCode;
                else if (actionStr == "MOVE_RIGHT")
                    s_keyBindings[KeyAction::MoveRight] = keyCode;
                else if (actionStr == "MOVE_UP")
                    s_keyBindings[KeyAction::MoveUp] = keyCode;
                else if (actionStr == "MOVE_DOWN")
                    s_keyBindings[KeyAction::MoveDown] = keyCode;
                else if (actionStr == "LOCK_CAMERA")
                    s_keyBindings[KeyAction::LockCamera] = keyCode;
                else if (actionStr == "UNLOCK_CAMERA")
                    s_keyBindings[KeyAction::UnlockCamera] = keyCode;
                else
                    Rapture::GE_WARN("Unknown action in keybindings config: {0}", actionStr);
            }
        }
    }
    
    int KeyBindings::getKeyForAction(KeyAction action)
    {
        auto it = s_keyBindings.find(action);
        if (it != s_keyBindings.end())
            return it->second;
            
        // Return 0 if not found (no key)
        return 0;
    }
    
    void KeyBindings::loadDefaults()
    {
        // WASD for movement
        s_keyBindings[KeyAction::MoveForward] = 87;
        s_keyBindings[KeyAction::MoveBackward] = 83;
        s_keyBindings[KeyAction::MoveLeft] = 65;
        s_keyBindings[KeyAction::MoveRight] = 68;
        
        // Space and Shift for up/down
        s_keyBindings[KeyAction::MoveUp] = 32;
        s_keyBindings[KeyAction::MoveDown] = 340;
        
        // 1 for lock, Escape for unlock
        s_keyBindings[KeyAction::LockCamera] = 49;
        s_keyBindings[KeyAction::UnlockCamera] = 256;
    }
