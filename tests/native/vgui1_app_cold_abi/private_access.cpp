#include "VGUI_App.h"
#include "cse_vgui1_app_cold_abi.h"

namespace cse { namespace vgui1_app_cold_abi {
CSE_VGUI1_COLD_MEMBER(RunningField, vgui::App, _running);
CSE_VGUI1_COLD_MEMBER(TimingField, vgui::App, _nextTickMillis);
CSE_VGUI1_COLD_MEMBER(CursorField, vgui::App, _cursor);
CSE_VGUI1_COLD_MEMBER(PrivateInit, vgui::App, init);
CSE_VGUI1_COLD_MEMBER(StaticGetter, vgui::App, getInstance);
CSE_VGUI1_COLD_MEMBER(DarCount, vgui::Dar<vgui::SurfaceBase*>, _count);

static_assert(std::is_same<RunningFieldType, bool vgui::App::*>::value, "Actual private field type");
static_assert(std::is_same<CursorFieldType, vgui::Cursor* (vgui::App::*)[vgui::Cursor::dc_last]>::value,
    "Actual array member type and extent");
static_assert(std::is_same<PrivateInitType, void (vgui::App::*)()>::value, "Actual private virtual type");
static_assert(std::is_same<DarCountType, int vgui::Dar<vgui::SurfaceBase*>::*>::value,
    "Actual protected template field type");
static_assert(FreeCallShape<StaticGetterType>::Value != 0, "Actual imported static getter call shape");
static_assert(TypeShape<bool>::Value != TypeShape<unsigned char>::Value, "Same-size types stay distinct");
static_assert(TypeShape<int>::Value != TypeShape<long>::Value, "Long is not globally normalized");
static_assert(TimingFieldShape<int>::Value == TimingFieldShape<std::int32_t>::Value,
    "Only explicit signed32 timing storage normalization");
static_assert(TypeShape<bool[3]>::Value != TypeShape<bool[4]>::Value, "Array extent participates");
static_assert(MethodShape<void (vgui::App::*)(vgui::Panel*)>::Value
    != MethodShape<void (vgui::App::*)(int, int, int, int, bool)>::Value,
    "Overload argument shapes participate");
static_assert(std::is_trivially_destructible<vgui::App>::value, "Actual trivial destructor");
static_assert(!std::has_virtual_destructor<vgui::App>::value, "Actual nonvirtual destructor");
} }
