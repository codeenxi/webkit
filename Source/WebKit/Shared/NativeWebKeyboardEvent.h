/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Portions Copyright (c) 2010 Motorola Mobility, Inc.  All rights reserved.
 * Copyright (C) 2011, 2012 Igalia S.L
 * Copyright (C) 2018 Sony Interactive Entertainment Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef NativeWebKeyboardEvent_h
#define NativeWebKeyboardEvent_h

#include "WebEvent.h"

#if USE(APPKIT)
#include <wtf/RetainPtr.h>
OBJC_CLASS NSView;

namespace WebCore {
struct KeypressCommand;
}
#endif

#if PLATFORM(GTK)
#include <WebCore/GUniquePtrGtk.h>
typedef union _GdkEvent GdkEvent;
#endif

#if PLATFORM(IOS_FAMILY)
#include <wtf/RetainPtr.h>
OBJC_CLASS WebEvent;
#endif

#if USE(LIBWPE)
struct wpe_input_keyboard_event;
#endif

#if PLATFORM(WIN)
#include <windows.h>
#endif

namespace WebKit {

class NativeWebKeyboardEvent : public WebKeyboardEvent {
public:
#if USE(APPKIT)
    // FIXME: Share iOS's HandledByInputMethod enum here instead of passing a boolean.
    NativeWebKeyboardEvent(NSEvent *, bool handledByInputMethod, bool replacesSoftSpace, const Vector<WebCore::KeypressCommand>&);
#elif PLATFORM(GTK)
    NativeWebKeyboardEvent(const NativeWebKeyboardEvent&);
    enum class HandledByInputMethod : bool { No, Yes };
    enum class FakedForComposition : bool { No, Yes };
    NativeWebKeyboardEvent(GdkEvent*, const String&, HandledByInputMethod, FakedForComposition, Vector<String>&& commands);
#elif PLATFORM(IOS_FAMILY)
    enum class HandledByInputMethod : bool { No, Yes };
    NativeWebKeyboardEvent(::WebEvent *, HandledByInputMethod);
#elif USE(LIBWPE)
    NativeWebKeyboardEvent(struct wpe_input_keyboard_event*);
#elif PLATFORM(WIN)
    NativeWebKeyboardEvent(HWND, UINT message, WPARAM, LPARAM);
#endif

#if USE(APPKIT)
    NSEvent *nativeEvent() const { return m_nativeEvent.get(); }
#elif PLATFORM(GTK)
    GdkEvent* nativeEvent() const { return m_nativeEvent.get(); }
    const String& text() const { return m_text; }
    bool handledByInputMethod() const { return m_handledByInputMethod == HandledByInputMethod::Yes; }
    bool fakedForComposition() const { return m_fakedForComposition == FakedForComposition::Yes; }
#elif PLATFORM(IOS_FAMILY)
    ::WebEvent* nativeEvent() const { return m_nativeEvent.get(); }
#elif PLATFORM(WIN)
    const MSG* nativeEvent() const { return &m_nativeEvent; }
#else
    const void* nativeEvent() const { return nullptr; }
#endif

private:
#if USE(APPKIT)
    RetainPtr<NSEvent> m_nativeEvent;
#elif PLATFORM(GTK)
    GUniquePtr<GdkEvent> m_nativeEvent;
    String m_text;
    HandledByInputMethod m_handledByInputMethod;
    FakedForComposition m_fakedForComposition;
#elif PLATFORM(IOS_FAMILY)
    RetainPtr<::WebEvent> m_nativeEvent;
#elif PLATFORM(WIN)
    MSG m_nativeEvent;
#endif
};

} // namespace WebKit

#endif // NativeWebKeyboardEvent_h
