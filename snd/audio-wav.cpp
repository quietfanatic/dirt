#include "audio.h"

#include "../uni/endian.h"

namespace snd {
using namespace in;

 // Only s16 PCM is supported
UniqueAudio audio_from_array_wav (Slice<u8> contents, Str filename) {
    Str mess;
    const u8* in = contents.reinterpret<u8>().begin();
    const u8* in_end = contents.reinterpret<u8>().end();
    u32 internal_size;
    u32 n_channels = 0;
    u32 sample_rate = 0;
    if (contents.size() < 12 ||
        read_u32le(in) != read_u32le("RIFF") ||
        read_u32le(in + 8) != read_u32le("WAVE")
    ) {
        mess = "File is not WAVE format"; goto bad;
    }
    internal_size = read_u32le(in+4);
    if (internal_size != contents.size() - 8) {
        mess = "Internal size doesn't match external size"; goto bad;
    }
    in += 12;
    while (in + 8 < in_end) {
        u32 tag = read_u32le(in);
        u32 chunk_size = read_u32le(in + 4);
        in += 8;
        if (in + chunk_size > in_end) {
            mess = "Block ran off end of file"; goto bad;
        }
        if (tag == read_u32le("fmt ")) {
            if (chunk_size != 16) goto bad_fmt;
            u32 format = read_u16le(in);
            if (format != 1) goto bad_sample;
            n_channels = read_u16le(in + 2);
            sample_rate = read_u32le(in + 4);
            if (!sample_rate || sample_rate > 0xffffff) {
                mess = "Unsupported sample rate"; goto bad;
            }
            u32 bytes_per_sec = read_u32le(in + 8);
            u32 bytes_per_block = read_u16le(in + 12);
            u32 bits_per_sample = read_u16le(in + 14);
            if (bits_per_sample != 16) goto bad_sample;
            if (!n_channels || n_channels > 8) {
                mess = "Unsupported n_channels"; goto bad;
            }
            if (bytes_per_block != bits_per_sample / 8 * n_channels ||
                bytes_per_sec != bytes_per_block * sample_rate
            ) {
                mess = "Inconsistent sample format"; goto bad;
            }
        }
        else if (tag == read_u32le("data")) {
            if (!n_channels) {
                mess = "Format chunk didn't precede data chunk"; goto bad;
            }
            if (chunk_size % 2) {
                mess = "Data size is misaligned"; goto bad;
            }
            UniqueAudio r (n_channels, chunk_size / 2, sample_rate);
            std::memcpy(r.samples, in, chunk_size);
            return r;
        }
         // Ignore all other chunks
        in += chunk_size;
    }
    mess = "No data chunk found";
    bad: raise_LoadAudioFailed(filename, mess);
    bad_sample: mess = "Unsupported sample format"; goto bad;
    bad_fmt: mess = "Bad WAV format chunk"; goto bad;
}

} // snd::in

