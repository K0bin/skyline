// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
// Copyright © 2018-2020 fincs (https://github.com/devkitPro/deko3d)

#include <boost/preprocessor/repeat.hpp>
#include <soc.h>
#include "fermi_2d.h"

namespace skyline::soc::gm20b::engine::fermi2d {
    Fermi2D::Fermi2D(const DeviceState &state, ChannelContext &channelCtx, MacroState &macroState, gpu::interconnect::CommandExecutor &executor)
        : MacroEngineBase(macroState),
          syncpoints(state.soc->host1x.syncpoints),
          context(*state.gpu, channelCtx, executor),
          channelCtx(channelCtx) {
        InitializeRegisters();
    }

    void Fermi2D::CallMethodFromMacro(u32 method, u32 argument) {
        HandleMethod(method, argument);
    }

    u32 Fermi2D::ReadMethodFromMacro(u32 method) {
        return registers.raw[method];
    }

    __attribute__((always_inline)) void Fermi2D::CallMethod(u32 method, u32 argument) {
        Logger::Verbose("Called method in Fermi 2D: 0x{:X} args: 0x{:X}", method, argument);

        HandleMethod(method, argument);
    }

    void Fermi2D::HandleMethod(u32 method, u32 argument) {
        bool redundant{registers.raw[method] == argument};
        registers.raw[method] = argument;

        if (redundant) {
            return;
        }

        if (method == ENGINE_STRUCT_OFFSET(pixelsFromMemory, srcY) + 1) {
            const auto& src = *registers.src;
            const auto& dst = *registers.dst;

            const auto& pixelsFromMemory = *registers.pixelsFromMemory;
            skyline::gpu::interconnect::BlitContext::BlitDesc desc{
                .src = src,
                .dst = dst,
                .dstX = pixelsFromMemory.dstX,
                .dstY = pixelsFromMemory.dstY,
                .dstWidth = pixelsFromMemory.dstWidth,
                .dstHeight = pixelsFromMemory.dstHeight,
                .srcX = static_cast<i32>(pixelsFromMemory.srcX >> 32),
                .srcY = static_cast<i32>(pixelsFromMemory.srcY >> 32),
                .srcWidth = static_cast<i32>((pixelsFromMemory.duDx * pixelsFromMemory.dstWidth) >> 32),
                .srcHeight = static_cast<i32>((pixelsFromMemory.duDy * pixelsFromMemory.dstHeight) >> 32)
            };
            context.Blit(desc);
        }
    }
}
