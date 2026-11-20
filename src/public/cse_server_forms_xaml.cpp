#include "cse_server_forms.h"

#include <expat.h>

#include <algorithm>
#include <charconv>
#include <iterator>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>

namespace cse::forms {
namespace {

constexpr int kMinimumWidth = 240;
constexpr int kMaximumWidth = 800;
constexpr int kMinimumHeight = 180;
constexpr int kMaximumHeight = 600;
constexpr int kMaxRange = 10000;
constexpr int kMaxTracks = 12;
constexpr int kMaxWeight = 100;
constexpr std::size_t kMaxOptions = 8;
constexpr std::size_t kMaxGroups = 9;
constexpr std::size_t kMaxLabelBytes = 160;
constexpr std::size_t kMaxDepth = 16;
constexpr std::size_t kMaxElements = 128;
constexpr std::size_t kMaxAttributes = 16;
constexpr char kNamespaceSeparator = '\x1f';
constexpr std::string_view kPresentationNamespace = "http://schemas.microsoft.com/winfx/2006/xaml/presentation";
constexpr std::string_view kXamlNamespace = "http://schemas.microsoft.com/winfx/2006/xaml";
constexpr std::string_view kFormsNamespace = "urn:counterstrikeextended:forms";
constexpr std::string_view kShortcutAttribute = "urn:counterstrikeextended:forms\x1f" "Shortcut";
constexpr std::string_view kWhitespace = " \t\r\n";

bool ReadInteger(std::string_view text, int& value) {
    if (text.empty()) {
        return false;
    }
    const auto end = text.data() + text.size();
    const auto result = std::from_chars(text.data(), end, value);
    return result.ec == std::errc{} && result.ptr == end;
}

bool Printable(std::string_view utf8, std::size_t limit) {
    if (utf8.size() > limit) {
        return false;
    }
    while (!utf8.empty()) {
        const auto lead = static_cast<std::uint8_t>(utf8.front());
        unsigned count = 1;
        unsigned codePoint = lead;
        unsigned minimum = 0;
        if (lead >= 0xf0 && lead <= 0xf4) {
            count = 4;
            codePoint = lead & 7U;
            minimum = 0x10000;
        } else if (lead >= 0xe0 && lead <= 0xef) {
            count = 3;
            codePoint = lead & 15U;
            minimum = 0x800;
        } else if (lead >= 0xc2 && lead <= 0xdf) {
            count = 2;
            codePoint = lead & 31U;
            minimum = 0x80;
        } else if (lead > 0x7f) {
            return false;
        }
        if (utf8.size() < count) {
            return false;
        }
        for (unsigned i = 1; i < count; ++i) {
            const auto next = static_cast<std::uint8_t>(utf8[i]);
            if ((next & 0xc0U) != 0x80U) {
                return false;
            }
            codePoint = (codePoint << 6) | (next & 0x3fU);
        }
        // Reject control characters, malformed UTF-8 and bidirectional override/isolate characters.
        if (codePoint < minimum || codePoint < 0x20 || (codePoint >= 0x7f && codePoint <= 0x9f) ||
            (codePoint >= 0xd800 && codePoint <= 0xdfff) || codePoint > 0x10ffff ||
            (codePoint >= 0x2028 && codePoint <= 0x202e) || (codePoint >= 0x2066 && codePoint <= 0x2069)) {
            return false;
        }
        utf8.remove_prefix(count);
    }
    return true;
}

std::string_view Trim(std::string_view text) {
    const auto first = text.find_first_not_of(kWhitespace);
    return first == text.npos ? std::string_view{} : text.substr(first, text.find_last_not_of(kWhitespace) - first + 1);
}

bool IntegerIn(std::string_view text, int& value, int minimum, int maximum) {
    return ReadInteger(Trim(text), value) && value >= minimum && value <= maximum;
}

struct Element {
    std::string name;
    std::vector<std::pair<std::string, std::string>> attributes;
    std::vector<Element> children;
};

class XmlDocument {
public:
    bool Parse(std::string_view bytes) {
        if (!parser_ || bytes.empty() || bytes.size() > kMaxFormBytes || bytes.find('\0') != bytes.npos) {
            return false;
        }
        XML_SetUserData(parser_.get(), this);
        XML_SetElementHandler(parser_.get(), StartElement, EndElement);
        XML_SetCharacterDataHandler(parser_.get(), Text);
        XML_SetStartDoctypeDeclHandler(parser_.get(), RejectDoctype);
        XML_SetProcessingInstructionHandler(parser_.get(), RejectInstruction);
        XML_SetStartCdataSectionHandler(parser_.get(), RejectCdata);
        XML_SetXmlDeclHandler(parser_.get(), CheckDeclaration);
        XML_SetStartNamespaceDeclHandler(parser_.get(), CheckNamespace);
        return XML_Parse(parser_.get(), bytes.data(), static_cast<int>(bytes.size()), XML_TRUE) == XML_STATUS_OK &&
            root.name == "Window";
    }

    Element root;

private:
    void Reject() {
        XML_StopParser(parser_.get(), XML_FALSE);
    }

    static void XMLCALL StartElement(void* context, const char* name, const char** attributes) {
        auto& document = *static_cast<XmlDocument*>(context);
        if (document.parents_.size() >= kMaxDepth || ++document.elements_ > kMaxElements) {
            document.Reject();
            return;
        }
        std::string_view tag(name);
        const auto separator = tag.find(kNamespaceSeparator);
        if (separator != tag.npos) {
            if (tag.substr(0, separator) != kPresentationNamespace) {
                document.Reject();
                return;
            }
            tag.remove_prefix(separator + 1);
        }
        Element& element = document.parents_.empty() ? document.root
            : document.parents_.back()->children.emplace_back();
        element.name = tag;
        for (; *attributes; attributes += 2) {
            if (element.attributes.size() >= kMaxAttributes) {
                document.Reject();
                return;
            }
            element.attributes.emplace_back(attributes[0], attributes[1]);
        }
        document.parents_.push_back(&element);
    }

    static void XMLCALL EndElement(void* context, const char*) {
        auto& parents = static_cast<XmlDocument*>(context)->parents_;
        if (!parents.empty()) {
            parents.pop_back();
        }
    }

    static void XMLCALL Text(void* context, const char* value, int size) {
        if (!Trim({value, static_cast<std::size_t>(size)}).empty()) {
            static_cast<XmlDocument*>(context)->Reject();
        }
    }

    static void XMLCALL RejectDoctype(void* context, const char*, const char*, const char*, int) {
        static_cast<XmlDocument*>(context)->Reject();
    }

    static void XMLCALL RejectInstruction(void* context, const char*, const char*) {
        static_cast<XmlDocument*>(context)->Reject();
    }

    static void XMLCALL RejectCdata(void* context) {
        static_cast<XmlDocument*>(context)->Reject();
    }

    static void XMLCALL CheckDeclaration(void* context, const char* version, const char* encoding, int) {
        if (!version || std::string_view(version) != "1.0" ||
            (encoding && std::string_view(encoding) != "utf-8" && std::string_view(encoding) != "UTF-8")) {
            static_cast<XmlDocument*>(context)->Reject();
        }
    }

    static void XMLCALL CheckNamespace(void* context, const char*, const char* uri) {
        if (uri && uri != kPresentationNamespace && uri != kFormsNamespace && uri != kXamlNamespace) {
            static_cast<XmlDocument*>(context)->Reject();
        }
    }

    std::unique_ptr<std::remove_pointer_t<XML_Parser>, decltype(&XML_ParserFree)> parser_{
        XML_ParserCreateNS("UTF-8", kNamespaceSeparator), XML_ParserFree};
    std::vector<Element*> parents_;
    std::size_t elements_{};
};

class Attributes {
public:
    explicit Attributes(const Element& element) : attributes_(element.attributes) {}

    std::optional<std::string_view> Take(std::string_view name) {
        for (std::size_t index = 0; index < attributes_.size(); ++index) {
            if (attributes_[index].first == name) {
                used_[index] = true;
                return attributes_[index].second;
            }
        }
        return {};
    }

    bool Int(std::string_view name, int& value, int minimum, int maximum, bool required = false) {
        const auto text = Take(name);
        return text ? IntegerIn(*text, value, minimum, maximum) : !required;
    }

    bool Text(std::string_view name, std::string& value, std::size_t limit, bool required = false) {
        const auto text = Take(name);
        if (!text) {
            return !required;
        }
        if ((required && Trim(*text).empty()) || !Printable(*text, limit) || Trim(*text).substr(0, 1) == "{") {
            return false;
        }
        value = *text;
        return true;
    }

    bool Checked(int& value) {
        const auto text = Take("IsChecked");
        if (!text || *text == "False" || *text == "false") {
            value = 0;
            return true;
        }
        value = 1;
        return *text == "True" || *text == "true";
    }

    bool Done() const {
        return std::all_of(used_.begin(), used_.begin() + static_cast<std::ptrdiff_t>(attributes_.size()),
                           [](bool used) { return used; });
    }

private:
    const std::vector<std::pair<std::string, std::string>>& attributes_;
    std::array<bool, kMaxAttributes> used_{};
};

enum class Layout { Widget, Grid, Stack, Canvas, Group };
enum class Alignment { Stretch, Start, Center, End };
enum class TrackKind { Fixed, Auto, Star };

struct Track {
    TrackKind kind{TrackKind::Star};
    int value{1};
};

struct Axis {
    int size{};
    int before{};
    int after{};
    int cell{};
    int span{1};
    int offset{};
    Alignment alignment{};
};

struct Node {
    Layout layout{};
    Axis horizontal;
    Axis vertical;
    bool verticalStack{true};
    std::vector<Track> columns;
    std::vector<Track> rows;
    std::vector<Node> children;
    Control control;
};

struct Rect {
    int x{};
    int y{};
    int width{};
    int height{};
};

bool ReadMargin(Attributes& attributes, Node& node) {
    const auto margin = attributes.Take("Margin");
    if (!margin) {
        return true;
    }
    std::array<int, 4> edges{};
    auto remaining = *margin;
    std::size_t count = 0;
    for (;;) {
        const auto end = remaining.find(',');
        if (count == edges.size() || !IntegerIn(remaining.substr(0, end), edges[count++], 0, kMaximumWidth)) {
            return false;
        }
        if (end == remaining.npos) {
            break;
        }
        remaining.remove_prefix(end + 1);
    }
    if (count != 1 && count != 2 && count != 4) {
        return false;
    }
    node.horizontal.before = edges[0];
    node.vertical.before = count == 1 ? edges[0] : edges[1];
    node.horizontal.after = count == 4 ? edges[2] : node.horizontal.before;
    node.vertical.after = count == 4 ? edges[3] : node.vertical.before;
    return true;
}

bool ReadAlignment(Attributes& attributes, bool vertical, Alignment& alignment) {
    const auto value = attributes.Take(vertical ? "VerticalAlignment" : "HorizontalAlignment");
    if (!value || *value == "Stretch") {
        alignment = Alignment::Stretch;
    } else if (*value == (vertical ? "Top" : "Left")) {
        alignment = Alignment::Start;
    } else if (*value == "Center") {
        alignment = Alignment::Center;
    } else if (*value == (vertical ? "Bottom" : "Right")) {
        alignment = Alignment::End;
    } else {
        return false;
    }
    return true;
}

bool ReadAxis(Attributes& attributes, Layout parent, bool vertical, Axis& axis) {
    if (!attributes.Int(vertical ? "Height" : "Width", axis.size, 1, kMaximumWidth) ||
        !ReadAlignment(attributes, vertical, axis.alignment)) {
        return false;
    }
    if (parent == Layout::Grid) {
        return attributes.Int(vertical ? "Grid.Row" : "Grid.Column", axis.cell, 0, kMaxTracks - 1) &&
            attributes.Int(vertical ? "Grid.RowSpan" : "Grid.ColumnSpan", axis.span, 1, kMaxTracks);
    }
    return parent != Layout::Canvas ||
        attributes.Int(vertical ? "Canvas.Top" : "Canvas.Left", axis.offset, 0, kMaximumWidth);
}

bool ReadTracks(const Element& element, bool vertical, std::vector<Track>& tracks) {
    if (element.children.empty() || element.children.size() > kMaxTracks || !element.attributes.empty() ||
        !tracks.empty()) {
        return false;
    }
    for (const auto& child : element.children) {
        if (child.name != (vertical ? "RowDefinition" : "ColumnDefinition") || !child.children.empty()) {
            return false;
        }
        Attributes attributes(child);
        const auto text = Trim(attributes.Take(vertical ? "Height" : "Width").value_or("*"));
        Track track;
        if (text == "Auto") {
            track.kind = TrackKind::Auto;
        } else if (!text.empty() && text.back() == '*') {
            if (text.size() > 1 && !IntegerIn(text.substr(0, text.size() - 1), track.value, 1, kMaxWeight)) {
                return false;
            }
        } else {
            track.kind = TrackKind::Fixed;
            if (!IntegerIn(text, track.value, 1, kMaximumWidth)) {
                return false;
            }
        }
        if (!attributes.Done()) {
            return false;
        }
        tracks.push_back(track);
    }
    return true;
}

bool ReadRange(Attributes& attributes, Control& control) {
    control.second = 100;
    if (!attributes.Int("Minimum", control.first, 0, kMaxRange) ||
        !attributes.Int("Maximum", control.second, 0, kMaxRange) || control.second <= control.first) {
        return false;
    }
    control.initial = control.first;
    return attributes.Int("Value", control.initial, control.first, control.second);
}

bool ReadChoices(const Element& element, Attributes& attributes, Control& control) {
    if (element.children.empty() || element.children.size() > kMaxOptions) {
        return false;
    }
    for (const auto& child : element.children) {
        if (!child.children.empty() || child.name != (control.type == ControlType::List ? "ListBoxItem" : "ComboBoxItem")) {
            return false;
        }
        std::string option;
        Attributes item(child);
        if (!item.Text("Content", option, kMaxValueBytes, true) || !item.Done()) {
            return false;
        }
        control.options.push_back(std::move(option));
    }
    return attributes.Int("SelectedIndex", control.first, 0, static_cast<int>(control.options.size()) - 1);
}

struct Schema {
    std::array<bool, 128> shortcuts{};
    std::vector<std::string> radioGroups;
    std::array<bool, kMaxGroups> selected{};
};

bool ReadRadio(Attributes& attributes, Control& control, Schema& schema) {
    const auto group = attributes.Take("GroupName").value_or("default");
    if (!IsName(group) || !attributes.Checked(control.second)) {
        return false;
    }
    const auto known = std::find(schema.radioGroups.begin(), schema.radioGroups.end(), group);
    const auto index = static_cast<std::size_t>(known - schema.radioGroups.begin());
    if (known == schema.radioGroups.end()) {
        if (schema.radioGroups.size() >= kMaxGroups) {
            return false;
        }
        schema.radioGroups.emplace_back(group);
    }
    control.first = static_cast<int>(index + 1);
    return !control.second || !std::exchange(schema.selected[index], true);
}

bool ReadShortcut(Attributes& attributes, Control& control, Schema& schema) {
    const auto text = attributes.Take(kShortcutAttribute);
    if (!text) {
        return true;
    }
    if (text->size() != 1) {
        return false;
    }
    char key = text->front();
    if (key >= 'A' && key <= 'Z') {
        key = static_cast<char>(key + ('a' - 'A'));
    }
    if (!((key >= 'a' && key <= 'z') || (key >= '0' && key <= '9')) ||
        std::exchange(schema.shortcuts[static_cast<unsigned char>(key)], true)) {
        return false;
    }
    control.key = key;
    return true;
}

bool ReadWidget(const Element& element, Attributes& attributes, Control& control, Schema& schema) {
    using Type = ControlType;
    constexpr std::pair<std::string_view, Type> widgetNames[]{
        {"TextBlock", Type::Label}, {"Label", Type::Label}, {"Button", Type::Button}, {"CheckBox", Type::Check},
        {"RadioButton", Type::Radio}, {"Slider", Type::Slider}, {"ComboBox", Type::Select}, {"TextBox", Type::Text},
        {"Image", Type::Image}, {"Separator", Type::Divider}, {"ToggleButton", Type::Toggle},
        {"ProgressBar", Type::Progress}, {"ListBox", Type::List},
    };
    const auto widget = std::find_if(std::begin(widgetNames), std::end(widgetNames),
        [&element](const auto& pair) { return pair.first == element.name; });
    if (widget == std::end(widgetNames)) {
        return false;
    }
    control.type = widget->second;
    const auto type = control.type;
    if (type == Type::Select || type == Type::List) {
        return ReadChoices(element, attributes, control) && ReadShortcut(attributes, control, schema);
    }
    if (!element.children.empty()) {
        return false;
    }
    switch (type) {
        case Type::Label:
            return attributes.Text(element.name == "Label" ? "Content" : "Text", control.text, kMaxLabelBytes, true);
        case Type::Button:
        case Type::Check:
        case Type::Toggle:
        case Type::Radio:
            return attributes.Text("Content", control.text, kMaxLabelBytes, true) &&
                ReadShortcut(attributes, control, schema) &&
                (type == Type::Button || (type == Type::Radio ? ReadRadio(attributes, control, schema)
                                                            : attributes.Checked(control.first)));
        case Type::Text:
            return attributes.Text("Text", control.text, kMaxValueBytes) && ReadShortcut(attributes, control, schema);
        case Type::Slider:
            return ReadRange(attributes, control) && ReadShortcut(attributes, control, schema);
        case Type::Progress:
            return ReadRange(attributes, control);
        case Type::Image:
            return attributes.Text("Source", control.text, kMaxNameBytes, true) && IsName(control.text);
        case Type::Divider:
            return true;
        default:
            return false;
    }
}

bool ReadNode(const Element& element, Layout parent, Node& node, Schema& schema) {
    Attributes attributes(element);
    if (!ReadAxis(attributes, parent, false, node.horizontal) || !ReadAxis(attributes, parent, true, node.vertical) ||
        !ReadMargin(attributes, node)) {
        return false;
    }

    if (element.name == "Grid") {
        node.layout = Layout::Grid;
    } else if (element.name == "StackPanel") {
        node.layout = Layout::Stack;
        const auto orientation = attributes.Take("Orientation").value_or("Vertical");
        if (orientation != "Horizontal" && orientation != "Vertical") {
            return false;
        }
        node.verticalStack = orientation == "Vertical";
    } else if (element.name == "Canvas") {
        node.layout = Layout::Canvas;
    } else if (element.name == "GroupBox") {
        node.layout = Layout::Group;
        node.control.type = ControlType::Group;
        if (element.children.size() != 1 || !attributes.Text("Header", node.control.text, kMaxLabelBytes, true)) {
            return false;
        }
    } else {
        return ReadWidget(element, attributes, node.control, schema) && attributes.Done();
    }
    if (!attributes.Done()) {
        return false;
    }

    for (const auto& child : element.children) {
        if (node.layout == Layout::Grid && (child.name == "Grid.RowDefinitions" || child.name == "Grid.ColumnDefinitions")) {
            const bool rows = child.name == "Grid.RowDefinitions";
            if (!ReadTracks(child, rows, rows ? node.rows : node.columns)) {
                return false;
            }
        } else if (!ReadNode(child, node.layout, node.children.emplace_back(), schema)) {
            return false;
        }
    }
    if (node.layout == Layout::Grid) {
        if (node.rows.empty()) {
            node.rows.emplace_back();
        }
        if (node.columns.empty()) {
            node.columns.emplace_back();
        }
        for (const auto& child : node.children) {
            if (child.vertical.cell + child.vertical.span > static_cast<int>(node.rows.size()) ||
                child.horizontal.cell + child.horizontal.span > static_cast<int>(node.columns.size())) {
                return false;
            }
        }
    }
    return !node.children.empty();
}

int DesiredSize(const Node& node, bool vertical) {
    const Axis& axis = vertical ? node.vertical : node.horizontal;
    if (axis.size || node.layout != Layout::Widget) {
        return axis.size;
    }
    constexpr int kDefaultWidth = 120;
    constexpr int kDefaultHeight = 32;
    constexpr int kImageSize = 48;
    if (node.control.type == ControlType::Image) {
        return kImageSize;
    }
    if (!vertical) {
        return kDefaultWidth;
    }
    switch (node.control.type) {
        case ControlType::Divider: return 2;
        case ControlType::Progress: return 20;
        case ControlType::List: return 100;
        default: return kDefaultHeight;
    }
}

bool ArrangeAxis(const Node& node, bool vertical, int& start, int& length) {
    const Axis& axis = vertical ? node.vertical : node.horizontal;
    start += axis.before;
    length -= axis.before + axis.after;
    const int desired = axis.alignment == Alignment::Stretch ? axis.size : DesiredSize(node, vertical);
    if (length <= 0 || desired > length) {
        return false;
    }
    if (desired) {
        const int spare = length - desired;
        if (axis.alignment == Alignment::Center) {
            start += spare / 2;
        } else if (axis.alignment == Alignment::End) {
            start += spare;
        }
        length = desired;
    }
    return true;
}

using TrackEdges = std::array<int, kMaxTracks + 1>;

bool ArrangeTracks(const Node& node, bool vertical, int length, TrackEdges& edges) {
    const auto& tracks = vertical ? node.rows : node.columns;
    std::array<int, kMaxTracks> sizes{};
    int remaining = length;
    int totalWeight = 0;
    for (std::size_t index = 0; index < tracks.size(); ++index) {
        const auto& track = tracks[index];
        if (track.kind == TrackKind::Star) {
            totalWeight += track.value;
        } else if (track.kind == TrackKind::Fixed) {
            sizes[index] = track.value;
        } else {
            sizes[index] = 1;
        }
    }
    for (const auto& child : node.children) {
        const Axis& axis = vertical ? child.vertical : child.horizontal;
        const auto index = static_cast<std::size_t>(axis.cell);
        if (axis.span == 1 && tracks[index].kind == TrackKind::Auto) {
            const int desired = DesiredSize(child, vertical);
            if (!desired) {
                return false;
            }
            sizes[index] = (std::max)(sizes[index], desired + axis.before + axis.after);
        }
    }
    for (std::size_t index = 0; index < tracks.size(); ++index) {
        remaining -= sizes[index];
    }
    if (remaining < 0) {
        return false;
    }
    int weight = 0;
    for (std::size_t index = 0; index < tracks.size(); ++index) {
        if (tracks[index].kind == TrackKind::Star) {
            sizes[index] = remaining * (weight + tracks[index].value) / totalWeight -
                remaining * weight / totalWeight;
            weight += tracks[index].value;
            if (!sizes[index]) {
                return false;
            }
        }
        edges[index + 1] = edges[index] + sizes[index];
    }
    return true;
}

bool AddControl(Control& control, Rect rect, Form& form) {
    if (form.controls.size() == kMaxControls) {
        return false;
    }
    control.x = rect.x;
    control.y = rect.y;
    control.width = rect.width;
    control.height = rect.height;
    form.controls.push_back(std::move(control));
    return true;
}

bool ArrangeNode(Node& node, Rect area, Form& form) {
    if (!ArrangeAxis(node, false, area.x, area.width) || !ArrangeAxis(node, true, area.y, area.height)) {
        return false;
    }

    switch (node.layout) {
        case Layout::Widget:
            return AddControl(node.control, area, form);
        case Layout::Group: {
            constexpr int kInset = 12;
            constexpr int kHeader = 24;
            return AddControl(node.control, area, form) &&
                ArrangeNode(node.children.front(),
                    {area.x + kInset, area.y + kHeader, area.width - 2 * kInset, area.height - kHeader - kInset}, form);
        }
        case Layout::Stack: {
            int remaining = node.verticalStack ? area.height : area.width;
            for (auto& child : node.children) {
                const Axis& axis = node.verticalStack ? child.vertical : child.horizontal;
                const int desired = DesiredSize(child, node.verticalStack);
                const int length = desired + axis.before + axis.after;
                if (!desired || length > remaining) {
                    return false;
                }
                Rect childArea = area;
                if (node.verticalStack) {
                    childArea.height = length;
                    area.y += length;
                } else {
                    childArea.width = length;
                    area.x += length;
                }
                remaining -= length;
                if (!ArrangeNode(child, childArea, form)) {
                    return false;
                }
            }
            return true;
        }
        case Layout::Canvas:
            for (auto& child : node.children) {
                const int left = child.horizontal.offset;
                const int top = child.vertical.offset;
                if (!ArrangeNode(child, {area.x + left, area.y + top, area.width - left, area.height - top}, form)) {
                    return false;
                }
            }
            return true;
        case Layout::Grid: {
            TrackEdges columns{};
            TrackEdges rows{};
            if (!ArrangeTracks(node, false, area.width, columns) || !ArrangeTracks(node, true, area.height, rows)) {
                return false;
            }
            for (auto& child : node.children) {
                const auto column = static_cast<std::size_t>(child.horizontal.cell);
                const auto row = static_cast<std::size_t>(child.vertical.cell);
                const int width = columns[column + static_cast<std::size_t>(child.horizontal.span)] - columns[column];
                const int height = rows[row + static_cast<std::size_t>(child.vertical.span)] - rows[row];
                if (!ArrangeNode(child, {area.x + columns[column], area.y + rows[row], width, height}, form)) {
                    return false;
                }
            }
            return true;
        }
    }
    return false;
}

} // namespace

bool ParseForm(std::string_view bytes, Form& form) {
    XmlDocument document;
    if (!document.Parse(bytes) || document.root.children.size() != 1) {
        return false;
    }
    Attributes attributes(document.root);
    Form parsed;
    if (!attributes.Int("Width", parsed.width, kMinimumWidth, kMaximumWidth, true) ||
        !attributes.Int("Height", parsed.height, kMinimumHeight, kMaximumHeight, true) ||
        !attributes.Text("Title", parsed.title, kMaxLabelBytes, true) || !attributes.Done()) {
        return false;
    }
    Schema schema;
    Node node;
    if (!ReadNode(document.root.children.front(), Layout::Group, node, schema) ||
        !ArrangeNode(node, {0, 0, parsed.width, parsed.height}, parsed)) {
        return false;
    }
    form = std::move(parsed);
    return true;
}

bool ValidAction(const Form& form, unsigned control, std::string_view value) {
    if (!control || control > form.controls.size() || value.size() > kMaxValueBytes) {
        return false;
    }
    const auto& item = form.controls[control - 1];
    int number = 0;
    switch (item.type) {
        case ControlType::Button: return value.empty();
        case ControlType::Toggle:
        case ControlType::Check: return value == "0" || value == "1";
        case ControlType::Radio: return value == "1";
        case ControlType::Slider: return ReadInteger(value, number) && number >= item.first && number <= item.second;
        case ControlType::List:
        case ControlType::Select:
            return ReadInteger(value, number) && number >= 0 && static_cast<std::size_t>(number) < item.options.size();
        case ControlType::Text: return Printable(value, kMaxValueBytes);
        default: return false;
    }
}

} // namespace cse::forms
