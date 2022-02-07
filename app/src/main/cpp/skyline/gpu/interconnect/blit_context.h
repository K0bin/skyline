//
// Created by robin on 3/20/22.
//

#pragma once

#include <boost/functional/hash.hpp>
#include <boost/container/static_vector.hpp>
#include <range/v3/algorithm.hpp>
#include <gpu/texture/format.h>
#include <gpu/buffer_manager.h>
#include <soc/gm20b/channel.h>
#include <soc/gm20b/gmmu.h>
#include <soc/gm20b/engines/maxwell/types.h>
#include <soc/gm20b/engines/fermi/types.h>

namespace skyline::gpu::interconnect {

    namespace fermi2d = skyline::soc::gm20b::engine::fermi2d::type;
    namespace maxwell3d = soc::gm20b::engine::maxwell3d::type;

    class BlitContext {
      private:
        GPU &gpu;
        soc::gm20b::ChannelContext &channelCtx;
        gpu::interconnect::CommandExecutor &executor;

        /**
         * @brief A host IOVA address composed of 32-bit low/high register values
         * @note This differs from maxwell3d::Address in that it is little-endian rather than big-endian ordered for the register values
         */
        union IOVA {
            u64 iova;
            struct {
                u32 low;
                u32 high;
            };

            operator u64 &() {
                return iova;
            }
        };
        static_assert(sizeof(IOVA) == sizeof(u64));

        gpu::GuestTexture GetTexture(const fermi2d::Surface &surface) {
            auto determineFormat = [&](maxwell3d::ColorRenderTarget::Format format) -> skyline::gpu::texture::Format {
                #define FORMAT_CASE(maxwellFmt, skFmt, fmtType) \
                    case maxwell3d::ColorRenderTarget::Format::maxwellFmt ## fmtType: \
                        return skyline::gpu::format::skFmt ## fmtType

                #define FORMAT_SAME_CASE(fmt, type) FORMAT_CASE(fmt, fmt, type)

                #define FORMAT_INT_CASE(maxwellFmt, skFmt) \
                    FORMAT_CASE(maxwellFmt, skFmt, Sint); \
                    FORMAT_CASE(maxwellFmt, skFmt, Uint)

                #define FORMAT_SAME_INT_CASE(fmt) FORMAT_INT_CASE(fmt, fmt)

                #define FORMAT_INT_FLOAT_CASE(maxwellFmt, skFmt) \
                    FORMAT_INT_CASE(maxwellFmt, skFmt); \
                    FORMAT_CASE(maxwellFmt, skFmt, Float)

                #define FORMAT_SAME_INT_FLOAT_CASE(fmt) FORMAT_INT_FLOAT_CASE(fmt, fmt)

                #define FORMAT_NORM_CASE(maxwellFmt, skFmt) \
                    FORMAT_CASE(maxwellFmt, skFmt, Snorm); \
                    FORMAT_CASE(maxwellFmt, skFmt, Unorm)

                #define FORMAT_NORM_INT_CASE(maxwellFmt, skFmt) \
                    FORMAT_NORM_CASE(maxwellFmt, skFmt); \
                    FORMAT_INT_CASE(maxwellFmt, skFmt)

                #define FORMAT_SAME_NORM_INT_CASE(fmt) FORMAT_NORM_INT_CASE(fmt, fmt)

                #define FORMAT_NORM_INT_SRGB_CASE(maxwellFmt, skFmt) \
                    FORMAT_NORM_INT_CASE(maxwellFmt, skFmt); \
                    FORMAT_CASE(maxwellFmt, skFmt, Srgb)

                #define FORMAT_NORM_INT_FLOAT_CASE(maxwellFmt, skFmt) \
                    FORMAT_NORM_INT_CASE(maxwellFmt, skFmt); \
                    FORMAT_CASE(maxwellFmt, skFmt, Float)

                #define FORMAT_SAME_NORM_INT_FLOAT_CASE(fmt) FORMAT_NORM_INT_FLOAT_CASE(fmt, fmt)

                switch (format) {
                    case maxwell3d::ColorRenderTarget::Format::None:
                        return {};

                    FORMAT_SAME_NORM_INT_CASE(R8);
                    FORMAT_SAME_NORM_INT_FLOAT_CASE(R16);
                    FORMAT_SAME_NORM_INT_CASE(R8G8);
                    FORMAT_SAME_CASE(B5G6R5, Unorm);
                    FORMAT_SAME_CASE(B5G5R5A1, Unorm);
                    FORMAT_SAME_INT_FLOAT_CASE(R32);
                    FORMAT_SAME_CASE(B10G11R11, Float);
                    FORMAT_SAME_NORM_INT_FLOAT_CASE(R16G16);
                    FORMAT_SAME_CASE(R8G8B8A8, Unorm);
                    FORMAT_SAME_CASE(R8G8B8A8, Srgb);
                    FORMAT_NORM_INT_SRGB_CASE(R8G8B8X8, R8G8B8A8);
                    FORMAT_SAME_CASE(B8G8R8A8, Unorm);
                    FORMAT_SAME_CASE(B8G8R8A8, Srgb);
                    FORMAT_SAME_CASE(A2B10G10R10, Unorm);
                    FORMAT_SAME_CASE(A2B10G10R10, Uint);
                    FORMAT_SAME_INT_CASE(R32G32);
                    FORMAT_SAME_CASE(R32G32, Float);
                    FORMAT_SAME_CASE(R16G16B16A16, Float);
                    FORMAT_NORM_INT_FLOAT_CASE(R16G16B16X16, R16G16B16A16);
                    FORMAT_SAME_CASE(R32G32B32A32, Float);
                    FORMAT_INT_FLOAT_CASE(R32G32B32X32, R32G32B32A32);

                    default:
                        throw exception("Cannot translate the supplied color RT format: 0x{:X}", static_cast<u32>(format));
                }

                #undef FORMAT_CASE
                #undef FORMAT_SAME_CASE
                #undef FORMAT_INT_CASE
                #undef FORMAT_SAME_INT_CASE
                #undef FORMAT_INT_FLOAT_CASE
                #undef FORMAT_SAME_INT_FLOAT_CASE
                #undef FORMAT_NORM_CASE
                #undef FORMAT_NORM_INT_CASE
                #undef FORMAT_SAME_NORM_INT_CASE
                #undef FORMAT_NORM_INT_SRGB_CASE
                #undef FORMAT_NORM_INT_FLOAT_CASE
                #undef FORMAT_SAME_NORM_INT_FLOAT_CASE
            };

            gpu::GuestTexture texture{};
            texture.format = determineFormat(surface.format);
            texture.layerCount = 1;
            texture.baseArrayLayer = static_cast<u16>(surface.layer);

            if (surface.memoryLayout == fermi2d::MemoryLayout::Pitch) {
                texture.type = gpu::texture::TextureType::e1D;
                texture.dimensions = gpu::texture::Dimensions{
                    surface.widthBytes / texture.format->bpb,
                    surface.height,
                    1,
                };
                texture.tileConfig = gpu::texture::TileConfig {
                    .mode = gpu::texture::TileMode::Pitch,
                    .pitch = surface.widthBytes
                };
            } else {
                texture.type = surface.depth > 0 ? gpu::texture::TextureType::e2D : gpu::texture::TextureType::e3D;
                texture.dimensions = gpu::texture::Dimensions{
                    surface.width,
                    surface.height,
                    surface.depth,
                };
                texture.tileConfig = gpu::texture::TileConfig {
                    .mode = gpu::texture::TileMode::Block,
                    .blockHeight = static_cast<u8>(1 << surface.blockHeightLog2),
                    .blockDepth = static_cast<u8>(1 << surface.blockDepthLog2),
                };
            }

            texture.layerCount = static_cast<u16>(std::max(1u, surface.depth)); // NO IDEA

            IOVA iova{surface.address};
            size_t layerStride{texture.GetLayerSize()};
            size_t size{layerStride * (texture.layerCount - texture.baseArrayLayer)};
            auto mappings{channelCtx.asCtx->gmmu.TranslateRange(iova, size)};
            texture.mappings.assign(mappings.begin(), mappings.end());

            return texture;
        }

        public:
          BlitContext(GPU &gpu, soc::gm20b::ChannelContext &channelCtx, gpu::interconnect::CommandExecutor &executor) : gpu(gpu), channelCtx(channelCtx), executor(executor) {
          }

          struct BlitDesc {
              const fermi2d::Surface& src;
              const fermi2d::Surface& dst;
              i32 srcX;
              i32 srcY;
              i32 srcWidth;
              i32 srcHeight;
              i32 dstX;
              i32 dstY;
              i32 dstWidth;
              i32 dstHeight;
          };

          void Blit(const BlitDesc& blitDesc) {
              // TODO: OOB blit: https://github.com/yuzu-emu/yuzu/commit/c7ad195fd3eeea380465be48264f4e69165178c7

              if (blitDesc.src.layer != 0 || blitDesc.dst.layer != 0) {
                  Logger::Warn("Blits for layers other than 0 are not implemented.");
                  return;
              }

              gpu::GuestTexture guestSrc{GetTexture(blitDesc.src)};
              gpu::GuestTexture guestDst{GetTexture(blitDesc.dst)};

              const auto srcTexture = gpu.texture.FindOrCreate(guestSrc);
              const auto dstTexture = gpu.texture.FindOrCreate(guestDst);

              executor.AddOutsideRpCommand([
                      srcTexture,
                      dstTexture,
                      srcX = blitDesc.srcX,
                      srcY = blitDesc.srcY,
                      srcWidth = blitDesc.srcWidth,
                      srcHeight = blitDesc.srcHeight,
                      dstX = blitDesc.dstX,
                      dstY = blitDesc.dstY,
                      dstWidth = blitDesc.dstWidth,
                      dstHeight = blitDesc.dstHeight
                  ](vk::raii::CommandBuffer &commandBuffer, const std::shared_ptr<FenceCycle> &cycle, GPU &) {
                      cycle->AttachObjects(srcTexture, dstTexture);
                      vk::Image srcImage = srcTexture->texture->GetBacking();
                      vk::Image dstImage = srcTexture->texture->GetBacking();

                      std::array<vk::Offset3D, 2> offsets{{ vk::Offset3D { static_cast<int32_t>(srcX), static_cast<int32_t>(srcY), 0 }, vk::Offset3D { static_cast<int32_t>(srcX + srcWidth), static_cast<int32_t>(srcY + srcHeight), 0 } }};

                      vk::ImageBlit region{
                          .srcSubresource = {
                              .aspectMask = vk::ImageAspectFlagBits::eColor,
                              .mipLevel = 0,
                              .baseArrayLayer = 0,
                              .layerCount = 1
                          },
                          .dstSubresource = {
                              .aspectMask = vk::ImageAspectFlagBits::eColor,
                              .mipLevel = 0,
                              .baseArrayLayer = 0,
                              .layerCount = 1
                          },
                          .srcOffsets = {
                              {{
                                  vk::Offset3D { static_cast<int32_t>(srcX), static_cast<int32_t>(srcY), 0 },
                                  vk::Offset3D { static_cast<int32_t>(srcX + srcWidth), static_cast<int32_t>(srcY + srcHeight), 0 }
                              }}
                          },
                          .dstOffsets = {
                              {{
                                   vk::Offset3D { static_cast<int32_t>(dstX), static_cast<int32_t>(dstY), 0 },
                                   vk::Offset3D { static_cast<int32_t>(dstX + dstWidth), static_cast<int32_t>(dstY + dstHeight), 0 }
                               }}
                          },
                      };

                      commandBuffer.blitImage(
                          srcImage,
                          vk::ImageLayout::eUndefined, // TODO
                          dstImage,
                          vk::ImageLayout::eUndefined, // TODO
                          region,
                          vk::Filter::eLinear
                      );
                  }
              );
          }
    };
}
