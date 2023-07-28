#include "tcp_receiver.hh"

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message, Reassembler& reassembler, Writer& inbound_stream )
{
  if ( message.SYN ) {
    zero_point_ = message.seqno;
  }

  if ( !zero_point_.has_value() ) {
    return;
  }

  auto check_point = inbound_stream.bytes_pushed();
  auto first_index = message.seqno.unwrap( zero_point_.value(), check_point );
  reassembler.insert( first_index, message.payload, message.FIN, inbound_stream );
  send( inbound_stream );
}

TCPReceiverMessage TCPReceiver::send( const Writer& inbound_stream ) const
{
  auto ret = TCPReceiverMessage {};
  if ( zero_point_.has_value() ) {
    ret.ackno = Wrap32::wrap( inbound_stream.bytes_pushed(), zero_point_.value() );
  }

  ret.window_size = static_cast<uint16_t>( inbound_stream.available_capacity() );
  return ret;
}
