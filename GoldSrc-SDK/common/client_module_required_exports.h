#pragma once

// Retained mandatory client exports. Optional voice/director/studio/UI exports stay optional.
#define CSE_CLIENT_REQUIRED_EXPORTS(Apply) \
    Apply(pInitFunc, "Initialize") \
    Apply(pHudVidInitFunc, "HUD_VidInit") \
    Apply(pHudInitFunc, "HUD_Init") \
    Apply(pHudRedrawFunc, "HUD_Redraw") \
    Apply(pHudUpdateClientDataFunc, "HUD_UpdateClientData") \
    Apply(pHudResetFunc, "HUD_Reset") \
    Apply(pClientMove, "HUD_PlayerMove") \
    Apply(pClientMoveInit, "HUD_PlayerMoveInit") \
    Apply(pClientTextureType, "HUD_PlayerMoveTexture") \
    Apply(pIN_ActivateMouse, "IN_ActivateMouse") \
    Apply(pIN_DeactivateMouse, "IN_DeactivateMouse") \
    Apply(pIN_MouseEvent, "IN_MouseEvent") \
    Apply(pIN_ClearStates, "IN_ClearStates") \
    Apply(pIN_Accumulate, "IN_Accumulate") \
    Apply(pCL_CreateMove, "CL_CreateMove") \
    Apply(pCL_IsThirdPerson, "CL_IsThirdPerson") \
    Apply(pCL_GetCameraOffsets, "CL_CameraOffset") \
    Apply(pCamThink, "CAM_Think") \
    Apply(pFindKey, "KB_Find") \
    Apply(pCalcRefdef, "V_CalcRefdef") \
    Apply(pAddEntity, "HUD_AddEntity") \
    Apply(pCreateEntities, "HUD_CreateEntities") \
    Apply(pDrawNormalTriangles, "HUD_DrawNormalTriangles") \
    Apply(pDrawTransparentTriangles, "HUD_DrawTransparentTriangles") \
    Apply(pStudioEvent, "HUD_StudioEvent") \
    Apply(pShutdown, "HUD_Shutdown") \
    Apply(pTxferLocalOverrides, "HUD_TxferLocalOverrides") \
    Apply(pProcessPlayerState, "HUD_ProcessPlayerState") \
    Apply(pTxferPredictionData, "HUD_TxferPredictionData") \
    Apply(pReadDemoBuffer, "Demo_ReadBuffer") \
    Apply(pConnectionlessPacket, "HUD_ConnectionlessPacket") \
    Apply(pGetHullBounds, "HUD_GetHullBounds") \
    Apply(pHudFrame, "HUD_Frame") \
    Apply(pKeyEvent, "HUD_Key_Event") \
    Apply(pPostRunCmd, "HUD_PostRunCmd") \
    Apply(pTempEntUpdate, "HUD_TempEntUpdate") \
    Apply(pGetUserEntity, "HUD_GetUserEntity")
