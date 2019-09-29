#define CATCH_CONFIG_MAIN  // This tells Catch to provide a main() - only do this in one cpp file
#include "catch.hpp"
#include "range_generator.hpp"
#include "fake_rand.hpp"
#include <random>
#include <stdio.h>

TEST_CASE("Extended Range Generator", "[rge]") {
  FakeRand f{1,2,3};
  uint32_t xx = f();
  REQUIRE(xx == 1);
  REQUIRE( f.size() == 2 );
  
  RangeGeneratorExtended fge(10);
  FakeRand g{0,0,0,2};
  uint32_t x = fge(g);
  REQUIRE( x == 0 );
  REQUIRE( g.size() == 0 );
  
  
  RangeGeneratorExtended fge2(0xFFFFFFFF);
  FakeRand g2{0xFFFFFFFE,1};
  x = fge2(g2);
  REQUIRE( x == 0xFFFFFFFD );
  REQUIRE( g2.size() == 0 );

  RangeGeneratorExtended fge3(10);
  FakeRand g3{UINT_MAX/10*3+1,0};
  x = fge3(g3);
  REQUIRE( x == 2 );
  REQUIRE( g3.empty() );
  
};


TEST_CASE("Infinite Precision Generator", "[rgi]") {

  RangeGeneratorInfinite fge4(0xFFFFFFFF);

  FakeRand g4{0};
  uint32_t x = fge4(g4);
  REQUIRE( x == 0 );
  REQUIRE( g4.size() == 0); 

  FakeRand g5{1,2};
  x = fge4(g5);
  REQUIRE( x == 1 );
  REQUIRE( g5.empty() ); 

  FakeRand g6{1,1,0};
  x = fge4(g6);
  REQUIRE( x == 0 );
  REQUIRE( g6.empty() ); 

  FakeRand g7{1,1,2};
  x = fge4(g7);
  REQUIRE( x == 1 );
  REQUIRE( g7.empty() ); 
};

TEST_CASE("works with random")
{
  //  std::mt19937 generator;
  std::random_device generator;
  
  RangeGeneratorInfinite rg(10);

  uint32_t x = rg(generator);
  REQUIRE( x < 10 );

  printf("max = %lu\n", generator.max());
  int counts[10];
  for( int &c: counts)
    c=0;

  for( int i = 0; i < 10000000; i++)
    counts[rg(generator)]++;

  for( int c: counts )
    printf("%d\n", c);
};
