#ifndef VGUI_MESSAGES_H
#define VGUI_MESSAGES_H

#include "vgui/IVGui.h"

class KeyValues;

struct MessageItem_t
{
	KeyValues*		_params;
	vgui2::HPanel	_messageTo;
	vgui2::HPanel	_from;

    double _arrivalTime;
	int				_messageID;
};

#endif
