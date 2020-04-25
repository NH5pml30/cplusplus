/* Nikolai Kholiavin, M3138 */

#include "big_integer.h"

#include <cstring>
#include <stdexcept>
#include <algorithm>
#include <cassert>
#include <iostream>

void big_integer::iterate(const big_integer &b,
                          std::function<void (place_t &l, place_t r)> action)
{
  resize(std::max(data.size(), b.data.size()));
  for (int i = 0; i < data.size(); i++)
    action(data[i], i < b.data.size() ? b.data[i] : b.default_place());
}

big_integer & big_integer::place_wise(const big_integer &b,
                                      std::function<place_t (place_t l, place_t r)> action)
{
  iterate(b,
    [&action, &b, this](place_t &l, place_t r)
    {
      l = action(l, r);
    });
  return shrink();
}

template<typename type>
  bool sign_bit(type val)
  {
    return val >> (std::numeric_limits<type>::digits - 1);
  }

bool big_integer::sign_bit() const
{
  return ::sign_bit(data.back());
}

big_integer & big_integer::shrink()
{
  while (data.size() > 1 && data.back() == default_place() &&
         sign_bit() == ::sign_bit(data[data.size() - 2]))
    // while last place is default and no change in sign
    data.pop_back();
  return *this;
}

template<typename type>
static type default_place(bool bit)
{
  return bit ? std::numeric_limits<type>::max() : 0;
}

big_integer::place_t big_integer::default_place() const
{
  return ::default_place<place_t>(sign_bit());
}

void big_integer::resize(size_t new_size)
{
  data.resize(new_size, default_place());
}

big_integer::big_integer()
{
}

big_integer::big_integer(const big_integer &other) : data(other.data)
{
}

big_integer::big_integer(int a) : data({(place_t)a})
{
}

big_integer::big_integer(std::string const &str)
{
  auto it = str.cbegin();
  bool is_negated = false;
  if (it != str.cend() && *it == '-')
  {
    is_negated = true;
    it++;
  }

  if (it == str.cend())
    throw std::runtime_error("invalid string");

  for (; it != str.cend(); it++)
  {
    *this *= 10;
    *this += *it - '0';
  }

  if (is_negated)
    *this = -*this;
}

big_integer::~big_integer()
{
}

big_integer & big_integer::operator=(const big_integer &other)
{
  data = other.data;
  return shrink();
}

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

static inline uint32_t low_bytes(uint64_t x)
{
  return x & 0xFFFF'FFFF;
}

static inline uint32_t high_bytes(uint64_t x)
{
  return x >> 32;
}

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

big_integer & big_integer::operator+=(const big_integer &rhs)
{
  bool old_sign = sign_bit(), rhs_sign = rhs.sign_bit();
  using limit = std::numeric_limits<place_t>;
  bool carry = 0;
  iterate(rhs, [&](place_t &l, place_t r) { l = addc(l, r, carry); });
  if (old_sign == rhs_sign && old_sign != sign_bit())
    data.push_back(::default_place<place_t>(old_sign));

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

big_integer & big_integer::operator*=(place_t other)
{
  bool old_sign = sign_bit();
  if (old_sign)
    *this = - *this;

  place_t carry = 0;
  for (size_t i = 0; i < data.size(); i++)
  {
    auto [res, new_carry] = mul(data[i], other);
    bool add_carry = false;
    data[i] = addc(res, carry, add_carry);
    carry = addc(new_carry, place_t{0}, add_carry);
  }
  if (carry != 0)
    data.push_back(carry);
  if (sign_bit())
    data.push_back(0);

  if (old_sign)
    *this = - *this;
  return shrink();
}

big_integer & big_integer::operator*=(const big_integer &rhs)
{
  bool sign = sign_bit() ^ rhs.sign_bit();
  if (sign_bit())
    *this = - *this;

  const big_integer &right =
    rhs.sign_bit() ? (const big_integer &)(-rhs) : rhs;

  big_integer res = 0;
  for (size_t i = 0; i < right.data.size(); i++)
    res += (*this * right.data[i]) << ((int)i * PLACE_BITS);
  *this = res;
  if (sign)
    *this = - *this;
  return *this;
}

std::pair<uint64_t, uint64_t> div(uint64_t lhs_low, uint64_t lhs_high, uint64_t rhs)
{
  uint64_t r = (lhs_low >> 32) | (lhs_high << 32);
  uint64_t ans = r / rhs;
  r = ((r % rhs) << 32) + (lhs_low & 0xFFFFFFFF);
  ans = (ans << 32) + r / rhs;

  return {ans, lhs_low - ans * rhs};
}

std::pair<uint32_t, uint32_t> div(uint32_t lhs_low, uint32_t lhs_high, uint32_t rhs)
{
  uint64_t lhs = ((uint64_t{lhs_high} << 32) | lhs_low);
  uint32_t ans = (uint32_t)(lhs / rhs);

  return {ans, (uint32_t)(lhs - uint64_t{ans} * rhs)};
}

big_integer & big_integer::short_divide(place_t rhs, place_t &rem)
{
  rem = 0;
  for (size_t i = 0; i < data.size(); i++)
  {
    auto [res, new_rem] = div(data[data.size() - i - 1], rem, rhs);
    data[data.size() - i - 1] = res;
    rem = new_rem;
  }
  return shrink();
}

big_integer & big_integer::operator/=(place_t rhs)
{
  place_t dummy;
  return short_divide(rhs, dummy);
}

big_integer & big_integer::long_divide(const big_integer &rhs, big_integer &rem)
{
  bool sign = sign_bit() ^ rhs.sign_bit();
  if (sign_bit())
    *this = - *this;
  const big_integer &right =
    rhs.sign_bit() ? (const big_integer &)(-rhs) : rhs;
  big_integer rem;

  if (right.data.size() == 1)
  {
    place_t r;
    short_divide(rhs.data.front(), r);
    rem = r;
  }
  else if (right > *this)
  {
    rem = *this;
    *this = 0;
  }
  else
  {

  }
  if (sign)
  {
    *this = - *this;
    rem = -rem;
  }
  return *this;
}

big_integer & big_integer::operator/=(const big_integer &rhs)
{
  bool sign = sign_bit() ^ rhs.sign_bit();
  const big_integer &right =
    rhs.sign_bit() ? (const big_integer &)(-rhs) : rhs;
  if (right.data.size() == 1)
    *this /= rhs.data.front();
  else if (right > *this)
    *this = 0;
  else
  {

  }
  if (sign)
    *this = - *this;
  return *this;
}

big_integer & big_integer::operator%=(const big_integer &rhs)
{
  ///
  return *this;
}

big_integer & big_integer::operator%=(place_t rhs)
{
  ///
  return *this;
}

big_integer & big_integer::operator&=(const big_integer &rhs)
{
  return place_wise(rhs, [](place_t l, place_t r) { return l & r; });
}

big_integer & big_integer::operator|=(const big_integer &rhs)
{
  return place_wise(rhs, [](place_t l, place_t r) { return l | r; });
}

big_integer & big_integer::operator^=(const big_integer &rhs)
{
  return place_wise(rhs, [](place_t l, place_t r) { return l ^ r; });
}

big_integer::place_t big_integer::get_or_default(int at) const
{
  if (at < 0)
    return 0;
  if (at >= data.size())
    return default_place();
  return data[at];
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
  data.swap(new_data);
  return shrink();
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
  return big_integer().place_wise(*this,
    [](place_t l, place_t r)
    {
      return ~r;
    });
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

big_integer operator*(big_integer a, big_integer::place_t b)
{
  return a *= b;
}

big_integer operator/(big_integer a, big_integer::place_t b)
{
  return a /= b;
}

big_integer::place_t operator%(big_integer a, big_integer::place_t b)
{
  big_integer::place_t rem;
  a.short_divide(b, rem);
  return rem;
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
  for (size_t i = 0; i < data.size(); i++)
    if (data[i] != 0)
      return 1;
  return 0;
}

int big_integer::compare(const big_integer &l, const big_integer &r)
{
  if (l.data.size() < r.data.size())
    return -1;
  int lsign = l.sign(), rsign = r.sign();
  if (lsign != rsign)
    return lsign - rsign;
  for (size_t i = 0; i < l.data.size(); i++)
    if (place_t a = l.data[l.data.size() - i - 1], b = r.data[r.data.size() - i - 1]; a != b)
      return a > b ? lsign : -lsign;
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
  bool sgn = a.sign_bit();
  std::vector<char> reverse;
  big_integer c = (sgn ? -a : a);
  if (c == 0)
    reverse.push_back('0');
  while (c != 0)
  {
    reverse.push_back((char)(c % 10 + '0'));
    c /= 10;
  }
  if (sgn)
    reverse.push_back('-');

  return std::string(reverse.rbegin(), reverse.rend());
}

std::ostream &operator<<(std::ostream &s, const big_integer &a)
{
  return s << to_string(a);
}
