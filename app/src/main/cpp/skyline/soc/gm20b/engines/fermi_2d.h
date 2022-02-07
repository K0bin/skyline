// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
// Copyright © 2018-2020 fincs (https://github.com/devkitPro/deko3d)

#pragma once

#include <gpu/interconnect/blit_context.h>
#include "engine.h"

namespace skyline::soc::gm20b {
    struct ChannelContext;
}

namespace skyline::soc::gm20b::engine::fermi2d {

    /**
     * @brief The Fermi 2D engine handles processing 2D graphics
     */
    class Fermi2D : public MacroEngineBase {

      private:

        host1x::SyncpointSet &syncpoints;
        gpu::interconnect::BlitContext context;

        /**
         * @brief Calls the appropriate function corresponding to a certain method with the supplied argument
         */
        void HandleMethod(u32 method, u32 argument);


      public:

        static constexpr u32 RegisterCount{0xE00}; //!< The number of Fermi 2D registers

        /**
         * @url https://github.com/devkitPro/deko3d/blob/master/source/maxwell/engine_2d.def
         */
        #pragma pack(push, 1)
        union Registers {
            std::array<u32, RegisterCount> raw;

            template<size_t Offset, typename Type>
            using Register = util::OffsetMember<Offset, Type, u32>;

            struct PixelsFromMemory {
                u32 blockShape : 3;
                u32 corralSize : 5;
                u32 safeOverlap : 1;
                u32 sampleMode; // TODO

                u32 _pad[8];

                i32 dstX;
                i32 dstY;
                i32 dstWidth;
                i32 dstHeight;
                i64 duDx;
                i64 duDy;
                i64 srcX;
                i64 srcY;
            };
            Register<0x200, type::Surface> dst;
            Register<0x230, type::Surface> src;
            Register<0x880, PixelsFromMemory> pixelsFromMemory;
        };
        //static_assert(sizeof(Registers) == (RegisterCount * sizeof(u32)));
        #pragma pack(pop)

        Registers registers{};

        ChannelContext &channelCtx;


        Fermi2D(const DeviceState &state, ChannelContext &channelCtx, MacroState &macroState, gpu::interconnect::CommandExecutor &executor);

        /**
         * @brief Initializes Fermi 2D registers to their default values
         */
        void InitializeRegisters();

        void CallMethod(u32 method, u32 argument);

        void CallMethodFromMacro(u32 method, u32 argument) override;

        u32 ReadMethodFromMacro(u32 method) override;

    };

}
