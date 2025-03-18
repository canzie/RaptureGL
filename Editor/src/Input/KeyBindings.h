#pragma once

#include <unordered_map>
#include <string>


    enum class KeyAction
    {
        MoveForward,
        MoveBackward,
        MoveLeft,
        MoveRight,
        MoveUp,
        MoveDown,
        LockCamera,
        UnlockCamera
    };

    class KeyBindings
    {
    public:
        static void init(const std::string& configFilePath = "keybindings.cfg");
        static int getKeyForAction(KeyAction action);
        
    private:
        static void loadDefaults();
        static std::unordered_map<KeyAction, int> s_keyBindings;
    };
