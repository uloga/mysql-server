/*
   Copyright (c) 2012 Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef NDB_LHLEVEL_H
#define NDB_LHLEVEL_H

/**
 * LHLevel keeps level information for linear hashing,
 **/

/**
 * Supports up to UINT32_MAX number of bucket addresses.
 * If support for more is needed, one also have to
 * increase hash key size.
 **/

#include <assert.h>
#include "Bitmask.hpp"

template<typename Int> class LHBits
{
public:
  explicit LHBits();
  explicit LHBits(Int bits);
  template<typename Int2> LHBits(LHBits<Int2> const& bits);
  ~LHBits();
  LHBits& operator=(LHBits const&);

  void clear();
static LHBits<Int> unpack(Int packed);
  Int pack() const;
  bool match(LHBits other) const;
  void shift_out();
  void shift_out(Uint8 bits);
  void shift_in(bool bit);
  void shift_in(Uint8 bits, Int value);
  Uint8 valid_bits() const;
  bool valid_bits(Int bits) const;
  bool valid_bit(Int bit) const;
  Int get_bits(Int bits) const;
  Int get_bit(Int bit) const;
private:
  Int highbit() const;
  Int m_bits;
};

typedef LHBits<Uint16> LHBits16;
typedef LHBits<Uint32> LHBits32;

class LHLevel
{
public:
  explicit LHLevel();
  explicit LHLevel(Uint32 size);
  ~LHLevel() {}
private:
  LHLevel(LHLevel const&); // Not to be implemented
  LHLevel&  operator=(LHLevel const&); // Not to be implemented
public:
  void clear();
  bool isEmpty() const;
  bool isFull() const;
  Uint32 getSize() const;
  void setSize(Uint32 size);
  Uint32 getTop() const;
public:
  Uint32 getBucketNumber(LHBits32 hash_value) const;
  bool getSplitBucket(Uint32& from, Uint32& to) const; // true if data move needed
  bool shouldMoveBeforeExpand(LHBits32 hash_value) const;
  void expand();
  bool getMergeBuckets(Uint32& from, Uint32& to) const; // true if data move needed
  void shrink();
private:
  enum {
    ADDR_MAX = 0xFFFFFFFEU,
    MAX_SIZE = 0xFFFFFFFFU,
    MAXP_EMPTY = 0xFFFFFFFFU,
  };
protected:
  Uint32 max_size() const;
  Uint8 hashcheckbit() const { return m_hashcheckbit; }
  Uint32 maxp() const { return m_maxp; }
  Uint32 p() const { return m_p; }
private:
  Uint32 m_maxp;
  Uint32 m_p;
  Uint8 m_hashcheckbit;
};

class LocalLHLevel : public LHLevel
{
public:
  explicit LocalLHLevel(Uint32& size): LHLevel(size), m_src(size) {}
  ~LocalLHLevel() { m_src = getSize(); }
private:
  LocalLHLevel(const LocalLHLevel&); // Not to be implemented
  LocalLHLevel&  operator=(const LocalLHLevel&); // Not to be implemented
  Uint32& m_src;
};

/**
 * Implementation LHBits<>
 **/

template<typename Int> inline LHBits<Int>::LHBits()
: m_bits(1)
{
}

template<typename Int> inline LHBits<Int>::LHBits(Int bits)
: m_bits(bits | highbit())
{
}

template<typename Int> template<typename Int2> inline LHBits<Int>::LHBits(LHBits<Int2> const& bits)
: m_bits(bits.pack())
{
  if (m_bits != bits.pack())
    m_bits |= highbit();
}

template<typename Int> inline LHBits<Int>::~LHBits()
{
}

template<typename Int> inline LHBits<Int>& LHBits<Int>::operator=(LHBits const& src)
{
  m_bits = src.m_bits;
  return *this;
}

template<typename Int> inline void LHBits<Int>::clear()
{
  m_bits = 1;
}

template<typename Int> inline LHBits<Int> LHBits<Int>::unpack(Int packed)
{
  LHBits<Int> x;
  x.m_bits = packed;
  return x;
}

template<typename Int> inline Int LHBits<Int>::pack() const
{
  return m_bits;
}

template<typename Int> inline bool LHBits<Int>::match(LHBits<Int> other) const
{
  assert(sizeof(Int) <= sizeof(Uint32));
  assert       (m_bits != 0) ;
  assert       (other.m_bits != 0);
  assert(sizeof(Int) <= sizeof(Uint32) &&
         (m_bits != 0) &&
         (other.m_bits != 0));
  // Warning: dont reduce to one shift below.
  // << is only defined for right values < 32.
  // and since m_bits != 0, clz(MIN(...)) is guaranteed <= 31
  return ((Uint32(m_bits ^ other.m_bits) << BitmaskImpl::clz(MIN(m_bits, other.m_bits))) << 1) == 0;
}

template<typename Int> inline void LHBits<Int>::shift_out()
{
  m_bits >>= 1;
  if (m_bits == 0)
    m_bits++;
}

template<typename Int> inline void LHBits<Int>::shift_out(Uint8 bits)
{
  assert(bits < 8 * sizeof(m_bits));
  m_bits >>= bits;
  if (m_bits == 0)
    m_bits++;
}

template<typename Int> inline void LHBits<Int>::shift_in(bool bit)
{
  if (m_bits >= highbit())
    m_bits |= (highbit() >> 1);
  m_bits = (m_bits << 1) | (bit ? 1 : 0);
}

template<typename Int> inline void LHBits<Int>::shift_in(Uint8 bits, Int value)
{
  assert(m_bits != 0);
  assert(bits < 8 * sizeof(m_bits));
  assert(value < (Int(1) << bits));
  if (bits == 0)
    return;
  if (m_bits >= (highbit() >> (bits - 1)))
    m_bits = highbit() | (m_bits << bits) | value;
  else
    m_bits = (m_bits << bits) | value;
}

template<typename Int> inline Uint8 LHBits<Int>::valid_bits() const
{
  assert(m_bits != 0);
  return BitmaskImpl::fls(m_bits);
}

template<typename Int> inline bool LHBits<Int>::valid_bits(Int bits) const
{
  // Only allow bits to be on the form 2^n-1
  assert((m_bits != 0) &&
         (((bits + 1U) | bits) == (bits << 1U) + 1U)); // bits is 0..01..1
  return m_bits > bits;
}

template<typename Int> inline bool LHBits<Int>::valid_bit(Int bit) const
{
  // Only allow bit to be on the form 2^n
  assert((m_bits != 0) &&
         ((((bit - 1U) | bit) >> 1U) == bit - 1U)); // bits is 0..010..0
  return (m_bits >> 1U) >= bit;
}

template<typename Int> inline Int LHBits<Int>::get_bits(Int bits) const
{
  assert(valid_bits(bits));
  return m_bits & bits;
}

template<typename Int> inline Int LHBits<Int>::get_bit(Int bit) const
{
  assert(valid_bit(bit));
  return m_bits & bit;
}

template<typename Int> inline Int LHBits<Int>::highbit() const
{
  return 1 << (sizeof(Int) * 8 - 1);
}

/**
 * Implementaion LHLevel
 **/

inline LHLevel::LHLevel()
{
  setSize(0);
}

inline LHLevel::LHLevel(Uint32 _size)
{
  setSize(_size);
}

inline void LHLevel::clear()
{
  m_maxp = MAXP_EMPTY;
  m_p = 0;
}

inline bool LHLevel::isEmpty() const
{
  return maxp() == MAXP_EMPTY;
}

inline bool LHLevel::isFull() const
{
  return !isEmpty() && (getTop() == ADDR_MAX);
}

inline Uint32 LHLevel::max_size() const
{
  return MAX_SIZE;
}

inline Uint32 LHLevel::getSize() const
{
  assert(!isEmpty() || (maxp() + 1 + p() == 0));
  return maxp() + 1 + p();
}

inline void LHLevel::setSize(Uint32 size)
{
  assert(size <= max_size());
  if (size == 0)
    clear();
  else
  {
    m_hashcheckbit = BitmaskImpl::fls(size);
    m_maxp = (1 << hashcheckbit()) - 1;
    m_p = size - 1 - maxp();
  }
}

inline Uint32 LHLevel::getTop() const
{
  assert(!isEmpty());
  return maxp() + p();
}

inline Uint32 LHLevel::getBucketNumber(LHBits32 hash_value) const
{
  assert(!isEmpty());
  Uint32 addr = hash_value.get_bits(maxp());
  if (addr < p())
  {
    addr |= hash_value.get_bit(maxp() + 1);
  }
  return addr;
}

inline bool LHLevel::getSplitBucket(Uint32& from, Uint32& to) const
{
  assert(!isFull());

  from = p();
  to = getSize(); // == getTop() + 1

  // true if data move needed, that is, it was not empty
  return to > 0;
}

inline void LHLevel::expand()
{
  assert(!isFull());

  if (isEmpty())
  {
    m_p = 0;
    m_hashcheckbit = 0;
    m_maxp = 0;
  }
  else if (p() == maxp())
  {
    m_maxp = (maxp() << 1) | 1;
    m_hashcheckbit ++;
    m_p = 0;
  }
  else
  {
    m_p ++;
  }
}

inline bool LHLevel::shouldMoveBeforeExpand(LHBits32 hash_value) const
{
  return hash_value.get_bit(1 << hashcheckbit());
}

inline bool LHLevel::getMergeBuckets(Uint32& from, Uint32& to) const
{
  assert(!isEmpty());

  from = getTop();

  if (likely(p() != 0))
  {
    to = p() - 1;
  }
  else
  {
    to = maxp() >> 1;
  }

  // true if data move needed, that is, all buckets disappeared
  return from > 0;
}

inline void LHLevel::shrink()
{
  assert(!isEmpty());

  if (p() != 0)
  {
    m_p --;
  }
  else if (maxp() == 0)
  {
    m_maxp --; // == MAXP_EMPTY
  }
  else
  {
    m_maxp >>= 1;
    m_hashcheckbit --;
    m_p = m_maxp;
  }
}

#endif
