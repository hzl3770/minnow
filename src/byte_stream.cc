#include <stdexcept>

#include "byte_stream.hh"

using namespace std;

ByteStream::ByteStream( uint64_t capacity ) : capacity_( capacity ) {}

void Writer::push( const string& data )
{
  // Your code here.
  if ( is_closed() ) {
    return;
  }
  size_t const size = min( data.size(), available_capacity() );
  for ( size_t i = 0; i < size; i++ ) {
    buff_.emplace_back( data[i] );
  }
  write_count_ += size;
}

void Writer::close()
{
  // Your code here.
  closed_ = true;
}

void Writer::set_error()
{
  // Your code here.
  is_error_ = true;
}

bool Writer::is_closed() const
{
  // Your code here.
  return closed_;
}

uint64_t Writer::available_capacity() const
{
  // Your code here.
  return capacity_ - buff_.size();
}

uint64_t Writer::bytes_pushed() const
{
  // Your code here.
  return write_count_;
}

string_view Reader::peek() const
{
  // Your code here.
  return { std::string_view( &buff_.front(), 1 ) };
}

bool Reader::is_finished() const
{
  // Your code here.
  return closed_ && buff_.empty();
}

bool Reader::has_error() const
{
  // Your code here.
  return is_error_;
}

void Reader::pop( uint64_t len )
{
  // Your code here.
  const size_t l = min( len, buff_.size() );
  read_count_ += l;
  for ( size_t i = 0; i < l; i++ ) {
    buff_.pop_front();
  }
}

uint64_t Reader::bytes_buffered() const
{
  // Your code here.
  return buff_.size();
}

uint64_t Reader::bytes_popped() const
{
  // Your code here.
  return read_count_;
}