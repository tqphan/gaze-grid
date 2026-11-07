#pragma once
#include "DesktopTextContext.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <functional>
struct IUIAutomationTextRange;
namespace WindowsAutomationContext {
DesktopTextContext current();
bool replaceCurrent(const DesktopTextContext &expected,const QString &candidate,const QString &language,bool space);
// Keep surrounding text inside the focused editor's document range, including
// when the provider lets character navigation cross into neighboring controls.
DesktopTextContext readRange(IUIAutomationTextRange *caret,IUIAutomationTextRange *document,
                             const QString &className={},const QString &accessibleName={});
// Explicit-window variants also support isolated native-control tests without
// focusing a window or sending input to the user's desktop.
DesktopTextContext readWindow(HWND window);
bool replaceWindow(HWND window,const DesktopTextContext &expected,const QString &candidate,const QString &language,bool space,const std::function<bool(const QString &)> &insert);
}
