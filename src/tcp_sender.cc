#include "tcp_sender.hh"
#include "tcp_config.hh"

#include <iostream>
#include <random>

using namespace std;

/* TCPSender constructor (uses a random ISN if none given) */
TCPSender::TCPSender( uint64_t initial_RTO_ms, optional<Wrap32> fixed_isn )
  : isn_( fixed_isn.value_or( Wrap32 { random_device()() } ) ), initial_RTO_ms_( initial_RTO_ms ), timer_()
{}

uint64_t TCPSender::sequence_numbers_in_flight() const
{
  // Your code here.
  printInfo( "sequence_numbers_in_flight" );
  return next_seqno_absolute - base_seqno;
}

uint64_t TCPSender::consecutive_retransmissions() const
{
  // Your code here.
  return consecutive_retransmissions_num_;
}

optional<TCPSenderMessage> TCPSender::maybe_send()
{
  // Your code here.
  printInfo( "meybe_send" );
  if ( pending_segment_.empty() ) {
    return {};
  }

  auto msg = pending_segment_.front();
  pending_segment_.pop_front();
  auto k = msg.seqno.unwrap( isn_, next_seqno_absolute ) + msg.sequence_length();

  auto it = outstanding_segment_.find( k );
  if ( it == outstanding_segment_.end() ) {
    outstanding_segment_[k] = msg;
  }

  // 开启定时器
  if ( !timer_.is_running() ) {
    timer_.start( initial_RTO_ms_ );
  }

  return { msg };
}

void TCPSender::push( Reader& outbound_stream )
{
  // Your code here.
  printInfo( "push 1" );

  // 已发送fin_表示没有数据可以发送
  if ( send_fin_ ) {
    return;
  }

  uint64_t const cur_window = window_ == 0 ? 1 : window_;

  clog << "*push*" << endl;
  printInfo( "push 2" );

  if ( base_seqno + cur_window <= next_seqno_absolute ) {
    return;
  }

  // 已经发送sync，且读取还没完成，但是现在可读取数据长度为0，直接返回
  if ( send_sync_ && !outbound_stream.is_finished() && outbound_stream.bytes_buffered() == 0 ) {
    return;
  }

  printInfo( "push 3" );

  // 获取可用的大小
  auto available_len = base_seqno + cur_window - next_seqno_absolute;
  while ( available_len > 0 ) {
    TCPSenderMessage msg;
    msg.seqno = Wrap32::wrap( next_seqno_absolute, isn_ );
    if ( !send_sync_ ) {
      send_sync_ = true;
      msg.SYN = true;
      // sync会占用一个序号位置
      available_len--;
    }

    auto len = min( TCPConfig::MAX_PAYLOAD_SIZE, available_len );

    std::string output_data;
    auto read_len = min( outbound_stream.bytes_buffered(), len );
    for ( size_t i = 0; i < read_len; i++ ) {
      output_data += outbound_stream.peek();
      outbound_stream.pop( 1 );
    }
    msg.payload = output_data;

    if ( msg.sequence_length() >= available_len ) {
      available_len = 0;
    } else {
      available_len -= msg.sequence_length();
    }

    if ( !send_fin_ && outbound_stream.is_finished() && available_len > 0 ) {
      send_fin_ = true;
      msg.FIN = true;
    }

    next_seqno_absolute += msg.sequence_length();
    pending_segment_.emplace_back( msg );

    if ( !outbound_stream.bytes_buffered() ) {
      break;
    }
  }
  clog << "sbydx" << endl;

  //  if ( outbound_stream.bytes_buffered() < available_len ) {
  //    while ( outbound_stream.bytes_buffered() ) {
  //      output_data += outbound_stream.peek();
  //      outbound_stream.pop( 1 );
  //    }
  //
  //    msg.payload = output_data;
  //
  //    // 还没发送fin，且已经读取已经完成
  //    if ( !send_fin_ && outbound_stream.is_finished() ) {
  //      send_fin_ = true;
  //      msg.FIN = true;
  //    }
  //
  //  } else {
  //    for ( size_t i = 0; i < available_len; i++ ) {
  //      output_data += outbound_stream.peek();
  //      outbound_stream.pop( 1 );
  //    }
  //    msg.payload = output_data;
  //  }

  printInfo( "push 4" );
}

TCPSenderMessage TCPSender::send_empty_message() const
{
  // Your code here.
  return { Wrap32::wrap( next_seqno_absolute, isn_ ) };
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  printInfo( "receive 1" );
  // 通过测试用例得知，无论收到咋样的，都要更新win
  window_ = msg.window_size;
  if ( !msg.ackno.has_value() ) {
    return;
  }

  const auto seqno = msg.ackno->unwrap( isn_, next_seqno_absolute );
  // （beyond next seqno) is ignored
  if ( seqno > next_seqno_absolute ) {
    return;
  }

  clog << "receive " << seqno << " base_seqno:" << base_seqno << endl;
  auto it = outstanding_segment_.find( seqno );
  if ( it == outstanding_segment_.end() ) {
    return;
  }

  outstanding_segment_.erase( outstanding_segment_.begin(), ++it );
  base_seqno = seqno;

  if ( outstanding_segment_.empty() ) {
    timer_.stop();
    consecutive_retransmissions_num_ = 0;
  } else {
    timer_.start( initial_RTO_ms_ );
  }

  printInfo( "receive 2" );
}

void TCPSender::tick( const size_t ms_since_last_tick )
{
  // Your code here.
  if ( !timer_.is_running() ) {
    return;
  }

  timer_.elapse( ms_since_last_tick );
  if ( !timer_.is_expire() ) {
    return;
  }

  if ( outstanding_segment_.empty() ) {
    return;
  }
  // 重新发送
  pending_segment_.emplace_back( outstanding_segment_.begin()->second );
  consecutive_retransmissions_num_++;

  timer_.reset(window_ != 0);
}

void TCPSender::printInfo( const std::string& name ) const
{
  cout << "-------" << name << "------" << endl;
  clog << "send_sync_ " << send_sync_ << endl;
  clog << "send_fin_ " << send_fin_ << endl;
  clog << "window_ " << window_ << endl;
  clog << "base_seqno " << base_seqno << endl;
  clog << "next_seqno_absolute " << next_seqno_absolute << endl;

  cout << "outstanding_segment_ size: " << outstanding_segment_.size() << endl;
  for ( auto [k, v] : outstanding_segment_ ) {
    clog << k << "," << v.seqno.unwrap( isn_, next_seqno_absolute ) << endl;
    clog << v.payload.operator std::string_view() << endl;
  }

  cout << "pending_segment_ size: " << pending_segment_.size() << endl;
  for ( const auto& it : pending_segment_ ) {
    clog << it.seqno.unwrap( isn_, next_seqno_absolute ) << ", size:" << it.sequence_length() << endl;
    //    clog << it.payload.operator std::string_view() << endl;
    clog << it.FIN << endl;
    clog << it.SYN << endl;
    clog << it.payload.size() << endl;
  }

  this->timer_.printInfo();
  cout << "--------------" << endl;
}

void Timer::start( uint64_t rto )
{
  rto_ = rto;
  remainder_ = rto;
  running_ = true;
}

void Timer::stop()
{
  running_ = false;
}

bool Timer::is_running() const
{
  return running_;
}

void Timer::elapse( uint64_t elapse_time )
{
  if ( elapse_time < remainder_ ) {
    remainder_ -= elapse_time;
    return;
  }

  remainder_ = 0;
}

bool Timer::is_expire() const
{
  return remainder_ == 0;
}

void Timer::reset( bool is_double )
{
  if ( is_double ) {
    rto_ *= 2;
  }
  remainder_ = rto_;
}
void Timer::printInfo() const
{
  clog << "rto: " << rto_ << " remainder: " << remainder_ << endl;
}
