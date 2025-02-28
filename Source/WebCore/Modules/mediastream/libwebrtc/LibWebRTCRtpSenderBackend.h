/*
 * Copyright (C) 2018 Apple Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(WEB_RTC)

#include "LibWebRTCMacros.h"
#include "LibWebRTCPeerConnectionBackend.h"
#include "RTCRtpSenderBackend.h"
#include "RealtimeOutgoingAudioSource.h"
#include "RealtimeOutgoingVideoSource.h"
#include <wtf/WeakPtr.h>

ALLOW_UNUSED_PARAMETERS_BEGIN

#include <webrtc/api/rtp_sender_interface.h>
#include <webrtc/api/scoped_refptr.h>

ALLOW_UNUSED_PARAMETERS_END

namespace WebCore {

class LibWebRTCPeerConnectionBackend;

class LibWebRTCRtpSenderBackend final : public RTCRtpSenderBackend {
    WTF_MAKE_FAST_ALLOCATED;
public:
    LibWebRTCRtpSenderBackend(LibWebRTCPeerConnectionBackend& backend, rtc::scoped_refptr<webrtc::RtpSenderInterface>&& rtcSender)
        : m_peerConnectionBackend(makeWeakPtr(&backend))
        , m_rtcSender(WTFMove(rtcSender))
    {
    }

    using Source = Variant<std::nullptr_t, Ref<RealtimeOutgoingAudioSource>, Ref<RealtimeOutgoingVideoSource>>;
    LibWebRTCRtpSenderBackend(LibWebRTCPeerConnectionBackend& backend, rtc::scoped_refptr<webrtc::RtpSenderInterface>&& rtcSender, Source&& source)
        : m_peerConnectionBackend(makeWeakPtr(&backend))
        , m_rtcSender(WTFMove(rtcSender))
        , m_source(WTFMove(source))
    {
    }
    ~LibWebRTCRtpSenderBackend();

    void setRTCSender(rtc::scoped_refptr<webrtc::RtpSenderInterface>&& rtcSender) { m_rtcSender = WTFMove(rtcSender); }
    webrtc::RtpSenderInterface* rtcSender() { return m_rtcSender.get(); }

    RealtimeOutgoingAudioSource* audioSource()
    {
        return WTF::switchOn(m_source,
            [] (Ref<RealtimeOutgoingAudioSource>& source) { return source.ptr(); },
            [] (const auto&) -> RealtimeOutgoingAudioSource* { return nullptr; }
        );
    }

    RealtimeOutgoingVideoSource* videoSource()
    {
        return WTF::switchOn(m_source,
            [] (Ref<RealtimeOutgoingVideoSource>& source) { return source.ptr(); },
            [] (const auto&) -> RealtimeOutgoingVideoSource* { return nullptr; }
        );
    }

    bool hasSource() const
    {
        return WTF::switchOn(m_source,
            [] (const std::nullptr_t&) { return false; },
            [] (const auto&) { return true; }
        );
    }

    void clearSource()
    {
        ASSERT(hasSource());
        m_source = nullptr;
    }

    void setSource(Source&& source)
    {
        ASSERT(!hasSource());
        m_source = WTFMove(source);
        ASSERT(hasSource());
    }

    void takeSource(LibWebRTCRtpSenderBackend& backend)
    {
        ASSERT(backend.hasSource());
        setSource(WTFMove(backend.m_source));
        backend.m_source = nullptr;
    }

private:
    void replaceTrack(ScriptExecutionContext&, RTCRtpSender&, RefPtr<MediaStreamTrack>&&, DOMPromiseDeferred<void>&&) final;
    RTCRtpSendParameters getParameters() const final;
    void setParameters(const RTCRtpSendParameters&, DOMPromiseDeferred<void>&&) final;
    std::unique_ptr<RTCDTMFSenderBackend> createDTMFBackend() final;

    WeakPtr<LibWebRTCPeerConnectionBackend> m_peerConnectionBackend;
    rtc::scoped_refptr<webrtc::RtpSenderInterface> m_rtcSender;
    Source m_source;
    mutable Optional<webrtc::RtpParameters> m_currentParameters;
};

} // namespace WebCore

#endif // ENABLE(WEB_RTC)
