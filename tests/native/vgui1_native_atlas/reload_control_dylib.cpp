// Independent sanitizer reload control: no VGUI, Runtime, provider or SDK code.
class ReloadControl {
public:
    virtual ~ReloadControl();
    virtual int Value() const;
};

ReloadControl::~ReloadControl() = default;

int ReloadControl::Value() const {
    constexpr int ControlValue = 73;
    return ControlValue;
}

extern "C" int ReloadControlValue() {
    ReloadControl value;
    return value.Value();
}
