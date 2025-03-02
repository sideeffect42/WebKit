/*
 * Copyright (C) 2017 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if USE(WPE_RENDERER)

#include "AcceleratedSurface.h"
#include <wtf/TZoneMalloc.h>
#include <wtf/unix/UnixFileDescriptor.h>

struct wpe_renderer_backend_egl_target;

namespace WebKit {

class WebPage;

class AcceleratedSurfaceLibWPE final : public AcceleratedSurface {
    WTF_MAKE_NONCOPYABLE(AcceleratedSurfaceLibWPE);
    WTF_MAKE_TZONE_ALLOCATED(AcceleratedSurfaceLibWPE);
public:
    static std::unique_ptr<AcceleratedSurfaceLibWPE> create(WebPage&, Function<void()>&& frameCompleteHandler);
    ~AcceleratedSurfaceLibWPE();

    uint64_t window() const override;
    uint64_t surfaceID() const override;
    bool resize(const WebCore::IntSize&) override;
    void finalize() override;
    void willRenderFrame() override;
    void didRenderFrame() override;

private:
    AcceleratedSurfaceLibWPE(WebPage&, Function<void()>&& frameCompleteHandler);

    void initialize();

    UnixFileDescriptor m_hostFD;
    WebCore::IntSize m_initialSize;
    struct wpe_renderer_backend_egl_target* m_backend { nullptr };
};

} // namespace WebKit

#endif // USE(WPE_RENDERER)
