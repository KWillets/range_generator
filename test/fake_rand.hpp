#include <deque>

struct FakeRand: public std::deque<uint32_t>
{
  using deque<uint32_t>::deque;
  
  uint32_t operator ()(void) {
    uint32_t ret = deque<uint32_t>::front();
    deque<uint32_t>::pop_front();
    return ret;
  };
};

