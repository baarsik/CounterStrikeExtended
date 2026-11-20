#include "../../../src/public/cse_server_forms.h"

#include <cassert>
#include <fstream>
#include <iterator>
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
    assert(ParseForm(R"(<Window Width="640" Height="400" Title="Shop"
        xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation"
        xmlns:cse="urn:counterstrikeextended:forms">
        <Grid Margin="10">
            <Grid.ColumnDefinitions><ColumnDefinition Width="2*"/><ColumnDefinition Width="*"/></Grid.ColumnDefinitions>
            <Grid.RowDefinitions><RowDefinition Height="Auto"/><RowDefinition Height="*"/></Grid.RowDefinitions>
            <Slider Minimum="1" Maximum="10" Value="4" cse:Shortcut="1" Height="24"/>
            <TextBox Grid.Row="1" Text="Hello &amp; welcome" cse:Shortcut="A" Height="24"/>
            <Image Grid.Column="1" Grid.Row="1" Source="emblem" Width="32" Height="32"/>
        </Grid>
    </Window>)", form));
    assert(form.controls.size() == 3 && form.controls[1].key == 'a');
    assert(form.controls[0].x == 10 && form.controls[0].y == 10 && form.controls[0].width == 413);
    assert(form.controls[1].y == 34 && form.controls[1].text == "Hello & welcome");
    assert(form.controls[2].x == 423);
    assert(ValidAction(form, 1, "10") && !ValidAction(form, 1, "11"));
    assert(ValidAction(form, 2, "Dobrý") && !ValidAction(form, 2, "first\nsecond"));
    assert(!ValidAction(form, 3, "") && !ValidAction(form, 4, ""));
    auto window = [](std::string_view body) {
        return std::string(R"(<Window Width="640" Height="400" Title="Demo" xmlns:cse="urn:counterstrikeextended:forms">)") +
            std::string(body) + "</Window>";
    };
    assert(!ParseForm(window(R"(<StackPanel><Button Content="First" cse:Shortcut="A"/>
        <Button Content="Second" cse:Shortcut="a"/></StackPanel>)"), form));
    assert(!ParseForm(window(R"(<Image Source="../emblem"/>)"), form));
    assert(!ParseForm(window(R"(<Image Source="https://example.invalid/emblem"/>)"), form));
    assert(!ParseForm(window(R"(<Grid><Button Grid.ColumnSpan="2" Content="Outside"/></Grid>)"), form));
    assert(!ParseForm(window(R"(<Button Content="{Binding Name}"/>)"), form));
    assert(!ParseForm(window(R"(<Button Content="Click" Click="RunCommand"/>)"), form));
    assert(!ParseForm(window(R"(<Button xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
        Content="Click" x:Class="Unknown"/>)"), form));
    assert(!ParseForm(window(R"(<Button xmlns="urn:unknown" Content="Click"/>)"), form));
    assert(!ParseForm(window("<Button Content=\"\xC0\xAF\"/>"), form));
    assert(!ParseForm(window(R"(<Button Content="&#x202e;"/>)"), form));
    assert(!ParseForm(window(R"(<Button Content="Click" Content="Again"/>)"), form));
    assert(!ParseForm(window("<Button Content=\"Broken\"></ToggleButton>"), form));
    assert(!ParseForm("<!DOCTYPE Window>" + window(R"(<Button Content="Click"/>)"), form));
    assert(!ParseForm("<?test nope?>" + window(R"(<Button Content="Click"/>)"), form));
    assert(!ParseForm(window("<TextBlock Text=\"&unregistered;\"/>"), form));
    assert(!ParseForm(window("<TextBlock><![CDATA[No]]></TextBlock>"), form));
    std::string deep;
    for (int depth = 0; depth < 30; ++depth) {
        deep += "<Grid>";
    }
    deep += "<Button Content=\"Deep\"/>";
    for (int depth = 0; depth < 30; ++depth) {
        deep += "</Grid>";
    }
    assert(!ParseForm(window(deep), form));
    assert(!ParseForm("form 640 400 Legacy\nbutton 0 0 50 30 1 0 0 Old\n", form));
    assert(form.controls.size() == 3 && form.controls[1].text == "Hello & welcome");

    assert(ParseForm(window(R"(<StackPanel Margin="8"><ToggleButton Content="Guide" IsChecked="True"/>
        <ProgressBar Minimum="0" Maximum="10" Value="8" Height="12"/>
        <ListBox Height="70" SelectedIndex="1"><ListBoxItem Content="One"/><ListBoxItem Content="Two"/></ListBox>
        </StackPanel>)"), form));
    assert(form.controls[1].y == 40 && form.controls[2].y == 52);
    assert(ValidAction(form, 1, "1") && !ValidAction(form, 1, ""));
    assert(!ValidAction(form, 2, "8"));
    assert(ValidAction(form, 3, "1") && !ValidAction(form, 3, "2"));

    const std::ifstream showcase(CSE_FORMS_SHOWCASE, std::ios::binary);
    const std::string sample{std::istreambuf_iterator<char>(showcase.rdbuf()), std::istreambuf_iterator<char>()};
    assert(ParseForm(sample, form));
    assert(form.controls.size() == 27 && form.controls[2].initial == 100);
    assert(form.controls[11].type == ControlType::Progress && form.controls[11].initial == 72);
    assert(form.controls[15].type == ControlType::Toggle && form.controls[24].type == ControlType::List);
    assert(form.controls[26].key == 'q');

    Image image{};
    const std::string bytes("\1\0\1\0\xff\xff\0\xff", 8);
    assert(ParseImage(bytes, image) && image.width == 1 && image.height == 1 && image.pixels[2] == 0);
    assert(!ParseImage(bytes.substr(0, 7), image));
    assert(!ParseImage(std::string("\xff\xff\xff\xff", 4), image));
}
