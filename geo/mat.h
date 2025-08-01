// 2D matrixes (matrices if you speak Latin)
// Unlike GVec and GRect, these only support float and double.

#pragma once

#include "../ayu/reflection/describe.h"
#include "vec.h"

namespace geo {
using namespace uni;

template <class T, usize cols, usize rows>
struct GMat;

using Mat = GMat<float, 2, 2>;
using Mat2x3 = GMat<float, 2, 3>;
using Mat2x4 = GMat<float, 2, 4>;
using Mat3x2 = GMat<float, 3, 2>;
using Mat3 = GMat<float, 3, 3>;
using Mat3x4 = GMat<float, 3, 4>;
using Mat4x2 = GMat<float, 4, 2>;
using Mat4x3 = GMat<float, 4, 3>;
using Mat4 = GMat<float, 4, 4>;

using DMat = GMat<double, 2, 2>;
using DMat2x3 = GMat<double, 2, 3>;
using DMat2x4 = GMat<double, 2, 4>;
using DMat3x2 = GMat<double, 3, 2>;
using DMat3 = GMat<double, 3, 3>;
using DMat3x4 = GMat<double, 3, 4>;
using DMat4x2 = GMat<double, 4, 2>;
using DMat4x3 = GMat<double, 4, 3>;
using DMat4 = GMat<double, 4, 4>;

 // Stored in column-major order to match OpenGL
template <class T, usize cols, usize rows>
struct GMat {
    GVec<GVec<T, rows>, cols> e;

    constexpr GMat () { }

     // Construct from individual elements
    template <class... Args>
        requires (sizeof...(Args) == cols * rows)
    constexpr GMat (Args... args) {
         // Hope this is optimizable
        T es [cols * rows] {T(args)...};
         // You're not allowed to make a matrix with some but not all elements
         // defined
        for (usize c = 0; c < cols; c++)
        for (usize r = 0; r < rows; r++) {
            e[c][r] = es[c * rows + r];
        }
#ifndef NDEBUG
        expect(valid(*this));
#endif
    }

     // Construct from column vectors
    template <class... Args>
        requires (sizeof...(Args) == cols)
    constexpr GMat (Args... args) : e{GVec<T, rows>{args}...} {
#ifndef NDEBUG
        expect(valid(*this));
#endif
}

     // Construct from one diagonal vector
     // Mat(diag) * p == diag * p
    template <usize n>
        requires (n == cols && n == rows)
    constexpr explicit GMat (GVec<T, n> diag) {
         // TODO: Check for definedness
        for (usize i = 0; i < n; i++) {
            e[i][i] = diag[i];
        }
    }

     // Construct from one element (making a diagonal matrix)
     // Mat(scale) * p == scale * p
    template <class = void>
        requires (cols == rows)
    constexpr explicit GMat (T scale) {
         // TODO: Check for definedness
        for (usize i = 0; i < cols; i++) {
            e[i][i] = scale;
        }
    }

     // Construct the undefined matrix
    constexpr GMat (GNAN_t nan) {
        for (usize c = 0; c < cols; c++) {
            e[c] = nan;
        }
    }

     // Get a column as a vector
    constexpr GVec<float, rows>& operator [] (usize c) {
        expect(c < cols);
        return e[c];
    }
     // Same but const
    constexpr const GVec<float, rows>& operator [] (usize c) const {
        expect(c < cols);
        return e[c];
    }
     // Don't use this to check for definedness.  This only checks if each
     // element is exactly zero.
    constexpr explicit operator bool () const {
        for (usize c = 0; c < cols; c++) {
            if (!e[c]) return false;
        }
        return true;
    }
};

 // This is probably not necessary
template <class T, usize cols, usize rows>
struct TypeTraits<GMat<T, cols, rows>> {
    using Widened = GMat<Widen<T>, cols, rows>;
    static constexpr bool integral = false;
    static constexpr bool floating = false;
    static constexpr bool fractional = false;
    static constexpr bool is_signed = TypeTraits<T>::is_signed;
};

///// PROPERTIES

template <class T, usize cols, usize rows>
constexpr bool valid (const GMat<T, cols, rows>& a) {
    if constexpr (requires (T v) { defined(v); } && cols * rows > 0) {
        bool is_defined = defined(a[0][0]);
        for (usize c = 0; c < cols; c++)
        for (usize r = 0; r < rows; r++) {
            if (defined(a[c][r]) != is_defined) {
                return false;
            }
        }
    }
    return true;
}

template <class T, usize cols, usize rows>
constexpr bool defined (const GMat<T, cols, rows>& a) {
#ifndef NDEBUG
    expect(valid(a));
#endif
    if constexpr (cols * rows > 0) {
        return defined(a[0][0]);
    }
    else return true;
}

template <class T, usize n>
constexpr bool is_diagonal (const GMat<T, n, n>& a) {
    for (usize c = 0; c < n; c++)
    for (usize r = 0; r < n; r++) {
        if (r != c && a[c][r]) return false;
    }
    return true;
}
template <class T, usize n>
constexpr GVec<T, n> diagonal (const GMat<T, n, n>& a) {
    GVec<T, n> ret;
    for (usize i = 0; i < n; i++) {
        ret[i] = a[i][i];
    }
    return ret;
}

 // More properties (like determinant) NYI

///// MODIFIERS

#define GMAT_UNARY_OP(op) \
template <class T, usize cols, usize rows> \
constexpr auto operator op (const GMat<T, cols, rows>& a) { \
    GMat<decltype(op T()), cols, rows> ret; \
    for (usize c = 0; c < cols; c++) \
    for (usize r = 0; r < rows; r++) { \
        ret[c][r] = op a[c][r]; \
    } \
    return ret; \
}
GMAT_UNARY_OP(+)
GMAT_UNARY_OP(-)
#undef GMAT_UNARY_OP

template <class T, usize cols, usize rows>
constexpr GMat<T, cols, rows> transpose (const GMat<T, rows, cols>& a) {
    GMat<T, cols, rows> ret;
    for (usize c = 0; c < cols; c++)
    for (usize r = 0; r < rows; r++) {
        ret[c][r] = a[r][c];
    }
    return ret;
}

 // Inverse NYI

///// RELATIONSHIPS

template <class A, class B, usize cols, usize rows>
constexpr bool operator == (
    const GMat<A, cols, rows>& a, const GMat<B, cols, rows>& b
) {
    for (usize c = 0; c < cols; c++) {
        if (a[c] != b[c]) return false;
    }
    return true;
}
template <class A, class B, usize cols, usize rows>
constexpr bool operator != (
    const GMat<A, cols, rows>& a, const GMat<B, cols, rows>& b
) {
    return !(a == b);
}

///// COMBINERS
 // Matrix addition
template <class A, class B, usize cols, usize rows>
constexpr auto operator + (
    const GMat<A, cols, rows>& a, const GMat<B, cols, rows>& b
) {
    GMat<decltype(A() + B()), cols, rows> ret;
    for (usize c = 0; c < cols; c++)
    for (usize r = 0; r < rows; r++) {
        ret[c][r] = a[c][r] + b[c][r];
    }
    return ret;
}

template <class A, class B, usize cols, usize rows>
constexpr auto operator - (
    const GMat<A, cols, rows>& a, const GMat<B, cols, rows>& b
) {
    GMat<decltype(A() - B()), cols, rows> ret;
    for (usize c = 0; c < cols; c++)
    for (usize r = 0; r < rows; r++) {
        ret[c][r] = a[c][r] - b[c][r];
    }
    return ret;
}

 // Matrix multiplication
template <class A, class B, usize cols, usize mid, usize rows>
constexpr auto operator * (
    const GMat<A, mid, rows>& a, const GMat<B, cols, mid>& b
) {
    GMat<decltype(A() * B()), cols, rows> ret;
    for (usize c = 0; c < cols; c++)
    for (usize r = 0; r < rows; r++)
    for (usize m = 0; m < mid; m++) {
        ret[c][r] += a[m][r] * b[c][m];
    }
    return ret;
}

 // Scale by a scalar
template <class A, class B, usize cols, usize rows>
constexpr auto operator * (const GMat<A, cols, rows>& a, B b) {
    GMat<decltype(A() * B()), cols, rows> ret;
    for (usize c = 0; c < cols; c++)
    for (usize r = 0; r < rows; r++) {
        ret[c][r] = a[c][r] * b;
    }
    return ret;
}

template <class A, class B, usize cols, usize rows>
constexpr auto operator * (A a, const GMat<B, cols, rows>& b) {
    GMat<decltype(A() * B()), cols, rows> ret;
    for (usize c = 0; c < cols; c++)
    for (usize r = 0; r < rows; r++) {
        ret[c][r] = a * b[c][r];
    }
    return ret;
}

template <class A, class B, usize cols, usize rows>
constexpr auto operator / (const GMat<A, cols, rows>& a, B b) {
    GMat<decltype(A() / B()), cols, rows> ret;
    for (usize c = 0; c < cols; c++)
    for (usize r = 0; r < rows; r++) {
        ret[c][r] = a[c][r] / b;
    }
    return ret;
}

 // Multiply vector by matrix to get vector
template <class A, class B, usize cols, usize rows>
constexpr auto operator * (const GMat<A, cols, rows>& a, const GVec<B, rows>& b) {
    GVec<decltype(A() * B()), cols> ret;
    for (usize c = 0; c < cols; c++)
    for (usize r = 0; r < rows; r++) {
        ret[r] += a[c][r] * b[c];
    }
    return ret;
}

 // Assignment operators.  Not bothering to write these in in-place style.
#define GMAT_ASSIGN_OP(opeq, op) \
template <class A, class B, usize cols, usize rows> \
constexpr GMat<A, cols, rows> operator opeq (GMat<A, cols, rows>& a, const B& b) { \
    return (a = a op b); \
}
GMAT_ASSIGN_OP(+=, +)
GMAT_ASSIGN_OP(-=, -)
GMAT_ASSIGN_OP(*=, *)
GMAT_ASSIGN_OP(/=, /)
#undef GMAT_ASSIGN_OP

template <class A, class B, usize cols, usize rows>
constexpr GMat<A, cols+1, rows> add_column (
    const GMat<A, cols, rows>& m,
    const GVec<B, rows>& v
) {
    GMat<A, cols+1, rows> ret;
    for (usize c = 0; c < cols; c++)
    for (usize r = 0; r < rows; r++) {
        ret[c][r] = m[c][r];
    }
    for (usize r = 0; r < rows; r++) {
        ret[cols][r] = v[r];
    }
    return ret;
}

template <class A, class B, usize cols, usize rows>
constexpr GMat<A, cols, rows+1> add_row (
    const GMat<A, cols, rows>& m,
    const GVec<B, cols>& v
) {
    GMat<A, cols, rows+1> ret;
    for (usize c = 0; c < cols; c++) {
        for (usize r = 0; r < rows; r++) {
            ret[c][r] = m[c][r];
        }
        ret[c][rows] = v[c];
    }
    return ret;
}

} // namespace geo

AYU_DESCRIBE_TEMPLATE(
    AYU_DESCRIBE_TEMPLATE_PARAMS(class T, uni::usize cols, uni::usize rows),
    AYU_DESCRIBE_TEMPLATE_TYPE(geo::GMat<T, cols, rows>),
    []{
        using namespace uni;
        auto computed = []{
            return desc::computed_name([]()->AnyString{
                return cat("geo::GMat<",
                    ayu::Type::For<T>().name(), ", ",
                    std::to_string(cols), ", ",
                    std::to_string(rows), '>'
                );
            });
        };
        if constexpr (std::is_same_v<T, float>) {
            if constexpr (cols == 2) {
                if constexpr (rows == 2) return desc::name("geo::Mat");
                else if constexpr (rows == 3) return desc::name("geo::Mat2x3");
                else if constexpr (rows == 4) return desc::name("geo::Mat2x4");
                else return computed();
            }
            else if constexpr (cols == 3) {
                if constexpr (rows == 2) return desc::name("geo::Mat3x2");
                else if constexpr (rows == 3) return desc::name("geo::Mat3");
                else if constexpr (rows == 4) return desc::name("geo::Mat3x4");
                else return computed();
            }
            else if constexpr (cols == 4) {
                if constexpr (rows == 2) return desc::name("geo::Mat4x2");
                else if constexpr (rows == 3) return desc::name("geo::Mat4x3");
                else if constexpr (rows == 4) return desc::name("geo::Mat4");
                else return computed();
            }
            else return computed();
        }
        else if constexpr (std::is_same_v<T, double>) {
            if constexpr (cols == 2) {
                if constexpr (rows == 2) return desc::name("geo::DMat");
                else if constexpr (rows == 3) return desc::name("geo::DMat2x3");
                else if constexpr (rows == 4) return desc::name("geo::DMat2x4");
                else return computed();
            }
            else if constexpr (cols == 3) {
                if constexpr (rows == 2) return desc::name("geo::DMat3x2");
                else if constexpr (rows == 3) return desc::name("geo::DMat3");
                else if constexpr (rows == 4) return desc::name("geo::DMat3x4");
                else return computed();
            }
            else if constexpr (cols == 4) {
                if constexpr (rows == 2) return desc::name("geo::DMat4x2");
                else if constexpr (rows == 3) return desc::name("geo::DMat4x3");
                else if constexpr (rows == 4) return desc::name("geo::DMat4");
                else return computed();
            }
            else return computed();
        }
        else return computed();
    }(),
    []{
        using namespace geo;
        if constexpr (cols == 2 && rows == 2) {
             // Have some extra names for 2x2 matrices
            return desc::values(
                desc::value(double(GNAN), GMat<T, 2, 2>(GNAN)),
                desc::value(0, GMat<T, 2, 2>()),
                desc::value(1, GMat<T, 2, 2>(1)),
                desc::value("flipx", GMat<T, 2, 2>(-1, 0, 0, 1)),
                desc::value("flipy", GMat<T, 2, 2>(1, 0, 0, -1)),
                 // Rotations assume y points upward
                desc::value("rotcw", GMat<T, 2, 2>(0, -1, 1, 0)),
                desc::value("rotccw", GMat<T, 2, 2>(0, 1, -1, 0)),
                desc::value("rot180", GMat<T, 2, 2>(-1, 0, 0, -1))
            );
        }
        else {
            return desc::values(
                desc::value(double(GNAN), GMat<T, cols, rows>(GNAN)),
                desc::value(0, &GMat<T, cols, rows>()),
                desc::value(1, &GMat<T, cols, rows>(1))
            );
        }
    }(),
     // Serialize columns individually so we get a 2-dimensional representation.
    desc::length(desc::template constant<uni::usize>(cols)),
    desc::contiguous_elems([](geo::GMat<T, cols, rows>& v){
        return ayu::AnyPtr(&v[0]);
    })
)
