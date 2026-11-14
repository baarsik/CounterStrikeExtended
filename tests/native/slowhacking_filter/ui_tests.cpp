#include "actual_ui.inc"

static CScriptObject* AddSetting(CInfoDescription& description, const char* name) {
    CScriptObject* object = new CScriptObject;
    std::strcpy(object->cvarname, name);
    description.AddObject(object);
    return object;
}

static CScriptObject* LastSetting(CInfoDescription& description) {
    CScriptObject* last = nullptr;
    int filters = 0;
    for (CScriptObject* object = description.pObjList; object; object = object->pNext) {
        last = object;
        if (strcasecmp(object->cvarname, "cl_filterstuffcmd") == 0) {
            ++filters;
        }
    }
    assert(filters == 1);
    assert(last && std::strcmp(last->cvarname, "cl_filterstuffcmd") == 0);
    return last;
}

int main() {
    for (const char* game : {"Counter-Strike", "Half-Life"}) {
        mod.description = game;
        for (bool saved : {false, true}) {
            savedScriptLoaded = saved;
            CInfoDescription description(nullptr);
            CScriptObject* earlier = AddSetting(description, "earlier_setting");
            if (saved) {
                AddSetting(description, "CL_FILTERSTUFFCMD");
                AddSetting(description, "cl_filterstuffcmd");
                AddSetting(description, "last_saved_setting");
            }
            for (int reload = 0; reload < 2; ++reload) {
                assert(description.InitFromFile(const_cast<char*>("user.scr")) == saved);
                assert(description.pObjList == earlier);
                CScriptObject* option = LastSetting(description);
                assert(option->type == O_LIST && !option->bSetInfo);
                assert(std::strcmp(option->prompt, "#CSEMultiplayer_SlowhackingFilter") == 0);
                const char* labels[] = {"#CSEMultiplayer_FilterBasic", "#CSEMultiplayer_FilterDefault",
                                        "#CSEMultiplayer_FilterExtended"};
                const char* levels[] = {"0", "1", "2"};
                CScriptListItem* choice = option->pListItems;
                for (int index = 0; index < 3; ++index) {
                    assert(choice);
                    assert(std::strcmp(choice->szItemText, labels[index]) == 0);
                    assert(std::strcmp(choice->szValue, levels[index]) == 0);
                    choice = choice->pNext;
                }
                assert(!choice);
                assert(option->fdefValue == 1 && option->fcurValue == 1);
                for (const char* value : levels) {
                    engine->values["cl_filterstuffcmd"] = value;
                    description.TransferCurrentValues(nullptr);
                    assert(option->fdefValue == std::atof(value));
                    assert(option->fcurValue == std::atof(value));
                    option->WriteToConfig();
                    assert(engine->commands.back() == std::string("cl_filterstuffcmd \"") + value + "\"\n");
                }
            }
        }
    }
    std::puts("PASS CS/HL option order, saved-definition migration, duplicates, value transfer and configuration writes");
}
