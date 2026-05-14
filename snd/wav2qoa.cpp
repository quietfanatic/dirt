#include "audio.h"
#include "../uni/io.h"

using namespace snd;
using namespace uni;

int main (int argc, char** argv) {
    if (argc != 3) {
        warn_utf8(cat(
            "USAGE: ", (const char*)argv[0], " <in.wav> <out.qoa>\n"
        ));
        return 1;
    }
    Str in = (const char*)argv[1];
    Str out = (const char*)argv[2];
    UniqueAudio au = audio_from_file(in);
    audio_to_file_qoa(out, au);
    return 0;
}

