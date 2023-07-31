#include "wrapping_integers.hh"

using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  // Your code here.
  //  return Wrap32 { static_cast<uint32_t>( ( zero_point.raw_value_ + n ) % ( 1LL << 32 ) ) };
  return zero_point + static_cast<uint32_t>( n );
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  // Your code here.
  auto c = wrap( checkpoint, zero_point );
  uint32_t const offset = raw_value_ - c.raw_value_;
  uint64_t ret = offset + checkpoint;

  if ( offset >= 1UL << 31 && ret > UINT32_MAX ) {
    ret -= 1UL << 32;
  }
  return ret;
}
