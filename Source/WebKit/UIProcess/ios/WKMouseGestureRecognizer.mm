/*
* Copyright (C) 2019 Apple Inc. All rights reserved.
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

#import "config.h"
#import "WKMouseGestureRecognizer.h"

#if HAVE(HOVER_GESTURE_RECOGNIZER)

#import "NativeWebMouseEvent.h"
#import "UIKitSPI.h"
#import <wtf/Optional.h>

static OptionSet<WebKit::WebEvent::Modifier> webEventModifiersForUIKeyModifierFlags(UIKeyModifierFlags flags)
{
    OptionSet<WebKit::WebEvent::Modifier> modifiers;
    if (flags & UIKeyModifierShift)
        modifiers.add(WebKit::WebEvent::Modifier::ShiftKey);
    if (flags & UIKeyModifierControl)
        modifiers.add(WebKit::WebEvent::Modifier::ControlKey);
    if (flags & UIKeyModifierAlternate)
        modifiers.add(WebKit::WebEvent::Modifier::AltKey);
    if (flags & UIKeyModifierCommand)
        modifiers.add(WebKit::WebEvent::Modifier::MetaKey);
    if (flags & UIKeyModifierAlphaShift)
        modifiers.add(WebKit::WebEvent::Modifier::CapsLockKey);
    return modifiers;
}

@implementation WKMouseGestureRecognizer {
    RetainPtr<UIHoverEvent> _currentHoverEvent;
    RetainPtr<UITouch> _currentTouch;

    BOOL _touching;

    std::unique_ptr<WebKit::NativeWebMouseEvent> _lastEvent;
    Optional<CGPoint> _lastLocation;
}

- (instancetype)initWithTarget:(id)target action:(SEL)action
{
    self = [super initWithTarget:target action:action];
    if (!self)
        return nil;

    [self _setAcceptsFailureRequiments:NO];

    return self;
}

- (void)setView:(UIView *)view
{
    if (view == self.view)
        return;

    [super setView:view];

    if (view._window) {
        UIHoverEvent *hoverEvent = [UIApp _hoverEventForWindow:view._window];
        [hoverEvent setNeedsHitTestReset];
    }
}

- (WebKit::NativeWebMouseEvent *)lastMouseEvent
{
    return _lastEvent.get();
}

- (WTF::Optional<CGPoint>)lastMouseLocation
{
    return _lastLocation;
}

- (UITouch *)mouseTouch
{
    return _currentTouch.get();
}

- (BOOL)_wantsHoverEvents
{
    return YES;
}

- (void)reset
{
    [super reset];
    _currentHoverEvent = nil;
    _currentTouch = nil;
}

- (BOOL)_shouldReceiveTouch:(UITouch *)touch forEvent:(UIEvent *)event recognizerView:(UIView *)recognizerView
{
    return touch == _currentTouch;
}

- (BOOL)_shouldReceivePress:(UIPress *)press
{
    return NO;
}

- (std::unique_ptr<WebKit::NativeWebMouseEvent>)createMouseEventWithType:(WebKit::WebEvent::Type)type
{
    auto modifiers = webEventModifiersForUIKeyModifierFlags(self.modifierFlags);
    BOOL hasControlModifier = modifiers.contains(WebKit::WebEvent::Modifier::ControlKey);

    auto button = [&] {
        if (!_touching || type == WebKit::WebEvent::Type::MouseUp)
            return WebKit::WebMouseEvent::NoButton;
        if (hasControlModifier)
            return WebKit::WebMouseEvent::RightButton;
        return WebKit::WebMouseEvent::LeftButton;
    }();

    auto buttons = [&] {
        if (!_touching)
            return 0;
        if (hasControlModifier)
            return 2;
        return 1;
    }();

    WebCore::IntPoint point { [self locationInView:self.view] };

    auto timestamp = WallTime::fromRawSeconds(Seconds::fromNanoseconds(GSCurrentEventTimestamp()).seconds());

    return WTF::makeUnique<WebKit::NativeWebMouseEvent>(type, button, buttons, point, point, 0, 0, 0, [_currentTouch tapCount], modifiers, timestamp, 0);
}

- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    _touching = YES;

    _lastEvent = [self createMouseEventWithType:WebKit::WebEvent::MouseDown];
    _lastLocation = [self locationInView:self.view];

    self.state = UIGestureRecognizerStateChanged;
}

- (void)touchesMoved:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    _lastEvent = [self createMouseEventWithType:WebKit::WebEvent::MouseMove];
    _lastLocation = [self locationInView:self.view];

    self.state = UIGestureRecognizerStateChanged;
}

- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    _touching = NO;

    _lastEvent = [self createMouseEventWithType:WebKit::WebEvent::MouseUp];
    _lastLocation = [self locationInView:self.view];

    self.state = UIGestureRecognizerStateChanged;
}

- (void)touchesCancelled:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    [self touchesEnded:touches withEvent:event];
}

- (void)_hoverEntered:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    _lastEvent = [self createMouseEventWithType:WebKit::WebEvent::MouseMove];

    if (_currentHoverEvent == nil && touches.count == 1 && [event isKindOfClass:NSClassFromString(@"UIHoverEvent")]) {
        _currentHoverEvent = (UIHoverEvent *)event;
        _currentTouch = touches.anyObject;
        _lastLocation = [self locationInView:self.view];
        self.state = UIGestureRecognizerStateBegan;
    }

    [super _hoverEntered:touches withEvent:event];
}

- (void)_hoverMoved:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    if (_touching) {
        _lastEvent = nullptr;
        return;
    }

    _lastEvent = [self createMouseEventWithType:WebKit::WebEvent::MouseMove];
    _lastLocation = [self locationInView:self.view];

    if (_currentHoverEvent == event && [touches containsObject:_currentTouch.get()])
        self.state = UIGestureRecognizerStateChanged;

    [super _hoverMoved:touches withEvent:event];
}

- (void)_hoverExited:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    _lastEvent = [self createMouseEventWithType:WebKit::WebEvent::MouseMove];
    _lastLocation = [self locationInView:self.view];

    if (_currentHoverEvent == event) {
        _currentHoverEvent = nil;
        _currentTouch = nil;
        self.state = UIGestureRecognizerStateEnded;
    }

    [super _hoverExited:touches withEvent:event];
}

- (void)_hoverCancelled:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    [self _hoverExited:touches withEvent:event];
    [super _hoverCancelled:touches withEvent:event];
}

- (CGPoint)locationInView:(UIView *)view
{
    if (!_currentTouch)
        return CGPointMake(-1, -1);
    return [_currentTouch locationInView:view];
}

- (BOOL)canPreventGestureRecognizer:(UIGestureRecognizer *)preventedGestureRecognizer
{
    return NO;
}

- (BOOL)canBePreventedByGestureRecognizer:(UIGestureRecognizer *)preventingGestureRecognizer
{
    return NO;
}

@end

#endif
