#include "reassembler.hh"
#include <iostream>
using namespace std;

void Reassembler::insert( uint64_t first_index, const string& data, bool is_last_substring, Writer& output )
{
  // Your code here.

//  cout << "\n\n"
//       << "insert first_idx: " << first_index << " data.size(): " << data.size()
//       << " is_last: " << is_last_substring << "  cc: " << ++cc << " Uxxx: " << un_assembled_index_
//       << " last_idx: " << last_index_ << endl;
  if ( is_last_substring ) {
    last_index_ = first_index + data.size();
    is_last_ = true;
  }

  if ( data.empty() && last_index_ == output.bytes_pushed() && is_last_ ) {
    output.close();
    return;
  }

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
    new_data = new_data.erase( first_unacceptable_idx - first_index );
  }

  if ( first_index < un_assembled_index_ ) {
    new_data = new_data.substr( un_assembled_index_ - first_index );
    new_first_index = un_assembled_index_;
  }

//  cout << "new_first_idx: " << new_first_index << "  old_first_idx: " << first_index << endl;
//  cout << "new_data.size(): " << new_data.size() << "  old_data.size(): " << data.size() << endl;

  // cache合并
  merge( new_data, new_first_index );

  // push到bytestream
  auto it = cache_.find( un_assembled_index_ );
  if ( it != cache_.end() ) {
    auto count = it->second.size();
    if ( count > output.available_capacity() ) {
      count = output.available_capacity();
      auto idx = it->first + count;
      cache_[idx] = it->second.substr( count );
    }

    output.push( it->second );

    un_assembled_index_ += count;
    cache_.erase( it );
  }

  if ( output.bytes_pushed() == last_index_ && is_last_ ) {
    output.close();
  }

//  cout << "cache_ ---- size: " << cache_.size() << " all_count: " << bytes_pending() << endl;
//  for ( const auto& t : cache_ ) {
//    cout << t.first << " size: " << t.second.size() << endl;
//  }
//  cout << "----- cache_ end-------" << endl;
}

void Reassembler::merge( string& new_data, uint64_t new_first_index )
{
//  cout << "\n new_first_index: " << new_first_index << " size: " << new_data.size() << " into merge" << endl;
  auto it = cache_.lower_bound( new_first_index );
  if ( it == cache_.end() ) {
    if ( !cache_.empty() ) {
      it--;
      auto t_tail = it->first + it->second.size();
      /*
       *   ==========
       *            -
       */
      if ( t_tail > new_first_index && new_first_index + new_data.size() <= t_tail ) {
        return;
      }

      /*
       *   =========
       *            ----
       *   =========
       *         -------
       */

      if ( t_tail >= new_first_index && new_first_index + new_data.size() > t_tail ) {
// !!! 找到1天半的       new_data = it->second + new_data.substr( new_first_index + new_data.size() - t_tail );
        new_data = it->second.substr( 0, new_first_index - it->first ) + new_data;
        new_first_index = it->first;
      }
    }

    cache_[new_first_index] = new_data;
//    cout << "new idx 2 inert cache: " << new_first_index << "  size: " << new_data.size() << endl;
    return;
  }
  /*
   * t: ========
   * ni:     --------
   *    =====***-----
   */
  if ( it->first > new_first_index && it != cache_.begin() ) {
//    cout << "qk 2 start: " << it->first << endl;
    --it;

    if ( it->first + it->second.size() >= new_first_index ) {
      if ( it->first + it->second.size() >= new_first_index + new_data.size() ) {
        return;
      }

      new_data = it->second.substr( 0, new_first_index - it->first ) + new_data;

      new_first_index = it->first;
//      cout << "qqq1: " << it->first << endl;
      it = cache_.erase( it );
//      cout << "hhh: " << new_first_index << endl;
//      cout << "qqq2: " << it->first << endl;
    } else {
      it++;
    }
//    cout << "qk 2 end: " << it->first << endl;
  }

  while ( it != cache_.end() ) {
    auto new_data_tail = new_first_index + new_data.size();
    auto item_tail = it->first + it->second.size();
    if ( new_data_tail < it->first ) {
//      cout << "break sbydx" << endl;
      break;
    }

    /*
     *            |
     * it: ===========
     * ni: -------
     *     -------====
     */
    if ( new_first_index == it->first && it->second.size() > new_data.size() ) {
      new_data = it->second;
    }

    /*
     *
     *            |
     * it:    ===========
     * ni: -------
     *     ---****=======
     */
    if ( new_first_index < it->first && new_data_tail < item_tail ) {
      new_data += it->second.substr( new_data_tail - it->first );
    }
//    cout << "qqq1 s: " << it->first << endl;
    cache_.erase( it++ );
//    cout << "qqq1 e: " << it->first << endl;
  }

//  cout << "new idx 2 inert cache: " << new_first_index << "  size: " << new_data.size() << endl;
  cache_[new_first_index] = new_data;
}

uint64_t Reassembler::bytes_pending() const
{
  // Your code here.
  uint64_t ret = 0;
  for ( const auto& it : cache_ ) {
    ret += it.second.size();
  }
  return ret;
}
