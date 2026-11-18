#include "../../../src/public/cse_server_forms.h"

#include <cassert>
#include <string>

using namespace cse::forms;

int main() {
    assert(CanAppendCapability(243, 256));
    assert(!CanAppendCapability(244, 256));
    assert(!CanAppendCapability(0, 0));
    const auto digest = Hash("abc");
    assert(EncodeHex(digest.data(), digest.size()) ==
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    Nonce nonce{};
    assert(DecodeHex("000102030405060708090a0b0c0d0e0f", nonce.data(), nonce.size()));
    const auto proof = MakeProof(nonce, "asset");
    assert(EncodeHex(proof.data(), proof.size()) ==
        "ac4b2dc4f00792c91f6041ee19f8758d097908f89ac9d7485daa8f2260a35250");
    nonce[0] ^= 1;
    assert(proof != MakeProof(nonce, "asset"));
    assert(proof != MakeProof(Nonce{}, "other"));

    Form form{};
    assert(ParseForm("form 640 400 Shop\n"
        "slider 10 10 200 24 1 1 10 Volume\n"
        "text 10 40 200 24 A 0 0 Hello\n"
        "image 10 70 32 32 - 0 0 emblem\n", form));
    assert(form.controls.size() == 3 && form.controls[1].key == 'a');
    assert(ValidAction(form, 1, "10") && !ValidAction(form, 1, "11"));
    assert(ValidAction(form, 2, "Dobrý") && !ValidAction(form, 2, "first\nsecond"));
    assert(!ValidAction(form, 3, "") && !ValidAction(form, 4, ""));
    assert(!ParseForm("form 640 400 Bad\n"
        "button 10 10 100 24 A 0 0 First\n"
        "button 10 40 100 24 a 0 0 Second\n", form));
    assert(!ParseForm("form 640 400 Bad\nimage 0 0 32 32 - 0 0 ../emblem\n", form));
    assert(!ParseForm("form 640 400 Bad\nbutton 630 0 32 32 0 0 0 Outside\n", form));
    assert(!ParseForm("form 640 400 Bad\nbutton 10 10 100 24 1 0 0 \xC0\xAF\n", form));

    Image image{};
    const std::string bytes("\1\0\1\0\xff\xff\0\xff", 8);
    assert(ParseImage(bytes, image) && image.width == 1 && image.height == 1 && image.pixels[2] == 0);
    assert(!ParseImage(bytes.substr(0, 7), image));
    assert(!ParseImage(std::string("\xff\xff\xff\xff", 4), image));
}
