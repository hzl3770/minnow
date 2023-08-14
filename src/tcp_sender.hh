#pragma once

#include "byte_stream.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"
#include <map>

class Timer
{
  bool running_ { false };
  uint64_t rto_ {};
  uint64_t remainder_ {};

public:
  void printInfo() const;
  void start( uint64_t rto );
  void stop();
  bool is_running() const;
  void elapse( uint64_t elapse_time );
  bool is_expire() const;
  void reset(bool is_double);
};

class TCPSender
{
  Wrap32 isn_;
  uint64_t initial_RTO_ms_;

  // 是否已经发送sync
  bool send_sync_ { false };

  // 是否已经发送fin
  bool send_fin_ { false };

  // 默认对方窗口为1
  uint16_t window_ { 1 };

  // 最小未使用的seqno
  uint64_t next_seqno_absolute { 0 };

  // 最小未确认的seqno
  uint64_t base_seqno { 0 };

  // 待seq_no_absolute = seqno.unwrap(isn, new_next_seqno_absolute) + sequence_length
  std::map<uint64_t, TCPSenderMessage> outstanding_segment_ {};

  // 存放待发送数据
  std::deque<TCPSenderMessage> pending_segment_ {};

  // 连续重发次数
  uint64_t consecutive_retransmissions_num_ { 0 };

  Timer timer_;

public:
  /* Construct TCP sender with given default Retransmission Timeout and possible ISN */
  TCPSender( uint64_t initial_RTO_ms, std::optional<Wrap32> fixed_isn );

  /* Push bytes from the outbound stream */
  void push( Reader& outbound_stream );

  /* Send a TCPSenderMessage if needed (or empty optional otherwise) */
  std::optional<TCPSenderMessage> maybe_send();

  /* Generate an empty TCPSenderMessage */
  TCPSenderMessage send_empty_message() const;

  /* Receive an act on a TCPReceiverMessage from the peer's receiver */
  void receive( const TCPReceiverMessage& msg );

  /* Time has passed by the given # of milliseconds since the last time the tick() method was called. */
  void tick( uint64_t ms_since_last_tick );

  /* Accessors for use in testing */
  uint64_t sequence_numbers_in_flight() const;  // How many sequence numbers are outstanding?
  uint64_t consecutive_retransmissions() const; // How many consecutive *re*transmissions have happened?

  void printInfo( const std::string& name ) const;
};