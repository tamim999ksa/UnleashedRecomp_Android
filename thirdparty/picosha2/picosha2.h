/*
The MIT License (MIT)

Copyright (C) 2017 okdshin

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/
#ifndef PICOSHA2_H
#define PICOSHA2_H

// picosha2:20140213

#ifndef PICOSHA2_BUFFER_SIZE_FOR_INPUT_ITERATOR
#define PICOSHA2_BUFFER_SIZE_FOR_INPUT_ITERATOR     1048576  //=1024*1024: default is 1MB memory
#endif

#include <algorithm>
#include <cassert>
#include <iterator>
#include <sstream>
#include <vector>
#include <fstream>
#include <array>

namespace picosha2 {
namespace detail {
    typedef unsigned long word_t;
    typedef unsigned char byte_t;
}
using detail::word_t;
using detail::byte_t;

static const size_t k_digest_size = 32;

namespace detail {
inline byte_t mask_8bit(byte_t x) { return x & 0xff; }

inline word_t mask_32bit(word_t x) { return x & 0xffffffff; }

const word_t add_constant[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1,
    0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
    0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
    0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
    0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
    0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

const word_t initial_message_digest[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372,
                                          0xa54ff53a, 0x510e527f, 0x9b05688c,
                                          0x1f83d9ab, 0x5be0cd19};

inline word_t ch(word_t x, word_t y, word_t z) { return (x & y) ^ ((~x) & z); }

inline word_t maj(word_t x, word_t y, word_t z) {
  return (x & y) ^ (x & z) ^ (y & z);
}

inline word_t rotr(word_t x, std::size_t n) {
  assert(n < 32);
  return mask_32bit((x >> n) | (x << (32 - n)));
}

inline word_t bsig0(word_t x) { return rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22); }

inline word_t bsig1(word_t x) { return rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25); }

inline word_t shr(word_t x, std::size_t n) {
  assert(n < 32);
  return x >> n;
}

inline word_t ssig0(word_t x) { return rotr(x, 7) ^ rotr(x, 18) ^ shr(x, 3); }

inline word_t ssig1(word_t x) { return rotr(x, 17) ^ rotr(x, 19) ^ shr(x, 10); }

template <typename RaIter1, typename RaIter2>
void hash256_block(RaIter1 message_block, RaIter2 current_hash) {
  word_t w[64];
  word_t a, b, c, d, e, f, g, h;
  word_t t1, t2;
  std::copy(message_block, message_block + 16, w);
  for (size_t i = 16; i < 64; ++i) {
    w[i] = mask_32bit(ssig1(w[i - 2]) + w[i - 7] + ssig0(w[i - 15]) +
                      w[i - 16]);
  }
  a = current_hash[0];
  b = current_hash[1];
  c = current_hash[2];
  d = current_hash[3];
  e = current_hash[4];
  f = current_hash[5];
  g = current_hash[6];
  h = current_hash[7];
  for (size_t i = 0; i < 64; ++i) {
    t1 = h + bsig1(e) + ch(e, f, g) + add_constant[i] + w[i];
    t2 = bsig0(a) + maj(a, b, c);
    h = g;
    g = f;
    f = e;
    e = mask_32bit(d + t1);
    d = c;
    c = b;
    b = a;
    a = mask_32bit(t1 + t2);
  }
  current_hash[0] = mask_32bit(current_hash[0] + a);
  current_hash[1] = mask_32bit(current_hash[1] + b);
  current_hash[2] = mask_32bit(current_hash[2] + c);
  current_hash[3] = mask_32bit(current_hash[3] + d);
  current_hash[4] = mask_32bit(current_hash[4] + e);
  current_hash[5] = mask_32bit(current_hash[5] + f);
  current_hash[6] = mask_32bit(current_hash[6] + g);
  current_hash[7] = mask_32bit(current_hash[7] + h);
}

}  // namespace detail

template <typename InIter>
void output_hex(InIter first, InIter last, std::ostream& os) {
  os.setf(std::ios::hex, std::ios::basefield);
  while (first != last) {
    os.width(2);
    os.fill('0');
    os << static_cast<unsigned int>(*first);
    ++first;
  }
  os.setf(std::ios::dec, std::ios::basefield);
}

template <typename InIter>
void bytes_to_hex_string(InIter first, InIter last, std::string& hex_str) {
  std::ostringstream oss;
  output_hex(first, last, oss);
  hex_str = oss.str();
}

template <typename InContainer>
void bytes_to_hex_string(const InContainer& bytes, std::string& hex_str) {
  bytes_to_hex_string(bytes.begin(), bytes.end(), hex_str);
}

template <typename InIter>
std::string bytes_to_hex_string(InIter first, InIter last) {
  std::string hex_str;
  bytes_to_hex_string(first, last, hex_str);
  return hex_str;
}

template <typename InContainer>
std::string bytes_to_hex_string(const InContainer& bytes) {
  std::string hex_str;
  bytes_to_hex_string(bytes, hex_str);
  return hex_str;
}

class hash256_one_by_one {
 public:
  hash256_one_by_one() { init(); }

  void init() {
    buffer_.clear();
    std::copy(detail::initial_message_digest,
              detail::initial_message_digest + 8, state_);
  }

  template <typename RaIter>
  void process(RaIter first, RaIter last) {
    add_to_data_length(static_cast<word_t>(std::distance(first, last)));
    std::copy(first, last, std::back_inserter(buffer_));
    size_t i = 0;
    for (; i + 64 <= buffer_.size(); i += 64) {
      detail::word_t block[16];
      for (size_t j = 0; j < 16; ++j) {
        block[j] = (static_cast<detail::word_t>(buffer_[i + j * 4 + 0]) << 24) |
                   (static_cast<detail::word_t>(buffer_[i + j * 4 + 1]) << 16) |
                   (static_cast<detail::word_t>(buffer_[i + j * 4 + 2]) << 8) |
                   (static_cast<detail::word_t>(buffer_[i + j * 4 + 3]));
      }
      detail::hash256_block(block, state_);
    }
    buffer_.erase(buffer_.begin(), buffer_.begin() + i);
  }

  void finish() {
    byte_t temp[64];
    std::fill(temp, temp + 64, 0);
    std::size_t remains = buffer_.size();
    std::copy(buffer_.begin(), buffer_.end(), temp);
    temp[remains] = 0x80;

    if (remains > 55) {
      std::fill(temp + remains + 1, temp + 64, 0);
      detail::word_t block[16];
      for (size_t j = 0; j < 16; ++j) {
        block[j] = (static_cast<detail::word_t>(temp[j * 4 + 0]) << 24) |
                   (static_cast<detail::word_t>(temp[j * 4 + 1]) << 16) |
                   (static_cast<detail::word_t>(temp[j * 4 + 2]) << 8) |
                   (static_cast<detail::word_t>(temp[j * 4 + 3]));
      }
      detail::hash256_block(block, state_);
      std::fill(temp, temp + 64, 0);
    } else {
      std::fill(temp + remains + 1, temp + 64, 0);
    }

    unsigned long long bit_length = data_length_ * 8;
    for (int i = 0; i < 8; ++i) {
      temp[56 + i] = static_cast<byte_t>((bit_length >> (56 - i * 8)) & 0xff);
    }
    detail::word_t block[16];
    for (size_t j = 0; j < 16; ++j) {
      block[j] = (static_cast<detail::word_t>(temp[j * 4 + 0]) << 24) |
                 (static_cast<detail::word_t>(temp[j * 4 + 1]) << 16) |
                 (static_cast<detail::word_t>(temp[j * 4 + 2]) << 8) |
                 (static_cast<detail::word_t>(temp[j * 4 + 3]));
    }
    detail::hash256_block(block, state_);
  }

  template <typename OutIter>
  void get_hash_bytes(OutIter first, OutIter last) const {
    for (const auto& x : state_) {
      *first++ = (x >> 24) & 0xff;
      *first++ = (x >> 16) & 0xff;
      *first++ = (x >> 8) & 0xff;
      *first++ = x & 0xff;
    }
  }

 private:
  void add_to_data_length(word_t n) {
    double carry = 0;
    using namespace std;
    if (data_length_ >= numeric_limits<unsigned long long>::max() - n) {
      carry = 1;
    }
    data_length_ += n;
    data_length_digits_ += carry;
  }

  std::vector<byte_t> buffer_;
  unsigned long long data_length_ = 0;
  unsigned long long data_length_digits_ = 0;
  detail::word_t state_[8];
};

template <typename RaIter, typename OutIter>
void hash256(RaIter first, RaIter last, OutIter first2, OutIter last2) {
  hash256_one_by_one hasher;
  hasher.process(first, last);
  hasher.finish();
  hasher.get_hash_bytes(first2, last2);
}

template <typename RaIter, typename OutContainer>
void hash256(RaIter first, RaIter last, OutContainer& dst) {
  hash256(first, last, dst.begin(), dst.end());
}

template <typename InContainer, typename OutContainer>
void hash256(const InContainer& src, OutContainer& dst) {
  hash256(src.begin(), src.end(), dst.begin(), dst.end());
}

template <typename InContainer>
void hash256(const InContainer& src, std::vector<byte_t>& dst) {
  if (dst.empty()) {
    dst.resize(k_digest_size);
  }
  hash256(src.begin(), src.end(), dst.begin(), dst.end());
}

template <typename OutIter>
void hash256(std::ifstream& f, OutIter first, OutIter last) {
  hash256_one_by_one hasher;
  std::vector<char> buffer(PICOSHA2_BUFFER_SIZE_FOR_INPUT_ITERATOR);
  while (f.read(&buffer[0], buffer.size())) {
    hasher.process(&buffer[0], &buffer[0] + f.gcount());
  }
  hasher.process(&buffer[0], &buffer[0] + f.gcount());
  hasher.finish();
  hasher.get_hash_bytes(first, last);
}

template <typename OutContainer>
void hash256(std::ifstream& f, OutContainer& dst) {
  hash256(f, dst.begin(), dst.end());
}

}  // namespace picosha2
#endif // PICOSHA2_H
