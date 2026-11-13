#pragma once

#include "htmlmessages.h"
#include <cstdint>
#include "html_legacy_interfaces.h"

class CUtlString;
class IHTMLResponses;
class IHTMLResponses_HL1;
class IHTMLSerializer;

#if UINTPTR_MAX > UINT32_MAX
class IHTMLChromeController {
public:
    virtual ~IHTMLChromeController() = default;
    virtual bool Init( const char* cachePath, const char* cookiePath ) = 0;
    virtual void Shutdown() = 0;
    virtual bool RunFrame() = 0;
    virtual void SetWebCookie( const char* host, const char* name, const char* value, const char* path, int expires ) = 0;
    virtual void GetWebCookiesForURL( CUtlString* cookies, const char* url, const char* host ) = 0;
    virtual void SetClientBuildID( std::uint64_t buildID ) = 0;
    virtual bool BHasPendingMessages() = 0;
    virtual void CreateBrowser( IHTMLResponses* responses, bool popup, const char* userAgent ) = 0;
    virtual void RemoveBrowser( IHTMLResponses* responses ) = 0;
    virtual void WakeThread() = 0;
    virtual HTMLCommandBuffer_t* GetFreeCommandBuffer( EHTMLCommands command, int browserID ) = 0;
    virtual void PushCommand( HTMLCommandBuffer_t* command ) = 0;
    virtual void SetCefThreadTargetFrameRate( std::uint32_t frameRate ) = 0;
    virtual IHTMLSerializer* CreateSerializer( IHTMLResponses_HL1* responses ) = 0;
};
#else
class IHTMLChromeController
{
public:
	virtual ~IHTMLChromeController();
	virtual bool Init( const char* cachePath, const char* cookiePath );
	virtual void Shutdown();
	virtual bool RunFrame();
	virtual void SetWebCookie( const char* host, const char* name, const char* value, const char* path, int expires );
	virtual void GetWebCookiesForURL( CUtlString* cookies, const char* url, const char* host );
	virtual void SetClientBuildID( std::uint64_t buildID );
	virtual bool BHasPendingMessages();
	virtual void CreateBrowser( IHTMLResponses* responses, bool popup, const char* userAgent );
	virtual void RemoveBrowser( IHTMLResponses* responses );
	virtual void WakeThread();
	virtual HTMLCommandBuffer_t* GetFreeCommandBuffer( EHTMLCommands command, int browserID );
	virtual void PushCommand( HTMLCommandBuffer_t* command );
	virtual void SetCefThreadTargetFrameRate( std::uint32_t frameRate );
	virtual IHTMLSerializer* CreateSerializer( IHTMLResponses_HL1* responses );
};
#endif
