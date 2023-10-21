#pragma once
class CGameRules
{
public:
    char pad0[68];                              // 0    | 68 bytes   | 68
    char pad1[1];                               // 72   | 1 byte     | 73
    int roundmode;                             // 68   | 4 bytes   | 72
    bool m_bInSetup;                            // 73   | 1 byte     | 74
    char pad2[1];                              // 75   | 1 byte     | 76
    bool isPVEMode;                              // 1054 | 1 byte    | 1055
    int winning_team;                            // 76   | 4 bytes   | 80
    char pad3[4];                                // 80   | 4 bytes    | 84
    bool m_bInWaitingForPlayers;                // 84   | 1 byte     | 85
    bool bool_pad0[3];                          // 85   | 3 bytes    | 88
    int isUsingSpells;                           // 1070 | 4 bytes   | 1074
    char pad4[1037];                            // 88   | 1037 bytes | 1125
    char bool_pad1[3];                          // 1127 | 3 bytes    | 1130
    char bool_pad2[3];                          // 1131 | 3 bytes    | 1134
    char pad5[8];                               // 1134 | 8 bytes    | 1142
    char bool_pad3[2];                          // 1144 | 2 bytes    | 1146
    char pad6[1062];                            // 1146 | 1062 bytes | 2208
    int halloweenScenario;            // 1864 | 4 bytes   | 1868

    bool isUsingSpells_fn()
    {
        auto tf_spells_enabled = g_ICvar->FindVar("tf_spells_enabled");
        if (tf_spells_enabled->GetBool())
            return true;

        // Hightower
        if (halloweenScenario == 4)
            return true;

        return isUsingSpells;
    }
} __attribute__((packed));
