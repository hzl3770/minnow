#include "tcp_receiver.hh"

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message, Reassembler& reassembler, Writer& inbound_stream )
{
  if ( message.SYN ) {
    is_sync_ = message.SYN;
    zero_point_ = message.seqno;
  }
  if ( is_sync_ && message.FIN ) {
    is_fin_ = message.FIN;
  }

  if ( zero_point_.has_value() ) {
    auto check_point = inbound_stream.bytes_pushed();
    auto first_index = message.seqno.unwrap( zero_point_.value(), check_point );
    if ( !message.SYN ) {
      first_index--;
    }
    reassembler.insert( first_index, message.payload, message.FIN, inbound_stream );
  }

}

TCPReceiverMessage TCPReceiver::send( const Writer& inbound_stream ) const
{
  auto ret = TCPReceiverMessage {};
  if ( zero_point_.has_value() ) {
    auto ack_no = inbound_stream.bytes_pushed();
    if ( is_sync_ ) {
      ack_no++;
    }

    if ( is_fin_ && inbound_stream.is_closed() ) {
      ack_no++;
    }

    ret.ackno = Wrap32::wrap( ack_no, zero_point_.value() );
  }

  if ( inbound_stream.available_capacity() >= UINT16_MAX ) {
    ret.window_size = UINT16_MAX;
  } else {
    ret.window_size = static_cast<uint16_t>( inbound_stream.available_capacity() );
  }

  return ret;
}
