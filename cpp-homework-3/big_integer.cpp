/* Nikolai Kholiavin, M3138 */

#include <cstring>
#include <stdexcept>
#include <algorithm>
#include <cassert>
#include <iostream>
#include <sstream>

#include "big_integer.h"

/***
 * Shared buffer (copy-on-write optimization) implementation
 ***/

big_integer::shared_buffer * big_integer::shared_buffer::allocate_buffer(size_t capacity)
{
  // make space for possible carry and sign place
  capacity += 2;

  shared_buffer *self = static_cast<shared_buffer *>(operator new(sizeof(shared_buffer) +
    sizeof(place_t) * capacity));
  self->ref_count = 1;
  self->capacity = capacity;
  return self;
}

big_integer::shared_buffer * big_integer::shared_buffer::add_ref(shared_buffer *self)
{
  self->ref_count++;
  return self;
}

void big_integer::shared_buffer::release(shared_buffer *self)
{
  if (self == nullptr)
    return;
  if (--self->ref_count == 0)
    operator delete(self);
}

/***
 * Buffer (small-object optimization) implementation
 ***/

void big_integer::buffer::allocate(size_t new_size, place_t default_val)
{
  assert(new_size > 1);
  dynamic_data = shared_buffer::allocate_buffer(new_size);
  std::fill(dynamic_data->data, dynamic_data->data + new_size, default_val);
}

void big_integer::buffer::allocate(size_t new_size, const place_t *old_data, place_t default_val)
{
  assert(new_size > 1);
  dynamic_data = shared_buffer::allocate_buffer(new_size);
  std::copy(old_data, old_data + std::min(size_, new_size), dynamic_data->data);
  if (new_size > size_)
    std::fill(dynamic_data->data + size_, dynamic_data->data + new_size, default_val);
}

void big_integer::buffer::unshare_resize(size_t new_size, place_t default_val)
{
  new_size = new_size == -1 ? size_ : new_size;
  assert(new_size != size_ || (size_ > 1 && dynamic_data->ref_count > 1));

  place_t buf, *old_data;
  shared_buffer *to_release = nullptr;
  if (size_ == 1)
  {
    buf = static_data;
    old_data = &buf;
  }
  else
  {
    old_data = dynamic_data->data;
    to_release = dynamic_data;
  }

  if (new_size == 1)
    static_data = *old_data;
  else
    allocate(std::max(size_ * 3 / 2, new_size), old_data, default_val);
  shared_buffer::release(to_release);
  size_ = new_size;
}

big_integer::buffer::buffer(size_t size, place_t default_val)
{
  assert(size > 0);
  if (size == 1)
    static_data = default_val;
  else
    allocate(size, default_val);
  size_ = size;
}

big_integer::buffer::buffer(const std::vector<place_t> &vec) : size_(vec.size())
{
  assert(!vec.empty());
  if (vec.size() == 1)
    static_data = vec[0];
  else
    allocate(vec.size(), vec.data());
}

big_integer::buffer::buffer(const buffer &other) : size_(other.size_)
{
  if (size_ == 1)
    static_data = other.static_data;
  else
    dynamic_data = shared_buffer::add_ref(other.dynamic_data);
}

big_integer::buffer & big_integer::buffer::operator=(const buffer &other)
{
  if (this == &other)
    return *this;
  buffer swapper(other);
  swap(swapper);
  return *this;
}
void big_integer::buffer::swap(buffer &other)
{
  if (this == &other)
    return;

  if (size_ == 1)
  {
    if (other.size_ == 1)
      std::swap(static_data, other.static_data);
    else
    {
      shared_buffer *tmp = other.dynamic_data;
      other.static_data = static_data;
      dynamic_data = tmp;
    }
  }
  else
    if (other.size_ == 1)
    {
      place_t tmp = other.static_data;
      other.dynamic_data = dynamic_data;
      static_data = tmp;
    }
    else
      std::swap(dynamic_data, other.dynamic_data);
  std::swap(size_, other.size_);
}

size_t big_integer::buffer::size() const
{
  return size_;
}
void big_integer::buffer::inflate(size_t new_size, place_t default_val)
{
  assert(new_size >= size_);
  if (new_size == size_)
    return;
  if (size_ == 1 || dynamic_data->ref_count > 1 || new_size > dynamic_data->capacity)
    unshare_resize(new_size, default_val);
  else
  {
    std::fill(dynamic_data->data + size_, dynamic_data->data + new_size, default_val);
    size_ = new_size;
  }
}

big_integer::place_t big_integer::buffer::back() const
{
  return size_ == 1 ? static_data : dynamic_data->data[size_ - 1];
}
big_integer::place_t & big_integer::buffer::back()
{
  if (size_ == 1)
    return static_data;
  if (dynamic_data->ref_count != 1)
    unshare_resize();
  return dynamic_data->data[size_ - 1];
}

void big_integer::buffer::push_back(place_t val)
{
  inflate(size_ + 1, val);
}
void big_integer::buffer::pop_back()
{
  assert(size_ > 1);
  if (size_ == 2)
    unshare_resize(1);
  else
    size_--;
}

big_integer::buffer::operator const big_integer::place_t *() const
{
  return data();
}
big_integer::buffer::operator big_integer::place_t *()
{
  return data();
}
const big_integer::place_t * big_integer::buffer::data() const
{
  return size_ == 1 ? &static_data : dynamic_data->data;
}
big_integer::place_t * big_integer::buffer::data()
{
  if (size_ > 1 && dynamic_data->ref_count > 1)
    unshare_resize();
  return size_ == 1 ? &static_data : dynamic_data->data;
}

big_integer::place_t * big_integer::buffer::begin()
{
  return data();
}
const big_integer::place_t * big_integer::buffer::begin() const
{
  return data();
}
big_integer::place_t * big_integer::buffer::end()
{
  return begin() + size_;
}
const big_integer::place_t * big_integer::buffer::end() const
{
  return begin() + size_;
}

bool big_integer::buffer::operator==(const buffer &other) const
{
  if (size_ != other.size_)
    return false;
  if (size_ == 1)
    return static_data == other.static_data;
  return std::equal(dynamic_data->data, dynamic_data->data + size_,
    other.dynamic_data->data);
}
bool big_integer::buffer::operator!=(const buffer &other) const
{
  return !operator==(other);
}

big_integer::buffer::~buffer()
{
  if (size_ > 1)
    shared_buffer::release(dynamic_data);
}

/***
 * Big integer implementation
 ***/

/* Basis for big integer functions */
template<typename type>
  bool sign_bit(type val) { return val >> (std::numeric_limits<type>::digits - 1); }

bool big_integer::sign_bit() const { return ::sign_bit(data.back()); }

template<typename type>
static type default_place(bool bit) { return bit ? std::numeric_limits<type>::max() : 0; }

big_integer::place_t big_integer::default_place() const
{
  return ::default_place<place_t>(sign_bit());
}

big_integer::place_t big_integer::get_or_default(int at) const
{
  if (at < 0) return 0;
  if ((size_t)at >= data.size()) return default_place();
  return data[at];
}

big_integer & big_integer::shrink()
{
  // make sure invariant holds
  while (data.size() > 1 && data.back() == default_place() &&
         sign_bit() == ::sign_bit(data[data.size() - 2]))
    // while last place is default and no change in sign
    data.pop_back();
  return *this;
}

void big_integer::resize(size_t new_size) { data.inflate(new_size, default_place()); }
size_t big_integer::size() const { return data.size(); }

size_t big_integer::unsigned_size() const
{
  if (sign_bit()) return 0;
  return std::max(size_t{1}, data.size() - (data.back() == 0));
}

big_integer & big_integer::correct_sign_bit(bool expected_sign_bit, std::optional<place_t> carry)
{
  // invariant does not hold for now
  // zero is always accepted, but could be non-shrinked
  try
  {
    if (carry.has_value())
      data.push_back(carry.value());
    if (sign_bit() != expected_sign_bit && !(expected_sign_bit &&
      std::all_of(data.begin(), data.end(), [](auto i) { return i == 0; })))
      data.push_back(::default_place<place_t>(expected_sign_bit));
  }
  catch (...)
  {
    // exception thrown, but maybe invariant doesn't hold
    shrink();
    throw;
  }
  return shrink();
}

big_integer & big_integer::revert_sign(bool sign)
{
  // if *this == 0 nothing changes
  if (sign_bit() != sign)
    *this = - *this;
  return *this;
}

bool big_integer::make_absolute()
{
  bool sign = sign_bit();
  revert_sign(0);
  return sign;
}

void big_integer::iterate(const big_integer &b,
  const std::function<place_t (place_t, place_t)> &action)
{
  resize(std::max(data.size(), b.data.size()));
  auto l_end = data.end();
  auto r_end = b.data.end(), r_it = b.data.begin();
  for (auto it = data.begin(); it != l_end; it++, r_it++)
    *it = action(*it, r_it >= r_end ? b.default_place() : *r_it);
}
void big_integer::iterate(const std::function<place_t (place_t)> &action)
{
  auto end = data.end();
  for (auto it = data.begin(); it != end; it++)
    *it = action(*it);
}
void big_integer::iterate_r(const std::function<place_t (place_t)> &action)
{
  auto begin = data.begin();
  for (auto it = data.end(); it != begin; it--)
    *(it - 1) = action(*(it - 1));
}
void big_integer::iterate(const std::function<void (place_t)> &action) const
{
  auto end = data.end();
  for (auto it = data.begin(); it != end; it++)
    action(*it);
}
void big_integer::iterate_r(const std::function<void (place_t)> &action) const
{
  auto begin = data.begin();
  for (auto it = data.end(); it != begin; it--)
    action(*(it - 1));
}

big_integer & big_integer::place_wise(const big_integer &b,
  const std::function<place_t (place_t l, place_t r)> &action)
{
  iterate(b, action);
  return shrink();
}

/***
 * Major functions for big_integer
 ***/

big_integer::big_integer() : data(1, 0)
{}

big_integer::big_integer(const big_integer &other) : data(other.data)
{}

big_integer::big_integer(int a) : data(1, (place_t)a)
{}

big_integer::big_integer(place_t place) : data(1, place)
{
  // must be unsigned
  correct_sign_bit(0);
}

big_integer::big_integer(const std::string &str) : data(1, 0)
{
  auto it = str.cbegin();
  bool is_negated = false;
  if (it != str.cend() && *it == '-')
  {
    is_negated = true;
    it++;
  }

  if (it == str.cend())
    throw std::runtime_error("Cannot read number from string: '" + str + "'");

  for (; it != str.cend(); it++)
  {
    short_multiply(10);
    *this += *it - '0';
  }

  revert_sign(is_negated);
}

big_integer::~big_integer()
{
}

big_integer & big_integer::operator=(const big_integer &other)
{
  data = other.data;
  return *this;
}

/***
 * Arithmetic functions for 32/64-bit numbers
 **/

template<typename type>
  static type addc(type left, type right, bool &carry)
  {
    using limit = std::numeric_limits<type>;
    bool old_carry = carry;
    carry = (limit::max() - left < right ||
             left == limit::max() && carry == 1 ||
             limit::max() - left - carry < right);
    return left + right + old_carry;
  }

static inline uint32_t low_bytes(uint64_t x) { return x & 0xFFFF'FFFF; }
static inline uint32_t high_bytes(uint64_t x) { return x >> 32; }

static std::pair<uint64_t, uint64_t> mul(uint64_t left, uint64_t right)
{
  uint64_t
    left_low = low_bytes(left),
    right_low = low_bytes(right),
    left_high = high_bytes(left),
    right_high = high_bytes(right),
    bd = left_low * right_low,
    ad = left_high * right_low,
    bc = left_low * right_high,
    ac = left_high * right_high;

  // res = ac * 2^64 + (ad + bc) * 2^32 + bd

  uint32_t carry = high_bytes(uint64_t{high_bytes(bd)} + low_bytes(bc) + low_bytes(ad));
  return
  {
    bd + (ad << 32) + (bc << 32),
    ac + high_bytes(ad) + high_bytes(bc) + carry
  };
}

static std::pair<uint32_t, uint32_t> mul(uint32_t left, uint32_t right)
{
  uint64_t res = uint64_t{left} * right;
  return {low_bytes(res), high_bytes(res)};
}

static bool less_3_digits(uint64_t lhs_low, uint32_t lhs_high,
                          uint64_t rhs_low, uint32_t rhs_high)
{
  return lhs_high < rhs_high || lhs_high == rhs_high && lhs_low < rhs_low;
}

static uint64_t get_3_digits(uint64_t low, uint32_t high, int at)
{
  switch (at)
  {
  case -1:
    return high;
  case 0:
    return (uint64_t{high} << 16) | (low >> 48);
  case 1:
    return (uint64_t{high << 16} << 16) | (low >> 32);
  }
  return 0;
}

static std::pair<uint64_t, uint32_t> sub_5_digits(uint64_t lhs_low, uint32_t lhs_high,
                                                  uint64_t rhs_low, uint16_t rhs_high, int at)
{
  if (at == 0)
  {
    uint64_t l64 = (uint64_t{lhs_high & 0x0000'FFFF} << 48)| (lhs_low >> 16);
    bool borrow = 0;
    l64 = addc(l64, ~rhs_low + 1, borrow);
    uint16_t l16 = lhs_high >> 16;
    l16 -= borrow;
    borrow = 0;
    l16 = addc(l16, (uint16_t)(~rhs_high + 1), borrow);
    lhs_high = (uint32_t{l16} << 16) | (uint32_t)(l64 >> 48);
    lhs_low = (l64 << 16) | (lhs_low & 0x0000'FFFF);
  }
  else
  {
    bool borrow = 0;
    lhs_low = addc(lhs_low, ~rhs_low + 1, borrow);
    lhs_high -= borrow;
    borrow = 0;
    lhs_high = addc(lhs_high, (uint32_t)(~rhs_high + 1), borrow);
  }
  return {lhs_low, lhs_high};
}

static std::pair<uint32_t, uint64_t> div3_2(uint32_t lhs_low, uint32_t lhs_med, uint32_t lhs_high,
                                            uint32_t rhs_low, uint32_t rhs_high)
{
  // divide in base 2^16 with trial digits on prefixes
  uint64_t
    r_low64 = (uint64_t{lhs_med} << 32) | lhs_low,
    d = (uint64_t{rhs_high} << 32) | rhs_low;
  uint32_t
    r_high = lhs_high,
    d2 = rhs_high;

  if (get_3_digits(r_low64, r_high, -1) / d2 != 0)
    // overflow
    return {std::numeric_limits<uint32_t>::max(), 0};

  // high trial digit
  uint16_t qt1 = (uint16_t)(get_3_digits(r_low64, r_high, 0) / d2);
  auto dq = mul(d, uint64_t{qt1});
  if (less_3_digits(r_low64, r_high, dq.first, low_bytes(dq.second)))
  {
    // correct estimate
    qt1--;
    dq = mul(d, uint64_t{qt1});
  }
  // subtract from remainder
  auto rmdq = sub_5_digits(r_low64, r_high, dq.first, (uint16_t)dq.second, 0);
  r_low64 = rmdq.first;
  r_high = rmdq.second;

  // low trial digit
  uint16_t qt2 = (uint16_t)(get_3_digits(r_low64, r_high, 1) / d2);
  dq = mul(d, uint64_t{qt2});
  if (less_3_digits(r_low64, r_high, dq.first, low_bytes(dq.second)))
  {
    // correct estimate
    qt2--;
    dq = mul(d, uint64_t{qt2});
  }
  // subtract from remainder
  rmdq = sub_5_digits(r_low64, r_high, dq.first, (uint16_t)dq.second, 1);
  r_low64 = rmdq.first;
  r_high = rmdq.second;

  return {(uint32_t{qt1} << 16) | qt2, r_low64};
}

static std::pair<uint32_t, uint32_t> div2_1(uint32_t lhs_low, uint32_t lhs_high, uint32_t rhs)
{
  uint64_t lhs = ((uint64_t{lhs_high} << 32) | lhs_low);
  return {(uint32_t)(lhs / rhs), (uint32_t)(lhs % rhs)};
}

/***
 * Rest of arithmetic operators for big_integer
 ***/

big_integer & big_integer::operator+=(const big_integer &rhs)
{
  bool old_sign = sign_bit(), rhs_sign = rhs.sign_bit();
  using limit = std::numeric_limits<place_t>;
  bool carry = 0;
  iterate(rhs, [&](place_t l, place_t r) { return addc(l, r, carry); });
  if (old_sign == rhs_sign)
    correct_sign_bit(old_sign);

  // 1... + 1... => 0... + carry  | new place
  //                1... + carry  |
  // 0... + 0... => 1... no carry | new place
  //                0... no carry |
  // 1... + 0... => 1... no carry |
  //                0... + carry  |

  return shrink();
}

big_integer & big_integer::operator-=(const big_integer &rhs)
{
  return *this += -rhs;
}

// multiplication by a positive integer that fits into place_t
big_integer & big_integer::short_multiply(place_t rhs)
{
  bool old_sign = make_absolute();

  place_t carry = 0;
  iterate([&](place_t datai)
    {
      auto [res, new_carry] = mul(datai, rhs);
      bool add_carry = false;
      place_t result = addc(res, carry, add_carry);
      carry = addc(new_carry, place_t{ 0 }, add_carry);
      return result;
    });
  correct_sign_bit(0, carry == 0 ? std::optional<place_t>() : carry);
  return revert_sign(old_sign);
}

big_integer & big_integer::operator*=(const big_integer &rhs)
{
  bool sign = make_absolute() ^ rhs.sign_bit();

  const big_integer &right =
    rhs.sign_bit() ? static_cast<const big_integer &>(-rhs) : rhs;

  big_integer res = 0;
  int i = 0;
  right.iterate([&](place_t rdatai)
    {
      res += big_integer(*this).short_multiply(rdatai) << ((int)i * PLACE_BITS);
      i++;
    });
  return *this = res.revert_sign(sign);
}

// division by a positive integer that fits into place_t,
// (requires place_t to be uint32_t because of div2_1)
big_integer & big_integer::short_divide(place_t rhs, place_t &rem)
{
  rem = 0;
  iterate_r([&](place_t datai)
    {
      auto [res, new_rem] = div2_1(datai, rem, rhs);
      rem = new_rem;
      return res;
    });
  return shrink();
}

// long division using algorithm with trial digits and division of prefixes
// (requires place_t to be uint32_t because of short_divide, div2_1 & div3_2)
big_integer & big_integer::long_divide(const big_integer &rhs, big_integer &rem)
{
  bool this_sign = make_absolute(), sign = this_sign ^ rhs.sign_bit();

  const big_integer &right =
    rhs.sign_bit() ? static_cast<const big_integer &>(-rhs) : rhs;

  size_t n = unsigned_size(), m = right.unsigned_size();

  if (m == 1)
  {
    place_t r;
    short_divide(right.data[0], r);
    rem = big_integer(r);
  }
  else if (m > n)
  {
    // divisor is greater than dividend, quotient is 0, remainder is dividend
    rem = *this;
    *this = 0;
  }
  else
  {
    // divide with base 2^PLACE_BITS
    // 2 <= m <= n -- true

    // copy operands: starting remainder is this, divisor is right
    rem = *this;
    big_integer d = right;

    // normalize divisor d (largest place >= base / 2)
    place_t f = d.data[m - 1] == std::numeric_limits<place_t>::max() ?
      1 : div2_1(0, 1, d.data[m - 1] + 1).first;
    rem.short_multiply(f);
    d.short_multiply(f);

    // 2 leading digits of divisor for quotient digits estimate
    place_t
      d2_high = d.data[m - 1],
      d2_low = d.data[m - 2];
    // compute quotient digits
    std::vector<place_t> new_data(n - m + 1);
    for (size_t ki = 0; ki <= n - m; ki++)
    {
      int k = (int)(n - m - ki);
      place_t
        // first, count 3 leading digits of remainder
        r3_high = k + m >= rem.data.size() ? 0 : rem.data[k + m],
        r3_med = k + m - 1 >= rem.data.size() ? 0 : rem.data[k + m - 1],
        r3_low = k + m - 2 >= rem.data.size() ? 0 : rem.data[k + m - 2],
        // obtain k-th digit estimate
        qt = div3_2(r3_low, r3_med, r3_high, d2_low, d2_high).first;
      // count result with estimate
      big_integer dq = big_integer(d).short_multiply(qt) <<= k * PLACE_BITS;
      if (rem < dq)
      {
        // wrong, correct estimate
        qt--;
        dq = big_integer(d).short_multiply(qt) <<= k * PLACE_BITS;
      }
      // set digit in quotient
      new_data[k] = qt;
      // subtract current result from remainder
      rem -= dq;
    }
    place_t dummy;
    rem.short_divide(f, dummy);
    data.swap(buffer(new_data));
    correct_sign_bit(0);
  }
  rem.revert_sign(this_sign);
  return revert_sign(sign);
}

big_integer & big_integer::operator/=(const big_integer &rhs)
{
  big_integer dummy;
  return long_divide(rhs, dummy);
}

big_integer & big_integer::operator%=(const big_integer &rhs)
{
  big_integer(*this).long_divide(rhs, *this);
  return *this;
}

big_integer & big_integer::operator&=(const big_integer &rhs)
{
  return place_wise(rhs, [](place_t l, place_t r) { return l & r; });
}

big_integer & big_integer::operator|=(const big_integer &rhs)
{
  return place_wise(rhs, std::bit_or<>());
}

big_integer & big_integer::operator^=(const big_integer &rhs)
{
  return place_wise(rhs, [](place_t l, place_t r) { return l ^ r; });
}

big_integer & big_integer::operator<<=(int rhs)
{
  return bit_shift(rhs);
}

big_integer & big_integer::operator>>=(int rhs)
{
  return bit_shift(-rhs);
}

big_integer & big_integer::bit_shift(int rhs)
{
  // rhs > 0 -> left shift
  // rhs < 0 -> right shift

  // [lsrc][rsrc]
  //       <-> bits
  //    [plce]
  bool sign = sign_bit();
  int places = rhs / PLACE_BITS, bits = rhs % PLACE_BITS;
  if (rhs < 0 && bits != 0)
    places--, bits += PLACE_BITS;
  std::vector<place_t> new_data(std::max(data.size() + places + (bits > 0), size_t{1}),
                                get_or_default(rhs < 0 ? (int)data.size() : -1));
  for (size_t i = std::max(places, 0); i < data.size() + places + (bits > 0); i++)
  {
    // handle 64 shift undefined behavior with cases
    if (bits == 0)
      new_data[i] = data[i - places];
    else
    {
      place_t
        lsrc = get_or_default((int)i - places),
        rsrc = get_or_default((int)i - places - 1);
      new_data[i] = (lsrc << bits) | (rsrc >> (PLACE_BITS - bits));
    }
  }
  data.swap(buffer(new_data));
  return correct_sign_bit(sign);
}

big_integer big_integer::operator+() const
{
  return *this;
}

big_integer big_integer::operator-() const
{
  return ~ *this + 1;
}

big_integer big_integer::operator~() const
{
  return big_integer().place_wise(*this, [](auto x, auto y) { return ~y; });
}

big_integer & big_integer::operator++()
{
  return *this += 1;
}

big_integer big_integer::operator++(int)
{
  big_integer r = *this;
  ++ *this;
  return r;
}

big_integer & big_integer::operator--()
{
  return *this -= 1;
}

big_integer big_integer::operator--(int)
{
  big_integer r = *this;
  -- *this;
  return r;
}

big_integer operator+(big_integer a, const big_integer &b)
{
  return a += b;
}

big_integer operator-(big_integer a, const big_integer &b)
{
  return a -= b;
}

big_integer operator*(big_integer a, const big_integer &b)
{
  return a *= b;
}

big_integer operator/(big_integer a, const big_integer &b)
{
  return a /= b;
}

big_integer operator%(big_integer a, const big_integer &b)
{
  return a %= b;
}

big_integer operator&(big_integer a, const big_integer &b)
{
  return a &= b;
}

big_integer operator|(big_integer a, const big_integer &b)
{
  return a |= b;
}

big_integer operator^(big_integer a, const big_integer &b)
{
  return a ^= b;
}

big_integer operator<<(big_integer a, int b)
{
  return a <<= b;
}

big_integer operator>>(big_integer a, int b)
{
  return a >>= b;
}

int big_integer::sign() const
{
  if (sign_bit())
    return -1;
  if (data.size() == 1 && data[0] == 0)
    return 0;
  return 1;
}

int big_integer::compare(const big_integer &l, const big_integer &r)
{
  bool lsign = l.sign_bit(), rsign = r.sign_bit();
  if (lsign != rsign)
    return rsign - lsign;
  int sign = lsign ? -1 : 1;
  if (l.data.size() != r.data.size())
    return l.data.size() > r.data.size() ? sign : -sign;
  const place_t *ldata = l.data.data(), *rdata = r.data.data();
  for (size_t i = 0; i < l.data.size(); i++)
    if (place_t a = ldata[l.data.size() - i - 1], b = rdata[r.data.size() - i - 1]; a != b)
      return a > b ? sign : -sign;
  return 0;
}

bool operator==(const big_integer &a, const big_integer &b)
{
  return a.data == b.data;
}

bool operator!=(const big_integer &a, const big_integer &b)
{
  return a.data != b.data;
}

bool operator<(const big_integer &a, const big_integer &b)
{
  return big_integer::compare(a, b) < 0;
}

bool operator>(const big_integer &a, const big_integer &b)
{
  return big_integer::compare(a, b) > 0;
}

bool operator<=(const big_integer &a, const big_integer &b)
{
  return big_integer::compare(a, b) <= 0;
}

bool operator>=(const big_integer &a, const big_integer &b)
{
  return big_integer::compare(a, b) >= 0;
}

std::string to_string(const big_integer &a)
{
  big_integer c = a;
  bool sgn = c.make_absolute();
  std::vector<char> reverse;
  if (c == 0)
    reverse.push_back('0');
  while (c != 0)
  {
    uint32_t rem;
    c.short_divide(10, rem);
    reverse.push_back((char)((char)rem + '0'));
  }
  if (sgn)
    reverse.push_back('-');

  return std::string(reverse.rbegin(), reverse.rend());
}

std::ostream & operator<<(std::ostream &s, const big_integer &a)
{
  return s << to_string(a);
}
