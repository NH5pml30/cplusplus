/* Nikolai Kholiavin, M3138 */

#include "big_integer.h"

#include <cstring>
#include <stdexcept>
#include <algorithm>
#include <cassert>
#include <iostream>
#include <sstream>

void big_integer::iterate(const big_integer &b,
                          std::function<void (place_t &l, place_t r)> action)
{
  resize(std::max(data.size(), b.data.size()));
  for (size_t i = 0; i < data.size(); i++)
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

template<typename type>
  bool is_vector_zeroed(const std::vector<type> &v)
  {
    for (auto el : v)
      if (el != 0)
        return false;
    return true;
  }

big_integer & big_integer::correct_sign_bit(bool expected_sign_bit)
{
  // invariant does not hold for now
  // zero always accepted, but can be non-shrinked
  if (sign_bit() != expected_sign_bit && !(expected_sign_bit && is_vector_zeroed(data)))
    data.push_back(::default_place<place_t>(expected_sign_bit));
  return shrink();
}

void big_integer::resize(size_t new_size)
{
  data.resize(new_size, default_place());
}

bool big_integer::make_absolute()
{
  bool sign = sign_bit();
  if (sign)
    *this = - *this;
  return sign;
}

big_integer & big_integer::revert_sign(bool sign)
{
  // if *this == 0 nothing changes
  if (sign_bit() != sign)
    *this = - *this;
  return *this;
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

big_integer::big_integer(place_t place) : data({place})
{
  // must be unsigned
  correct_sign_bit(0);
}

big_integer::big_integer(const std::vector<place_t> &data) : data(data)
{
}

big_integer::big_integer(const std::string &str)
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

big_integer & big_integer::short_multiply(place_t rhs)
{
  bool old_sign = make_absolute();

  place_t carry = 0;
  for (size_t i = 0; i < data.size(); i++)
  {
    auto [res, new_carry] = mul(data[i], rhs);
    bool add_carry = false;
    data[i] = addc(res, carry, add_carry);
    carry = addc(new_carry, place_t{0}, add_carry);
  }
  if (carry != 0)
    data.push_back(carry);

  correct_sign_bit(0);
  return revert_sign(old_sign);
}

big_integer & big_integer::operator*=(const big_integer &rhs)
{
  bool sign = make_absolute() ^ rhs.sign_bit();

  const big_integer &right =
    rhs.sign_bit() ? static_cast<const big_integer &>(-rhs) : rhs;

  big_integer res = 0;
  for (size_t i = 0; i < right.data.size(); i++)
    res += big_integer(*this).short_multiply(right.data[i]) << ((int)i * PLACE_BITS);
  return (*this = res).revert_sign(sign);
}

std::pair<uint64_t, uint32_t> div(uint64_t lhs_low, uint32_t lhs_high, uint32_t rhs)
{
  // basic division algorithm in base 2^32 (assuming no overflow)
  // rem -- dividend two-digit prefix
  // rhs -- divisor consisting of one digit
  // ans -- quotient consisting of two digits
  uint64_t rem = (uint64_t{lhs_high} << 32) | high_bytes(lhs_low);
  // write high answer digit
  uint64_t ans = (rem / rhs) << 32;
  // apply previous step division, consume next digit
  rem = ((rem % rhs) << 32) | low_bytes(lhs_low);
  // write low answer digit
  ans |= rem / rhs;
  // result ready
  return {ans, (uint32_t)(rem % rhs)};
}

std::pair<uint32_t, uint32_t> div(uint32_t lhs_low, uint32_t lhs_high, uint32_t rhs)
{
  uint64_t lhs = ((uint64_t{lhs_high} << 32) | lhs_low);
  return {(uint32_t)(lhs / rhs), (uint32_t)(lhs % rhs)};
}

// division by a positive integer that fits into uint32_t
big_integer & big_integer::short_divide(uint32_t rhs, uint32_t &rem)
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

// operations with 'type' as a composite of 4 parts with equal length
namespace quarter_base
{
  template<typename type>
    constexpr size_t b_bits = std::numeric_limits<type>::digits / 4;
  template<typename type>
    constexpr type b = 1 << b_bits<type>;
  template<typename type>
    constexpr type b_mask = b<type> - 1;

  template<typename type>
    static size_t b_size(const std::vector<type> &v)
    {
      size_t res = v.size() * 4;
      while (res > 1 && b_get(v, res - 1) == 0)
        res--;
      return res;
    }
  // if at is out of bounds, returns 0
  template<typename type>
    static type b_get(const std::vector<type> &v, size_t at)
    {
      type p = at / 4 >= v.size() ? 0 : v[at / 4];
      return (p >> ((at % 4) * b_bits<type>)) & b_mask<type>;
    }
  // if at is out of bounds, resize happens
  template<typename type>
    void b_set(std::vector<type> &v, size_t at, type digit)
    {
      size_t pat = at / 4;
      if (pat >= v.size())
        v.resize(pat + 1);
      v[pat] &= ~(b_mask<type> << ((at % 4) * b_bits<type>));
      v[pat] |= digit << ((at % 4) * b_bits<type>);
    }
}

big_integer & big_integer::long_divide(const big_integer &rhs, big_integer &rem)
{
  bool sign = make_absolute() ^ rhs.sign_bit();

  const big_integer &right =
    rhs.sign_bit() ? static_cast<const big_integer &>(-rhs) : rhs;

  using namespace quarter_base;
  // base so that 3-digit numbers fit into place_t
  constexpr place_t pl_b = b<place_t>;
  constexpr size_t pl_b_bits = b_bits<place_t>;
  size_t n = b_size(data), m = b_size(right.data);

  if (m * pl_b_bits <= 32)
  {
    // right fits into 32 bits
    uint32_t r;
    short_divide((uint32_t)right.data[0], r);
    // uint32_t fits into place_t
    rem = big_integer(place_t{r});
  }
  else if (m > n)
  {
    // divisor is greater than dividend, quotient is 0, remainder is dividend
    rem = *this;
    *this = 0;
  }
  else
  {
    // 2 <= m <= n -- true

    // copy operands: starting remainder is this, divisor is right
    rem = *this;
    big_integer d = right;

    // normalize divisor d (largest place >= b / 2)
    place_t f = pl_b / (b_get(d.data, b_size(d.data) - 1) + 1);
    rem.short_multiply(f);
    d.short_multiply(f);

    // 2 leading digits of divisor for quotient digits estimate
    place_t d2 = b_get(d.data, m - 1) * pl_b + b_get(d.data, m - 2);
    // compute quotient digits
    data.clear();
    for (size_t ki = 0; ki <= n - m; ki++)
    {
      int k = (int)(n - m - ki);
      place_t
        // first, count 3 leading digits of remainder
        r3 = (b_get(rem.data, k + m) * pl_b + b_get(rem.data, k + m - 1)) * pl_b + b_get(rem.data, k + m - 2),
        // obtain k-th digit estimate
        qt = std::min(r3 / d2, pl_b - 1);
      // count result with estimate
      big_integer dq = big_integer(d).short_multiply(qt) <<= k * pl_b_bits;
      if (rem < dq)
      {
        // wrong, correct estimate
        qt--;
        dq = big_integer(d).short_multiply(qt) <<= k * pl_b_bits;
      }
      // set digit in quotient
      b_set(data, k, qt);
      // subtract current result from remainder
      rem -= dq;
    }
    uint32_t dummy;
    rem.short_divide((uint32_t)f, dummy); // f < b == 2^(PLACE_BITS/4) <= 2^16
    correct_sign_bit(0);
  }
  revert_sign(sign);
  if (sign)
    rem = -rem;
  return *this;
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
  if ((size_t)at >= data.size())
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
  data.swap(new_data);
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
  for (size_t i = 0; i < l.data.size(); i++)
    if (place_t a = l.data[l.data.size() - i - 1], b = r.data[r.data.size() - i - 1]; a != b)
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

std::ostream &operator<<(std::ostream &s, const big_integer &a)
{
  return s << to_string(a);
}
