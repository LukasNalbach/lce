/**
 * @file libsais40_types.hpp
 * @brief the 40-bit integer types the libsais40 port stores its arrays in
 *
 * libsais keeps two type layers apart: sa_sint_t is what the arrays are made of, fast_sint_t is what
 * a computation runs in. Only the first one has to shrink, so these types carry no arithmetic of
 * their own -- they convert to int64_t/uint64_t implicitly, every expression is evaluated at 64 bits,
 * and the result is truncated back to 40 on assignment. That is the invariant libsais already relies
 * on, and it keeps the ported code textually identical to the original.
 *
 * A write touches its five bytes and no others, so two threads writing neighbouring entries do not
 * race. That rules out the faster "load eight, mask, store eight" form, which would carry three bytes
 * of the next entry through the store.
 */

#pragma once

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>

static_assert(std::endian::native == std::endian::little,
    "the 40-bit types store the low five bytes of a value, which assumes a little-endian layout");

#define SA_INT40_MAX ((int64_t { 1 } << 39) - 1)
#define SA_INT40_MIN (-(int64_t { 1 } << 39))

/* The largest length libsais40 accepts. Positions are stored in 40 bits, of which the algorithm
   claims the top two for its own markers (the sign bit, and SUFFIX_GROUP_BIT one below it). */
#define SA_INT40_MAX_N (int64_t { 1 } << 38)

/* Assembled as a word and a byte rather than by a five-byte memcpy: the latter has no register to put
   the value together in and round-trips through the stack (see uint40_t in misc/utils.hpp). */
inline uint64_t sa40_load(const uint8_t* b)
{
    uint32_t low;
    std::memcpy(&low, b, 4);
    return uint64_t(low) | (uint64_t(b[4]) << 32);
}

inline void sa40_store(uint8_t* b, uint64_t v)
{
    uint32_t low = uint32_t(v);
    std::memcpy(b, &low, 4);
    b[4] = uint8_t(v >> 32);
}

/* Every compound operator evaluates at 64 bits and truncates back; the plain binary operators are
   deliberately absent, so that an expression converts to int64_t and is computed there. */
#define SA_INT40_OPERATORS(type, value_t)                                                        \
    type& operator+=(value_t v) { return assign(value_t(*this) + v); }                           \
    type& operator-=(value_t v) { return assign(value_t(*this) - v); }                           \
    type& operator*=(value_t v) { return assign(value_t(*this) * v); }                           \
    type& operator/=(value_t v) { return assign(value_t(*this) / v); }                           \
    type& operator%=(value_t v) { return assign(value_t(*this) % v); }                           \
    type& operator|=(value_t v) { return assign(value_t(*this) | v); }                           \
    type& operator&=(value_t v) { return assign(value_t(*this) & v); }                           \
    type& operator^=(value_t v) { return assign(value_t(*this) ^ v); }                            \
    type& operator<<=(int v) { return assign(value_t(*this) << v); }                             \
    type& operator>>=(int v) { return assign(value_t(*this) >> v); }                             \
    type& operator++() { return *this += 1; }                                                    \
    type& operator--() { return *this -= 1; }                                                    \
    type operator++(int) { type v = *this; *this += 1; return v; }                               \
    type operator--(int) { type v = *this; *this -= 1; return v; }                               \
    type& assign(value_t v) { sa40_store(bytes, uint64_t(v)); return *this; }

/** @brief a signed 40-bit integer -- the element type of libsais40's suffix array and buckets */
struct sa_int40_t {
    uint8_t bytes[5];

    sa_int40_t() = default;
    sa_int40_t(int64_t v) { sa40_store(bytes, uint64_t(v)); }
    // the two widths meet wherever an algorithm keeps positions unsigned and markers signed

    // sign-extended from bit 39, so that libsais' negative markers compare as negative
    operator int64_t() const { return (int64_t(sa40_load(bytes)) << 24) >> 24; }

    SA_INT40_OPERATORS(sa_int40_t, int64_t)
};

/** @brief an unsigned 40-bit integer -- how libsais40 reads an entry's raw bit pattern */
struct sa_uint40_t {
    uint8_t bytes[5];

    sa_uint40_t() = default;
    sa_uint40_t(uint64_t v) { sa40_store(bytes, v); }

    operator uint64_t() const { return sa40_load(bytes); }

    SA_INT40_OPERATORS(sa_uint40_t, uint64_t)
};

#undef SA_INT40_OPERATORS

static_assert(sizeof(sa_int40_t) == 5 && alignof(sa_int40_t) == 1);
static_assert(sizeof(sa_uint40_t) == 5 && alignof(sa_uint40_t) == 1);

/* libsais carves its bucket arrays out of the free space behind the suffix array and aligns that
   pointer by a whole number of entries' worth of bytes. At eight bytes per entry, aligning the
   address keeps the pointer on the array's entry grid as a side effect; at five bytes it does not,
   and off the grid the pointer difference by which libsais measures the remaining free space
   (buckets - &SA[n]) is no longer a whole number of entries. So the index is aligned instead, which
   is what the original means by it: the same spacing, and the grid is kept by construction.

   The reserved entries below the aligned position are what makes this fit -- rounding up moves the
   start by less than `alignment` entries, and that many were left free for it. */
inline int64_t sa40_align_index(int64_t index, int64_t alignment)
{
    return (index + alignment - 1) & -alignment;
}
