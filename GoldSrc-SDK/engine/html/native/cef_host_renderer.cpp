#include "cef_host.h"

#include "include/cef_browser.h"
#include "include/cef_frame.h"
#include "include/cef_parser.h"
#include "include/cef_process_message.h"
#include "include/cef_render_process_handler.h"
#include "include/cef_v8.h"

#include <string>

namespace {
class RendererApp final : public CefApp, public CefRenderProcessHandler {
public:
    CefRefPtr<CefRenderProcessHandler> GetRenderProcessHandler() override {
        return this;
    }

    bool OnProcessMessageReceived(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame> frame, CefProcessId source,
        CefRefPtr<CefProcessMessage> message) override {
        if (source != PID_BROWSER || message->GetName() != "cse.dom.query") {
            return false;
        }
        const auto arguments = message->GetArgumentList();
        const int operation = arguments->GetInt(0);
        const int x = arguments->GetInt(1);
        const int y = arguments->GetInt(2);
        std::string script;
        if (operation == 0) {
            script = "JSON.stringify([innerWidth,innerHeight,Math.max(document.documentElement.scrollWidth,"
                "document.body?document.body.scrollWidth:0),Math.max(document.documentElement.scrollHeight,"
                "document.body?document.body.scrollHeight:0),scrollX,scrollY])";
        } else if (operation == 1) {
            script = "JSON.stringify((()=>{let e=document.elementFromPoint(" + std::to_string(x) + "," +
                std::to_string(y) + ");let a=e?e.closest('a[href]'):null;return a?a.href:''})())";
        } else {
            return false;
        }
        auto context = frame->GetV8Context();
        if (!context || !context->Enter()) {
            return true;
        }
        CefRefPtr<CefV8Value> result;
        CefRefPtr<CefV8Exception> exception;
        const bool evaluated = context->Eval(script, frame->GetURL(), 0, result, exception);
        if (evaluated && result && result->IsString()) {
            auto reply = CefProcessMessage::Create("cse.dom.reply");
            auto values = reply->GetArgumentList();
            values->SetInt(0, operation);
            values->SetInt(1, x);
            values->SetInt(2, y);
            values->SetString(3, result->GetStringValue());
            frame->SendProcessMessage(PID_BROWSER, reply);
        }
        context->Exit();
        return true;
    }

private:
    IMPLEMENT_REFCOUNTING(RendererApp);
};
}

CefRefPtr<CefApp> CreateCefHostApp() {
    return new RendererApp;
}
