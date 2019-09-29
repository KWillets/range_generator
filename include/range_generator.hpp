#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <random>
#include <vector>

struct RandomSource{
  uint32_t operator()(void) { return random()<<15 ^ random(); };
};

/*
  To generate a random integer within a range, we start
  with a random fractional number (in [0,1)) and multiply it
  by the range.  The integer part of the result is the output,
  but we check the fractional part and reject the result if
  it biases the result (some integers have one more random value 
  than others).
 */
/*
  This class formalizes the accessors and a couple of arithmetic
  operations for a 32.32 bit fixed point type.
  These are all very simple but slightly obscure parts of C:
  casting 32 to 64 bits and vice-versa, right-shifting, 
  32/64 multiplication, etc.
 */
template<class rand_t>
struct Defs;
// fixedpoint value is twice as large as the random value
//template<> struct Defs<uint64_t> { using uintN_t = uint128_t;}; 
template<> struct Defs<uint32_t> { using uintN_t = uint64_t;}; 
template<> struct Defs<uint16_t> { using uintN_t = uint32_t;};
template<> struct Defs<uint8_t>  { using uintN_t = uint16_t;};

template<class uint_halfN_t> 
struct FixedPoint
{
  typename Defs<uint_halfN_t>::uintN_t val;

  FixedPoint() : val(0) {};
  void setFraction(uint_halfN_t fraction) {val = fraction;};
  void addFraction(uint_halfN_t fraction) {val += fraction;};
  FixedPoint& operator *= (uint_halfN_t x) { val *= x; return *this; };

  uint_halfN_t fraction() { return (uint_halfN_t) val; };
  uint_halfN_t floor() { return val>>(8*sizeof(uint_halfN_t)); }
};


/*
  RangeGenerator generates random integers in a range without bias.

  It uses fixed point arithmetic to generate a random value in [0,1) and multiplies it by range to get
  a number x in [0, range).  It returns floor(x) unless x is an "extra" value which is defined as follows:
 
  Since x is a multiple of range, 

  1. The fractional part of x is the first possible fraction corresponding to floor(x).  

  2. This first possible fraction is small enough for there to be room for one more value at the end.

  
 */
template <class Source>
class RangeGenerator
{
private:
  uint32_t range;
  uint32_t _extraValueThreshold;

  /*
    fraction values occur every
    (range) positions, so a fraction less than (range) must be the first
   */
  bool isFirstFraction(uint32_t f) { return f < range; };

  /*
    Calculate 2^32 mod range (a 33-bit operation)
    
    (2^32 - range) mod range is the same thing and fits in 32 bits

    This is the most expensive operation (40+ cycles), so we 
    lazy-evaluate and save the result.

   */
  uint64_t extraValueThreshold() 
  { 
    return _extraValueThreshold == UINT_MAX ? 
      _extraValueThreshold = -range % range : _extraValueThreshold;
  }

  /*
    Although fraction values occur at a regular interval, their alignment 
    with the start of the fraction range [0,2^32) may vary.

   If the first value is small enough (less than (2^32 mod range)), 
   there is room for one extra value, so we discard it to 
   maintain unbiased-ness.
   */
  bool isExtraValue(uint32_t f) { return f < extraValueThreshold(); };
  bool isRejectedValue(uint32_t f) { return isFirstFraction(f) && isExtraValue(f); }

public:
  
  RangeGenerator(uint32_t _range): range(_range), _extraValueThreshold(UINT_MAX)
  {
    if(_range <= 1)
      throw std::invalid_argument("range must be greater than 1");
  };

  uint32_t operator()(Source &src)
  {
    FixedPoint<uint32_t> x;
    do {
      x.setFraction(src());
      x *= range;
    } while(isRejectedValue(x.fraction()));

    return x.floor();
  }
};

// Similar idea but using 32.32 extending to 32.64 fixed point only as needed
/*
  With the RangeGenerator above, when range approaches 2^32, a large number of 
  generated values are less than range, meaning they have to be checked against 
  the expensive modulus.  To lessen that case, we expand the fraction value range to 2^64.

  This version uses a 64-bit fraction which is only fully calculated if the most significant 32 bits 
  don't preclude rejection.

  Rejection requires the 64-bit fraction to be less than (range), meaning its upper 32 bits must be 0.
  So we check these conditions in order before rejecting:

      1. upper 32 bits of fraction could become 0 if the product is extended by 32 more random bits.
      2. the upper 32 bits of the fraction do in fact become 0.
      3. the 64-bit fraction is less than 2^64 mod range.

  To become zero after the 32 bit extension, the upper 32 bits must be zero or within
  (range) below zero, since extending another 32 bits will only add up to (range-1) 
  carried from the right.  Negation, even for unsigned, gives us this distance.

 */

class RangeGeneratorExtended 
{
private:

  uint32_t range;
  
  uint32_t extraValueThreshold() { return - (uint64_t) range % range; };  // 2^64 mod range
  bool isFirstFraction(uint32_t f) { return f < range; };
  bool isExtraValue(uint32_t f) { return f < extraValueThreshold(); };
  bool isRejectedValue(uint32_t f) { return isFirstFraction(f) && isExtraValue(f); };
  bool canZero(uint32_t f) { return -f < range; };// f + range <= range

public:

  RangeGeneratorExtended(uint32_t _range): range(_range)
  {}

  template<class Source>
  uint32_t operator ()(Source &src)
  {
    FixedPoint<uint32_t> x, addend; 

    do {
      x.setFraction(src());
      x *= range;

      if(canZero(x.fraction()))
	{
	  addend.setFraction(src());
	  addend *= range;

	  x.addFraction(addend.floor());
	}
    } while(x.fraction() == 0 && isRejectedValue(addend.fraction()));

    return x.floor();
  }
};

/*
  This class avoids bias by calculating x to in(de)finite precision.
  The calculation stops when additional bits will not affect the result, 
  i.e. they cannot carry into the integer digits.
 */

class RangeGeneratorInfinite {

  uint32_t range;

  /*
  Rightward Continued range*(random fraction) product where we only care if it carries 

  We produce each column in this sum left-to-right:

  I0.F0
  +  I1.F1
  +     I2.F2
  +       ...


 , determine if it can carry left and if so add bits and see what happens.

 The argument fprev is the fraction from the row above, eg we start with fprev = F0 and find I1.F1 
 as needed, possibly recursing with F1.

-- Algorithm:

  First, fprev may be too small to carry under any possible addend in [0,range); if so return 0.

  Otherwise, add bits (cur) at the current position and sum (fprev + cur.floor)

  return a carry if:

  current sum wraps (CF==1) before a carry from the right is applied
  or
  current sum == 0xFFFFFFF, and we get a carry from the right (recursion)

  otherwise, the additional bits fell short, so return 0
--

 This is tail recursive; it could be a loop.

 The C idiom a + b < a typically compiles to carry flag (CF) usage

*/
  template<class Source>
  uint32_t carry(uint32_t fprev, Source &src)
  {
    if( range + fprev < fprev ) // fprev + range - 1 either wraps or reaches 0xFFFFFFFF
      {
	FixedPoint<uint32_t> cur;
	cur.setFraction(src());
	cur *= range;

	uint32_t cur_sum = cur.floor() + fprev;	

	if( cur_sum < fprev )
	  return 1;
	else if( cur_sum == -1 )
	  return carry(cur.fraction(), src);  // tail; no stack vars
      }
    return 0;
  };

public:

  RangeGeneratorInfinite(uint32_t _range): range(_range)
  {}

  template<class Source>
  uint32_t operator ()(Source &src)
  {
    FixedPoint<uint32_t> x;
    x.setFraction(src());
    x *= range;

    return x.floor() + carry(x.fraction(), src);
  };
};

