/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
#ifndef EIFACE_H
#define EIFACE_H

#include "archtypes.h"     // DAL
#include "cse_string.h"
#include "cse_symbol.h"
#include "cse_entity.h"

#ifdef HLDEMO_BUILD
#define INTERFACE_VERSION       001
#else  // !HLDEMO_BUILD, i.e., regular version of HL
#define INTERFACE_VERSION		140
#endif // !HLDEMO_BUILD

#include <stdio.h>
#include <stddef.h>
#include "custom.h"
#include "cvardef.h"
#include "Sequence.h"
//
// Defines entity interface between engine and DLLs.
// This header file included by engine files and DLL files.
//
// Before including this header, DLLs must:
//		include progdefs.h
// This is conveniently done for them in extdll.h
//

/*
#ifdef _WIN32
#define DLLEXPORT __stdcall
#else
#define DLLEXPORT  __attribute__ ((visibility("default")))
#endif
*/

typedef enum
	{
	at_notice,
	at_console,		// same as at_notice, but forces a ConPrintf, not a message box
	at_aiconsole,	// same as at_console, but only shown if developer level is 2!
	at_warning,
	at_error,
	at_logged		// Server print to console ( only in multiplayer games ).
	} ALERT_TYPE;

// 4-22-98  JOHN: added for use in pfnClientPrintf
typedef enum
	{
	print_console,
	print_center,
	print_chat,
	} PRINT_TYPE;

// For integrity checking of content on clients
typedef enum
{
	force_exactfile,					// File on client must exactly match server's file
	force_model_samebounds,				// For model files only, the geometry must fit in the same bbox
	force_model_specifybounds,			// For model files only, the geometry must fit in the specified bbox
	force_model_specifybounds_if_avail,	// For Steam model files only, the geometry must fit in the specified bbox (if the file is available)
} FORCE_TYPE;

// Returned by TraceLine
#ifdef _MSC_VER
#pragma warning( push )
#pragma warning( disable: 26495 )
#endif
typedef struct
	{
	int		fAllSolid;			// if true, plane is not valid
	int		fStartSolid;		// if true, the initial point was in a solid area
	int		fInOpen;
	int		fInWater;
	float	flFraction;			// time completed, 1.0 = didn't hit anything
	vec3_t	vecEndPos;			// final position
	float	flPlaneDist;
	vec3_t	vecPlaneNormal;		// surface normal at impact
	edict_t	*pHit;				// entity the surface is on
	int		iHitgroup;			// 0 == generic, non zero is specific body part
	} TraceResult;
#ifdef _MSC_VER
#pragma warning( pop )
#endif

// CD audio status
typedef struct 
{
	int	fPlaying;// is sound playing right now?
	int	fWasPlaying;// if not, CD is paused if WasPlaying is true.
	int	fInitialized;
	int	fEnabled;
	int	fPlayLooping;
	float	cdvolume;
	//BYTE 	remap[100];
	int	fCDRom;
	int	fPlayTrack;
} CDStatus;

#include "../common/crc.h"


#include "../common/game_engine_functions.h"

// Passed to pfnKeyValue
typedef struct KeyValueData_s
{
	char	*szClassName;	// in: entity classname
	char	*szKeyName;		// in: name of key
	char	*szValue;		// in: value of key
	int32	fHandled;		// out: DLL sets to true if key-value pair was understood
} KeyValueData;


#ifdef _MSC_VER
#pragma warning( push )
#pragma warning( disable: 26495 )
#endif
typedef struct
{
	char		mapName[ 32 ];
	char		landmarkName[ 32 ];
	edict_t	*pentLandmark;
	vec3_t		vecLandmarkOrigin;
} LEVELLIST;
#ifdef _MSC_VER
#pragma warning( pop )
#endif
#define MAX_LEVEL_CONNECTIONS	16		// These are encoded in the lower 16bits of ENTITYTABLE->flags

typedef struct 
{
	int			id;				// Ordinal ID of this entity (used for entity <--> pointer conversions)
	edict_t	*pent;			// Pointer to the in-game entity

	int			location;		// Offset from the base data of this entity
	int			size;			// Byte size of this entity's data
	int			flags;			// This could be a short -- bit mask of transitions that this entity is in the PVS of
	string_t	classname;		// entity class name

} ENTITYTABLE;

#define FENTTABLE_PLAYER		0x80000000
#define FENTTABLE_REMOVED		0x40000000
#define FENTTABLE_MOVEABLE		0x20000000
#define FENTTABLE_GLOBAL		0x10000000

typedef struct saverestore_s SAVERESTOREDATA;

#ifdef _WIN32
typedef 
#endif
struct saverestore_s
{
	char		*pBaseData;		// Start of all entity save data
	char		*pCurrentData;	// Current buffer pointer for sequential access
	int			size;			// Current data size
	int			bufferSize;		// Total space for data
	int			tokenSize;		// Size of the linear list of tokens
	int			tokenCount;		// Number of elements in the pTokens table
	char		**pTokens;		// Hash table of entity strings (sparse)
	int			currentIndex;	// Holds a global entity table ID
	int			tableCount;		// Number of elements in the entity table
	int			connectionCount;// Number of elements in the levelList[]
	ENTITYTABLE	*pTable;		// Array of ENTITYTABLE elements (1 for each entity)
	LEVELLIST	levelList[ MAX_LEVEL_CONNECTIONS ];		// List of connections from this level

	// smooth transition
	int			fUseLandmark;
	char		szLandmarkName[20];// landmark we'll spawn near in next level
	vec3_t		vecLandmarkOffset;// for landmark transitions
	float		time;
	char		szCurrentMapName[32];	// To check global entities

} 
#ifdef _WIN32
SAVERESTOREDATA 
#endif
;

#include "../common/game_save_descriptions.h"

#ifndef ARRAYSIZE
#define ARRAYSIZE(p)		(sizeof(p)/sizeof(p[0]))
#endif // ARRAYSIZE


typedef struct 
{
	// Initialize/shutdown the game (one-time call after loading of game .dll )
	void			(*pfnGameInit)			( void );				
	int				(*pfnSpawn)				( edict_t *pent );
	void			(*pfnThink)				( edict_t *pent );
	void			(*pfnUse)				( edict_t *pentUsed, edict_t *pentOther );
	void			(*pfnTouch)				( edict_t *pentTouched, edict_t *pentOther );
	void			(*pfnBlocked)			( edict_t *pentBlocked, edict_t *pentOther );
	void			(*pfnKeyValue)			( edict_t *pentKeyvalue, KeyValueData *pkvd );
	void			(*pfnSave)				( edict_t *pent, SAVERESTOREDATA *pSaveData );
	int 			(*pfnRestore)			( edict_t *pent, SAVERESTOREDATA *pSaveData, int globalEntity );
	void			(*pfnSetAbsBox)			( edict_t *pent );

	void			(*pfnSaveWriteFields)	( SAVERESTOREDATA *, const char *, void *, TYPEDESCRIPTION *, int );
	void			(*pfnSaveReadFields)	( SAVERESTOREDATA *, const char *, void *, TYPEDESCRIPTION *, int );

	void			(*pfnSaveGlobalState)		( SAVERESTOREDATA * );
	void			(*pfnRestoreGlobalState)	( SAVERESTOREDATA * );
	void			(*pfnResetGlobalState)		( void );

	qboolean		(*pfnClientConnect)		( edict_t *pEntity, const char *pszName, const char *pszAddress, char szRejectReason[ 128 ] );
	
	void			(*pfnClientDisconnect)	( edict_t *pEntity );
	void			(*pfnClientKill)		( edict_t *pEntity );
	void			(*pfnClientPutInServer)	( edict_t *pEntity );
	void			(*pfnClientCommand)		( edict_t *pEntity );
	void			(*pfnClientUserInfoChanged)( edict_t *pEntity, char *infobuffer );

	void			(*pfnServerActivate)	( edict_t *pEdictList, int edictCount, int clientMax );
	void			(*pfnServerDeactivate)	( void );

	void			(*pfnPlayerPreThink)	( edict_t *pEntity );
	void			(*pfnPlayerPostThink)	( edict_t *pEntity );

	void			(*pfnStartFrame)		( void );
	void			(*pfnParmsNewLevel)		( void );
	void			(*pfnParmsChangeLevel)	( void );

	 // Returns string describing current .dll.  E.g., TeamFotrress 2, Half-Life
	const char     *(*pfnGetGameDescription)( void );     

	// Notify dll about a player customization.
	void            (*pfnPlayerCustomization) ( edict_t *pEntity, customization_t *pCustom );  

	// Spectator funcs
	void			(*pfnSpectatorConnect)		( edict_t *pEntity );
	void			(*pfnSpectatorDisconnect)	( edict_t *pEntity );
	void			(*pfnSpectatorThink)		( edict_t *pEntity );

	// Notify game .dll that engine is going to shut down.  Allows mod authors to set a breakpoint.
	void			(*pfnSys_Error)			( const char *error_string );

	void			(*pfnPM_Move) ( struct playermove_s *ppmove, qboolean server );
	void			(*pfnPM_Init) ( struct playermove_s *ppmove );
	char			(*pfnPM_FindTextureType)( char *name );
	void			(*pfnSetupVisibility)( struct edict_s *pViewEntity, struct edict_s *pClient, unsigned char **pvs, unsigned char **pas );
	void			(*pfnUpdateClientData) ( const struct edict_s *ent, int sendweapons, struct clientdata_s *cd );
	int				(*pfnAddToFullPack)( struct entity_state_s *state, int e, edict_t *ent, edict_t *host, int hostflags, int player, unsigned char *pSet );
	void			(*pfnCreateBaseline) ( int player, int eindex, struct entity_state_s *baseline, struct edict_s *entity, int playermodelindex, vec3_t player_mins, vec3_t player_maxs );
	void			(*pfnRegisterEncoders)	( void );
	int				(*pfnGetWeaponData)		( struct edict_s *player, struct weapon_data_s *info );

	void			(*pfnCmdStart)			( const edict_t *player, const struct usercmd_s *cmd, unsigned int random_seed );
	void			(*pfnCmdEnd)			( const edict_t *player );

	// Return 1 if the packet is valid.  Set response_buffer_size if you want to send a response packet.  Incoming, it holds the max
	//  size of the response_buffer, so you must zero it out if you choose not to respond.
	int				(*pfnConnectionlessPacket )	( const struct netadr_s *net_from, const char *args, char *response_buffer, int *response_buffer_size );

	// Enumerates player hulls.  Returns 0 if the hull number doesn't exist, 1 otherwise
	int				(*pfnGetHullBounds)	( int hullnumber, float *mins, float *maxs );

	// Create baselines for certain "unplaced" items.
	void			(*pfnCreateInstancedBaselines) ( void );

	// One of the pfnForceUnmodified files failed the consistency check for the specified player
	// Return 0 to allow the client to continue, 1 to force immediate disconnection ( with an optional disconnect message of up to 256 characters )
	int				(*pfnInconsistentFile)( const struct edict_s *player, const char *filename, char *disconnect_message );

	// The game .dll should return 1 if lag compensation should be allowed ( could also just set
	//  the sv_unlag cvar.
	// Most games right now should return 0, until client-side weapon prediction code is written
	//  and tested for them.
	int				(*pfnAllowLagCompensation)( void );
} DLL_FUNCTIONS;

extern DLL_FUNCTIONS		gEntityInterface;

// Current version.
#define NEW_DLL_FUNCTIONS_VERSION	1

typedef struct
{
	// Called right before the object's memory is freed. 
	// Calls its destructor.
	void			(*pfnOnFreeEntPrivateData)(edict_t *pEnt);
	void			(*pfnGameShutdown)(void);
	int				(*pfnShouldCollide)( edict_t *pentTouched, edict_t *pentOther );
	void			(*pfnCvarValue)( const edict_t *pEnt, const char *value );
	void			(*pfnCvarValue2)( const edict_t *pEnt, int requestID, const char *cvarName, const char *value );
} NEW_DLL_FUNCTIONS;
typedef int	(*NEW_DLL_FUNCTIONS_FN)( NEW_DLL_FUNCTIONS *pFunctionTable, int *interfaceVersion );

// Pointers will be null if the game DLL doesn't support this API.
extern NEW_DLL_FUNCTIONS	gNewDLLFunctions;

typedef int	(*APIFUNCTION)( DLL_FUNCTIONS *pFunctionTable, int interfaceVersion );
typedef int	(*APIFUNCTION2)( DLL_FUNCTIONS *pFunctionTable, int *interfaceVersion );

#endif // EIFACE_H
