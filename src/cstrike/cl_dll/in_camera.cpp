//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "hud.h"
#include "cl_util.h"
#include "camera.h"
#include "kbutton.h"
#include <stdint.h>
#if UINTPTR_MAX > UINT32_MAX
#include "client_camera_mouse.h"
#include "native_mouse_polling.h"
#endif
#include "../../cl_dll/in_defs.h"
#include <stdlib.h>

#ifndef _WIN32
struct POINT_s
{
	int x;
	int y;
};

typedef POINT_s POINT;
#endif

float CL_KeyState( kbutton_t* key );
void KeyDown( kbutton_t* key );
void KeyUp( kbutton_t* key );
void CAM_ToThirdPerson();
void CAM_ToFirstPerson();
void CAM_StartDistance();
void CAM_EndDistance();

//-------------------------------------------------- Global Variables

cvar_t* cam_command;
cvar_t* cam_snapto;
cvar_t* cam_idealyaw;
cvar_t* cam_idealpitch;
cvar_t* cam_idealdist;
cvar_t* cam_contain;
cvar_t* c_maxpitch;
cvar_t* c_minpitch;
cvar_t* c_maxyaw;
cvar_t* c_minyaw;
cvar_t* c_maxdistance;
cvar_t* c_mindistance;

// pitch, yaw, dist
vec3_t cam_ofs;


// In third person
int cam_thirdperson;
int cam_mousemove; //true if we are moving the cam with the mouse, False if not
int iMouseInUse = 0;
int cam_distancemove;
int cam_old_mouse_x;
int cam_old_mouse_y;
POINT cam_mouse;

#if UINTPTR_MAX > UINT32_MAX
static bool s_cameraDragRequested = false;
static bool s_cameraDistanceRequested = false;
static bool s_cameraPublishedOwnership = false;
static cse::CameraMouseSnapshot s_cameraMouse;
#define CAM_MOUSE_CENTER_X() s_cameraMouse.centerX
#define CAM_MOUSE_HALF_DIFFERENCE(a, b) cse::CameraMouseHalfDifference(a, b)
#define CAM_MOUSE_CENTER_Y() s_cameraMouse.centerY
#define CAM_WARP_MOUSE(x, y) gEngfuncs.pfnSetMousePos(x, y)

static bool CAM_ReadNativeMouse(bool distanceMove) {
    if (!cse::TryReadCameraMouse(gEngfuncs.pfnGetClientMousePositionAndCenter,
        gHUD.GetSensitivity(), !distanceMove, s_cameraMouse)) {
        return false;
    }
    cam_mouse.x = s_cameraMouse.x;
    cam_mouse.y = s_cameraMouse.y;
    return true;
}

static void CAM_UpdateNativeMouseOwnership() {
    const bool owned = s_cameraDragRequested || s_cameraDistanceRequested;
    cam_distancemove = s_cameraDistanceRequested ? 1 : 0;
    cam_mousemove = owned ? 1 : 0;
    iMouseInUse = owned ? 1 : 0;
    if (owned != s_cameraPublishedOwnership) {
        if (!owned) {
            cse::CameraMouseSnapshot releasedMouse;
            if (cse::TryReadCameraMouse(gEngfuncs.pfnGetClientMousePositionAndCenter,
                0.0f, true, releasedMouse)) {
                gEngfuncs.pfnSetMousePos(releasedMouse.centerX, releasedMouse.centerY);
            }
        }
        IN_SetCameraMouseOwnership(owned);
        s_cameraPublishedOwnership = owned;
    }
}

static void CAM_StopNativeMouse() {
    s_cameraDragRequested = false;
    s_cameraDistanceRequested = false;
    CAM_UpdateNativeMouseOwnership();
}
#else
#define CAM_MOUSE_CENTER_X() gEngfuncs.GetWindowCenterX()
#define CAM_MOUSE_HALF_DIFFERENCE(a, b) ((a - b) / 2)
#define CAM_MOUSE_CENTER_Y() gEngfuncs.GetWindowCenterY()
#define CAM_WARP_MOUSE(x, y) SDL_SetCursorPos(x, y)
#endif


static kbutton_t cam_pitchup;
static kbutton_t cam_pitchdown;
static kbutton_t cam_yawleft;
static kbutton_t cam_yawright;
static kbutton_t cam_in;
static kbutton_t cam_out;

void SDL_GetCursorPos( POINT* )
{
}

void SDL_SetCursorPos( int, int )
{
}

//-------------------------------------------------- Local Functions

float MoveToward( float current, float goal, float )
{
	if ( current != goal )
	{
		if ( abs( (int)( current - goal ) ) > 180 )
		{
			if ( current < goal )
				current += 360.0f;
			else
				current -= 360.0f;
		}
		if ( current < goal )
		{
			if ( current < goal - 1.0f )
				current += ( goal - current ) / 4.0f;
			else
				current = goal;
		}
		else
		{
			if ( current > goal + 1.0f )
				current -= ( current - goal ) / 4.0f;
			else
				current = goal;
		}
	}
	if ( current < 0.0f )
		current += 360.0f;
	else if ( current >= 360.0f )
		current -= 360.0f;
	return current;
}

extern "C" void EXPORT CAM_Think()
{
	switch ( (int)cam_command->value )
	{
	case 1:
		CAM_ToThirdPerson();
		break;
	case 2:
		CAM_ToFirstPerson();
		break;
	}
	if ( !cam_thirdperson )
		return;
#if UINTPTR_MAX > UINT32_MAX
    if ((cam_mousemove || cam_distancemove) && !CAM_ReadNativeMouse(cam_distancemove != 0)) {
        return;
    }
#endif

	float pitch = cam_idealpitch->value;
	float yaw = cam_idealyaw->value;
	if ( cam_mousemove && !cam_distancemove )
	{

		//keep the camera within certain limits around the player (ie avoid certain bad viewing angles)  
		if ( cam_mouse.x > CAM_MOUSE_CENTER_X() )
		{
			if ( yaw < c_maxyaw->value )
				yaw += 0.5f * CAM_MOUSE_HALF_DIFFERENCE(cam_mouse.x, CAM_MOUSE_CENTER_X());
			if ( yaw > c_maxyaw->value )
				yaw = c_maxyaw->value;
		}
		else if ( cam_mouse.x < CAM_MOUSE_CENTER_X() )
		{
			if ( yaw > c_minyaw->value )
				yaw -= 0.5f * CAM_MOUSE_HALF_DIFFERENCE(CAM_MOUSE_CENTER_X(), cam_mouse.x);
			if ( yaw < c_minyaw->value )
				yaw = c_minyaw->value;
		}
		if ( cam_mouse.y > CAM_MOUSE_CENTER_Y() )
		{
			if ( pitch < c_maxpitch->value )
				pitch += 0.5f * CAM_MOUSE_HALF_DIFFERENCE(cam_mouse.y, CAM_MOUSE_CENTER_Y());
			if ( pitch > c_maxpitch->value )
				pitch = c_maxpitch->value;
		}
		else if ( cam_mouse.y < CAM_MOUSE_CENTER_Y() )
		{
			if ( pitch > c_minpitch->value )
				pitch -= 0.5f * CAM_MOUSE_HALF_DIFFERENCE(CAM_MOUSE_CENTER_Y(), cam_mouse.y);
			if ( pitch < c_minpitch->value )
				pitch = c_minpitch->value;
		}
		float sensitivity = gHUD.GetSensitivity();
		if ( sensitivity != 0.0f )
		{
			cam_old_mouse_x = (int)( cam_mouse.x * sensitivity );
			cam_old_mouse_y = (int)( cam_mouse.y * sensitivity );
		}
		else
		{
			cam_old_mouse_x = cam_mouse.x;
			cam_old_mouse_y = cam_mouse.y;
		}
		int centerY = CAM_MOUSE_CENTER_Y();
		int centerX = CAM_MOUSE_CENTER_X();
		CAM_WARP_MOUSE( centerX, centerY );
	}

	//Nathan code here
	if ( CL_KeyState( &cam_pitchup ) )
		pitch += 2.5f;
	else if ( CL_KeyState( &cam_pitchdown ) )
		pitch -= 2.5f;
	if ( CL_KeyState( &cam_yawleft ) )
		yaw -= 2.5f;
	else if ( CL_KeyState( &cam_yawright ) )
		yaw += 2.5f;

	float distance = 150.0f;
	if ( CL_KeyState( &cam_in ) )
		distance -= 1.0f;
	else if ( CL_KeyState( &cam_out ) )
		distance += 1.0f;
	if ( cam_distancemove )
	{
		if ( cam_mouse.y > CAM_MOUSE_CENTER_Y() )
		{
			if ( distance < c_maxdistance->value )
				distance += (float)CAM_MOUSE_HALF_DIFFERENCE(cam_mouse.y, CAM_MOUSE_CENTER_Y());
			if ( distance > c_maxdistance->value )
				distance = c_maxdistance->value;
		}
		else if ( cam_mouse.y < CAM_MOUSE_CENTER_Y() )
		{
			if ( distance > c_mindistance->value )
				distance -= (float)CAM_MOUSE_HALF_DIFFERENCE(CAM_MOUSE_CENTER_Y(), cam_mouse.y);
			if ( distance < c_mindistance->value )
				distance = c_mindistance->value;
		}
		cam_old_mouse_x = (int)( cam_mouse.x * gHUD.GetSensitivity() );
		cam_old_mouse_y = (int)( cam_mouse.y * gHUD.GetSensitivity() );
		int centerY = CAM_MOUSE_CENTER_Y();
		int centerX = CAM_MOUSE_CENTER_X();
		CAM_WARP_MOUSE( centerX, centerY );
	}
	cam_idealpitch->value = pitch;
	cam_idealyaw->value = yaw;
	cam_idealdist->value = 150.0f;

	vec3_t angles;
	gEngfuncs.GetViewAngles( angles );
	pitch = cam_ofs[PITCH];
	yaw = cam_ofs[YAW];
	if ( cam_snapto->value )
	{
		yaw = cam_idealyaw->value + angles[YAW];
		pitch = cam_idealpitch->value + angles[PITCH];
	}
	else
	{
		if ( yaw - angles[YAW] != cam_idealyaw->value )
			yaw = MoveToward( yaw, cam_idealyaw->value + angles[YAW], 2.5f );
		if ( pitch - angles[PITCH] != cam_idealpitch->value )
			pitch = MoveToward( pitch, cam_idealpitch->value + angles[PITCH], 2.5f );
	}
	cam_ofs[PITCH] = pitch;
	cam_ofs[YAW] = yaw;
	cam_ofs[2] = distance;
}

void CAM_PitchUpDown()
{
	KeyDown( &cam_pitchup );
}

void CAM_PitchUpUp()
{
	KeyUp( &cam_pitchup );
}

void CAM_PitchDownDown()
{
	KeyDown( &cam_pitchdown );
}

void CAM_PitchDownUp()
{
	KeyUp( &cam_pitchdown );
}

void CAM_YawLeftDown()
{
	KeyDown( &cam_yawleft );
}

void CAM_YawLeftUp()
{
	KeyUp( &cam_yawleft );
}

void CAM_YawRightDown()
{
	KeyDown( &cam_yawright );
}

void CAM_YawRightUp()
{
	KeyUp( &cam_yawright );
}

void CAM_InDown()
{
	KeyDown( &cam_in );
}

void CAM_InUp()
{
	KeyUp( &cam_in );
}

void CAM_OutDown()
{
	KeyDown( &cam_out );
}

void CAM_OutUp()
{
	KeyUp( &cam_out );
}

void CAM_ToThirdPerson()
{
	if ( gEngfuncs.GetMaxClients() > 1 )
		return;
	vec3_t angles;
	gEngfuncs.GetViewAngles( angles );
	if ( !cam_thirdperson )
	{
		cam_thirdperson = 1;
		cam_ofs[YAW] = angles[YAW];
		cam_ofs[PITCH] = angles[PITCH];
		cam_ofs[2] = 30.0f;
	}
	gEngfuncs.Cvar_SetValue( "cam_command", 0.0f );
}

void CAM_ToFirstPerson()
{
	cam_thirdperson = 0;
	gEngfuncs.Cvar_SetValue( "cam_command", 0.0f );
#if UINTPTR_MAX > UINT32_MAX
    CAM_StopNativeMouse();
#endif
}

void CAM_ToggleSnapto()
{
	cam_snapto->value = !cam_snapto->value;
}

void CAM_Init()
{
	gEngfuncs.pfnAddCommand( "+campitchup", CAM_PitchUpDown );
	gEngfuncs.pfnAddCommand( "-campitchup", CAM_PitchUpUp );
	gEngfuncs.pfnAddCommand( "+campitchdown", CAM_PitchDownDown );
	gEngfuncs.pfnAddCommand( "-campitchdown", CAM_PitchDownUp );
	gEngfuncs.pfnAddCommand( "+camyawleft", CAM_YawLeftDown );
	gEngfuncs.pfnAddCommand( "-camyawleft", CAM_YawLeftUp );
	gEngfuncs.pfnAddCommand( "+camyawright", CAM_YawRightDown );
	gEngfuncs.pfnAddCommand( "-camyawright", CAM_YawRightUp );
	gEngfuncs.pfnAddCommand( "+camin", CAM_InDown );
	gEngfuncs.pfnAddCommand( "-camin", CAM_InUp );
	gEngfuncs.pfnAddCommand( "+camout", CAM_OutDown );
	gEngfuncs.pfnAddCommand( "-camout", CAM_OutUp );
	gEngfuncs.pfnAddCommand( "thirdperson", CAM_ToThirdPerson );
	gEngfuncs.pfnAddCommand( "firstperson", CAM_ToFirstPerson );
	gEngfuncs.pfnAddCommand( "+cammousemove", CAM_StartMouseMove );
	gEngfuncs.pfnAddCommand( "-cammousemove", CAM_EndMouseMove );
	gEngfuncs.pfnAddCommand( "+camdistance", CAM_StartDistance );
	gEngfuncs.pfnAddCommand( "-camdistance", CAM_EndDistance );
	gEngfuncs.pfnAddCommand( "snapto", CAM_ToggleSnapto );
	cam_command = CVAR_CREATE( "cam_command", "0", 0 );
	cam_snapto = CVAR_CREATE( "cam_snapto", "0", 0 );
	cam_idealyaw = CVAR_CREATE( "cam_idealyaw", "90", 0 );
	cam_idealpitch = CVAR_CREATE( "cam_idealpitch", "0", 0 );
	cam_idealdist = CVAR_CREATE( "cam_idealdist", "64", 0 );
	c_maxpitch = CVAR_CREATE( "c_maxpitch", "90.0", 0 );
	c_minpitch = CVAR_CREATE( "c_minpitch", "0.0", 0 );
	c_maxyaw = CVAR_CREATE( "c_maxyaw", "135.0", 0 );
	c_minyaw = CVAR_CREATE( "c_minyaw", "-135.0", 0 );
	c_maxdistance = CVAR_CREATE( "c_maxdistance", "200.0", 0 );
	c_mindistance = CVAR_CREATE( "c_mindistance", "30.0", 0 );
}

void CAM_ClearStates()
{
	vec3_t angles;
	gEngfuncs.GetViewAngles( angles );
	cam_pitchup.state = 0;
	cam_pitchdown.state = 0;
	cam_yawleft.state = 0;
	cam_yawright.state = 0;
	cam_in.state = 0;
	cam_out.state = 0;
	cam_thirdperson = 0;
	cam_command->value = 0.0f;
	cam_mousemove = 0;
	cam_snapto->value = 0.0f;
	cam_distancemove = 0;
	cam_ofs[0] = 0.0f;
	cam_ofs[1] = 0.0f;
	cam_ofs[2] = 30.0f;
	cam_idealpitch->value = angles[PITCH];
	cam_idealyaw->value = angles[YAW];
	cam_idealdist->value = 30.0f;
#if UINTPTR_MAX > UINT32_MAX
    CAM_StopNativeMouse();
#endif
}

void CAM_StartMouseMove()
{
#if UINTPTR_MAX > UINT32_MAX
    if (!cam_thirdperson) {
        CAM_StopNativeMouse();
        return;
    }
    if (!s_cameraDragRequested && CAM_ReadNativeMouse(false)) {
        s_cameraDragRequested = true;
        cam_old_mouse_x = s_cameraMouse.historyX;
        cam_old_mouse_y = s_cameraMouse.historyY;
        CAM_UpdateNativeMouseOwnership();
    }
#else

	//only move the cam with mouse if we are in third person.
	if ( cam_thirdperson )
	{
		//set appropriate flags and initialize the old mouse position
		//variables for mouse camera movement
		if ( !cam_mousemove )
		{
			cam_mousemove = 1;
			iMouseInUse = 1;
			float sensitivity = gHUD.GetSensitivity();
			if ( sensitivity != 0.0f )
			{
				cam_old_mouse_x = (int)( cam_mouse.x * sensitivity );
				cam_old_mouse_y = (int)( cam_mouse.y * sensitivity );
			}
			else
			{
				cam_old_mouse_x = cam_mouse.x;
				cam_old_mouse_y = cam_mouse.y;
			}
		}
	}
	//we are not in 3rd person view..therefore do not allow camera movement
	else
	{
		cam_mousemove = 0;
		iMouseInUse = 0;
	}
#endif
}

//the key has been released for camera movement
//tell the engine that mouse camera movement is off
void CAM_EndMouseMove()
{
#if UINTPTR_MAX > UINT32_MAX
    s_cameraDragRequested = false;
    CAM_UpdateNativeMouseOwnership();
#else
	cam_mousemove = 0;
	iMouseInUse = 0;
#endif
}


//----------------------------------------------------------
//routines to start the process of moving the cam in or out 
//using the mouse
//----------------------------------------------------------
void CAM_StartDistance()
{
#if UINTPTR_MAX > UINT32_MAX
    if (!cam_thirdperson) {
        CAM_StopNativeMouse();
        return;
    }
    if (!s_cameraDistanceRequested && CAM_ReadNativeMouse(true)) {
        s_cameraDistanceRequested = true;
        cam_old_mouse_x = s_cameraMouse.historyX;
        cam_old_mouse_y = s_cameraMouse.historyY;
        CAM_UpdateNativeMouseOwnership();
    }
#else
	//only move the cam with mouse if we are in third person.
	if ( cam_thirdperson )
	{
		//set appropriate flags and initialize the old mouse position
		//variables for mouse camera movement
		if ( !cam_distancemove )
		{
			cam_distancemove = 1;
			cam_mousemove = 1;
			iMouseInUse = 1;
			cam_old_mouse_x = (int)( cam_mouse.x * gHUD.GetSensitivity() );
			cam_old_mouse_y = (int)( cam_mouse.y * gHUD.GetSensitivity() );
		}
	}
	//we are not in 3rd person view..therefore do not allow camera movement
	else
	{
		cam_distancemove = 0;
		cam_mousemove = 0;
		iMouseInUse = 0;
	}
#endif
}

//the key has been released for camera movement
//tell the engine that mouse camera movement is off
void CAM_EndDistance()
{
#if UINTPTR_MAX > UINT32_MAX
    s_cameraDistanceRequested = false;
    CAM_UpdateNativeMouseOwnership();
#else
	cam_distancemove = 0;
	cam_mousemove = 0;
	iMouseInUse = 0;
#endif
}

extern "C" int EXPORT CL_IsThirdPerson()
{
	return cam_thirdperson || ( g_iUser1 && g_iUser2 == gEngfuncs.GetLocalPlayer()->index );
}

extern "C" void EXPORT CL_CameraOffset( float* offset )
{
//	RecClCL_GetCameraOffsets(ofs);

	VectorCopy( cam_ofs, offset );
}
