/* Nikolai Kholiavin, M3138 */

#include "big_integer.h"

#include <cstring>
#include <stdexcept>
#include <algorithm>

void big_integer::iterate(const big_integer &b,
                          std::function<bool (uint64_t &l, uint64_t r)> action)
{
  data.resize(std::max(data.size(), b.data.size()));
  for (int i = 0; i < data.size(); i++)
    if (!action(data[i], i < b.data.size() ? b.data[i] : 0))
      return;
}

void big_integer::iterate(const big_integer &b,
                          std::function<void (uint64_t &l, uint64_t r)> action)
{
  iterate(b, [](uint64_t &l, uint64_t r) { action(l, r); return true; });
}

big_integer & big_integer::qword_wise(const big_integer &b,
                                      std::function<uint64_t (uint64_t l, uint64_t r)> action)
{
  iterate(b,
  [&](uint64_t &l, uint64_t r)
  {
    l = action(l, r);
  });
  shrink();
  return *this;
}

void big_integer::shrink()
{
  while (data.size() > 1 && data.back() == uint64_t{0})
    data.pop_back();
}

big_integer::big_integer()
{
}

big_integer::big_integer(const big_integer &other) : data(other.data)
{
}

big_integer::big_integer(int a) : data({uint64_t{a}})
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
    short_multiply(10);
    *this += *it;
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
  shrink();
  return *this;
}

static uint64_t addc(uint64_t left, uint64_t right, bool &carry)
{
  using limit = std::numeric_limits<uint64_t>;
  bool old_carry = carry;
  carry = (limit::max() - left < right ||
           left == limit::max() && carry == 1 ||
           limit::max() - left - carry < right);
  return left + right + old_carry;
}

static std::pair<uint64_t, uint64_t> mul(uint64_t left, uint64_t right)
{
  uint32_t
    left_low = left & 0xFFFFFFFF,
    right_low = left & 0xFFFFFFFF;
  uint64_t
    left_high = left >> 32 << 32,
    right_high = right >> 32 << 32;

  bool carry = 0;
  return
  {
    left_low * right_low,
    left_low * right_high + left_high * right_low + left_high * right_high
  };
}

big_integer & big_integer::operator+=(const big_integer &rhs)
{
  using limit = std::numeric_limits<uint64_t>;
  bool carry = 0;
  iterate(rhs, [&](uint64_t &l, uint64_t r) { l = addc(l, r, carry); });
  if (carry)
    data.push_back(1);

  shrink();
  return *this;
}

big_integer & big_integer::operator-=(const big_integer &rhs)
{
  return *this += -rhs;
}

big_integer & big_integer::short_multiply(uint64_t other)
{
  uint64_t carry = 0;
  for (size_t i = 0; i < data.size(); i++)
  {
    auto [res, new_carry] = mul(data[i], other);
    bool add_carry = false;
    data[i] = addc(res, carry, add_carry);
    new_carry = addc(new_carry, uint64_t{add_carry}, add_carry);
  }
  return *this;
}

big_integer big_integer::long_multiply(big_integer left, big_integer right)
{
  int sign = left.get_sign() * right.get_sign();
  if (left < 0)
    left = -left;
  if (right > 0)
    right = -right;

  big_integer res = 0;
  for (size_t i = 0; i < right.data.size(); i++)
  {
    big_integer summand = left;
    res += summand.short_multiply(right.data[i]) << (i * 64);
  }
  return sign < 0 ? -res : res;
}

big_integer & big_integer::operator*=(const big_integer &rhs)
{
  return *this = long_multiply(*this, rhs);
}

big_integer & big_integer::operator/=(const big_integer &rhs)
{
  ///
  return *this;
}

big_integer & big_integer::operator%=(const big_integer &rhs)
{
  ///
  return *this;
}

big_integer & big_integer::operator&=(const big_integer &rhs)
{
  return qword_wise(rhs, [](uint64_t l, uint64_t r) { return l & r; });
}

big_integer & big_integer::operator|=(const big_integer &rhs)
{
  return qword_wise(rhs, [](uint64_t l, uint64_t r) { return l | r; });
}

big_integer & big_integer::operator^=(const big_integer &rhs)
{
  return qword_wise(rhs, [](uint64_t l, uint64_t r) { return l ^ r; });
}

big_integer & big_integer::operator<<=(int rhs)
{
  int qwords = rhs / 64, bits = rhs % 64;
  data.resize(data.size() + qwords + (bits > 0));
  for (size_t i = 0; i < data.size() - qwords; i++)
  {
    uint64_t
      lsrc = data[data.size() - i - qwords - 1],
      rsrc = i < data.size() - qwords - 1 ? data[data.size() - i - qwords - 2] : 0;
    data[data.size() - i - 1] = (lsrc << bits) | (rsrc >> (64 - bits));
  }
  return *this;
}

big_integer & big_integer::operator>>=(int rhs)
{
  int qwords = rhs / 64, bits = rhs % 64;
  for (size_t i = 0; i < data.size() - qwords; i++)
  {
    uint64_t
      lsrc = i + qwords + 1 < data.size() ? data[i + qwords + 1] : 0,
      rsrc = data[i + qwords];
    data[i] = (lsrc >> bits) | (rsrc << (64 - bits));
  }
  data.resize(std::max(data.size() - qwords, size_t{1}));
  return *this;
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
  big_integer r;
  r.qword_wise(*this, [](uint64_t l, uint64_t r) { return ~r; });
  return r;
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

int big_integer::get_sign() const
{
  if ((data.back() >> 63) != 0)
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
  int lsign = l.get_sign(), rsign = r.get_sign();
  if (lsign != rsign))
    return lsign - rsign;
  for (size_t i = 0; i < l.data.size(); i++)
    if (l.data[i] - r.data[i] != 0)
      return (l.data[i] - r.data[i]) * lsign;
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
  std::vector<char> reverse;
  big_integer c = a;
  while (c != 0)
  {
    reverse.push_back(c % 10);
    c /= 10;
  }

  return std::string(reverse.rbegin(), reverse.rend());
}

std::ostream &operator<<(std::ostream &s, const big_integer &a)
{
  return s << to_string(a);
}
