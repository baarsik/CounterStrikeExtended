// Consolidated from Valve SDK VGUI HTML declarations; inherited SDK license terms apply.
#ifndef CSE_HTML_LEGACY_INTERFACES_H
#define CSE_HTML_LEGACY_INTERFACES_H

#include <cstdint>

class IHTMLResponses;

// Owned native modules share the established Windows HTML callback/serializer contract.
// Character messages carry an int, preserving Windows UTF-16 code units and full native scalar inputs.
class IHTMLResponses_HL1 {
public:
    virtual void BrowserReady() = 0;
    virtual void BrowserNeedsPaint( int textureid, int wide, int tall, const unsigned char* rgba, int updatex,
        int updatey, int updatewide, int updatetall, int combobox_wide, int combobox_tall,
        const unsigned char* combobox_rgba ) = 0;

    virtual void BrowserStartRequest( const char* url, const char* target, const char* postdata, bool isredirect ) = 0;
    virtual void BrowserURLChanged( const char* url, const char* postdata, bool isredirect ) = 0;
    virtual void BrowserFinishedRequest( const char* url, const char* pagetitle ) = 0;

    virtual void BrowserShowPopup() = 0;
    virtual void BrowserHidePopup() = 0;
    virtual void BrowserSizePopup( int x, int y, int wide, int tall ) = 0;

    virtual void BrowserHorizontalScrollBarSizeResponse( int x, int y, int wide, int tall, int scroll, int scroll_max,
        float zoom ) = 0;
    virtual void BrowserVerticalScrollBarSizeResponse( int x, int y, int wide, int tall, int scroll, int scroll_max,
        float zoom ) = 0;
    virtual void BrowserGetZoomResponse( float zoom ) = 0;
    virtual void BrowserCanGoBackandForward( bool back, bool forward ) = 0;

    virtual void BrowserJSAlert( const char* message ) = 0;
    virtual void BrowserJSConfirm( const char* message ) = 0;

    virtual void BrowserPopupHTMLWindow( const char* url, int wide, int tall, int x, int y ) = 0;
    virtual void BrowserSetHTMLTitle( const char* title ) = 0;
    virtual void BrowserLoadingResource() = 0;
    virtual void BrowserStatusText( const char* text ) = 0;
    virtual void BrowserSetCursor( int cursor ) = 0;
    virtual void BrowserFileLoadDialog() = 0;

    virtual void BrowserShowToolTip( const char* text ) = 0;
    virtual void BrowserUpdateToolTip( const char* text ) = 0;
    virtual void BrowserHideToolTip() = 0;

    virtual void BrowserClose() = 0;
    virtual void BrowserLinkAtPositionResponse( const char* url, int x, int y ) = 0;
};

class IHTMLSerializer {
public:
    virtual IHTMLResponses* GetResponseTarget() = 0;
    virtual int BrowserGetIndex() = 0;
    virtual void BrowserPosition( int x, int y ) = 0;
    virtual void PostURL( const char* url, const char* postData ) = 0;
    virtual void BrowserErrorStrings( const char* title, const char* connectionProblem, const char* retry,
        const char* unknown, const char* cacheMiss, const char* badURL, const char* detailBadURL ) = 0;
    virtual void StopLoad() = 0;
    virtual void Reload() = 0;
    virtual void GoBack() = 0;
    virtual void GoForward() = 0;
    virtual void MouseDown( int code ) = 0;
    virtual void MouseUp( int code ) = 0;
    virtual void MouseWheel( int delta ) = 0;
    virtual void MouseMove( int x, int y ) = 0;
    virtual void MouseDoubleClick( int code ) = 0;
    virtual void BrowserSize( int wide, int tall ) = 0;
    virtual void KeyUp( int virtualKey, int modifiers ) = 0;
    virtual void KeyDown( int virtualKey, int modifiers ) = 0;
    virtual void KeyChar( int unichar ) = 0;
    virtual void RunJavaScript( const char* script ) = 0;
    virtual void SetHorizontalScroll( int value ) = 0;
    virtual void SetVerticalScroll( int value ) = 0;
    virtual void SetFocus( bool state ) = 0;
    virtual void AddHeader( const char* header, const char* value ) = 0;
    virtual void NeedsPaintResponse( int textureID ) = 0;
    virtual void StartRequestResponse( bool allow ) = 0;
    virtual void RequestBrowserSizes() = 0;
    virtual void JSDialogResponse( int result ) = 0;
    virtual void GetLinkAtPosition( int x, int y ) = 0;

    std::uintptr_t GetBrowserHandle() {
        return reinterpret_cast<std::uintptr_t>(GetResponseTarget());
    }
    int GetBrowserIndex() {
        return BrowserGetIndex();
    }
    void ExecuteJavascript( const char* script ) {
        RunJavaScript( script );
    }
    void FullRepaint() {
        RequestBrowserSizes();
    }
    void LinkAtPosition( int x, int y ) {
        GetLinkAtPosition( x, y );
    }
};
#endif
