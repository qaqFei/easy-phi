/*
 * Copyright 2020 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SkCustomTypeface_DEFINED
#define SkCustomTypeface_DEFINED

#include "skia/core/SkDrawable.h"
#include "skia/core/SkFontMetrics.h"
#include "skia/core/SkFontStyle.h"
#include "skia/core/SkPath.h"
#include "skia/core/SkRect.h"
#include "skia/core/SkRefCnt.h"
#include "skia/core/SkTypeface.h"
#include "skia/core/SkTypes.h"

#include <memory>
#include <vector>

class SkStream;
class SkStreamAsset;
struct SkFontArguments;

class SK_API SkCustomTypefaceBuilder {
public:
    SkCustomTypefaceBuilder();

    void setGlyph(SkGlyphID, float advance, const SkPath&);
    void setGlyph(SkGlyphID, float advance, sk_sp<SkDrawable>, const SkRect& bounds);

    void setMetrics(const SkFontMetrics& fm, float scale = 1);
    void setFontStyle(SkFontStyle);

    sk_sp<SkTypeface> detach();

    static constexpr SkTypeface::FactoryId FactoryId = SkSetFourByteTag('u','s','e','r');
    static sk_sp<SkTypeface> MakeFromStream(std::unique_ptr<SkStreamAsset>, const SkFontArguments&);

private:
    struct GlyphRec {
        // logical union
        SkPath            fPath;
        sk_sp<SkDrawable> fDrawable;

        SkRect            fBounds  = {0,0,0,0}; // only used for drawable glyphs atm
        float             fAdvance = 0;

        bool isDrawable() const {
            SkASSERT(!fDrawable || fPath.isEmpty());
            return fDrawable != nullptr;
        }
    };

    std::vector<GlyphRec> fGlyphRecs;
    SkFontMetrics         fMetrics;
    SkFontStyle           fStyle;

    GlyphRec& ensureStorage(SkGlyphID);

    static sk_sp<SkTypeface> Deserialize(SkStream*);

    friend class SkTypeface;
    friend class SkUserTypeface;
};

#endif
