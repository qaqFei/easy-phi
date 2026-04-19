/*
 * Copyright 2016 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef SkImageGeneratorCG_DEFINED
#define SkImageGeneratorCG_DEFINED

// This is needed as clients may override the target platform
// using SkUserConfig
#include "skia/private/base/SkLoadUserConfig.h"

#if defined(SK_BUILD_FOR_MAC) || defined(SK_BUILD_FOR_IOS)

#include "skia/core/SkData.h"
#include "skia/core/SkImageGenerator.h"
#include "skia/core/SkRefCnt.h"
#include "skia/private/base/SkAPI.h"

#include <memory>

namespace SkImageGeneratorCG {
SK_API std::unique_ptr<SkImageGenerator> MakeFromEncodedCG(sk_sp<SkData>);
}  // namespace SkImageGeneratorCG

#endif  // defined(SK_BUILD_FOR_MAC) || defined(SK_BUILD_FOR_IOS)
#endif  // SkImageGeneratorCG_DEFINED
