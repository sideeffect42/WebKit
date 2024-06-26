/*
 * Copyright (C) 2014-2019 Apple Inc. All rights reserved.
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

#include "LineMetadata.h"
#include "Mutex.h"
#include "Sizes.h"
#include "StaticPerProcess.h"
#include "Vector.h"
#include <array>
#include <mutex>

#if !BUSE(LIBPAS)

namespace bmalloc {

class HeapConstants : public StaticPerProcess<HeapConstants> {
public:
    HeapConstants(const LockHolder&);
    ~HeapConstants() = delete;

    inline size_t pageClass(size_t sizeClass) const { return m_pageClasses[sizeClass]; }
    inline size_t smallLineCount() const { return bmalloc::smallLineCount(m_vmPageSizePhysical); }
    inline unsigned char startOffset(size_t sizeClass, size_t lineNumber) const { return lineMetadata(sizeClass, lineNumber).startOffset; }
    inline unsigned char objectCount(size_t sizeClass, size_t lineNumber) const { return lineMetadata(sizeClass, lineNumber).objectCount; }

private:
    void initializeLineMetadata();
    void initializePageMetadata();

    inline const LineMetadata& lineMetadata(size_t sizeClass, size_t lineNumber) const
    {
        return m_smallLineMetadata[sizeClass * smallLineCount() + lineNumber];
    }

    size_t m_vmPageSizePhysical;
    const LineMetadata* m_smallLineMetadata { };
    Vector<LineMetadata> m_smallLineMetadataStorage;
    std::array<size_t, sizeClassCount> m_pageClasses;
};
BALLOW_DEPRECATED_DECLARATIONS_BEGIN
DECLARE_STATIC_PER_PROCESS_STORAGE(HeapConstants);
BALLOW_DEPRECATED_DECLARATIONS_END

} // namespace bmalloc

#endif
