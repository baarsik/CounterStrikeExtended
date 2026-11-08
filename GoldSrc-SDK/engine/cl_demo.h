#pragma once

#include "mathlib.h"
#include "filesystem.h"
#include "common.h"
#include "engine_apiproxy.h"
#include "crc.h"
#include "net.h"
#include "../common/Sequence.h"
#include "../common/event_args.h"
#include "../common/ref_params.h"
#include "../common/usercmd.h"
#include "../pm_shared/pm_movevars.h"

#include <cstdio>
#include "../common/legacy_demo_records.h"

#define DEMO_PROTOCOL 5

typedef struct demo_info_s
{
	float timestamp;
	ref_params_t rp;
	usercmd_t cmd;
	movevars_t movevars;
	vec3_t view;
	int viewmodel;
} demo_info_t;

typedef struct sequence_info_s
{
	int incoming_sequence;
	int incoming_acknowledged;
	int incoming_reliable_acknowledged;
	int incoming_reliable_sequence;
	int outgoing_sequence;
	int reliable_sequence;
	int last_reliable_sequence;
	int length;
} sequence_info_t;

typedef cse::demo::LegacyDemoEntry demoentry_t;

typedef struct demodirectory_s
{
    int nEntries;
    demoentry_t *p_rgEntries;
} demodirectory_t;

typedef cse::demo::LegacyDemoHeader demoheader_t;

extern float g_demotimescale;
extern client_textmessage_t tm_demomessage;

void		CL_Record_f();
void		CL_Stop_f();
void		CL_BeginDemoStartup();
void		CL_WriteDemoStartup( int length, sizebuf_t* buffer );
void		CL_WriteDemoMessage( int length, sizebuf_t* buffer );
void		CL_DemoWriteSequenceInfo( FileHandle_t file, int length );
void		CL_DemoWriteCmdInfo( FileHandle_t file );
void		CL_WriteDLLUpdate( client_data_t* data );
void		CL_DemoAnim( int animation, int body );
void		CL_DemoEvent( int flags, int index, float delay, event_args_t* args );
void		CL_DemoQueueSound( int channel, const char* sample, float volume, float attenuation, int flags, int pitch );
void		CL_DemoCopyFileChunk( FILE* destination, FILE* source, int size );

void		CL_PlayDemo_f();
void		CL_TimeDemo_f();
void		CL_WavePlayLen_f();
void		CL_StopPlayback();
void		CL_FinishTimeDemo();
void		CL_FinishGameGauge();
qboolean	CL_ReadDemoMessage();
qboolean	CL_ReadDemoMessage_OLD();
void		CL_ReadNetchanState();
void		CL_ReadNetchanState_OLD();
void		CL_ReadClientDLLData();
void		CL_DemoReadCmdInfo( float time );
void		CL_DemoFrameInfo( demo_info_t* info, float time );
void		CL_DemoSectionStart();
qboolean	CL_SwitchToNextDemoSection();
void		CL_DemoPlaySound( int channel, char* sample, float attenuation, float volume, int flags, int pitch );
void		CL_DemoParseSound();
void		CL_DemoParseEvent();
void		CL_DemoParseAnim();
void		CL_QueueEvent( int flags, int index, float delay, event_args_t* args );
void		SetDemoMessage( const char* message, float fadein, float fadeout, float holdtime );
void		hudWeaponAnim( int animation, int body );

void		CL_FixPointers( demo_info_t* info );
void		CL_SetDemoViewInfo( ref_params_t* params, vec_t* view, int viewmodel );
void		CL_GetDemoViewInfo( ref_params_t* params, float* view, int* viewmodel );
