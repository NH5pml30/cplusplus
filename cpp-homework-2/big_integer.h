/* Nikolai Kholiavin, M3138 */

#ifndef BIG_INTEGER_H
#define BIG_INTEGER_H

#include <cstddef>
#include <iosfwd>
#include <cstdint>
#include <vector>
#include <string>
#include <functional>

class big_integer
{
private:
  using place_t = uint64_t;
  std::vector<place_t> data = {place_t{0}};
  static constexpr int PLACE_BITS = std::numeric_limits<place_t>::digits;

public:
  big_integer();
  big_integer(const big_integer &other);
  big_integer(int a);
  explicit big_integer(std::string const &str);
  ~big_integer();

  big_integer & operator=(const big_integer &other);

  big_integer & operator+=(const big_integer &rhs);
  big_integer & operator-=(const big_integer &rhs);
  big_integer & operator*=(const big_integer &rhs);
  big_integer & operator/=(const big_integer &rhs);
  big_integer & operator%=(const big_integer &rhs);

  big_integer & operator*=(place_t rhs);
  big_integer & operator/=(place_t rhs);
  big_integer & operator%=(place_t rhs);

  big_integer & operator&=(const big_integer &rhs);
  big_integer & operator|=(const big_integer &rhs);
  big_integer & operator^=(const big_integer &rhs);

  big_integer & operator<<=(int rhs);
  big_integer & operator>>=(int rhs);

  big_integer operator+() const;
  big_integer operator-() const;
  big_integer operator~() const;

  big_integer & operator++();
  big_integer operator++(int);

  big_integer & operator--();
  big_integer operator--(int);

  friend big_integer operator*(big_integer a, place_t other);
  friend big_integer operator/(big_integer a, place_t other);
  friend place_t operator%(big_integer a, place_t other);

  friend bool operator==(const big_integer &a, const big_integer &b);
  friend bool operator!=(const big_integer &a, const big_integer &b);
  friend bool operator<(const big_integer &a, const big_integer &b);
  friend bool operator>(const big_integer &a, const big_integer &b);
  friend bool operator<=(const big_integer &a, const big_integer &b);
  friend bool operator>=(const big_integer &a, const big_integer &b);

  friend std::string to_string(const big_integer &a);

private:
  big_integer & long_divide(const big_integer &rhs, big_integer &rem);
  big_integer & short_divide(place_t rhs, place_t &rem);
  big_integer & bit_shift(int bits);

  big_integer & shrink();
  void resize(size_t new_size);
  place_t default_place() const;
  place_t get_or_default(int at) const;

  void iterate(const big_integer &b, std::function<void (place_t &l, place_t r)> action);
  big_integer & place_wise(const big_integer &b, std::function<place_t (place_t l, place_t r)> action);

  int sign() const;
  bool sign_bit() const;

  static int compare(const big_integer &l, const big_integer &r);

};

big_integer operator+(big_integer a, const big_integer &b);
big_integer operator-(big_integer a, const big_integer &b);
big_integer operator*(big_integer a, const big_integer &b);
big_integer operator/(big_integer a, const big_integer &b);
big_integer operator%(big_integer a, const big_integer &b);
big_integer operator*(big_integer a, big_integer::place_t other);
big_integer operator/(big_integer a, big_integer::place_t other);
big_integer::place_t operator%(big_integer a, big_integer::place_t other);

big_integer operator&(big_integer a, const big_integer &b);
big_integer operator|(big_integer a, const big_integer &b);
big_integer operator^(big_integer a, const big_integer &b);

big_integer operator<<(big_integer a, int b);
big_integer operator>>(big_integer a, int b);

bool operator==(const big_integer &a, const big_integer &b);
bool operator!=(const big_integer &a, const big_integer &b);
bool operator<(const big_integer &a, const big_integer &b);
bool operator>(const big_integer &a, const big_integer &b);
bool operator<=(const big_integer &a, const big_integer &b);
bool operator>=(const big_integer &a, const big_integer &b);

std::string to_string(const big_integer &a);
std::ostream &operator<<(std::ostream &s, const big_integer &a);

#endif // BIG_INTEGER_H
