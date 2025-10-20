#include "plugin.h"
#include <map>
#include "TextFileTable.h"
#include <filesystem>
using namespace std::filesystem;

using namespace plugin;

uintptr_t OrigTextLookup = 0;
uintptr_t OrigSetLanguage = 0;
uintptr_t OrigTextFromCache = 0;
uintptr_t StringCtorAddr = 0;

class CustomTextLoader {
public:
    static std::map<unsigned int, std::string> &GetTranslationTable() {
        static std::map<unsigned int, std::string> table;
        return table;
    }

    static void * METHOD OnTextLookup(void *t, DUMMY_ARG, void *str, unsigned int id) {
        auto it = GetTranslationTable().find(id);
        if (it != GetTranslationTable().end()) {
            CallMethodDynGlobal(StringCtorAddr, str, (*it).second.c_str());
            return str;
        }
        return CallMethodAndReturnDynGlobal<void *>(OrigTextLookup, t, str, id);
    }

    static void METHOD OnSetLanguage(void *t, DUMMY_ARG, char const *language) {
        GetTranslationTable().clear();
        CallMethodDynGlobal(OrigSetLanguage, t, language);
        std::string gameLanguage = ToLower(language);
        if (gameLanguage.empty())
            gameLanguage = "eng";
        for (const auto& p : recursive_directory_iterator("plugins\\")) {
            if (p.path().extension() == L".tr") {
                TextFileTable file;
                if (file.Read(p.path(), 0)) {
                    std::string line;
                    bool canAddLines = true;
                    for (size_t r = 0; r < file.NumRows(); r++) {
                        auto const &row = file.Row(r);
                        line = row.size() > 0 ? row[0] : "";
                        if (!line.empty() && line[0] != ';' && line[0] != '#') {
                            if (line[0] == '[') {
                                auto bcPos = line.find(']', 1);
                                if (bcPos != std::string::npos) {
                                    std::string currTranslationLanguage = line.substr(1, bcPos - 1);
                                    Trim(currTranslationLanguage);
                                    if (currTranslationLanguage.empty())
                                        canAddLines = true;
                                    else {
                                        canAddLines = false;
                                        currTranslationLanguage = ToLower(currTranslationLanguage);
                                        auto languages = Split(currTranslationLanguage, ',', true, true, false);
                                        for (auto const &l : languages) {
                                            if (l == gameLanguage) {
                                                canAddLines = true;
                                                break;
                                            }
                                        }
                                    }
                                }
                            }
                            else {
                                if (canAddLines) {
                                    auto beginPos = line.find_first_not_of(" \t");
                                    if (beginPos != std::string::npos) {
                                        auto splitPos = line.find_first_of(" \t", beginPos + 1);
                                        unsigned int id = 0;
                                        bool hasId = true;
                                        std::string value;
                                        if (splitPos == std::string::npos) {
                                            try {
                                                id = static_cast<unsigned int>(std::stoull(line, 0, 10));
                                            }
                                            catch (...) {
                                                hasId = false;
                                            }
                                        }
                                        else {
                                            std::string idStr = line.substr(beginPos, splitPos - beginPos);
                                            try {
                                                id = static_cast<unsigned int>(std::stoull(line, 0, 10));
                                                value = line.substr(splitPos + 1);
                                            }
                                            catch (...) {
                                                hasId = false;
                                            }
                                        }
                                        if (hasId) {
                                            GetTranslationTable()[id] = value;
                                            ::Message("%d: %s", id, value.c_str());
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    static void METHOD OnTextFromCache(void *t, DUMMY_ARG, void *str, unsigned int id, bool *result) {
        auto it = GetTranslationTable().find(id);
        if (it != GetTranslationTable().end()) {
            CallMethodDynGlobal(StringCtorAddr, str, (*it).second.c_str());
            *result = true;
        }
        else
            CallMethodDynGlobal(OrigTextFromCache, t, str, id, result);
    }

    CustomTextLoader() {
        if (!CheckPluginName(Magic<'C','u','s','t','o','m','T','e','x','t','L','o','a','d','e','r','.','a','s','i'>()))
            return;
        auto version = FIFA::GetAppVersion().id();
        if (version == ID_FIFA10_1000_RZR) {
            OrigSetLanguage = patch::RedirectCall(0x4C3212, OnSetLanguage);
            OrigTextLookup = patch::RedirectCall(0x454CA0, OnTextLookup);
            patch::RedirectCall(0x46AE65, OnTextLookup);
            StringCtorAddr = 0x40C9D0;
        }
        else if (version == VERSION_EURO_08) {
            OrigSetLanguage = patch::RedirectCall(0x4D3572, OnSetLanguage);
            OrigTextLookup = patch::RedirectCall(0x4672C0, OnTextLookup);
            patch::RedirectCall(0x4771B5, OnTextLookup);
            StringCtorAddr = 0x409D00;
        }
        else if (version == ID_FIFA08_1200_VTY || version == ID_FIFA08_1200_BFF) {
            OrigSetLanguage = patch::RedirectCall(0x4E7982, OnSetLanguage);
            OrigTextLookup = patch::RedirectCall(0x484C70, OnTextLookup);
            patch::RedirectCall(0x496C25, OnTextLookup);
            StringCtorAddr = 0x4AC110;
        }
        else if (version == ID_FIFA07_1100_RLD) {
            OrigSetLanguage = patch::RedirectCall(0x5210A2, OnSetLanguage);
            OrigTextLookup = patch::RedirectCall(0x482320, OnTextLookup);
            patch::RedirectCall(0x492365, OnTextLookup);
            OrigTextFromCache = patch::RedirectCall(0x49230C, OnTextFromCache);
            StringCtorAddr = 0x409F80;
        }
        else if (version == VERSION_WC_06) {
            OrigSetLanguage = patch::RedirectCall(0x41B9A9, OnSetLanguage);
            patch::RedirectCall(0x41B9CC, OnSetLanguage); // 1 2
            patch::RedirectCall(0x41BA02, OnSetLanguage);
            //patch::RedirectCall(0x41CD73, OnSetLanguage<0x449A40, CP_UTF8>); // 3
            //patch::RedirectJump(0x449BFF, OnSetLanguage<0x449A40, CP_UTF8>);
            //patch::RedirectCall(0x463F6B, OnSetLanguage<0x449A40, CP_UTF8>);
            //patch::RedirectCall(0x567488, OnSetLanguage<0x449A40, CP_UTF8>);
            //patch::RedirectCall(0x567546, OnSetLanguage<0x449A40, CP_UTF8>);
            //patch::RedirectCall(0x5680F8, OnSetLanguage<0x449A40, CP_UTF8>);
            //patch::RedirectCall(0x568341, OnSetLanguage<0x449A40, CP_UTF8>);
            OrigTextLookup = patch::RedirectCall(0x441200, OnTextLookup);
            patch::RedirectCall(0x4498D5, OnTextLookup);
            StringCtorAddr = 0x4234E0;
        }
        else if (version == VERSION_CL_04_05) {
            OrigSetLanguage = patch::RedirectCall(0x420AA6, OnSetLanguage);
            patch::RedirectCall(0x420AC6, OnSetLanguage);
            patch::RedirectCall(0x49268F, OnSetLanguage);
            patch::RedirectCall(0x548952, OnSetLanguage);
            patch::RedirectCall(0x5F28BE, OnSetLanguage);
            patch::RedirectCall(0x5F299F, OnSetLanguage);
            OrigTextLookup = patch::RedirectCall(0x44F680, OnTextLookup);
            patch::RedirectCall(0x4539C5, OnTextLookup);
            StringCtorAddr = 0x437F00;
        }
    }
} customTextLoader;
