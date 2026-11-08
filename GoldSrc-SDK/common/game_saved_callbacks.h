#pragma once

// Included after cbase.h: callback types must describe the complete native
// classes, especially MSVC's member-pointer inheritance representation.
using CseMoveDonePointer = void (CBaseToggle::*)();

enum class CseSavedCallbackKind { Invalid, Think, Touch, Use, Blocked, MoveDone };

bool CseRegisterThink(const char *name, BASEPTR value);
bool CseRegisterTouch(const char *name, ENTITYFUNCPTR value);
bool CseRegisterUse(const char *name, USEPTR value);
bool CseRegisterBlocked(const char *name, ENTITYFUNCPTR value);
bool CseRegisterMoveDone(const char *name, CseMoveDonePointer value);
bool CseInitializeSavedCallbacks();

CseSavedCallbackKind CseCallbackKind(const TYPEDESCRIPTION *fields, const TYPEDESCRIPTION &field);
bool CseCallbackEmpty(CseSavedCallbackKind kind, const void *storage);
bool CseClearCallback(CseSavedCallbackKind kind, void *storage);
const char *CseCallbackName(CseSavedCallbackKind kind, const void *storage);
bool CseRestoreCallback(CseSavedCallbackKind kind, void *storage, const void *bytes, int size, const void *base);
