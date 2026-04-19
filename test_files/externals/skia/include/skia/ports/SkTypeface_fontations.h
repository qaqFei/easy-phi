/*
 * Copyright 2023 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SkTypeface_fontations_DEFINED
#define SkTypeface_fontations_DEFINED

#include "skia/core/SkFontArguments.h"
#include "skia/core/SkRefCnt.h"
#include "skia/core/SkTypeface.h"
#include "skia/core/SkTypes.h"

#include <memory>

SK_API sk_sp<SkTypeface> SkTypeface_Make_Fontations(std::unique_ptr<SkStreamAsset> fontData,
                                                    const SkFontArguments& args);

#endif
