/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Portions Copyright (c) 2010 Motorola Mobility, Inc.  All rights reserved.
 * Copyright (C) 2011 Igalia S.L.
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

#include "config.h"
#include "WebEventFactory.h"

#include <WebCore/GtkUtilities.h>
#include <WebCore/PlatformKeyboardEvent.h>
#include <WebCore/Scrollbar.h>
#include <WebCore/WindowsKeyboardCodes.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <wtf/ASCIICType.h>

namespace WebKit {

using namespace WebCore;

static inline bool isGdkKeyCodeFromKeyPad(unsigned keyval)
{
    return keyval >= GDK_KEY_KP_Space && keyval <= GDK_KEY_KP_9;
}

static inline OptionSet<WebEvent::Modifier> modifiersForEvent(const GdkEvent* event)
{
    OptionSet<WebEvent::Modifier> modifiers;
    GdkModifierType state;

    // Check for a valid state in GdkEvent.
    if (!gdk_event_get_state(event, &state))
        return modifiers;

    if (state & GDK_CONTROL_MASK)
        modifiers.add(WebEvent::Modifier::ControlKey);
    if (state & GDK_SHIFT_MASK)
        modifiers.add(WebEvent::Modifier::ShiftKey);
    if (state & GDK_MOD1_MASK)
        modifiers.add(WebEvent::Modifier::AltKey);
    if (state & GDK_META_MASK)
        modifiers.add(WebEvent::Modifier::MetaKey);
    if (PlatformKeyboardEvent::modifiersContainCapsLock(state))
        modifiers.add(WebEvent::Modifier::CapsLockKey);

    return modifiers;
}

static inline WebMouseEvent::Button buttonForEvent(const GdkEvent* event)
{
    unsigned button = 0;
    GdkEventType type = gdk_event_get_event_type(event);
    switch (type) {
    case GDK_ENTER_NOTIFY:
    case GDK_LEAVE_NOTIFY:
    case GDK_MOTION_NOTIFY: {
        button = WebMouseEvent::NoButton;
        GdkModifierType state;
        gdk_event_get_state(event, &state);
        if (state & GDK_BUTTON1_MASK)
            button = WebMouseEvent::LeftButton;
        else if (state & GDK_BUTTON2_MASK)
            button = WebMouseEvent::MiddleButton;
        else if (state & GDK_BUTTON3_MASK)
            button = WebMouseEvent::RightButton;
        break;
    }
    case GDK_BUTTON_PRESS:
    case GDK_2BUTTON_PRESS:
    case GDK_3BUTTON_PRESS:
    case GDK_BUTTON_RELEASE: {
        guint eventButton;
        gdk_event_get_button(event, &eventButton);

        if (eventButton == 1)
            button = WebMouseEvent::LeftButton;
        else if (eventButton == 2)
            button = WebMouseEvent::MiddleButton;
        else if (eventButton == 3)
            button = WebMouseEvent::RightButton;
        break;
    }
    default:
        ASSERT_NOT_REACHED();
    }

    return static_cast<WebMouseEvent::Button>(button);
}

static inline short pressedMouseButtons(GdkModifierType state)
{
    // MouseEvent.buttons
    // https://www.w3.org/TR/uievents/#ref-for-dom-mouseevent-buttons-1

    // 0 MUST indicate no button is currently active.
    short buttons = 0;

    // 1 MUST indicate the primary button of the device (in general, the left button or the only button on
    // single-button devices, used to activate a user interface control or select text).
    if (state & GDK_BUTTON1_MASK)
        buttons |= 1;

    // 4 MUST indicate the auxiliary button (in general, the middle button, often combined with a mouse wheel).
    if (state & GDK_BUTTON2_MASK)
        buttons |= 4;

    // 2 MUST indicate the secondary button (in general, the right button, often used to display a context menu),
    // if present.
    if (state & GDK_BUTTON3_MASK)
        buttons |= 2;

    return buttons;
}

WebMouseEvent WebEventFactory::createWebMouseEvent(const GdkEvent* event, int currentClickCount, Optional<IntPoint> delta)
{
    double x, y, xRoot, yRoot;
    gdk_event_get_coords(event, &x, &y);
    gdk_event_get_root_coords(event, &xRoot, &yRoot);

    GdkModifierType state = static_cast<GdkModifierType>(0);
    gdk_event_get_state(event, &state);

    guint eventButton;
    gdk_event_get_button(event, &eventButton);

    WebEvent::Type type = static_cast<WebEvent::Type>(0);
    IntPoint movementDelta;

    GdkEventType eventType = gdk_event_get_event_type(event);
    switch (eventType) {
    case GDK_MOTION_NOTIFY:
    case GDK_ENTER_NOTIFY:
    case GDK_LEAVE_NOTIFY:
        type = WebEvent::MouseMove;
        if (delta)
            movementDelta = delta.value();
        break;
    case GDK_BUTTON_PRESS:
    case GDK_2BUTTON_PRESS:
    case GDK_3BUTTON_PRESS: {
        type = WebEvent::MouseDown;
        auto modifier = stateModifierForGdkButton(eventButton);
        state = static_cast<GdkModifierType>(state | modifier);
        break;
    }
    case GDK_BUTTON_RELEASE: {
        type = WebEvent::MouseUp;
        auto modifier = stateModifierForGdkButton(eventButton);
        state = static_cast<GdkModifierType>(state & ~modifier);
        break;
    }
    default :
        ASSERT_NOT_REACHED();
    }

    return WebMouseEvent(type,
        buttonForEvent(event),
        pressedMouseButtons(state),
        IntPoint(x, y),
        IntPoint(xRoot, yRoot),
        movementDelta.x(),
        movementDelta.y(),
        0 /* deltaZ */,
        currentClickCount,
        modifiersForEvent(event),
        wallTimeForEvent(event));
}

WebWheelEvent WebEventFactory::createWebWheelEvent(const GdkEvent* event)
{
    WebWheelEvent::Phase phase = gdk_event_is_scroll_stop_event(event) ?
        WebWheelEvent::Phase::PhaseEnded :
        WebWheelEvent::Phase::PhaseChanged;
    return createWebWheelEvent(event, phase, WebWheelEvent::Phase::PhaseNone);
}

WebWheelEvent WebEventFactory::createWebWheelEvent(const GdkEvent* event, WebWheelEvent::Phase phase, WebWheelEvent::Phase momentumPhase)
{
    FloatSize wheelTicks = FloatSize(0, 0);
    double x, y;
    gdk_event_get_coords(event, &x, &y);
    double xRoot, yRoot;
    gdk_event_get_root_coords(event, &xRoot, &yRoot);

    GdkScrollDirection direction;
    if (!gdk_event_get_scroll_direction(event, &direction)) {
        direction = GDK_SCROLL_SMOOTH;
        double deltaX, deltaY;
        if (gdk_event_get_scroll_deltas(event, &deltaX, &deltaY))
            wheelTicks = FloatSize(-deltaX, -deltaY);
    }

    if (wheelTicks.isZero()) {
        switch (direction) {
        case GDK_SCROLL_UP:
            wheelTicks = FloatSize(0, 1);
            break;
        case GDK_SCROLL_DOWN:
            wheelTicks = FloatSize(0, -1);
            break;
        case GDK_SCROLL_LEFT:
            wheelTicks = FloatSize(1, 0);
            break;
        case GDK_SCROLL_RIGHT:
            wheelTicks = FloatSize(-1, 0);
            break;
        case GDK_SCROLL_SMOOTH:
            break;
        default:
            ASSERT_NOT_REACHED();
        }
    }

    // FIXME: [GTK] Add a setting to change the pixels per line used for scrolling
    // https://bugs.webkit.org/show_bug.cgi?id=54826
    float step = static_cast<float>(Scrollbar::pixelsPerLineStep());
    FloatSize delta(wheelTicks.width() * step, wheelTicks.height() * step);

    return WebWheelEvent(WebEvent::Wheel,
        IntPoint(x, y),
        IntPoint(xRoot, yRoot),
        delta,
        wheelTicks,
        phase,
        momentumPhase,
        WebWheelEvent::ScrollByPixelWheelEvent,
        modifiersForEvent(event),
        wallTimeForEvent(event));
}

WebKeyboardEvent WebEventFactory::createWebKeyboardEvent(const GdkEvent* event, const String& text, bool handledByInputMethod, Vector<String>&& commands)
{
    guint keyval;
    gdk_event_get_keyval(event, &keyval);
    guint16 keycode;
    gdk_event_get_keycode(event, &keycode);
    GdkEventType type = gdk_event_get_event_type(event);

    return WebKeyboardEvent(
        type == GDK_KEY_RELEASE ? WebEvent::KeyUp : WebEvent::KeyDown,
        text.isNull() ? PlatformKeyboardEvent::singleCharacterString(keyval) : text,
        PlatformKeyboardEvent::keyValueForGdkKeyCode(keyval),
        PlatformKeyboardEvent::keyCodeForHardwareKeyCode(keycode),
        PlatformKeyboardEvent::keyIdentifierForGdkKeyCode(keyval),
        PlatformKeyboardEvent::windowsKeyCodeForGdkKeyCode(keyval),
        static_cast<int>(keyval),
        handledByInputMethod,
        WTFMove(commands),
        isGdkKeyCodeFromKeyPad(keyval),
        modifiersForEvent(event),
        wallTimeForEvent(event));
}

#if ENABLE(TOUCH_EVENTS)
WebTouchEvent WebEventFactory::createWebTouchEvent(const GdkEvent* event, Vector<WebPlatformTouchPoint>&& touchPoints)
{
    WebEvent::Type type = WebEvent::NoType;
    GdkEventType eventType = gdk_event_get_event_type(event);
    switch (eventType) {
    case GDK_TOUCH_BEGIN:
        type = WebEvent::TouchStart;
        break;
    case GDK_TOUCH_UPDATE:
        type = WebEvent::TouchMove;
        break;
    case GDK_TOUCH_END:
        type = WebEvent::TouchEnd;
        break;
    case GDK_TOUCH_CANCEL:
        type = WebEvent::TouchCancel;
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    return WebTouchEvent(type, WTFMove(touchPoints), modifiersForEvent(event), wallTimeForEvent(event));
}
#endif

} // namespace WebKit
