/**
 * part of LukasNalbach/lce
 *
 * MIT License
 *
 * Copyright (c) Lukas Nalbach
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#pragma once

#include <algorithm>
#include <atomic>
#include <bit>
#include <cstdint>
#include <cstring>
#include <vector>

#include "util/memory.hpp"

namespace lce::util {

struct bit_aligned_view {
    const uint64_t* data = nullptr;
    uint64_t mask = 0;
    uint8_t width = 0;

    inline uint64_t operator[](uint64_t i) const
    {
        const uint64_t bit = i * width;
        uint64_t block;
        std::memcpy(&block, reinterpret_cast<const uint8_t*>(data) + (bit >> 3), 8);
        return (block >> (bit & 7)) & mask;
    }
};

class bit_aligned_vector {
public:
    bit_aligned_vector() = default;

    bit_aligned_vector(uint64_t size, uint64_t max_value)
        : m_width(std::max<uint8_t>(1, std::bit_width(max_value)))
        , m_mask((uint64_t(1) << m_width) - 1)
        , m_size(size)
    {
        lce::util::no_init_resize(m_data, (size * m_width) / 64 + 4);
        lce::util::advise_huge_pages(m_data.data(), m_data.size() * 8);
    }

    void reset(uint64_t capacity, uint64_t max_value)
    {
        m_width = std::max<uint8_t>(1, std::bit_width(max_value));
        m_mask = (uint64_t(1) << m_width) - 1;
        m_size = 0;
        m_word = 0;
        m_acc = 0;
        m_used = 0;
        lce::util::no_init_resize(m_data, (capacity * m_width) / 64 + 4);
        lce::util::advise_huge_pages(m_data.data(), m_data.size() * 8);
    }

    inline void push_back(uint64_t value)
    {
        const uint64_t masked = value & m_mask;
        m_acc |= masked << m_used;

        if (m_used + m_width >= 64) {
            m_data[m_word++] = m_acc;
            m_acc = m_used == 0 ? 0 : (masked >> (64 - m_used));
            m_used = m_used + m_width - 64;
        } else {
            m_used += m_width;
        }

        m_size++;
    }

    void finish()
    {
        if (m_used != 0) m_data[m_word] = m_acc;
        m_data.resize((m_size * m_width) / 64 + 4);
        m_data.shrink_to_fit();
        lce::util::advise_huge_pages(m_data.data(), m_data.size() * 8);
    }

    inline uint64_t operator[](uint64_t i) const
    {
        const uint64_t bit = i * m_width;
        uint64_t block;
        std::memcpy(&block, reinterpret_cast<const uint8_t*>(m_data.data()) + (bit >> 3), 8);
        return (block >> (bit & 7)) & m_mask;
    }

    inline void set(uint64_t i, uint64_t value)
    {
        const uint64_t bit = i * m_width;
        uint8_t* at = reinterpret_cast<uint8_t*>(m_data.data()) + (bit >> 3);
        uint64_t block;
        std::memcpy(&block, at, 8);
        block = (block & ~(m_mask << (bit & 7))) | ((value & m_mask) << (bit & 7));
        std::memcpy(at, &block, 8);
    }

    template <typename fnc_t>
    void fill(uint64_t from, uint64_t to, fnc_t value_at)
    {
        uint64_t i = from;
        while (i < to && ((i * m_width) & 63) != 0) { set_parallel(i, value_at(i)); i++; }
        uint64_t word = (i * m_width) / 64;
        uint64_t acc = 0;
        uint8_t used = 0;

        for (; i < to; i++) {
            const uint64_t value = value_at(i) & m_mask;
            acc |= value << used;

            if (used + m_width >= 64) {
                m_data[word++] = acc;
                acc = used == 0 ? 0 : (value >> (64 - used));
                used = used + m_width - 64;
            } else {
                used += m_width;
            }
        }

        if (used != 0) {
            const uint64_t mask = (uint64_t(1) << used) - 1;
            store_bits(m_data[word], mask, acc & mask);
        }
    }

    void set_parallel(uint64_t i, uint64_t value)
    {
        const uint64_t bit = i * m_width;
        const uint64_t word = bit >> 6;
        const uint64_t offset = bit & 63;
        const uint64_t val = value & m_mask;
        const uint64_t bits_lo = std::min<uint64_t>(64 - offset, m_width);
        const uint64_t mask_lo = (bits_lo == 64 ? ~uint64_t(0) : ((uint64_t(1) << bits_lo) - 1)) << offset;
        store_bits(m_data[word], mask_lo, (val << offset) & mask_lo);

        if (bits_lo < m_width) {
            const uint64_t mask_hi = (uint64_t(1) << (m_width - bits_lo)) - 1;
            store_bits(m_data[word + 1], mask_hi, (val >> bits_lo) & mask_hi);
        }
    }

    void clear()
    {
        m_data.clear();
        m_size = 0;
        m_word = 0;
        m_acc = 0;
        m_used = 0;
    }

    void shrink_to_fit() { m_data.shrink_to_fit(); }

    bit_aligned_view view() const
    {
        return bit_aligned_view { .data = m_data.data(), .mask = m_mask, .width = m_width };
    }

    bool empty() const { return m_size == 0; }

    uint64_t size() const { return m_size; }
    uint8_t width() const { return m_width; }
    uint64_t size_in_bytes() const { return m_data.size() * 8; }

private:
    static void store_bits(uint64_t& word, uint64_t mask, uint64_t value)
    {
        std::atomic_ref<uint64_t> ref(word);
        uint64_t old = ref.load(std::memory_order_relaxed);
        while (!ref.compare_exchange_weak(old, (old & ~mask) | value, std::memory_order_relaxed)) { }
    }

    std::vector<uint64_t> m_data;
    uint64_t m_word = 0;
    uint64_t m_acc = 0;
    uint8_t m_used = 0;
    uint8_t m_width = 0;
    uint64_t m_mask = 0;
    uint64_t m_size = 0;
};

}
