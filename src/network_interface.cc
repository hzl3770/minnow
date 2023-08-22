#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

using namespace std;

// ethernet_address: Ethernet (what ARP calls "hardware") address of the interface
// ip_address: IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface( const EthernetAddress& ethernet_address, const Address& ip_address )
  : ethernet_address_( ethernet_address ), ip_address_( ip_address )
{
  cerr << "DEBUG: Network interface has Ethernet address " << to_string( ethernet_address_ ) << " and IP address "
       << ip_address.ip() << "\n";
}

// dgram: the IPv4 datagram to be sent
// next_hop: the IP address of the interface to send it to (typically a router or default gateway, but
// may also be another host if directly connected to the same network as the destination)

// Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) by using the
// Address::ipv4_numeric() method.
void NetworkInterface::send_datagram( const InternetDatagram& dgram, const Address& next_hop )
{
  clog << "DEBUG: NetworkInterface::send_datagram: " << dgram.header.to_string() << " next_hop: " << next_hop.ip()
       << endl;

  clog << "DEBUG: NetworkInterface::send_datagram: ip_address_: " << ip_address_.ip() << endl;
  clog << "DEBUG: NetworkInterface::send_datagram: ethernet_address_: " << to_string( ethernet_address_ ) << endl;

  const auto dst_ip = next_hop.ipv4_numeric();
  auto it = arp_cache_.find( dst_ip );
  if ( it != arp_cache_.end() ) {
    clog << "DEBUG: NetworkInterface::send_datagram: arp_cache_ hit: " << to_string( it->second ) << endl;
    auto ef = make_frame( it->second, EthernetHeader::TYPE_IPv4, serialize( dgram ) );
    pending_frames_.emplace_back( std::move( ef ) );
    clog << "DEBUG: NetworkInterface::send_datagram: pending_frames_.size(): " << pending_frames_.size() << endl;
    return;
  }

  clog << "DEBUG: NetworkInterface::send_datagram: arp_cache_ miss" << endl;
  // 构造arp request
  auto arp_msg = make_arp( ARPMessage::OPCODE_REQUEST, {}, next_hop.ipv4_numeric() );
  clog << "DEBUG: NetworkInterface::send_datagram: arp_msg: " << arp_msg.to_string() << endl;
  auto ef = make_frame( ETHERNET_BROADCAST, EthernetHeader::TYPE_ARP, serialize( arp_msg ) );
  pending_frames_.emplace_back( std::move( ef ) );

  unknown_dst_datagrams_[dst_ip] = dgram;
  clog << "DEBUG: NetworkInterface::send_datagram: pending_frames_.size(): " << pending_frames_.size() << endl;
}

// frame: the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame( const EthernetFrame& frame )
{
  clog << "DEBUG: NetworkInterface::recv_frame: receiver_mac" << to_string( ethernet_address_ ) << endl;
  clog << "DEBUG: NetworkInterface::recv_frame: receiver_ip" << ip_address_.ip() << endl;
  clog << "DEBUG: NetworkInterface::recv_frame: " << frame.header.to_string() << endl;
  if ( frame.header.dst != ETHERNET_BROADCAST && frame.header.dst != ethernet_address_ ) {
    return {};
  }

  if ( frame.header.type == EthernetHeader::TYPE_IPv4 ) {
    clog << "DEBUG: NetworkInterface::recv_frame: TYPE_IPv4" << endl;
    InternetDatagram dgram;
    auto ret = parse( dgram, frame.payload );
    if ( !ret ) {
      clog << "DEBUG: NetworkInterface::recv_frame: parse failed" << endl;
      return {};
    }
    clog << "DEBUG: NetworkInterface::recv_frame: dgram: " << dgram.header.to_string() << endl;
    return { dgram };
  }

  if ( frame.header.type == EthernetHeader::TYPE_ARP ) {
    clog << "DEBUG: NetworkInterface::recv_frame: TYPE_ARP" << endl;
    ARPMessage arg_msg;
    auto ret = parse( arg_msg, frame.payload );
    if ( !ret ) {
      return {};
    }

    // 目的ip 不是自己，无视
    if ( arg_msg.target_ip_address != ip_address_.ipv4_numeric() ) {
      return {};
    }
    clog << "DEBUG: NetworkInterface::recv_frame: arg_msg: " << arg_msg.to_string() << endl;
    if ( arg_msg.opcode == ARPMessage::OPCODE_REPLY ) {
      arp_cache_[arg_msg.sender_ip_address] = arg_msg.sender_ethernet_address;
      arp_cache_remainder_[arg_msg.sender_ip_address] = expired_ms;

      auto it = unknown_dst_datagrams_.find( arg_msg.sender_ip_address );
      if ( it != unknown_dst_datagrams_.end() ) {
        auto ef = make_frame( arg_msg.sender_ethernet_address, EthernetHeader::TYPE_IPv4, serialize( it->second ) );
        pending_frames_.emplace_back( ef );
        unknown_dst_datagrams_.erase( it );
      }
    }

    if ( arg_msg.opcode == ARPMessage::OPCODE_REQUEST ) {
      arp_cache_[arg_msg.sender_ip_address] = arg_msg.sender_ethernet_address;
      arp_cache_remainder_[arg_msg.sender_ip_address] = expired_ms;

      auto arp = make_arp( ARPMessage::OPCODE_REPLY, arg_msg.sender_ethernet_address, arg_msg.sender_ip_address );
      auto ef = make_frame( arg_msg.sender_ethernet_address, EthernetHeader::TYPE_ARP, serialize( arp ) );
      pending_frames_.emplace_back( ef );
    }
  }

  return {};
}

// ms_since_last_tick: the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  for ( auto it = arp_cache_remainder_.begin(); it != arp_cache_remainder_.end(); ) {
    if ( it->second > ms_since_last_tick ) {
      it->second -= ms_since_last_tick;
      it++;
      continue;
    }

    arp_cache_.erase( it->first );
    arp_cache_remainder_.erase( it++ );
  }

  for ( auto it = arp_pending_remainder_.begin(); it != arp_pending_remainder_.end(); ) {
    if ( it->second > ms_since_last_tick ) {
      it->second -= ms_since_last_tick;
      it++;
      continue;
    }
    arp_pending_remainder_.erase( it++ );
  }
}

optional<EthernetFrame> NetworkInterface::maybe_send()
{
  if ( pending_frames_.empty() ) {
    clog << "DEBUG: NetworkInterface::maybe_send: pending_frames_.empty()" << endl;
    return {};
  }

  auto frame = pending_frames_.front();
  pending_frames_.pop_front();
  clog << "DEBUG: NetworkInterface::maybe_send: " << frame.header.to_string() << endl;

  // 打印pending_frames_的长度
  clog << "DEBUG: NetworkInterface::maybe_send: pending_frames_.size(): " << pending_frames_.size() << endl;

  if ( frame.header.type == EthernetHeader::TYPE_ARP ) {
    clog << "DEBUG: NetworkInterface::maybe_send: TYPE_ARP" << endl;
    ARPMessage arp;
    auto ret = parse( arp, frame.payload );
    if ( !ret ) {
      return {};
    }

    auto it = arp_pending_remainder_.find( arp.target_ip_address );
    if ( it != arp_pending_remainder_.end() ) {
      return {};
    }

    arp_pending_remainder_[arp.target_ip_address] = pending_ms;
  }

  return { frame };
}
EthernetFrame NetworkInterface::make_frame( const EthernetAddress& dst, uint16_t type, std::vector<Buffer> payload )
{

  EthernetFrame frame;
  frame.header.src = ethernet_address_;
  frame.header.dst = dst;
  frame.header.type = type;
  frame.payload = std::move( payload );
  return frame;
}

ARPMessage NetworkInterface::make_arp( uint16_t opcode,
                                       EthernetAddress target_ethernet_address,
                                       const uint32_t target_ip_address )
{
  ARPMessage arp;
  arp.opcode = opcode;
  arp.sender_ethernet_address = ethernet_address_;
  arp.sender_ip_address = ip_address_.ipv4_numeric();
  arp.target_ethernet_address = target_ethernet_address;
  arp.target_ip_address = target_ip_address;
  return arp;
}
