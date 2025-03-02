/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#import "UIPasteboardConsistencyEnforcer.h"

#if PLATFORM(IOS_FAMILY)

#import <UIKit/UIKit.h>
#import <wtf/RetainPtr.h>
#import <wtf/cocoa/TypeCastsCocoa.h>

@implementation UIPasteboardConsistencyEnforcer {
    RetainPtr<UIPasteboard> _pasteboardToReset;
    RetainPtr<UIPasteboardName> _pasteboardName;
}

- (instancetype)initWithPasteboardName:(UIPasteboardName)pasteboardName
{
    if (!(self = [super init]))
        return nil;

    _pasteboardName = pasteboardName;
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(pasteboardChanged:) name:UIPasteboardChangedNotification object:nil];
    return self;
}

- (void)clearPasteboard
{
    [std::exchange(_pasteboardToReset, nil) setItems:@[ ]];
}

- (void)pasteboardChanged:(NSNotification *)notification
{
    if (_pasteboardToReset)
        return;

    auto pasteboard = dynamic_objc_cast<UIPasteboard>(notification.object);
    if (![_pasteboardName isEqualToString:pasteboard.name])
        return;

    auto userInfoDictionary = dynamic_objc_cast<NSDictionary>(notification.userInfo);
    if ([dynamic_objc_cast<NSArray>([userInfoDictionary objectForKey:UIPasteboardChangedTypesAddedKey]) count])
        _pasteboardToReset = pasteboard;
}

@end

#endif // PLATFORM(IOS_FAMILY)
