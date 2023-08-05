#include "../file.h"

#include "../../uni/utf.h"

namespace ayu {

namespace in {

[[noreturn, gnu::cold]] NOINLINE static
void raise_io_error (ErrorCode code, StaticString details, Str filename, int errnum) {
    raise(code, cat(
        details, filename, ": ", std::strerror(errnum)
    ));
}

} using namespace in;

UniqueString string_from_file (AnyString filename) {
    FILE* f = fopen_utf8(filename.c_str(), "rb");
    if (!f) {
        raise_io_error(e_OpenFailed,
            "Failed to open for reading ", filename, errno
        );
    }

    fseek(f, 0, SEEK_END);
    usize size = ftell(f);
    rewind(f);

    char* buf = SharableBuffer<char>::allocate(size);
    usize did_read = fread(buf, 1, size, f);
    if (did_read != size) {
        int errnum = errno;
        fclose(f);
        SharableBuffer<char>::deallocate(buf);
        raise_io_error(e_ReadFailed, "Failed to read from ", filename, errnum);
    }

    if (fclose(f) != 0) {
        int errnum = errno;
        SharableBuffer<char>::deallocate(buf);
        raise_io_error(e_CloseFailed, "Failed to close ", filename, errnum);
    }
    return UniqueString::UnsafeConstructOwned(buf, size);
}

void string_to_file (Str content, AnyString filename) {
    FILE* f = fopen_utf8(filename.c_str(), "wb");
    if (!f) {
        raise_io_error(e_OpenFailed,
            "Failed to open for writing ", filename, errno
        );
    }
    usize did_write = fwrite(content.data(), 1, content.size(), f);
    if (did_write != content.size()) {
        int errnum = errno;
        fclose(f);
        raise_io_error(e_WriteFailed, "Failed to write to ", filename, errnum);
    }
    if (fclose(f) != 0) {
        raise_io_error(e_CloseFailed, "Failed to close ", filename, errno);
    }
}

} // ayu
