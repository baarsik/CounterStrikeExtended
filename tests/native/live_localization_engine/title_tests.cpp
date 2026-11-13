using qboolean = int;
#include "localized_titles.h"
#include <cassert>
#include <cstring>

int main() {
    cse::localization::TitleSnapshots titles;
    const std::string english = "$position -1 0.2\n$holdtime 4\nONE\n{\nEnglish\n}\nTWO\n{\nFallback\n}\n";
    assert(titles.Prepare(english, "ONE\n{\nPolski\n}\n"));
    assert(titles.Active() == nullptr);
    titles.Commit();
    const auto* old = titles.Active()->Data();
    assert(titles.Active()->Count() == 2);
    assert(std::strcmp(old[0].pMessage, "Polski\r") == 0);
    assert(std::strcmp(old[1].pMessage, "Fallback\r") == 0);
    assert(old[1].holdtime == 4.0f);
    assert(titles.IsCatalogue(old) && titles.IsCatalogue(old + 1));
    client_textmessage_t dynamic{};
    assert(!titles.IsCatalogue(&dynamic) && !titles.IsCatalogue(nullptr));
    assert(!titles.Prepare(english, "ONE\n{\nUnfinished"));
    assert(titles.Active()->Data() == old);
    assert(!titles.Prepare("", ""));
    assert(titles.Prepare(english, "ONE\n{\nDeutsch\n}\n"));
    titles.Commit();
    assert(std::strcmp(old[0].pMessage, "Polski\r") == 0);
    assert(titles.IsCatalogue(old));
    assert(std::strcmp(titles.Active()->Data()[0].pMessage, "Deutsch\r") == 0);
    assert(titles.Prepare(english, "ONE\n{\nPolski\n}\n"));
    titles.Commit();
    assert(titles.Active()->Data() == old);
    assert(!titles.Prepare(english, "$position nan 0\nONE\n{\nBad\n}\n"));
    titles.Clear();
    assert(!titles.IsCatalogue(old));
}
