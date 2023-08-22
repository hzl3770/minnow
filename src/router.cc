#include "router.hh"

#include <iostream>
#include <limits>
#include <map>

using namespace std;

// route_prefix: The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
// prefix_length: For this route to be applicable, how many high-order (most-significant) bits of
//    the route_prefix will need to match the corresponding bits of the datagram's destination address?
// next_hop: The IP address of the next hop. Will be empty if the network is directly attached to the router (in
//    which case, the next hop address should be the datagram's final destination).
// interface_num: The index of the interface to send the datagram out on.
void Router::add_route( const uint32_t route_prefix,
                        const uint8_t prefix_length,
                        const optional<Address> next_hop,
                        const size_t interface_num )
{
  cerr << "DEBUG: adding route " << Address::from_ipv4_numeric( route_prefix ).ip() << "/"
       << static_cast<int>( prefix_length ) << " => " << ( next_hop.has_value() ? next_hop->ip() : "(direct)" )
       << " on interface " << interface_num << "\n";

  RoutingTableEntry entry;
  entry.route_prefix = route_prefix;
  entry.prefix_length = prefix_length;
  entry.next_hop = next_hop;
  entry.interface_num = interface_num;

  routing_table_.emplace_back( entry );
}

void Router::route()
{
  int i = 0;
  for ( auto& interface : interfaces_ ) {
    clog << "-----route-----" << i++ << "-------" << endl;
    auto dgram = interface.maybe_receive();
    if ( !dgram.has_value() ) {
      continue;
    }
    clog << "route::dgram: " << dgram.value().header.to_string() << endl;
    const auto dst_ip = dgram.value().header.dst;

    std::map<uint8_t, RoutingTableEntry> appropriateRoutes {};
    for ( auto& entry : routing_table_ ) {
      if ( compareIP( entry.route_prefix, dst_ip, entry.prefix_length ) ) {
        appropriateRoutes[entry.prefix_length] = entry;
      }
    }

    if ( appropriateRoutes.empty() ) {
      continue;
    }

    if ( dgram.value().header.ttl <= 1 ) {
      continue;
    }
    dgram.value().header.ttl--;

    // !!!!!!!!!!!
    dgram.value().header.compute_checksum();

    auto route = appropriateRoutes.rbegin()->second;
    clog << "route_prefix: " << route.route_prefix << " interface_num: " << route.interface_num << endl;
    clog << "prefix_length: " << route.prefix_length
         << ( route.next_hop.has_value() ? route.next_hop->ip() : "(direct)" ) << endl;

    auto& out_interface = this->interface( route.interface_num );

    if ( route.next_hop.has_value() ) {
      clog << "next_hop: " << route.next_hop->ip() << endl;
      out_interface.send_datagram( dgram.value(), route.next_hop.value() );
    } else {
      clog << "dst_ip: " << Address::from_ipv4_numeric( dst_ip ).ip() << endl;
      clog << Address::from_ipv4_numeric( dst_ip ).to_string() << endl;
      out_interface.send_datagram( dgram.value(), Address::from_ipv4_numeric( dst_ip ) );
    }

    clog << "route end " << endl;
  }
}
bool Router::compareIP( uint32_t ip1, uint32_t ip2, uint8_t prefix_length )
{
  uint32_t const mask = prefix_length == 0 ? 0 : ( 0xFFFFFFFF << ( 32 - prefix_length ) );
  return ( ip1 & mask ) == ( ip2 & mask );
}