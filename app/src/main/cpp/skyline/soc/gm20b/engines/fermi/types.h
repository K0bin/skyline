// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)
// Copyright © 2018-2020 fincs (https://github.com/devkitPro/deko3d)

#pragma once

#include <common.h>

#include <soc/gm20b/engines/maxwell/types.h>
#include <soc/gm20b/engines/engine.h>

namespace skyline::soc::gm20b::engine::fermi2d::type {
    #pragma pack(push, 1)

    enum class MemoryLayout {
        BlockLinear = 0,
        Pitch = 1
    };

    struct Surface {
        engine::maxwell3d::type::ColorRenderTarget::Format format;
        MemoryLayout memoryLayout;
        u32 blockWidthLog2 : 4;
        u32 blockHeightLog2 : 4;
        u32 blockDepthLog2 : 4;
        u32 depth;
        u32 layer;
        u32 widthBytes;
        u32 width;
        u32 height;
        Address address;
    };

    #pragma pack(pop)
}
