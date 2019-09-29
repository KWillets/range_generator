# range_generator
Some variations on Lemire's  Fast Random Integer Generation in an Interval

Lemire's technique for generating unbiased integers in an interval is based on fixed-point multiplication of an
integer range and a fixed-precision random value in [0,1), and a rule for discarding "extra" values based on the fractional
part of the product.

The technique avoids division/modulo operations for small intervals, 
but as the interval approaches the entire integer range the case for division approaches 100%.  

The main reason for this is a coincidence, as the fractional part we are evaluating is the same precision as the range argument.
Since the method's initial check is whether the fraction is less than the range, we can construct a pathological case of, say,
range = 0xFFFFFFFF (for 32-bit) and cause a modulo under every possible random value except one (1).  

One way to get around this is to make the fraction longer than the range, for instance a 64-bit fraction for a 32-bit range argument.
We use the same rule for discarding values, but in a 64-bit range of fractions, and our initial condition for discarding 
is against a 32-bit value.  

This is called 32.64 fixed point, and we use a trick which may speed things up:  given such a wide range of fractions, and a 
small range to check against, we can calculate the most significant bits of the fraction first and only calculate the least significant
when discarding has not been ruled out, or when the lsb's may carry into the integer portion of the result (these turn out to be almost the same condition).

## To Infinity and Beyond 

After figuring all this out, I realized that another way to avoid bias is to carry out the calculation to "infinite" precision.
The same method for left-to-right multiplication allows us to get the integer portion of the result without evaluating an infinite number of bits.

## Other notes

Part of the fun is using the carry flag (CF) to figure out if the fraction can wraparound through 0 if more bits are 
calculated.  A more artful programmer might be able to use ADC or some such with this.

I'm only part way through generalizing 32/64 bit templating.

This uses the std::random interface to allow use of various random bit sources.  

For testing, there's a FakeRandom class which will feed a supplied sequence of values to the caller.




