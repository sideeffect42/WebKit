/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
#include "WebFullScreenManagerProxy.h"

#if ENABLE(FULLSCREEN_API)

#include "APIFullscreenClient.h"
#include "APIPageConfiguration.h"
#include "MessageSenderInlines.h"
#include "RemotePageFullscreenManagerProxy.h"
#include "WebAutomationSession.h"
#include "WebFullScreenManagerMessages.h"
#include "WebFullScreenManagerProxyMessages.h"
#include "WebPageProxy.h"
#include "WebProcessPool.h"
#include "WebProcessProxy.h"
#include <WebCore/IntRect.h>
#include <WebCore/MIMETypeRegistry.h>
#include <WebCore/ScreenOrientationType.h>
#include <wtf/LoggerHelper.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/MakeString.h>

namespace WebKit {
using namespace WebCore;

#if ENABLE(QUICKLOOK_FULLSCREEN)
static WorkQueue& sharedQuickLookFileQueue()
{
    static NeverDestroyed<Ref<WorkQueue>> queue(WorkQueue::create("com.apple.WebKit.QuickLookFileQueue"_s, WorkQueue::QOS::UserInteractive));
    return queue.get();
}
#endif

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebFullScreenManagerProxy);

Ref<WebFullScreenManagerProxy> WebFullScreenManagerProxy::create(WebPageProxy& page, WebFullScreenManagerProxyClient& client)
{
    return adoptRef(*new WebFullScreenManagerProxy(page, client));
}

WebFullScreenManagerProxy::WebFullScreenManagerProxy(WebPageProxy& page, WebFullScreenManagerProxyClient& client)
    : m_page(page)
    , m_client(client)
#if !RELEASE_LOG_DISABLED
    , m_logger(page.logger())
    , m_logIdentifier(page.logIdentifier())
#endif
{
    page.protectedLegacyMainFrameProcess()->addMessageReceiver(Messages::WebFullScreenManagerProxy::messageReceiverName(), page.webPageIDInMainFrameProcess(), *this);
}

WebFullScreenManagerProxy::~WebFullScreenManagerProxy()
{
    if (RefPtr page = m_page.get())
        page->protectedLegacyMainFrameProcess()->removeMessageReceiver(Messages::WebFullScreenManagerProxy::messageReceiverName(), page->webPageIDInMainFrameProcess());
    if (CheckedPtr client = m_client)
        client->closeFullScreenManager();
    callCloseCompletionHandlers();
}

std::optional<SharedPreferencesForWebProcess> WebFullScreenManagerProxy::sharedPreferencesForWebProcess(const IPC::Connection& connection) const
{
    return dynamicDowncast<WebProcessProxy>(AuxiliaryProcessProxy::fromConnection(connection))->sharedPreferencesForWebProcess();
}

void WebFullScreenManagerProxy::willEnterFullScreen(CompletionHandler<void(bool)>&& completionHandler)
{
    ALWAYS_LOG(LOGIDENTIFIER);
    m_fullscreenState = FullscreenState::EnteringFullscreen;

    if (RefPtr page = m_page.get())
        page->fullscreenClient().willEnterFullscreen(page.get());
    completionHandler(true);
}

template<typename M> void WebFullScreenManagerProxy::sendToWebProcess(M&& message)
{
    RefPtr page = m_page.get();
    if (!page)
        return;
    RefPtr fullScreenProcess = m_fullScreenProcess.get();
    if (!fullScreenProcess)
        return;
    fullScreenProcess->send(std::forward<M>(message), page->webPageIDInProcess(*fullScreenProcess));
}

void WebFullScreenManagerProxy::didEnterFullScreen()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    RefPtr page = m_page.get();
    if (!page)
        return;

    m_fullscreenState = FullscreenState::InFullscreen;
    page->fullscreenClient().didEnterFullscreen(page.get());
    sendToWebProcess(Messages::WebFullScreenManager::DidEnterFullScreen());

    if (page->isControlledByAutomation()) {
        if (RefPtr automationSession = page->configuration().processPool().automationSession())
            automationSession->didEnterFullScreenForPage(*page);
    }
}

void WebFullScreenManagerProxy::willExitFullScreen()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    RefPtr page = m_page.get();
    if (!page)
        return;

    m_fullscreenState = FullscreenState::ExitingFullscreen;
    page->fullscreenClient().willExitFullscreen(page.get());
    sendToWebProcess(Messages::WebFullScreenManager::WillExitFullScreen());
}

void WebFullScreenManagerProxy::callCloseCompletionHandlers()
{
    auto closeMediaCallbacks = WTFMove(m_closeCompletionHandlers);
    for (auto& callback : closeMediaCallbacks)
        callback();
}

void WebFullScreenManagerProxy::closeWithCallback(CompletionHandler<void()>&& completionHandler)
{
    m_closeCompletionHandlers.append(WTFMove(completionHandler));
    close();
}

void WebFullScreenManagerProxy::didExitFullScreen()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    m_fullscreenState = FullscreenState::NotInFullscreen;
    if (RefPtr page = m_page.get()) {
        page->fullscreenClient().didExitFullscreen(page.get());
        sendToWebProcess(Messages::WebFullScreenManager::DidExitFullScreen());

        if (page->isControlledByAutomation()) {
            if (RefPtr automationSession = page->configuration().processPool().automationSession())
                automationSession->didExitFullScreenForPage(*page);
        }
    }
    callCloseCompletionHandlers();
}

void WebFullScreenManagerProxy::setAnimatingFullScreen(bool animating)
{
    sendToWebProcess(Messages::WebFullScreenManager::SetAnimatingFullScreen(animating));
}

void WebFullScreenManagerProxy::requestRestoreFullScreen(CompletionHandler<void(bool)>&& completionHandler)
{
    ALWAYS_LOG(LOGIDENTIFIER);
    RefPtr page = m_page.get();
    if (!page)
        return;
    RefPtr fullScreenProcess = m_fullScreenProcess.get();
    if (!fullScreenProcess)
        return;
    fullScreenProcess->sendWithAsyncReply(Messages::WebFullScreenManager::RequestRestoreFullScreen(), WTFMove(completionHandler));
}

void WebFullScreenManagerProxy::requestExitFullScreen()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    sendToWebProcess(Messages::WebFullScreenManager::RequestExitFullScreen());
}

void WebFullScreenManagerProxy::saveScrollPosition()
{
    sendToWebProcess(Messages::WebFullScreenManager::SaveScrollPosition());
}

void WebFullScreenManagerProxy::restoreScrollPosition()
{
    sendToWebProcess(Messages::WebFullScreenManager::RestoreScrollPosition());
}

void WebFullScreenManagerProxy::setFullscreenInsets(const WebCore::FloatBoxExtent& insets)
{
    sendToWebProcess(Messages::WebFullScreenManager::SetFullscreenInsets(insets));
}

void WebFullScreenManagerProxy::setFullscreenAutoHideDuration(Seconds duration)
{
    sendToWebProcess(Messages::WebFullScreenManager::SetFullscreenAutoHideDuration(duration));
}

void WebFullScreenManagerProxy::close()
{
    if (CheckedPtr client = m_client)
        client->closeFullScreenManager();
}

void WebFullScreenManagerProxy::detachFromClient()
{
    close();
    m_client = nullptr;
}

void WebFullScreenManagerProxy::attachToNewClient(WebFullScreenManagerProxyClient& client)
{
    m_client = &client;
}

bool WebFullScreenManagerProxy::isFullScreen()
{
    return m_client && m_client->isFullScreen();
}

bool WebFullScreenManagerProxy::blocksReturnToFullscreenFromPictureInPicture() const
{
    return m_blocksReturnToFullscreenFromPictureInPicture;
}

void WebFullScreenManagerProxy::enterFullScreen(IPC::Connection& connection, bool blocksReturnToFullscreenFromPictureInPicture, FullScreenMediaDetails&& mediaDetails, CompletionHandler<void(bool)>&& completionHandler)
{
    m_fullScreenProcess = dynamicDowncast<WebProcessProxy>(AuxiliaryProcessProxy::fromConnection(connection));
    m_blocksReturnToFullscreenFromPictureInPicture = blocksReturnToFullscreenFromPictureInPicture;
#if PLATFORM(IOS_FAMILY)

#if ENABLE(VIDEO_USES_ELEMENT_FULLSCREEN)
    m_isVideoElement = mediaDetails.type == FullScreenMediaDetails::Type::Video;
#endif
#if ENABLE(QUICKLOOK_FULLSCREEN)
    if (mediaDetails.imageHandle) {
        auto sharedMemoryBuffer = SharedMemory::map(WTFMove(*mediaDetails.imageHandle), WebCore::SharedMemory::Protection::ReadOnly);
        if (sharedMemoryBuffer)
            m_imageBuffer = sharedMemoryBuffer->createSharedBuffer(sharedMemoryBuffer->size());
    }
    m_imageMIMEType = mediaDetails.mimeType;
#endif // QUICKLOOK_FULLSCREEN

    auto mediaDimensions = mediaDetails.mediaDimensions;
    if (CheckedPtr client = m_client)
        client->enterFullScreen(mediaDimensions, WTFMove(completionHandler));
    else
        completionHandler(false);
#else
    UNUSED_PARAM(mediaDetails);
    if (CheckedPtr client = m_client)
        client->enterFullScreen(WTFMove(completionHandler));
    else
        completionHandler(false);
#endif
}

#if ENABLE(QUICKLOOK_FULLSCREEN)
void WebFullScreenManagerProxy::updateImageSource(FullScreenMediaDetails&& mediaDetails)
{
    if (mediaDetails.imageHandle) {
        if (auto sharedMemoryBuffer = SharedMemory::map(WTFMove(*mediaDetails.imageHandle), WebCore::SharedMemory::Protection::ReadOnly))
            m_imageBuffer = sharedMemoryBuffer->createSharedBuffer(sharedMemoryBuffer->size());
    }
    m_imageMIMEType = mediaDetails.mimeType;

    if (CheckedPtr client = m_client)
        client->updateImageSource();
}
#endif // ENABLE(QUICKLOOK_FULLSCREEN)

void WebFullScreenManagerProxy::exitFullScreen()
{
#if ENABLE(QUICKLOOK_FULLSCREEN)
    m_imageBuffer = nullptr;
#endif
    if (CheckedPtr client = m_client)
        client->exitFullScreen();
}

#if ENABLE(QUICKLOOK_FULLSCREEN)
void WebFullScreenManagerProxy::prepareQuickLookImageURL(CompletionHandler<void(URL&&)>&& completionHandler) const
{
    if (!m_imageBuffer)
        return completionHandler(URL());

    sharedQuickLookFileQueue().dispatch([buffer = m_imageBuffer, mimeType = crossThreadCopy(m_imageMIMEType), completionHandler = WTFMove(completionHandler)]() mutable {
        auto suffix = makeString('.', WebCore::MIMETypeRegistry::preferredExtensionForMIMEType(mimeType));
        auto [filePath, fileHandle] = FileSystem::openTemporaryFile("QuickLook"_s, suffix);
        ASSERT(FileSystem::isHandleValid(fileHandle));

        size_t byteCount = FileSystem::writeToFile(fileHandle, buffer->span());
        ASSERT_UNUSED(byteCount, byteCount == buffer->size());
        FileSystem::closeFile(fileHandle);

        RunLoop::main().dispatch([filePath, completionHandler = WTFMove(completionHandler)]() mutable {
            completionHandler(URL::fileURLWithFileSystemPath(filePath));
        });
    });
}
#endif

void WebFullScreenManagerProxy::beganEnterFullScreen(const IntRect& initialFrame, const IntRect& finalFrame)
{
    RefPtr page = m_page.get();
    if (!page)
        return;

    page->callAfterNextPresentationUpdate([weakThis = WeakPtr { *this }, initialFrame = initialFrame, finalFrame = finalFrame] {
        if (CheckedPtr client = weakThis ? weakThis->m_client : nullptr)
            client->beganEnterFullScreen(initialFrame, finalFrame);
    });
}

void WebFullScreenManagerProxy::beganExitFullScreen(const IntRect& initialFrame, const IntRect& finalFrame)
{
    if (CheckedPtr client = m_client)
        client->beganExitFullScreen(initialFrame, finalFrame);
}

bool WebFullScreenManagerProxy::lockFullscreenOrientation(WebCore::ScreenOrientationType orientation)
{
    if (CheckedPtr client = m_client)
        return client->lockFullscreenOrientation(orientation);
    return false;
}

void WebFullScreenManagerProxy::unlockFullscreenOrientation()
{
    if (CheckedPtr client = m_client)
        client->unlockFullscreenOrientation();
}

#if !RELEASE_LOG_DISABLED
WTFLogChannel& WebFullScreenManagerProxy::logChannel() const
{
    return WebKit2LogFullscreen;
}
#endif

} // namespace WebKit

#endif // ENABLE(FULLSCREEN_API)
