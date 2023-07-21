#include "reassembler.hh"

using namespace std;

void Reassembler::insert( uint64_t first_index, const string& data, bool is_last_substring, Writer& output )
{
  // Your code here.
  uint64_t const first_unacceptable_idx = un_assembled_index_ + output.available_capacity();
  if ( first_index >= first_unacceptable_idx ) {
    return;
  }

  if ( first_index + data.size() <= un_assembled_index_ ) {
    return;
  }

  auto new_data = data;
  auto new_first_index = first_index;
  if ( first_index + data.size() > first_unacceptable_idx ) {
    new_data = new_data.erase( first_unacceptable_idx );
  }

  if ( first_index < un_assembled_index_ ) {
    new_data = new_data.substr( un_assembled_index_ );
    new_first_index = un_assembled_index_;
  }

  // cache合并
  auto it = cache_.lower_bound( new_first_index );
  if ( it == cache_.end() ) {
    cache_[new_first_index] = new_data;
  } else {
    merge( new_first_index, new_data)
  }

  // push到bytestream
  it = cache_.find( un_assembled_index_ );
  if ( it != cache_.end() ) {
    auto count = it->second.size();
    if ( count > output.available_capacity() ) {
      count = output.available_capacity();
      auto idx = it->first + count;
      cache_[idx] = it->second.substr( idx );
    }

    output.push( it->second );

    un_assembled_index_ += count;
    cache_.erase( it->first );
  }
}

uint64_t Reassembler::bytes_pending() const
{
  // Your code here.
  return pending_size;
}
