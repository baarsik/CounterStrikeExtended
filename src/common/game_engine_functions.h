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

#pragma once

// Engine hands this to DLLs for functionality callbacks
typedef struct enginefuncs_s
{
	int			(*pfnPrecacheModel)			(char* s);
	int			(*pfnPrecacheSound)			(char* s);
	void		(*pfnSetModel)				(edict_t *e, const char *m);
	int			(*pfnModelIndex)			(const char *m);
	int			(*pfnModelFrames)			(int modelIndex);
	void		(*pfnSetSize)				(edict_t *e, const float *rgflMin, const float *rgflMax);
	void		(*pfnChangeLevel)			(char* s1, char* s2);
	void		(*pfnGetSpawnParms)			(edict_t *ent);
	void		(*pfnSaveSpawnParms)		(edict_t *ent);
	float		(*pfnVecToYaw)				(const float *rgflVector);
	void		(*pfnVecToAngles)			(const float *rgflVectorIn, float *rgflVectorOut);
	void		(*pfnMoveToOrigin)			(edict_t *ent, const float *pflGoal, float dist, int iMoveType);
	void		(*pfnChangeYaw)				(edict_t* ent);
	void		(*pfnChangePitch)			(edict_t* ent);
	edict_t*	(*pfnFindEntityByString)	(edict_t *pEdictStartSearchAfter, const char *pszField, const char *pszValue);
	int			(*pfnGetEntityIllum)		(edict_t* pEnt);
	edict_t*	(*pfnFindEntityInSphere)	(edict_t *pEdictStartSearchAfter, const float *org, float rad);
	edict_t*	(*pfnFindClientInPVS)		(edict_t *pEdict);
	edict_t* (*pfnEntitiesInPVS)			(edict_t *pplayer);
	void		(*pfnMakeVectors)			(const float *rgflVector);
	void		(*pfnAngleVectors)			(const float *rgflVector, float *forward, float *right, float *up);
	edict_t*	(*pfnCreateEntity)			(void);
	void		(*pfnRemoveEntity)			(edict_t* e);
	edict_t*	(*pfnCreateNamedEntity)		(string_t className);
	void		(*pfnMakeStatic)			(edict_t *ent);
	int			(*pfnEntIsOnFloor)			(edict_t *e);
	int			(*pfnDropToFloor)			(edict_t* e);
	int			(*pfnWalkMove)				(edict_t *ent, float yaw, float dist, int iMode);
	void		(*pfnSetOrigin)				(edict_t *e, const float *rgflOrigin);
	void		(*pfnEmitSound)				(edict_t *entity, int channel, const char *sample, /*int*/float volume, float attenuation, int fFlags, int pitch);
	void		(*pfnEmitAmbientSound)		(edict_t *entity, float *pos, const char *samp, float vol, float attenuation, int fFlags, int pitch);
	void		(*pfnTraceLine)				(const float *v1, const float *v2, int fNoMonsters, edict_t *pentToSkip, TraceResult *ptr);
	void		(*pfnTraceToss)				(edict_t* pent, edict_t* pentToIgnore, TraceResult *ptr);
	int			(*pfnTraceMonsterHull)		(edict_t *pEdict, const float *v1, const float *v2, int fNoMonsters, edict_t *pentToSkip, TraceResult *ptr);
	void		(*pfnTraceHull)				(const float *v1, const float *v2, int fNoMonsters, int hullNumber, edict_t *pentToSkip, TraceResult *ptr);
	void		(*pfnTraceModel)			(const float *v1, const float *v2, int hullNumber, edict_t *pent, TraceResult *ptr);
	const char *(*pfnTraceTexture)			(edict_t *pTextureEntity, const float *v1, const float *v2 );
	void		(*pfnTraceSphere)			(const float *v1, const float *v2, int fNoMonsters, float radius, edict_t *pentToSkip, TraceResult *ptr);
	void		(*pfnGetAimVector)			(edict_t* ent, float speed, float *rgflReturn);
	void		(*pfnServerCommand)			(char* str);
	void		(*pfnServerExecute)			(void);
	void		(*pfnClientCommand)			(edict_t* pEdict, char* szFmt, ...);
	void		(*pfnParticleEffect)		(const float *org, const float *dir, float color, float count);
	void		(*pfnLightStyle)			(int style, char* val);
	int			(*pfnDecalIndex)			(const char *name);
	int			(*pfnPointContents)			(const float *rgflVector);
	void		(*pfnMessageBegin)			(int msg_dest, int msg_type, const float *pOrigin, edict_t *ed);
	void		(*pfnMessageEnd)			(void);
	void		(*pfnWriteByte)				(int iValue);
	void		(*pfnWriteChar)				(int iValue);
	void		(*pfnWriteShort)			(int iValue);
	void		(*pfnWriteLong)				(int iValue);
	void		(*pfnWriteAngle)			(float flValue);
	void		(*pfnWriteCoord)			(float flValue);
	void		(*pfnWriteString)			(const char *sz);
	void		(*pfnWriteEntity)			(int iValue);
	void		(*pfnCVarRegister)			(cvar_t *pCvar);
	float		(*pfnCVarGetFloat)			(const char *szVarName);
	const char*	(*pfnCVarGetString)			(const char *szVarName);
	void		(*pfnCVarSetFloat)			(const char *szVarName, float flValue);
	void		(*pfnCVarSetString)			(const char *szVarName, const char *szValue);
	void		(*pfnAlertMessage)			(ALERT_TYPE atype, char *szFmt, ...);
	void		(*pfnEngineFprintf)			(void *pfile, char *szFmt, ...);
    void *(*pfnPvAllocEntPrivateData)(edict_t *pEdict, size_t bytes);
	void*		(*pfnPvEntPrivateData)		(edict_t *pEdict);
	void		(*pfnFreeEntPrivateData)	(edict_t *pEdict);
	const char*	(*pfnSzFromIndex)			(string_t iString);
	string_t		(*pfnAllocString)			(const char *szValue);
	struct entvars_s*	(*pfnGetVarsOfEnt)			(edict_t *pEdict);
	edict_t*	(*pfnPEntityOfEntOffset)	(EOFFSET iEntOffset);
	EOFFSET		(*pfnEntOffsetOfPEntity)	(const edict_t *pEdict);
	int			(*pfnIndexOfEdict)			(const edict_t *pEdict);
	edict_t*	(*pfnPEntityOfEntIndex)		(int iEntIndex);
	edict_t*	(*pfnFindEntityByVars)		(struct entvars_s* pvars);
	void*		(*pfnGetModelPtr)			(edict_t* pEdict);
	int			(*pfnRegUserMsg)			(const char *pszName, int iSize);
	void		(*pfnAnimationAutomove)		(const edict_t* pEdict, float flTime);
	void		(*pfnGetBonePosition)		(const edict_t* pEdict, int iBone, float *rgflOrigin, float *rgflAngles );
    CseSymbolAddress (*pfnFunctionFromName)(const char *pName);
    const char *(*pfnNameForFunction)(CseSymbolAddress function);
	void		(*pfnClientPrintf)			( edict_t* pEdict, PRINT_TYPE ptype, const char *szMsg ); // JOHN: engine callbacks so game DLL can print messages to individual clients
	void		(*pfnServerPrint)			( const char *szMsg );
	const char *(*pfnCmd_Args)				( void );		// these 3 added
	const char *(*pfnCmd_Argv)				( int argc );	// so game DLL can easily
	int			(*pfnCmd_Argc)				( void );		// access client 'cmd' strings
	void		(*pfnGetAttachment)			(const edict_t *pEdict, int iAttachment, float *rgflOrigin, float *rgflAngles );
	void		(*pfnCRC32_Init)			(CRC32_t *pulCRC);
	void        (*pfnCRC32_ProcessBuffer)   (CRC32_t *pulCRC, void *p, int len);
	void		(*pfnCRC32_ProcessByte)     (CRC32_t *pulCRC, unsigned char ch);
	CRC32_t		(*pfnCRC32_Final)			(CRC32_t pulCRC);
	int32		(*pfnRandomLong)			(int32  lLow,  int32  lHigh);
	float		(*pfnRandomFloat)			(float flLow, float flHigh);
	void		(*pfnSetView)				(const edict_t *pClient, const edict_t *pViewent );
	float		(*pfnTime)					( void );
	void		(*pfnCrosshairAngle)		(const edict_t *pClient, float pitch, float yaw);
	byte *      (*pfnLoadFileForMe)         (char *filename, int *pLength);
	void        (*pfnFreeFile)              (void *buffer);
	void        (*pfnEndSection)            (const char *pszSectionName); // trigger_endsection
	int 		(*pfnCompareFileTime)       (char *filename1, char *filename2, int *iCompare);
	void        (*pfnGetGameDir)            (char *szGetGameDir);
	void		(*pfnCvar_RegisterVariable) (cvar_t *variable);
	void        (*pfnFadeClientVolume)      (const edict_t *pEdict, int fadePercent, int fadeOutSeconds, int holdTime, int fadeInSeconds);
	void        (*pfnSetClientMaxspeed)     (const edict_t *pEdict, float fNewMaxspeed);
	edict_t *	(*pfnCreateFakeClient)		(const char *netname);	// returns NULL if fake client can't be created
	void		(*pfnRunPlayerMove)			(edict_t *fakeclient, const float *viewangles, float forwardmove, float sidemove, float upmove, unsigned short buttons, byte impulse, byte msec );
	int			(*pfnNumberOfEntities)		(void);
	char*		(*pfnGetInfoKeyBuffer)		(edict_t *e);	// passing in NULL gets the serverinfo
	char*		(*pfnInfoKeyValue)			(char *infobuffer, char *key);
	void		(*pfnSetKeyValue)			(char *infobuffer, char *key, char *value);
	void		(*pfnSetClientKeyValue)		(int clientIndex, char *infobuffer, char *key, char *value);
	int			(*pfnIsMapValid)			(char *filename);
	void		(*pfnStaticDecal)			( const float *origin, int decalIndex, int entityIndex, int modelIndex );
	int			(*pfnPrecacheGeneric)		(char* s);
	int			(*pfnGetPlayerUserId)		(edict_t *e ); // returns the server assigned userid for this player.  useful for logging frags, etc.  returns -1 if the edict couldn't be found in the list of clients
	void		(*pfnBuildSoundMsg)			(edict_t *entity, int channel, const char *sample, /*int*/float volume, float attenuation, int fFlags, int pitch, int msg_dest, int msg_type, const float *pOrigin, edict_t *ed);
	int			(*pfnIsDedicatedServer)		(void);// is this a dedicated server?
	cvar_t		*(*pfnCVarGetPointer)		(const char *szVarName);
	unsigned int (*pfnGetPlayerWONId)		(edict_t *e); // returns the server assigned WONid for this player.  useful for logging frags, etc.  returns -1 if the edict couldn't be found in the list of clients

	// YWB 8/1/99 TFF Physics additions
	void		(*pfnInfo_RemoveKey)		( char *s, const char *key );
	const char *(*pfnGetPhysicsKeyValue)	( const edict_t *pClient, const char *key );
	void		(*pfnSetPhysicsKeyValue)	( const edict_t *pClient, const char *key, const char *value );
	const char *(*pfnGetPhysicsInfoString)	( const edict_t *pClient );
	unsigned short (*pfnPrecacheEvent)		( int type, const char*psz );
	void		(*pfnPlaybackEvent)			( int flags, const edict_t *pInvoker, unsigned short eventindex, float delay, float *origin, float *angles, float fparam1, float fparam2, int iparam1, int iparam2, int bparam1, int bparam2 );

	unsigned char *(*pfnSetFatPVS)			( float *org );
	unsigned char *(*pfnSetFatPAS)			( float *org );

	int			(*pfnCheckVisibility )		( const edict_t *entity, unsigned char *pset );

	void		(*pfnDeltaSetField)			( struct delta_s *pFields, const char *fieldname );
	void		(*pfnDeltaUnsetField)		( struct delta_s *pFields, const char *fieldname );
	void		(*pfnDeltaAddEncoder)		( char *name, void (*conditionalencode)( struct delta_s *pFields, const unsigned char *from, const unsigned char *to ) );
	int			(*pfnGetCurrentPlayer)		( void );
	int			(*pfnCanSkipPlayer)			( const edict_t *player );
	int			(*pfnDeltaFindField)		( struct delta_s *pFields, const char *fieldname );
	void		(*pfnDeltaSetFieldByIndex)	( struct delta_s *pFields, int fieldNumber );
	void		(*pfnDeltaUnsetFieldByIndex)( struct delta_s *pFields, int fieldNumber );

	void		(*pfnSetGroupMask)			( int mask, int op );

	int			(*pfnCreateInstancedBaseline) ( string_t classname, struct entity_state_s *baseline );
	void		(*pfnCvar_DirectSet)		( struct cvar_s *var, char *value );

	// Forces the client and server to be running with the same version of the specified file
	//  ( e.g., a player model ).
	// Calling this has no effect in single player
	void		(*pfnForceUnmodified)		( FORCE_TYPE type, float *mins, float *maxs, const char *filename );

	void		(*pfnGetPlayerStats)		( const edict_t *pClient, int *ping, int *packet_loss );

	void		(*pfnAddServerCommand)		( char *cmd_name, void (*function) (void) );

	// For voice communications, set which clients hear eachother.
	// NOTE: these functions take player entity indices (starting at 1).
	qboolean	(*pfnVoice_GetClientListening)(int iReceiver, int iSender);
	qboolean	(*pfnVoice_SetClientListening)(int iReceiver, int iSender, qboolean bListen);

	const char *(*pfnGetPlayerAuthId)		( edict_t *e );

	// PSV: Added for CZ training map
//	const char *(*pfnKeyNameForBinding)		( const char* pBinding );

	sequenceEntry_s*	(*pfnSequenceGet)			( const char* fileName, const char* entryName );
	sentenceEntry_s*	(*pfnSequencePickSentence)	( const char* groupName, int pickMethod, int *picked );

	// LH: Give access to filesize via filesystem
	int			(*pfnGetFileSize)			( char *filename );

	unsigned int (*pfnGetApproxWavePlayLen) (const char *filepath);
	// MDC: Added for CZ career-mode
	int			(*pfnIsCareerMatch)			( void );

	// BGC: return the number of characters of the localized string referenced by using "label"
	int			(*pfnGetLocalizedStringLength)(const char *label);

	// BGC: added to facilitate persistent storage of tutor message decay values for
	// different career game profiles.  Also needs to persist regardless of mp.dll being
	// destroyed and recreated.
	void (*pfnRegisterTutorMessageShown)(int mid);
	int (*pfnGetTimesTutorMessageShown)(int mid);
	void (*ProcessTutorMessageDecayBuffer)(int *buffer, int bufferLength);
	void (*ConstructTutorMessageDecayBuffer)(int *buffer, int bufferLength);
	void (*ResetTutorMessageDecayData)( void );

	void (*pfnQueryClientCvarValue)( const edict_t *player, const char *cvarName );
	void (*pfnQueryClientCvarValue2)( const edict_t *player, const char *cvarName, int requestID );
	int (*pfnCheckParm)( const char *pchCmdLineToken, char **ppnext );

	edict_t*	(*pfnPEntityOfEntIndexAllEntities)		(int iEntIndex);
    // Owned runtime extension; returns zero when absent and an allocated native string otherwise.
    string_t (*pfnGetWorldspawnString)(const char *key);
} enginefuncs_t;


// ONLY ADD NEW FUNCTIONS TO THE END OF THIS STRUCT.  INTERFACE VERSION IS FROZEN AT 138
