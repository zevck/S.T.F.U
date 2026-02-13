#include "Config.h"
#include <yaml-cpp/yaml.h>
#include <filesystem>
#include <unordered_map>

namespace Config
{
    static Settings g_settings;
    
    // Auto-generated vanilla Skyrim subtype corrections
    // These topics have incorrect DATA.subtype - SNAM is authoritative
    // Total corrections: 596
    static std::unordered_map<uint32_t, uint16_t> VanillaSubtypeCorrections = {
        {0x00003337, 26}, {0x00003499, 79}, {0x0000349F, 79}, {0x000034A2, 89}, {0x000034A3, 26},
        {0x000034A7, 89}, {0x000034AA, 26}, {0x000034AE, 79}, {0x000039A8, 94}, {0x00004212, 79},
        {0x00004C81, 94}, {0x00004C82, 79}, {0x00004C90, 108}, {0x00004C91, 109}, {0x00004C92, 110},
        {0x00004C93, 111}, {0x00004C94, 105}, {0x00004C95, 62}, {0x00004C96, 106}, {0x00004C97, 96},
        {0x00004C9F, 93}, {0x00004CA0, 95}, {0x00004CA2, 89}, {0x00004CA3, 65}, {0x00004CA8, 73},
        {0x00004CAC, 26}, {0x00004DAA, 79}, {0x00004DCA, 79}, {0x00005853, 79}, {0x00005D7C, 79},
        {0x0000652C, 79}, {0x000069A2, 79}, {0x0000701B, 79}, {0x000098B1, 79}, {0x0000A83F, 79},
        {0x0000CF00, 26}, {0x0000CF03, 65}, {0x0000CF09, 79}, {0x0000D04D, 79}, {0x0000E89A, 79},
        {0x00010DAF, 89}, {0x00010DB0, 89}, {0x00010DB1, 89}, {0x00011959, 79}, {0x000133A0, 94},
        {0x00013A9E, 79}, {0x00013EE3, 26}, {0x00013F30, 73}, {0x00014037, 79}, {0x00014128, 79},
        {0x00014295, 79}, {0x00014340, 79}, {0x00014599, 79}, {0x00014BA4, 79}, {0x00014F96, 89},
        {0x00014F99, 73}, {0x00014F9D, 26}, {0x0001577B, 79}, {0x000159F0, 79}, {0x00015A4C, 79},
        {0x00015C67, 79}, {0x00015C87, 79}, {0x00015F98, 79}, {0x00015FE8, 79}, {0x00016048, 79},
        {0x0001630C, 89}, {0x000167D8, 89}, {0x000167DE, 26}, {0x00016828, 79}, {0x0001793B, 62},
        {0x0001793E, 93}, {0x0001793F, 95}, {0x00017941, 89}, {0x00017942, 65}, {0x00017947, 73},
        {0x0001794B, 26}, {0x00017E67, 79}, {0x00017E6A, 79}, {0x000190A6, 79}, {0x000190A7, 79},
        {0x00019391, 79}, {0x0001955D, 79}, {0x0001956A, 79}, {0x000195AB, 94}, {0x000195AC, 79},
        {0x000195B8, 89}, {0x000195BD, 26}, {0x0001990E, 79}, {0x00019B69, 94}, {0x00019B6C, 79},
        {0x00019B6E, 105}, {0x00019B6F, 62}, {0x00019B70, 106}, {0x00019B71, 96}, {0x00019B79, 93},
        {0x00019B7A, 95}, {0x00019B7C, 89}, {0x00019B7D, 65}, {0x00019B82, 73}, {0x00019B86, 26},
        {0x00019B93, 79}, {0x00019FCA, 79}, {0x0001A1ED, 79}, {0x0001A4D7, 79}, {0x0001A912, 79},
        {0x0001A916, 79}, {0x0001AA60, 79}, {0x0001AA86, 79}, {0x0001AA8F, 79}, {0x0001B628, 79},
        {0x0001B62C, 79}, {0x0001BB2D, 79}, {0x0001BB96, 79}, {0x0001BFB2, 79}, {0x0001C482, 79},
        {0x0001C5E9, 79}, {0x0001C5F1, 79}, {0x0001CDBC, 79}, {0x0001D8C8, 79}, {0x0001D927, 79},
        {0x0001DA2F, 79}, {0x0001DD1C, 79}, {0x0001DF87, 108}, {0x0001DF8A, 111}, {0x0001EE9A, 79},
        {0x0001F181, 79}, {0x0001F332, 79}, {0x0001F6C9, 79}, {0x0001F7F5, 79}, {0x0001F83C, 79},
        {0x0002009A, 89}, {0x0002009B, 26}, {0x0002071F, 79}, {0x00020971, 89}, {0x00020B97, 79},
        {0x00020EA7, 79}, {0x000211F7, 79}, {0x0002124E, 79}, {0x0002124F, 79}, {0x000212CA, 79},
        {0x0002137B, 79}, {0x000215AE, 79}, {0x00021A64, 79}, {0x00022435, 79}, {0x00022662, 79},
        {0x0002287A, 79}, {0x000228A5, 79}, {0x00022ED2, 79}, {0x00022F2B, 79}, {0x00023704, 79},
        {0x00023C6F, 89}, {0x00023DFC, 79}, {0x00023F5F, 79}, {0x0002410C, 79}, {0x000241F9, 79},
        {0x00024313, 79}, {0x0002438F, 94}, {0x00024C98, 62}, {0x00024C99, 89}, {0x00024C9B, 73},
        {0x00024C9E, 26}, {0x00025137, 79}, {0x00025154, 79}, {0x0002523D, 79}, {0x00025737, 94},
        {0x00025D3C, 79}, {0x00025D4A, 79}, {0x00025F56, 79}, {0x000261E5, 119}, {0x000261EF, 94},
        {0x000261F2, 94}, {0x00026540, 79}, {0x000266C7, 26}, {0x000266C9, 73}, {0x000266CA, 62},
        {0x000266CE, 95}, {0x000266D1, 106}, {0x000266D2, 96}, {0x000266D6, 65}, {0x000266D8, 89},
        {0x000266D9, 105}, {0x000268D9, 79}, {0x00026AF5, 79}, {0x00026BD2, 79}, {0x00026DF9, 79},
        {0x00026DFC, 79}, {0x0002707A, 79}, {0x000270FC, 79}, {0x000271F0, 79}, {0x0002748D, 79},
        {0x0002753A, 79}, {0x00027577, 79}, {0x00027809, 79}, {0x00027DE5, 89}, {0x00027DF2, 79},
        {0x00027DF3, 94}, {0x00027E8C, 79}, {0x00027F90, 89}, {0x000281FA, 26}, {0x000284F6, 79},
        {0x00028597, 89}, {0x00028599, 26}, {0x0002865F, 79}, {0x000286FA, 89}, {0x000289DD, 79},
        {0x00028A57, 79}, {0x00028D1D, 79}, {0x00028DD7, 79}, {0x00029263, 26}, {0x00029265, 79},
        {0x00029388, 62}, {0x0002938A, 73}, {0x0002972B, 79}, {0x00029A44, 89}, {0x00029A47, 26},
        {0x0002A457, 62}, {0x0002A45B, 95}, {0x0002A574, 79}, {0x0002A87F, 79}, {0x0002A8EA, 79},
        {0x0002A977, 79}, {0x0002A982, 26}, {0x0002ABAF, 79}, {0x0002ABB0, 79}, {0x0002ABB1, 79},
        {0x0002ABB2, 79}, {0x0002ABB3, 79}, {0x0002ABB4, 79}, {0x0002B04C, 79}, {0x0002B8A6, 79},
        {0x0002B9FD, 89}, {0x0002BA01, 73}, {0x0002BA05, 26}, {0x0002BA4B, 79}, {0x0002BC40, 89},
        {0x0002BDDD, 79}, {0x0002BF82, 94}, {0x0002C03D, 79}, {0x0002C15E, 104}, {0x0002C15F, 104},
        {0x0002C2F7, 79}, {0x0002C30D, 26}, {0x0002C30E, 79}, {0x0002C42F, 89}, {0x0002C489, 79},
        {0x0002CCC0, 79}, {0x0002E0EC, 79}, {0x0002E0ED, 79}, {0x0002E0EF, 79}, {0x0002E136, 79},
        {0x0002E13B, 79}, {0x0002E33D, 89}, {0x0002E470, 79}, {0x0002E6D6, 79}, {0x0002E7AD, 79},
        {0x0002E8E0, 79}, {0x0002EC23, 79}, {0x0002EC29, 79}, {0x00030093, 79}, {0x000300E6, 79},
        {0x00030257, 79}, {0x0003029D, 79}, {0x00030304, 79}, {0x0003030D, 79}, {0x000303CE, 79},
        {0x0003054B, 79}, {0x00032D54, 79}, {0x000330D9, 79}, {0x0003360F, 79}, {0x00034B31, 79},
        {0x00034C1F, 79}, {0x00034F93, 79}, {0x00035253, 26}, {0x000352D6, 79}, {0x00035E07, 79},
        {0x00036190, 79}, {0x0003626B, 79}, {0x00036F38, 96}, {0x00037B3A, 79}, {0x00037B95, 79},
        {0x00037D67, 79}, {0x00038730, 79}, {0x00038731, 79}, {0x00038A2D, 94}, {0x00039072, 79},
        {0x00039073, 79}, {0x0003916B, 79}, {0x00039622, 79}, {0x0003982E, 26}, {0x00039C2A, 106},
        {0x00039C2B, 96}, {0x00039F68, 79}, {0x0003A8AD, 79}, {0x0003A8AE, 79}, {0x0003B683, 79},
        {0x0003B68E, 79}, {0x0003B997, 79}, {0x0003C79B, 79}, {0x0003CF0D, 79}, {0x0003DD26, 79},
        {0x0003E248, 65}, {0x0003E99F, 65}, {0x0003F02A, 79}, {0x0003F02B, 79}, {0x000402FA, 79},
        {0x00040760, 79}, {0x00042651, 79}, {0x00043CF9, 79}, {0x0004461B, 79}, {0x000453E9, 79},
        {0x00046648, 89}, {0x00046649, 89}, {0x0004664A, 65}, {0x000466A6, 26}, {0x00046FA4, 26},
        {0x00047614, 26}, {0x000480EE, 79}, {0x00048C86, 79}, {0x0004B856, 79}, {0x0004BC92, 79},
        {0x0004C2D2, 26}, {0x0004C493, 89}, {0x0004C592, 26}, {0x0004D1DA, 79}, {0x0004D5FF, 79},
        {0x0004D782, 79}, {0x0004FD4A, 79}, {0x000513F6, 79}, {0x00054E49, 79}, {0x000550CC, 79},
        {0x00055DEB, 94}, {0x000587EE, 79}, {0x0005B1B0, 79}, {0x0005B2C9, 79}, {0x0005B2D2, 79},
        {0x0005D2DE, 79}, {0x0006133F, 79}, {0x00061F99, 62}, {0x0006429A, 73}, {0x00064771, 79},
        {0x000648EE, 79}, {0x000649CF, 79}, {0x00064AF8, 79}, {0x00064E78, 79}, {0x00065A1B, 79},
        {0x00065AD8, 79}, {0x00067CBF, 79}, {0x00067EBB, 94}, {0x00068373, 79}, {0x00068932, 79},
        {0x00069F8E, 79}, {0x0006A007, 26}, {0x0006AA57, 26}, {0x0006AEA3, 94}, {0x0006B135, 79},
        {0x0006B1A6, 62}, {0x0006B940, 79}, {0x0006C806, 79}, {0x0006CCD8, 26}, {0x0006E010, 26},
        {0x0006EFED, 79}, {0x0006F3E8, 79}, {0x0006F411, 73}, {0x0006F415, 26}, {0x0006F95B, 89},
        {0x0006F95C, 89}, {0x00072BC2, 79}, {0x00073E72, 26}, {0x00073FB8, 79}, {0x0007467A, 26},
        {0x0007734B, 79}, {0x00077355, 79}, {0x0007741F, 79}, {0x00077C21, 79}, {0x0007D426, 79},
        {0x0007D931, 79}, {0x0008306C, 79}, {0x000847E3, 79}, {0x000848F2, 94}, {0x00084967, 94},
        {0x00084969, 79}, {0x0008548A, 94}, {0x00085495, 89}, {0x0008CDD2, 79}, {0x0008EB7B, 79},
        {0x00092A3F, 79}, {0x00093A67, 79}, {0x00093FD5, 62}, {0x00094032, 79}, {0x000940E9, 89},
        {0x000956AF, 79}, {0x000958B0, 79}, {0x00095C29, 79}, {0x00096FC4, 79}, {0x000993B8, 79},
        {0x000996D9, 79}, {0x00099CF1, 94}, {0x0009D6CE, 79}, {0x0009DE41, 79}, {0x0009DEA7, 62},
        {0x0009DEA8, 94}, {0x000A1792, 79}, {0x000A2004, 79}, {0x000A27A5, 26}, {0x000A27AC, 79},
        {0x000A2C2F, 79}, {0x000A3C58, 94}, {0x000A3C59, 89}, {0x000A6CAA, 79}, {0x000A7FD8, 79},
        {0x000A7FE0, 79}, {0x000A85FC, 79}, {0x000A85FD, 79}, {0x000A85FE, 79}, {0x000A87D6, 26},
        {0x000A95D9, 79}, {0x000A9B1B, 79}, {0x000AA050, 73}, {0x000AB3AF, 79}, {0x000AB813, 79},
        {0x000AC973, 79}, {0x000AC975, 79}, {0x000AD07D, 79}, {0x000AD0CE, 79}, {0x000AD233, 79},
        {0x000ADE50, 79}, {0x000AE755, 73}, {0x000AE769, 79}, {0x000AF1B3, 79}, {0x000AF253, 89},
        {0x000AF255, 89}, {0x000B026A, 79}, {0x000B0383, 79}, {0x000B0E7F, 26}, {0x000B122F, 79},
        {0x000B2967, 79}, {0x000B31DD, 79}, {0x000B48F4, 73}, {0x000B6BD8, 79}, {0x000B70E3, 79},
        {0x000B7149, 79}, {0x000B7AA1, 79}, {0x000B809E, 79}, {0x000B82F8, 89}, {0x000B83B8, 79},
        {0x000B85AF, 79}, {0x000B8CEA, 89}, {0x000B90B9, 79}, {0x000B96C3, 79}, {0x000B9BC5, 79},
        {0x000B9BC7, 62}, {0x000B9BCA, 62}, {0x000BA0CD, 79}, {0x000BA2F6, 26}, {0x000BA2F7, 89},
        {0x000BA30C, 94}, {0x000BA521, 79}, {0x000BB367, 79}, {0x000BBA85, 62}, {0x000BBE01, 26},
        {0x000BC0AE, 79}, {0x000BC4C6, 26}, {0x000BC4C9, 89}, {0x000BCCBE, 79}, {0x000BCF0C, 89},
        {0x000BD177, 79}, {0x000BD730, 79}, {0x000BD73D, 79}, {0x000BD73E, 79}, {0x000BF20C, 79},
        {0x000C00EE, 62}, {0x000C016B, 26}, {0x000C04E7, 79}, {0x000C07AB, 79}, {0x000C083E, 79},
        {0x000C1A0A, 26}, {0x000C1A0F, 79}, {0x000C1A10, 26}, {0x000C1DFA, 79}, {0x000C1E17, 79},
        {0x000C275E, 26}, {0x000C3519, 79}, {0x000C4A75, 62}, {0x000C4B14, 79}, {0x000C62A7, 89},
        {0x000C6500, 79}, {0x000C725D, 79}, {0x000C96A4, 89}, {0x000C9809, 79}, {0x000CA62F, 89},
        {0x000CAB04, 79}, {0x000CB1B6, 79}, {0x000CB268, 73}, {0x000CC82F, 79}, {0x000CCFBF, 79},
        {0x000CD107, 94}, {0x000CDDEE, 89}, {0x000CEDFD, 79}, {0x000CEED9, 79}, {0x000D0736, 89},
        {0x000D11FF, 94}, {0x000D2B75, 119}, {0x000D2B81, 97}, {0x000D2B87, 94}, {0x000D2CD9, 89},
        {0x000D3BC3, 79}, {0x000D3C50, 79}, {0x000D42BD, 79}, {0x000D4FA7, 79}, {0x000D5583, 62},
        {0x000D5F10, 79}, {0x000D6602, 79}, {0x000D69A1, 79}, {0x000D69B2, 94}, {0x000D6C50, 94},
        {0x000D74CB, 94}, {0x000D76E3, 79}, {0x000D76F8, 79}, {0x000D8304, 79}, {0x000D83DF, 94},
        {0x000D8803, 79}, {0x000D8848, 94}, {0x000D8849, 79}, {0x000D8BE7, 79}, {0x000D8BEA, 79},
        {0x000D8E23, 79}, {0x000D8E57, 94}, {0x000D931C, 26}, {0x000D9387, 79}, {0x000D93D4, 79},
        {0x000D93D6, 79}, {0x000DAB65, 62}, {0x000DB285, 79}, {0x000DB366, 79}, {0x000DB865, 79},
        {0x000DB9FD, 89}, {0x000DD616, 79}, {0x000DDCD8, 94}, {0x000DDF7D, 79}, {0x000DEE84, 79},
        {0x000DFE3B, 89}, {0x000E16E3, 79}, {0x000E16E4, 89}, {0x000E1EE1, 79}, {0x000E2BDF, 79},
        {0x000E35F3, 79}, {0x000E3F47, 79}, {0x000E6528, 79}, {0x000E66BC, 79}, {0x000E67DA, 79},
        {0x000E6CA6, 79}, {0x000E782E, 94}, {0x000E782F, 94}, {0x000E88F6, 105}, {0x000EC394, 108},
        {0x000EC395, 110}, {0x000ECC75, 89}, {0x000ED358, 89}, {0x000EDF46, 26}, {0x000F1A23, 73},
        {0x000F1A8A, 105}, {0x000F1C66, 79}, {0x000F340B, 65}, {0x000F5366, 26}, {0x000F7B2A, 89},
        {0x000F9077, 79}, {0x000F935C, 79}, {0x000FD819, 94}, {0x00100719, 65}, {0x0010071A, 89},
        {0x00100720, 26}, {0x001007E8, 79}, {0x00101ACC, 89}, {0x001027D3, 94}, {0x0010321B, 94},
        {0x0010321D, 94}, {0x001034D3, 79}, {0x00105D09, 79}, {0x0010600D, 79}, {0x001063D0, 89},
        {0x001065E6, 79}, {0x001065E9, 79}, {0x00106636, 79}, {0x00106A3C, 79}, {0x00107704, 94},
        {0x00107705, 79}, {0x00108ED0, 79}, {0x001092BE, 26}, {0x001092C0, 89}, {0x001095CA, 94},
        {0x00109C3F, 119}, {0x00109C42, 26}, {0x0010AA5A, 94}, {0x0010D96D, 79}, {0x0010FEE0, 65},
        {0x0010FEE1, 89}
    };
    
    // Map user-friendly subtype names to enum values
    static std::unordered_map<std::string, uint16_t> SubtypeNameMap = {
        {"Custom", 0}, {"ForceGreet", 1}, {"Rumors", 2}, {"Intimidate", 4}, {"Flatter", 5},
        {"Bribe", 6}, {"AskGift", 7}, {"Gift", 8}, {"AskFavor", 9}, {"Favor", 10},
        {"ShowRelationships", 11}, {"Follow", 12}, {"Reject", 13}, {"Scene", 14}, {"Show", 15},
        {"Agree", 16}, {"Refuse", 17}, {"ExitFavorState", 18}, {"MoralRefusal", 19},
        {"Attack", 26}, {"PowerAttack", 27}, {"Bash", 28}, {"Hit", 29}, {"Flee", 30},
        {"Bleedout", 31}, {"AvoidThreat", 32}, {"Death", 33}, {"Block", 35}, {"Taunt", 36},
        {"AllyKilled", 37}, {"Steal", 38}, {"Yield", 39}, {"AcceptYield", 40},
        {"PickpocketCombat", 41}, {"Assault", 42}, {"Murder", 43}, {"AssaultNPC", 44},
        {"MurderNPC", 45}, {"PickpocketNPC", 46}, {"StealFromNPC", 47},
        {"TrespassAgainstNPC", 48}, {"Trespass", 49}, {"WereTransformCrime", 50},
        {"VoicePowerStartShort", 51}, {"VoicePowerStartLong", 52},
        {"VoicePowerEndShort", 53}, {"VoicePowerEndLong", 54},
        {"AlertIdle", 55}, {"LostIdle", 56}, {"NormalToAlert", 57},
        {"AlertToCombat", 58}, {"NormalToCombat", 59}, {"AlertToNormal", 60},
        {"CombatToNormal", 61}, {"CombatToLost", 62}, {"LostToNormal", 63},
        {"LostToCombat", 64}, {"DetectFriendDie", 65}, {"ServiceRefusal", 66},
        {"Repair", 67}, {"Travel", 68}, {"Training", 69}, {"BarterExit", 70},
        {"RepairExit", 71}, {"Recharge", 72}, {"RechargeExit", 73}, {"TrainingExit", 74},
        {"ObserveCombat", 75}, {"NoticeCorpse", 76}, {"TimeToGo", 77}, {"Goodbye", 78},
        {"Hello", 79}, {"SwingMeleeWeapon", 80}, {"ShootBow", 81}, {"ZKeyObject", 82},
        {"Jump", 83}, {"KnockOverObject", 84}, {"DestroyObject", 85},
        {"StandOnFurniture", 86}, {"LockedObject", 87}, {"PickpocketTopic", 88},
        {"PursueIdleTopic", 89}, {"SharedInfo", 90}, {"PlayerCastProjectileSpell", 91},
        {"PlayerCastSelfSpell", 92}, {"PlayerShout", 93}, {"Idle", 94},
        {"EnterSprintBreath", 95}, {"EnterBowZoomBreath", 96}, {"ExitBowZoomBreath", 97},
        {"ActorCollideWithActor", 98}, {"PlayerInIronSights", 99},
        {"OutOfBreath", 100}, {"CombatGrunt", 101}, {"LeaveWaterBreath", 102}
    };

    static std::string GetConfigPath()
    {
        static std::string path;
        if (path.empty()) {
            wchar_t buffer[MAX_PATH];
            GetModuleFileNameW(nullptr, buffer, MAX_PATH);
            std::filesystem::path exePath(buffer);
            path = (exePath.parent_path() / "Data" / "STFU Patcher" / "Config" / "STFU_SkyrimNetFilter.yaml").string();
        }
        return path;
    }

    static void LoadBlockedTopics()
    {
        std::string configPath = GetConfigPath();
        
        if (!std::filesystem::exists(configPath)) {
            return;
        }

        try {
            YAML::Node config = YAML::LoadFile(configPath);
            
            // Load blocked topics
            if (config["topics"] && config["topics"].IsSequence()) {
                for (const auto& entry : config["topics"]) {
                    if (entry.IsScalar()) {
                        std::string value = entry.as<std::string>();
                        
                        // Check if it's a FormID (starts with 0x) or EditorID
                        if (value.substr(0, 2) == "0x" || value.substr(0, 2) == "0X") {
                            // Parse as hex FormID
                            uint32_t formID = std::stoul(value, nullptr, 16);
                            g_settings.blacklistedFormIDs.insert(formID);
                        } else {
                            // Treat as EditorID
                            g_settings.blacklistedEditorIDs.insert(value);
                        }
                    }
                }
            }
            
            // Load blocked quests
            if (config["quests"] && config["quests"].IsSequence()) {
                for (const auto& entry : config["quests"]) {
                    if (entry.IsScalar()) {
                        std::string value = entry.as<std::string>();
                        
                        // Check if it's a FormID (starts with 0x) or EditorID
                        if (value.substr(0, 2) == "0x" || value.substr(0, 2) == "0X") {
                            // Parse as hex FormID
                            uint32_t formID = std::stoul(value, nullptr, 16);
                            g_settings.blacklistedQuestFormIDs.insert(formID);
                        } else {
                            // Treat as EditorID
                            g_settings.blacklistedQuestEditorIDs.insert(value);
                        }
                    }
                }
            }
            
            // Load blocked subtypes (user-friendly names like Hello, Attack, Idle)
            if (config["subtypes"] && config["subtypes"].IsSequence()) {
                for (const auto& entry : config["subtypes"]) {
                    if (entry.IsScalar()) {
                        std::string subtypeName = entry.as<std::string>();
                        
                        // Case-insensitive lookup
                        auto it = SubtypeNameMap.find(subtypeName);
                        if (it != SubtypeNameMap.end()) {
                            g_settings.blacklistedSubtypes.insert(it->second);
                        } else {
                            spdlog::warn("Unknown subtype name: {} (use names like Hello, Attack, Idle)", subtypeName);
                        }
                    }
                }
            }
            
            int totalTopics = g_settings.blacklistedFormIDs.size() + g_settings.blacklistedEditorIDs.size();
            int totalQuests = g_settings.blacklistedQuestFormIDs.size() + g_settings.blacklistedQuestEditorIDs.size();
            int totalSubtypes = g_settings.blacklistedSubtypes.size();
            
            if (totalTopics > 0 || totalQuests > 0 || totalSubtypes > 0) {
                spdlog::info("Loaded filter config: {} topics, {} quests, {} subtypes", 
                    totalTopics, totalQuests, totalSubtypes);
            }
        } catch (const YAML::Exception& e) {
            spdlog::error("YAML parse error: {}", e.what());
        }
    }

    void Load()
    {
        LoadBlockedTopics();
        
        // Look up toggle global from STFU.esp by EditorID
        auto dataHandler = RE::TESDataHandler::GetSingleton();
        if (dataHandler) {
            // Try to find STFU.esp in load order
            auto stfuFile = dataHandler->LookupModByName("STFU.esp");
            if (stfuFile) {
                // Search all globals for matching EditorID
                for (auto& global : dataHandler->GetFormArray<RE::TESGlobal>()) {
                    if (global && global->GetFormEditorID()) {
                        std::string editorID = global->GetFormEditorID();
                        if (editorID == "STFU_SkyrimNetFilter") {
                            g_settings.toggleGlobal = global;
                            break;
                        }
                    }
                }
            }
        }
    }

    const Settings& GetSettings()
    {
        return g_settings;
    }
    
    uint16_t GetAccurateSubtype(RE::TESTopic* topic)
    {
        if (!topic) {
            return 0;
        }
        
        uint32_t formID = topic->GetFormID();
        auto it = VanillaSubtypeCorrections.find(formID);
        if (it != VanillaSubtypeCorrections.end()) {
            // Return corrected subtype from SNAM
            return it->second;
        }
        
        // Fall back to DATA subtype for topics not in correction map
        return static_cast<uint16_t>(topic->data.subtype.get());
    }
}
