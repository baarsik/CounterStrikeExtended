//========= Copyright © 1996-2003, Valve LLC, All rights reserved. ============
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "ScrollBarSlider.h"
#include "Controls.h"

#include "vgui/IBorder.h"
#include "vgui/IInput.h"
#include "vgui/IScheme.h"
#include "vgui/ISurface.h"
#include "vgui/KeyValues.h"
#include "vgui/MessageMap.h"

#include <algorithm>
#include <cmath>

namespace vgui2
{

//-----------------------------------------------------------------------------
// Purpose: Get the ScrollBarSlider value of the nob.
//-----------------------------------------------------------------------------
int ScrollBarSlider::GetValue()
{
	return _value;
}

//-----------------------------------------------------------------------------
// Purpose: Whether the scroll bar is vertical (true) or not (false)
//-----------------------------------------------------------------------------
bool ScrollBarSlider::IsVertical()
{
	return _vertical;
}

void ScrollBarSlider::GetRange( int& minimum, int& maximum )
{
	minimum = _range[0];
	maximum = _range[1];
}

//-----------------------------------------------------------------------------
// Purpose: Get the number of lines visible in the window the ScrollBarSlider is attached to
//-----------------------------------------------------------------------------
int ScrollBarSlider::GetRangeWindow()
{
	return _rangeWindow;
}

//-----------------------------------------------------------------------------
// Purpose: Set the number of lines visible in the window the ScrollBarSlider is attached to
//-----------------------------------------------------------------------------
void ScrollBarSlider::SetRangeWindow( int rangeWindow )
{
	_rangeWindow = rangeWindow;
}

void ScrollBarSlider::GetNobPos( int& minimum, int& maximum )
{
	minimum = _nobPos[0];
	maximum = _nobPos[1];
}

bool ScrollBarSlider::HasFullRange()
{
	int wide, tall;
	GetPaintSize( wide, tall );

	float frangewindow = (float)_rangeWindow;
	float checkAgainst = (float)( _vertical ? tall : wide );
	if ( frangewindow > 0.0f )
		return (float)_buttonOffset + checkAgainst >= frangewindow;

	return false;
}

void ScrollBarSlider::SetButtonOffset( int offset )
{
	_buttonOffset = offset;
}

ScrollBarSlider::ScrollBarSlider( Panel* parent, const char* name, bool vertical ) : Panel( parent, name )
{
	_vertical = vertical;
	_dragging = false;
	_value = 0;
	_range[0] = 0;
	_range[1] = 0;
	_rangeWindow = 0;
	_buttonOffset = 0;
	_ScrollBarSliderBorder = NULL;
	ScrollBarSlider::RecomputeNobPosFromValue();
}

ScrollBarSlider::~ScrollBarSlider()
{
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void ScrollBarSlider::Paint()
{
	int wide, tall;
	GetPaintSize( wide, tall );

	int range = _range[1] - _range[0];
	if ( range <= 0 || range <= _rangeWindow )
		return;

	surface()->DrawSetColor( GetFgColor() );

	if ( _vertical )
	{
		// Nob
		surface()->DrawFilledRect( 0, _nobPos[0], wide - 1, _nobPos[1] );
		if ( _ScrollBarSliderBorder )
			_ScrollBarSliderBorder->Paint( 0, _nobPos[0], wide - 1, _nobPos[1] );
	}
	else
	{
		// horizontal nob
		surface()->DrawFilledRect( _nobPos[0], 0, _nobPos[1], tall );
		if ( _ScrollBarSliderBorder )
			_ScrollBarSliderBorder->Paint( _nobPos[0] - 1, 1, _nobPos[1], tall );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void ScrollBarSlider::PaintBackground()
{
//	BaseClass::PaintBackground();

	int wide, tall;
	GetPaintSize( wide, tall );

	surface()->DrawSetColor( GetBgColor() );
	surface()->DrawFilledRect( 0, 0, wide - 1, tall - 1 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ScrollBarSlider::PerformLayout()
{
	RecomputeNobPosFromValue();
	Panel::PerformLayout();
}

//-----------------------------------------------------------------------------
// Purpose: Respond to cursor movements, we only care about clicking and dragging
//-----------------------------------------------------------------------------
void ScrollBarSlider::OnCursorMoved( int x, int y )
{
	if ( !_dragging )
		return;

	int wide, tall;
	GetPaintSize( wide, tall );

	if ( _vertical )
	{
		_nobPos[0] = _nobDragStartPos[0] + ( y - _dragStartPos[1] );
		_nobPos[1] = _nobDragStartPos[1] + ( y - _dragStartPos[1] );

		if ( _nobPos[1] > tall )
		{
			_nobPos[0] = tall - ( _nobPos[1] - _nobPos[0] );
			_nobPos[1] = tall;
			SetValue( _range[1] - _rangeWindow );
		}
	}
	else
	{
		_nobPos[0] = _nobDragStartPos[0] + ( x - _dragStartPos[0] );
		_nobPos[1] = _nobDragStartPos[1] + ( x - _dragStartPos[0] );

		if ( _nobPos[1] > wide )
		{
			_nobPos[0] = wide - ( _nobPos[1] - _nobPos[0] );
			_nobPos[1] = wide;
		}
	}

	if ( _nobPos[0] < 0 )
	{
		_nobPos[1] = _nobPos[1] - _nobPos[0];
		_nobPos[0] = 0;
		SetValue( 0 );
	}

	InvalidateLayout( false, false );
	RecomputeValueFromNobPos();
//	Repaint();
	SendScrollBarSliderMovedMessage();
}

void ScrollBarSlider::OnMousePressed( MouseCode )
{
	int x, y;
	input()->GetCursorPos( x, y );
	Panel::ScreenToLocal( x, y );

	int pos = _vertical ? y : x;
	if ( pos < _nobPos[0] )
	{
		SetValue( GetValue() - _rangeWindow );
	}
	else if ( pos >= _nobPos[1] )
	{
		SetValue( GetValue() + _rangeWindow );
	}
	else
	{
		_dragging = true;
		input()->SetMouseCapture( GetVPanel() );
		_nobDragStartPos[0] = _nobPos[0];
		_nobDragStartPos[1] = _nobPos[1];
		_dragStartPos[0] = x;
		_dragStartPos[1] = y;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Treat double clicks as single clicks
//-----------------------------------------------------------------------------
void ScrollBarSlider::OnMouseDoublePressed( MouseCode code )
{
	OnMousePressed( code );
}

//-----------------------------------------------------------------------------
// Purpose: Stop looking for mouse events when mouse is up.
//-----------------------------------------------------------------------------
void ScrollBarSlider::OnMouseReleased( MouseCode )
{
	_dragging = false;
	input()->SetMouseCapture( 0 );
}

void ScrollBarSlider::ApplySchemeSettings( IScheme* scheme )
{
	Panel::ApplySchemeSettings( scheme );

	SetFgColor( GetSchemeColor( "ScrollBarSlider/ScrollBarSliderFgColor", scheme ) );
	SetBgColor( GetSchemeColor( "ScrollBarSlider/ScrollBarSliderBgColor", scheme ) );
	_ScrollBarSliderBorder = scheme->GetBorder( "ButtonBorder" );
}

//-----------------------------------------------------------------------------
// Purpose: Given the value of the ScrollBarSlider, adjust the ends of the nob.
//-----------------------------------------------------------------------------
void ScrollBarSlider::RecomputeNobPosFromValue() {
    int wide;
    int tall;
    GetPaintSize(wide, tall);

    const int visibleWindow = (std::max)(0, _rangeWindow);
    const double range = static_cast<double>(_range[1]) - _range[0];
    const int maximumValue = static_cast<int>((std::max)(static_cast<double>(_range[0]),
        static_cast<double>(_range[1]) - visibleWindow));
    _value = (std::max)(_range[0], (std::min)(_value, maximumValue));

    // Preserve the legacy bottom-border convention, including horizontal minimum size.
    constexpr int BorderPixels = 1;
    const int verticalExtent = tall > 0 ? tall - BorderPixels : 0;
    const int trackLength = _vertical ? verticalExtent : (std::max)(0, wide);
    _nobPos[0] = 0;
    _nobPos[1] = 0;
    if (visibleWindow <= 0 || trackLength <= 0) {
        Repaint();
        return;
    }
    if (range <= visibleWindow) {
        _nobPos[1] = trackLength;
        Repaint();
        return;
    }

    const float length = static_cast<float>(trackLength);
    const float minimumSize = static_cast<float>(_vertical ? (std::max)(0, wide) : verticalExtent);
    const float nobSize = fmaxf(minimumSize, static_cast<float>(visibleWindow) / static_cast<float>(range) * length);
    if (nobSize >= length) {
        _nobPos[1] = trackLength;
        Repaint();
        return;
    }
    const float freePixels = length - nobSize;
    const float scrollRange = static_cast<float>(range - visibleWindow);
    const float value = static_cast<float>(static_cast<double>(_value) - _range[0]);
    const float fraction = (std::max)(0.0f, (std::min)(1.0f, value / scrollRange));
    const float firstPixel = freePixels * fraction;
    const auto boundedPixel = [trackLength](float position) {
        if (position <= 0.0f) {
            return 0;
        }
        // A float-rounded INT_MAX can be one past int's range; clamp before conversion.
        if (static_cast<double>(position) >= trackLength) {
            return trackLength;
        }
        return static_cast<int>(position);
    };
    _nobPos[0] = boundedPixel(firstPixel);
    _nobPos[1] = boundedPixel(firstPixel + nobSize);
    Repaint();
}

void ScrollBarSlider::RecomputeValueFromNobPos() {
    int wide;
    int tall;
    GetPaintSize(wide, tall);

    const int visibleWindow = (std::max)(0, _rangeWindow);
    const double range = static_cast<double>(_range[1]) - _range[0];
    const int maximumValue = static_cast<int>((std::max)(static_cast<double>(_range[0]),
        static_cast<double>(_range[1]) - visibleWindow));
    _value = (std::max)(_range[0], (std::min)(_value, maximumValue));
    const int trackLength = (std::max)(0, _vertical ? tall : wide);
    if (visibleWindow <= 0 || range <= visibleWindow || trackLength <= 0) {
        return;
    }

    const float length = static_cast<float>(trackLength);
    const float minimumSize = static_cast<float>((std::max)(0, _vertical ? wide : tall));
    const float rangeSize = static_cast<float>(range);
    const float windowSize = static_cast<float>(visibleWindow);
    const float nobSize = fmaxf(minimumSize, windowSize / rangeSize * length);
    const float freePixels = length - nobSize;
    if (freePixels <= 0.0f) {
        // A collapsed track cannot encode a new value; retain the clamped logical position.
        return;
    }

    const float fraction = (std::max)(0.0f,
        (std::min)(1.0f, static_cast<float>(_nobPos[0]) / freePixels));
    const float value = fraction * static_cast<float>(range - visibleWindow);
    constexpr float RangeEndTolerance = 0.01f;
    if (fabs(windowSize + value - static_cast<float>(_range[1])) < rangeSize * RangeEndTolerance) {
        _value = maximumValue;
    } else {
        const float position = static_cast<float>(_range[0]) + value;
        constexpr double ValueRoundingBias = 0.5;
        const double roundedPosition = position + ValueRoundingBias;
        if (roundedPosition <= _range[0]) {
            _value = _range[0];
        } else if (roundedPosition >= maximumValue) {
            _value = maximumValue;
        } else {
            _value = static_cast<int>(roundedPosition);
        }
    }
}

//-----------------------------------------------------------------------------
// Purpose: Inform other watchers that the ScrollBarSlider was moved
//-----------------------------------------------------------------------------
void ScrollBarSlider::SendScrollBarSliderMovedMessage()
{
	// send a changed message
	PostActionSignal( new KeyValues( "ScrollBarSliderMoved", "position", _value ) );
}

//-----------------------------------------------------------------------------
// Purpose: Set the ScrollBarSlider value of the nob.
//-----------------------------------------------------------------------------
void ScrollBarSlider::SetValue( int value )
{
	int oldValue = _value;

	if ( value < _range[0] )
		value = _range[0];
	if ( value > _range[1] - _rangeWindow )
		value = _range[1] - _rangeWindow;

	_value = value;
	RecomputeNobPosFromValue();

	if ( _value != oldValue )
		SendScrollBarSliderMovedMessage();
}

void ScrollBarSlider::SetRange( int minimum, int maximum )
{
	if ( maximum < minimum )
		maximum = minimum;

	_range[0] = minimum;
	_range[1] = maximum;
	InvalidateLayout( false, false );
}

void ScrollBarSlider::SetSize( int width, int height )
{
	Panel::SetSize( width, height );
	RecomputeNobPosFromValue();
}
}
