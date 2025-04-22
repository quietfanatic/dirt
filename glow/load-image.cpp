#include "load-image.h"

#include <sail/sail_junior.h>
#include <sail-common/common.h>
#include <sail-common/image.h>
#include <sail-common/log.h>
#include <sail-common/palette.h>
#include <sail-manip/convert.h>
#include "../ayu/reflection/describe.h"
#include "../ayu/traversal/to-tree.h"
#include "../uni/io.h"
#include "gl.h"

namespace glow {

namespace in {
[[noreturn, gnu::cold]] static
void raise_LoadImageFailed (Str, sail_status_t);
} using namespace in;

constexpr u16 UNSUPPORTED = 0;
constexpr u16 CONVERT_RGB = 1;
constexpr u16 CONVERT_RGBA = 2;
constexpr u16 CONVERT_INDEXED = 3;
constexpr u16 NEED_CONVERT_BOUNARY = 3;

enum class FormatType : u16 {
    Unsupported,
    Normal,
    Convert,
    Indexed
};
using FT = FormatType;

struct FormatInfo {
    FormatType type;
    u16 gl_internal_format;
    u16 gl_format;
    u16 gl_type;
};
constexpr u32 n_formats = u32(SAIL_PIXEL_FORMAT_BPP64_YUVA)+1;
constexpr FormatInfo formats [n_formats] = {
    {FT::Unsupported,0,0,0}, // SPF_UNKNOWN
    {FT::Unsupported,0,0,0}, // SPF_BPP1
    {FT::Unsupported,0,0,0}, // SPF_BPP2
    {FT::Unsupported,0,0,0}, // SPF_BPP4
    {FT::Unsupported,0,0,0}, // SPF_BPP8
    {FT::Unsupported,0,0,0}, // SPF_BPP16
    {FT::Unsupported,0,0,0}, // SPF_BPP24
    {FT::Unsupported,0,0,0}, // SPF_BPP32
    {FT::Unsupported,0,0,0}, // SPF_BPP48
    {FT::Unsupported,0,0,0}, // SPF_BPP64
    {FT::Unsupported,0,0,0}, // SPF_BPP72
    {FT::Unsupported,0,0,0}, // SPF_BPP96
    {FT::Unsupported,0,0,0}, // SPF_BPP128
    {FT::Indexed,0,GL_RGBA,GL_UNSIGNED_BYTE}, // SPF_BPP1_INDEXED
    {FT::Indexed,0,GL_RGBA,GL_UNSIGNED_BYTE}, // SPF_BPP2_INDEXED
    {FT::Indexed,0,GL_RGBA,GL_UNSIGNED_BYTE}, // SPF_BPP4_INDEXED
    {FT::Indexed,0,GL_RGBA,GL_UNSIGNED_BYTE}, // SPF_BPP8_INDEXED
    {FT::Indexed,0,GL_RGBA,GL_UNSIGNED_BYTE}, // SPF_BPP16_INDEXED
    {FT::Convert,GL_R8,GL_RGBA,GL_UNSIGNED_BYTE}, // SPF_BPP1_GRAYSCALE
    {FT::Convert,GL_R8,GL_RGBA,GL_UNSIGNED_BYTE}, // SPF_BPP2_GRAYSCALE
    {FT::Convert,GL_R8,GL_RGBA,GL_UNSIGNED_BYTE}, // SPF_BPP4_GRAYSCALE
    {FT::Normal,GL_R8,GL_RED,GL_UNSIGNED_BYTE}, // SPF_BPP8_GRAYSCALE
    {FT::Normal,GL_R8,GL_RED,GL_UNSIGNED_SHORT}, // SPF_BPP16_GRAYSCALE
    {FT::Convert,GL_RG8,GL_RGBA,GL_UNSIGNED_BYTE}, // SPF_BPP4_GRAYSCALE_ALPHA
    {FT::Convert,GL_RG8,GL_RGBA,GL_UNSIGNED_BYTE}, // SPF_BPP8_GRAYSCALE_ALPHA
    {FT::Normal,GL_RG8,GL_RG,GL_UNSIGNED_BYTE}, // SPF_BPP16_GRAYSCALE_ALPHA
    {FT::Normal,GL_RG8,GL_RG,GL_UNSIGNED_INT}, // SPF_BPP32_GRAYSCALE_ALPHA
    {FT::Normal,GL_RGB5,GL_RGB,GL_UNSIGNED_SHORT_5_5_5_1}, // SPF_BPP16_RGB555
    {FT::Normal,GL_RGB5,GL_BGR,GL_UNSIGNED_SHORT_5_5_5_1}, // SPF_BPP16_BGR555
    {FT::Normal,GL_RGB8,GL_RGB,GL_UNSIGNED_SHORT_5_6_5}, // SPF_BPP16_RGB565
    {FT::Normal,GL_RGB8,GL_BGR,GL_UNSIGNED_SHORT_5_6_5}, // SPF_BPP16_BGR565
    {FT::Normal,GL_RGB8,GL_RGB,GL_UNSIGNED_BYTE}, // SPF_BPP24_RGB
    {FT::Normal,GL_RGB8,GL_BGR,GL_UNSIGNED_BYTE}, // SPF_BPP24_BGR
    {FT::Normal,GL_RGB8,GL_RGB,GL_UNSIGNED_SHORT}, // SPF_BPP48_RGB
    {FT::Normal,GL_RGB8,GL_BGR,GL_UNSIGNED_SHORT}, // SPF_BPP48_BGR
    {FT::Normal,GL_RGB4,GL_RGBA,GL_UNSIGNED_SHORT_4_4_4_4}, // SPF_BPP16_RGBX
    {FT::Normal,GL_RGB4,GL_BGRA,GL_UNSIGNED_SHORT_4_4_4_4}, // SPF_BPP16_BGRX
    {FT::Convert,GL_RGB4,GL_RGBA,GL_UNSIGNED_BYTE}, // SPF_BPP16_XRGB
    {FT::Convert,GL_RGB4,GL_RGBA,GL_UNSIGNED_BYTE}, // SPF_BPP16_XBGR
    {FT::Normal,GL_RGBA4,GL_RGBA,GL_UNSIGNED_SHORT_4_4_4_4}, // SPF_BPP16_RGBA
    {FT::Normal,GL_RGBA4,GL_BGRA,GL_UNSIGNED_SHORT_4_4_4_4}, // SPF_BPP16_BGRA
    {FT::Convert,GL_RGBA8,GL_RGBA,GL_UNSIGNED_BYTE}, // SPF_BPP16_ARGB
    {FT::Convert,GL_RGBA8,GL_RGBA,GL_UNSIGNED_BYTE}, // SPF_BPP16_ABGR
    {FT::Normal,GL_RGB8,GL_RGBA,GL_UNSIGNED_BYTE}, // SPF_BPP32_RGBX
    {FT::Normal,GL_RGB8,GL_BGRA,GL_UNSIGNED_BYTE}, // SPF_BPP32_BGRX
    {FT::Convert,GL_RGB8,GL_RGBA,GL_UNSIGNED_BYTE}, // SPF_BPP32_XRGB
    {FT::Convert,GL_RGB8,GL_RGBA,GL_UNSIGNED_BYTE}, // SPF_BPP32_XBGR
    {FT::Normal,GL_RGBA8,GL_RGBA,GL_UNSIGNED_BYTE}, // SPF_BPP32_RGBA
    {FT::Normal,GL_RGBA8,GL_BGRA,GL_UNSIGNED_BYTE}, // SPF_BPP32_BGRA
    {FT::Convert,GL_RGBA8,GL_RGBA,GL_UNSIGNED_BYTE}, // SPF_BPP32_ARGB
    {FT::Convert,GL_RGBA8,GL_RGBA,GL_UNSIGNED_BYTE}, // SPF_BPP32_ABGR
    {FT::Normal,GL_RGB8,GL_RGBA,GL_UNSIGNED_SHORT}, // SPF_BPP64_RGBX
    {FT::Normal,GL_RGB8,GL_BGRA,GL_UNSIGNED_SHORT}, // SPF_BPP64_BGRX
    {FT::Convert,GL_RGB8,GL_RGBA,GL_UNSIGNED_BYTE}, // SPF_BPP64_XRGB
    {FT::Convert,GL_RGB8,GL_RGBA,GL_UNSIGNED_BYTE}, // SPF_BPP64_XBGR
    {FT::Normal,GL_RGBA8,GL_RGBA,GL_UNSIGNED_SHORT}, // SPF_BPP64_RGBA
    {FT::Normal,GL_RGBA8,GL_BGRA,GL_UNSIGNED_SHORT}, // SPF_BPP64_BGRA
    {FT::Convert,GL_RGBA8,GL_RGBA,GL_UNSIGNED_BYTE}, // SPF_BPP64_ARGB
    {FT::Convert,GL_RGBA8,GL_RGBA,GL_UNSIGNED_BYTE}, // SPF_BPP64_ABGR
    {FT::Convert,GL_RGB8,GL_RGBA,GL_UNSIGNED_BYTE}, // SPF_BPP32_CMYK
    {FT::Convert,GL_RGB8,GL_RGBA,GL_UNSIGNED_BYTE}, // SPF_BPP64_CMYK
    {FT::Convert,GL_RGBA8,GL_RGBA,GL_UNSIGNED_BYTE}, // SPF_BPP40_CMYKA
    {FT::Convert,GL_RGBA8,GL_RGBA,GL_UNSIGNED_BYTE}, // SPF_BPP80_CMYKA
    {FT::Convert,GL_RGB8,GL_RGBA,GL_UNSIGNED_BYTE}, // SPF_BPP24_YCBCR
    {FT::Convert,GL_RGB8,GL_RGBA,GL_UNSIGNED_BYTE}, // SPF_BPP32_YCCK
    {FT::Unsupported,0,0,0}, // SPF_BPP24_CIE_LAB
    {FT::Unsupported,0,0,0}, // SPF_BPP40_CIE_LAB
    {FT::Unsupported,0,0,0}, // SPF_BPP24_CIE_LUV
    {FT::Unsupported,0,0,0}, // SPF_BPP40_CIE_LUV
    {FT::Convert,GL_RGB8,GL_RGBA,GL_UNSIGNED_BYTE}, // SPF_BPP24_YUV
    {FT::Convert,GL_RGB8,GL_RGBA,GL_UNSIGNED_BYTE}, // SPF_BPP30_YUV
    {FT::Convert,GL_RGB8,GL_RGBA,GL_UNSIGNED_BYTE}, // SPF_BPP36_YUV
    {FT::Convert,GL_RGB8,GL_RGBA,GL_UNSIGNED_BYTE}, // SPF_BPP48_YUV
    {FT::Convert,GL_RGBA8,GL_RGBA,GL_UNSIGNED_BYTE}, // SPF_BPP32_YUVA
    {FT::Convert,GL_RGBA8,GL_RGBA,GL_UNSIGNED_BYTE}, // SPF_BPP40_YUVA
    {FT::Convert,GL_RGBA8,GL_RGBA,GL_UNSIGNED_BYTE}, // SPF_BPP48_YUVA
    {FT::Convert,GL_RGBA8,GL_RGBA,GL_UNSIGNED_BYTE}, // SPF_BPP64_YUVA
};

void load_texture_from_file (u32 target, AnyString filename) {
    sail_set_log_barrier(SAIL_LOG_LEVEL_WARNING);
    sail_image* image;
    auto res = sail_load_from_file(filename.c_str(), &image);
    if (res != SAIL_OK) raise_LoadImageFailed(filename, res);
     // Translate SAIL formats into OpenGL formats
    require(u32(image->pixel_format) <= n_formats);
    auto format = formats[u32(image->pixel_format)];
    if (format.type != FT::Normal) {
         // Nontrivial format, so ask SAIL to convert
        warn_utf8(cat(
            "Converting ", filename, " from ", ayu::show(&image->pixel_format), '\n'
        ));
        if (format.type == FT::Indexed) {
            format.gl_internal_format =
                formats[u32(image->palette->pixel_format)].gl_internal_format;
        }
        sail_image* old_image = image;
        res = sail_convert_image(
            old_image, SAIL_PIXEL_FORMAT_BPP32_RGBA, &image
        );
        sail_destroy_image(old_image);
        if (res != SAIL_OK) raise_LoadImageFailed(filename, res);
    }
     // Detect greyscale images and unused alpha channels.  Only bothering to do
     // it for the most common formats.
    auto pixels = reinterpret_cast<u8*>(image->pixels);
    if (image->pixel_format == SAIL_PIXEL_FORMAT_BPP24_RGB
     || image->pixel_format == SAIL_PIXEL_FORMAT_BPP24_BGR
    ) {
        bool greyscale = true;
        for (u32 y = 0; y < image->height; y++)
        for (u32 x = 0; x < image->width; x++) {
            u8 r = pixels[y * image->bytes_per_line + x*3];
            u8 g = pixels[y * image->bytes_per_line + x*3 + 1];
            u8 b = pixels[y * image->bytes_per_line + x*3 + 2];
            if (r != g || g != b) {
                greyscale = false;
                goto done_24;
            }
        }
        done_24:;
        if (greyscale) {
            warn_utf8(cat(
                "Reducing ", filename, " from 3 channels to 1", '\n'
            ));
            format.gl_internal_format = GL_R8;
        }
    }
    else if (image->pixel_format == SAIL_PIXEL_FORMAT_BPP32_RGBA
          || image->pixel_format == SAIL_PIXEL_FORMAT_BPP32_BGRA
    ) {
        bool greyscale = true;
        bool unused_alpha = true;
        for (u32 y = 0; y < image->height; y++)
        for (u32 x = 0; x < image->width; x++) {
            u8 r = pixels[y * image->bytes_per_line + x*4];
            u8 g = pixels[y * image->bytes_per_line + x*4 + 1];
            u8 b = pixels[y * image->bytes_per_line + x*4 + 2];
            u8 a = pixels[y * image->bytes_per_line + x*4 + 3];
            if (r != g || g != b) {
                greyscale = false;
                if (!unused_alpha) goto done_32;
            }
            if (a != 255) {
                unused_alpha = false;
                if (!greyscale) goto done_32;
            }
        }
        done_32:;
        if (greyscale && unused_alpha) {
            warn_utf8(cat(
                "Reducing ", filename, " from 4 channels to 1\n"
            ));
            format.gl_internal_format = GL_R8;
        }
        else if (greyscale) {
            warn_utf8(cat(
                "Reducing ", filename, " from 4 channels to 2\n"
            ));
            format.gl_internal_format = GL_RG8; // G -> A
        }
        else if (unused_alpha) {
            warn_utf8(cat(
                "Reducing ", filename, " from 4 channels to 3\n"
            ));
            format.gl_internal_format = GL_RGB8;
        }
    }
     // Now upload texture
    require(image->width > 0 && image->height > 0);
     // Look out!  SDL and OpenGL expect rows to be aligned to 4 bytes, but SAIL
     // doesn't do that.  Fortunately we can tell OpenGL to fix that.
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
     // And to avoid confusion I guess?
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glTexImage2D(
        target, 0, format.gl_internal_format,
        image->width, image->height, 0,
        format.gl_format, format.gl_type, image->pixels
    );
    sail_destroy_image(image);
    glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
     // Make sure the right channels go to the right colors
    if (format.gl_internal_format == GL_R8) {
        glTexParameteri(target, GL_TEXTURE_SWIZZLE_G, GL_RED);
        glTexParameteri(target, GL_TEXTURE_SWIZZLE_B, GL_RED);
    }
    else if (format.gl_internal_format == GL_RG8) {
        glTexParameteri(target, GL_TEXTURE_SWIZZLE_G, GL_RED);
        glTexParameteri(target, GL_TEXTURE_SWIZZLE_B, GL_RED);
        glTexParameteri(target, GL_TEXTURE_SWIZZLE_A, GL_GREEN);
    }
}

UniqueImage load_image_from_file (AnyString filename) {
    sail_set_log_barrier(SAIL_LOG_LEVEL_WARNING);
    sail_image* image;
    auto res = sail_load_from_file(filename.c_str(), &image);
    if (res != SAIL_OK) raise_LoadImageFailed(filename, res);
    if (image->pixel_format != SAIL_PIXEL_FORMAT_BPP32_RGBA) {
        sail_image* old_image = image;
        res = sail_convert_image(old_image, SAIL_PIXEL_FORMAT_BPP32_RGBA, &image);
        sail_destroy_image(old_image);
        if (res != SAIL_OK) raise_LoadImageFailed(filename, res);
    }
    UniqueImage r (IVec(image->width, image->height), (RGBA8*)image->pixels);
    image->pixels = null;
    sail_destroy_image(image);
    return r;
}

void in::raise_LoadImageFailed (Str filename, sail_status_t res) {
     // TODO: tag
    raise(e_LoadImageFailed, cat(
        "Failed to load image from file (", ayu::show(&res), "): ", filename
    ));
}

} using namespace glow;

AYU_DESCRIBE(sail_status_t,
    values(
        value("SAIL_OK", SAIL_OK),
         // Common errors.
        value("SAIL_ERROR_NULL_PTR", SAIL_ERROR_NULL_PTR),
        value("SAIL_ERROR_MEMORY_ALLOCATION", SAIL_ERROR_MEMORY_ALLOCATION),
        value("SAIL_ERROR_OPEN_FILE", SAIL_ERROR_OPEN_FILE),
        value("SAIL_ERROR_READ_FILE", SAIL_ERROR_READ_FILE),
        value("SAIL_ERROR_SEEK_FILE", SAIL_ERROR_SEEK_FILE),
        value("SAIL_ERROR_CLOSE_FILE", SAIL_ERROR_CLOSE_FILE),
        value("SAIL_ERROR_LIST_DIR", SAIL_ERROR_LIST_DIR),
        value("SAIL_ERROR_PARSE_FILE", SAIL_ERROR_PARSE_FILE),
        value("SAIL_ERROR_INVALID_ARGUMENT", SAIL_ERROR_INVALID_ARGUMENT),
        value("SAIL_ERROR_READ_IO", SAIL_ERROR_READ_IO),
        value("SAIL_ERROR_WRITE_IO", SAIL_ERROR_WRITE_IO),
        value("SAIL_ERROR_FLUSH_IO", SAIL_ERROR_FLUSH_IO),
        value("SAIL_ERROR_SEEK_IO", SAIL_ERROR_SEEK_IO),
        value("SAIL_ERROR_TELL_IO", SAIL_ERROR_TELL_IO),
        value("SAIL_ERROR_CLOSE_IO", SAIL_ERROR_CLOSE_IO),
        value("SAIL_ERROR_EOF", SAIL_ERROR_EOF),
        value("SAIL_ERROR_NOT_IMPLEMENTED", SAIL_ERROR_NOT_IMPLEMENTED),
        value("SAIL_ERROR_UNSUPPORTED_SEEK_WHENCE", SAIL_ERROR_UNSUPPORTED_SEEK_WHENCE),
        value("SAIL_ERROR_EMPTY_STRING", SAIL_ERROR_EMPTY_STRING),
        value("SAIL_ERROR_INVALID_VARIANT", SAIL_ERROR_INVALID_VARIANT),
         // Encoding/decoding common errors.
        value("SAIL_ERROR_INVALID_IO", SAIL_ERROR_INVALID_IO),
         // Encoding/decoding specific errors.
        value("SAIL_ERROR_INCORRECT_IMAGE_DIMENSIONS", SAIL_ERROR_INCORRECT_IMAGE_DIMENSIONS),
        value("SAIL_ERROR_UNSUPPORTED_PIXEL_FORMAT", SAIL_ERROR_UNSUPPORTED_PIXEL_FORMAT),
        value("SAIL_ERROR_INVALID_PIXEL_FORMAT", SAIL_ERROR_INVALID_PIXEL_FORMAT),
        value("SAIL_ERROR_UNSUPPORTED_COMPRESSION", SAIL_ERROR_UNSUPPORTED_COMPRESSION),
        value("SAIL_ERROR_UNSUPPORTED_META_DATA", SAIL_ERROR_UNSUPPORTED_META_DATA),
        value("SAIL_ERROR_UNDERLYING_CODEC", SAIL_ERROR_UNDERLYING_CODEC),
        value("SAIL_ERROR_NO_MORE_FRAMES", SAIL_ERROR_NO_MORE_FRAMES),
        value("SAIL_ERROR_INTERLACING_UNSUPPORTED", SAIL_ERROR_INTERLACING_UNSUPPORTED),
        value("SAIL_ERROR_INCORRECT_BYTES_PER_LINE", SAIL_ERROR_INCORRECT_BYTES_PER_LINE),
        value("SAIL_ERROR_UNSUPPORTED_IMAGE_PROPERTY", SAIL_ERROR_UNSUPPORTED_IMAGE_PROPERTY),
        value("SAIL_ERROR_UNSUPPORTED_BIT_DEPTH", SAIL_ERROR_UNSUPPORTED_BIT_DEPTH),
        value("SAIL_ERROR_MISSING_PALETTE", SAIL_ERROR_MISSING_PALETTE),
        value("SAIL_ERROR_UNSUPPORTED_FORMAT", SAIL_ERROR_UNSUPPORTED_FORMAT),
        value("SAIL_ERROR_BROKEN_IMAGE", SAIL_ERROR_BROKEN_IMAGE),
         // Codecs-specific errors.
        value("SAIL_ERROR_CODEC_LOAD", SAIL_ERROR_CODEC_LOAD),
        value("SAIL_ERROR_CODEC_NOT_FOUND", SAIL_ERROR_CODEC_NOT_FOUND),
        value("SAIL_ERROR_UNSUPPORTED_CODEC_LAYOUT", SAIL_ERROR_UNSUPPORTED_CODEC_LAYOUT),
        value("SAIL_ERROR_CODEC_SYMBOL_RESOLVE", SAIL_ERROR_CODEC_SYMBOL_RESOLVE),
        value("SAIL_ERROR_INCOMPLETE_CODEC_INFO", SAIL_ERROR_INCOMPLETE_CODEC_INFO),
        value("SAIL_ERROR_UNSUPPORTED_CODEC_FEATURE", SAIL_ERROR_UNSUPPORTED_CODEC_FEATURE),
        value("SAIL_ERROR_UNSUPPORTED_CODEC_PRIORITY", SAIL_ERROR_UNSUPPORTED_CODEC_PRIORITY),
         // libsail errors.
        value("SAIL_ERROR_ENV_UPDATE", SAIL_ERROR_ENV_UPDATE),
        value("SAIL_ERROR_CONTEXT_UNINITIALIZED", SAIL_ERROR_CONTEXT_UNINITIALIZED),
        value("SAIL_ERROR_GET_DLL_PATH", SAIL_ERROR_GET_DLL_PATH),
        value("SAIL_ERROR_CONFLICTING_OPERATION", SAIL_ERROR_CONFLICTING_OPERATION)
    )
)

AYU_DESCRIBE(SailPixelFormat,
    values(
        value("SAIL_PIXEL_FORMAT_UNKNOWN", SAIL_PIXEL_FORMAT_UNKNOWN),
         // Formats with unknown pixel representation/model.
        value("SAIL_PIXEL_FORMAT_BPP1", SAIL_PIXEL_FORMAT_BPP1),
        value("SAIL_PIXEL_FORMAT_BPP2", SAIL_PIXEL_FORMAT_BPP2),
        value("SAIL_PIXEL_FORMAT_BPP4", SAIL_PIXEL_FORMAT_BPP4),
        value("SAIL_PIXEL_FORMAT_BPP8", SAIL_PIXEL_FORMAT_BPP8),
        value("SAIL_PIXEL_FORMAT_BPP16", SAIL_PIXEL_FORMAT_BPP16),
        value("SAIL_PIXEL_FORMAT_BPP24", SAIL_PIXEL_FORMAT_BPP24),
        value("SAIL_PIXEL_FORMAT_BPP32", SAIL_PIXEL_FORMAT_BPP32),
        value("SAIL_PIXEL_FORMAT_BPP48", SAIL_PIXEL_FORMAT_BPP48),
        value("SAIL_PIXEL_FORMAT_BPP64", SAIL_PIXEL_FORMAT_BPP64),
        value("SAIL_PIXEL_FORMAT_BPP72", SAIL_PIXEL_FORMAT_BPP72),
        value("SAIL_PIXEL_FORMAT_BPP96", SAIL_PIXEL_FORMAT_BPP96),
        value("SAIL_PIXEL_FORMAT_BPP128", SAIL_PIXEL_FORMAT_BPP128),
         //  Indexed formats with palette.
        value("SAIL_PIXEL_FORMAT_BPP1_INDEXED", SAIL_PIXEL_FORMAT_BPP1_INDEXED),
        value("SAIL_PIXEL_FORMAT_BPP2_INDEXED", SAIL_PIXEL_FORMAT_BPP2_INDEXED),
        value("SAIL_PIXEL_FORMAT_BPP4_INDEXED", SAIL_PIXEL_FORMAT_BPP4_INDEXED),
        value("SAIL_PIXEL_FORMAT_BPP8_INDEXED", SAIL_PIXEL_FORMAT_BPP8_INDEXED),
        value("SAIL_PIXEL_FORMAT_BPP16_INDEXED", SAIL_PIXEL_FORMAT_BPP16_INDEXED),
         //  Grayscale formats.
        value("SAIL_PIXEL_FORMAT_BPP1_GRAYSCALE", SAIL_PIXEL_FORMAT_BPP1_GRAYSCALE),
        value("SAIL_PIXEL_FORMAT_BPP2_GRAYSCALE", SAIL_PIXEL_FORMAT_BPP2_GRAYSCALE),
        value("SAIL_PIXEL_FORMAT_BPP4_GRAYSCALE", SAIL_PIXEL_FORMAT_BPP4_GRAYSCALE),
        value("SAIL_PIXEL_FORMAT_BPP8_GRAYSCALE", SAIL_PIXEL_FORMAT_BPP8_GRAYSCALE),
        value("SAIL_PIXEL_FORMAT_BPP16_GRAYSCALE", SAIL_PIXEL_FORMAT_BPP16_GRAYSCALE),
        value("SAIL_PIXEL_FORMAT_BPP4_GRAYSCALE_ALPHA", SAIL_PIXEL_FORMAT_BPP4_GRAYSCALE_ALPHA),
        value("SAIL_PIXEL_FORMAT_BPP8_GRAYSCALE_ALPHA", SAIL_PIXEL_FORMAT_BPP8_GRAYSCALE_ALPHA),
        value("SAIL_PIXEL_FORMAT_BPP16_GRAYSCALE_ALPHA", SAIL_PIXEL_FORMAT_BPP16_GRAYSCALE_ALPHA),
        value("SAIL_PIXEL_FORMAT_BPP32_GRAYSCALE_ALPHA", SAIL_PIXEL_FORMAT_BPP32_GRAYSCALE_ALPHA),
         // Packed formats.
        value("SAIL_PIXEL_FORMAT_BPP16_RGB555", SAIL_PIXEL_FORMAT_BPP16_RGB555),
        value("SAIL_PIXEL_FORMAT_BPP16_BGR555", SAIL_PIXEL_FORMAT_BPP16_BGR555),
        value("SAIL_PIXEL_FORMAT_BPP16_RGB565", SAIL_PIXEL_FORMAT_BPP16_RGB565),
        value("SAIL_PIXEL_FORMAT_BPP16_BGR565", SAIL_PIXEL_FORMAT_BPP16_BGR565),
         // RGB formats.
        value("SAIL_PIXEL_FORMAT_BPP24_RGB", SAIL_PIXEL_FORMAT_BPP24_RGB),
        value("SAIL_PIXEL_FORMAT_BPP24_BGR", SAIL_PIXEL_FORMAT_BPP24_BGR),
        value("SAIL_PIXEL_FORMAT_BPP48_RGB", SAIL_PIXEL_FORMAT_BPP48_RGB),
        value("SAIL_PIXEL_FORMAT_BPP48_BGR", SAIL_PIXEL_FORMAT_BPP48_BGR),
         // RGBA/X formats. X = unused color channel.
        value("SAIL_PIXEL_FORMAT_BPP16_RGBX", SAIL_PIXEL_FORMAT_BPP16_RGBX),
        value("SAIL_PIXEL_FORMAT_BPP16_BGRX", SAIL_PIXEL_FORMAT_BPP16_BGRX),
        value("SAIL_PIXEL_FORMAT_BPP16_XRGB", SAIL_PIXEL_FORMAT_BPP16_XRGB),
        value("SAIL_PIXEL_FORMAT_BPP16_XBGR", SAIL_PIXEL_FORMAT_BPP16_XBGR),
        value("SAIL_PIXEL_FORMAT_BPP16_RGBA", SAIL_PIXEL_FORMAT_BPP16_RGBA),
        value("SAIL_PIXEL_FORMAT_BPP16_BGRA", SAIL_PIXEL_FORMAT_BPP16_BGRA),
        value("SAIL_PIXEL_FORMAT_BPP16_ARGB", SAIL_PIXEL_FORMAT_BPP16_ARGB),
        value("SAIL_PIXEL_FORMAT_BPP16_ABGR", SAIL_PIXEL_FORMAT_BPP16_ABGR),
        value("SAIL_PIXEL_FORMAT_BPP32_RGBX", SAIL_PIXEL_FORMAT_BPP32_RGBX),
        value("SAIL_PIXEL_FORMAT_BPP32_BGRX", SAIL_PIXEL_FORMAT_BPP32_BGRX),
        value("SAIL_PIXEL_FORMAT_BPP32_XRGB", SAIL_PIXEL_FORMAT_BPP32_XRGB),
        value("SAIL_PIXEL_FORMAT_BPP32_XBGR", SAIL_PIXEL_FORMAT_BPP32_XBGR),
        value("SAIL_PIXEL_FORMAT_BPP32_RGBA", SAIL_PIXEL_FORMAT_BPP32_RGBA),
        value("SAIL_PIXEL_FORMAT_BPP32_BGRA", SAIL_PIXEL_FORMAT_BPP32_BGRA),
        value("SAIL_PIXEL_FORMAT_BPP32_ARGB", SAIL_PIXEL_FORMAT_BPP32_ARGB),
        value("SAIL_PIXEL_FORMAT_BPP32_ABGR", SAIL_PIXEL_FORMAT_BPP32_ABGR),
        value("SAIL_PIXEL_FORMAT_BPP64_RGBX", SAIL_PIXEL_FORMAT_BPP64_RGBX),
        value("SAIL_PIXEL_FORMAT_BPP64_BGRX", SAIL_PIXEL_FORMAT_BPP64_BGRX),
        value("SAIL_PIXEL_FORMAT_BPP64_XRGB", SAIL_PIXEL_FORMAT_BPP64_XRGB),
        value("SAIL_PIXEL_FORMAT_BPP64_XBGR", SAIL_PIXEL_FORMAT_BPP64_XBGR),
        value("SAIL_PIXEL_FORMAT_BPP64_RGBA", SAIL_PIXEL_FORMAT_BPP64_RGBA),
        value("SAIL_PIXEL_FORMAT_BPP64_BGRA", SAIL_PIXEL_FORMAT_BPP64_BGRA),
        value("SAIL_PIXEL_FORMAT_BPP64_ARGB", SAIL_PIXEL_FORMAT_BPP64_ARGB),
        value("SAIL_PIXEL_FORMAT_BPP64_ABGR", SAIL_PIXEL_FORMAT_BPP64_ABGR),
         // CMYK formats.
        value("SAIL_PIXEL_FORMAT_BPP32_CMYK", SAIL_PIXEL_FORMAT_BPP32_CMYK),
        value("SAIL_PIXEL_FORMAT_BPP64_CMYK", SAIL_PIXEL_FORMAT_BPP64_CMYK),
        value("SAIL_PIXEL_FORMAT_BPP40_CMYKA", SAIL_PIXEL_FORMAT_BPP40_CMYKA),
        value("SAIL_PIXEL_FORMAT_BPP80_CMYKA", SAIL_PIXEL_FORMAT_BPP80_CMYKA),
         // YCbCr formats.
        value("SAIL_PIXEL_FORMAT_BPP24_YCBCR", SAIL_PIXEL_FORMAT_BPP24_YCBCR),
         // YCCK formats.
        value("SAIL_PIXEL_FORMAT_BPP32_YCCK", SAIL_PIXEL_FORMAT_BPP32_YCCK),
         // CIE LAB formats.
        value("SAIL_PIXEL_FORMAT_BPP24_CIE_LAB", SAIL_PIXEL_FORMAT_BPP24_CIE_LAB), /* 8/8/8   */
        value("SAIL_PIXEL_FORMAT_BPP40_CIE_LAB", SAIL_PIXEL_FORMAT_BPP40_CIE_LAB), /* 8/16/16 */
         // CIE LUV formats.
        value("SAIL_PIXEL_FORMAT_BPP24_CIE_LUV", SAIL_PIXEL_FORMAT_BPP24_CIE_LUV), /* 8/8/8   */
        value("SAIL_PIXEL_FORMAT_BPP40_CIE_LUV", SAIL_PIXEL_FORMAT_BPP40_CIE_LUV), /* 8/16/16 */
         // YUV formats.
        value("SAIL_PIXEL_FORMAT_BPP24_YUV", SAIL_PIXEL_FORMAT_BPP24_YUV), /* 8-bit  */
        value("SAIL_PIXEL_FORMAT_BPP30_YUV", SAIL_PIXEL_FORMAT_BPP30_YUV), /* 10-bit */
        value("SAIL_PIXEL_FORMAT_BPP36_YUV", SAIL_PIXEL_FORMAT_BPP36_YUV), /* 12-bit */
        value("SAIL_PIXEL_FORMAT_BPP48_YUV", SAIL_PIXEL_FORMAT_BPP48_YUV), /* 16-bit */
        value("SAIL_PIXEL_FORMAT_BPP32_YUVA", SAIL_PIXEL_FORMAT_BPP32_YUVA),
        value("SAIL_PIXEL_FORMAT_BPP40_YUVA", SAIL_PIXEL_FORMAT_BPP40_YUVA),
        value("SAIL_PIXEL_FORMAT_BPP48_YUVA", SAIL_PIXEL_FORMAT_BPP48_YUVA),
        value("SAIL_PIXEL_FORMAT_BPP64_YUVA", SAIL_PIXEL_FORMAT_BPP64_YUVA)
    )
)
