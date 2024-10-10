#ifndef SPONGE_LIBSPONGE_TCP_SENDER_HH
#define SPONGE_LIBSPONGE_TCP_SENDER_HH

#include "buffer.hh"
#include "byte_stream.hh"
#include "tcp_config.hh"
#include "tcp_segment.hh"
#include "wrapping_integers.hh"

#include <functional>
#include <queue>

/*
 * TCPSender Class
 *
 * This class implements the sender side of a TCP connection. It is responsible for
 * managing the outgoing data stream, creating and sending TCP segments, handling
 * acknowledgments, and implementing the TCP congestion control algorithm.
 *
 * Key Components:
 * - Outbound queue of segments (segments_out)
 * - Retransmission timer
 * - Outgoing byte stream
 * - Sequence number tracking
 * - Sender window management
 * - Connection state tracking
 *
 * Main Operations:
 * - Filling the window with new segments
 * - Processing incoming acknowledgments
 * - Managing retransmissions
 * - Handling timeouts
 */

//! \brief The "sender" part of a TCP implementation.

//! Accepts a ByteStream, divides it up into segments and sends the
//! segments, keeps track of which segments are still in-flight,
//! maintains the Retransmission Timer, and retransmits in-flight
//! segments if the retransmission timer expires.
class TCPSender {
  private:
    //! Initial sequence number.
    const WrappingInt32 _isn;

    //! Outbound queue of segments that the TCPSender wants sent
    std::queue<TCPSegment> _segments_out{};

    //! Retransmission timer configuration
    unsigned int _initial_retransmission_timeout;
    unsigned int _current_rto;

    //! Outgoing stream of bytes
    ByteStream _stream;

    //! The (absolute) sequence number for the next byte to be sent
    uint64_t _next_seqno{0};

    //! Connection state flags
    enum class State { CLOSED, SYN_SENT, SYN_ACKED, FIN_SENT } _state{State::CLOSED};

    //! Sender window tracking
    uint64_t _bytes_in_flight = 0;
    uint16_t _window_size{1};  // Start with 1 to allow sending SYN
    std::queue<TCPSegment> _segments_outstanding{};

    //! Retransmission tracking
    uint16_t _consecutive_retransmissions{0};
    unsigned int _time_elapsed{0};
    bool _timer_running{false};

    // Lab4 modify:
    // bool _fill_window_called_by_ack_received{false};

    //! Helper methods
    bool is_ack_valid(uint64_t abs_ackno) const;
    void send_segment(TCPSegment &seg);
    void start_timer();
    void stop_timer();
    void reset_timer();

  public:
    //! Initialize a TCPSender
    TCPSender(const size_t capacity = TCPConfig::DEFAULT_CAPACITY,
              const uint16_t retx_timeout = TCPConfig::TIMEOUT_DFLT,
              const std::optional<WrappingInt32> fixed_isn = {});

    //! \name "Input" interface for the writer
    //!@{
    ByteStream &stream_in() { return _stream; }
    const ByteStream &stream_in() const { return _stream; }
    //!@}

    //! \name Methods that can cause the TCPSender to send a segment
    //!@{

    //! \brief A new acknowledgment was received
    void ack_received(const WrappingInt32 ackno, const uint16_t window_size);

    //! \brief Generate an empty-payload segment (useful for creating empty ACK segments)
    void send_empty_segment();

    //! \brief create and send segments to fill as much of the window as possible
    void fill_window();

    //! \brief Notifies the TCPSender of the passage of time
    void tick(const size_t ms_since_last_tick);
    //!@}

    //! \name Accessors
    //!@{

    //! \brief How many sequence numbers are occupied by segments sent but not yet acknowledged?
    //! \note count is in "sequence space," i.e. SYN and FIN each count for one byte
    //! (see TCPSegment::length_in_sequence_space())
    size_t bytes_in_flight() const { return _bytes_in_flight; };

    //! \brief Number of consecutive retransmissions that have occurred in a row
    unsigned int consecutive_retransmissions() const { return _consecutive_retransmissions; };

    //! \brief TCPSegments that the TCPSender has enqueued for transmission.
    //! \note These must be dequeued and sent by the TCPConnection,
    //! which will need to fill in the fields that are set by the TCPReceiver
    //! (ackno and window size) before sending.
    std::queue<TCPSegment> &segments_out() { return _segments_out; }
    //!@}

    //! \name What is the next sequence number? (used for testing)
    //!@{

    //! \brief absolute seqno for the next byte to be sent
    uint64_t next_seqno_absolute() const { return _next_seqno; }

    //! \brief relative seqno for the next byte to be sent
    WrappingInt32 next_seqno() const { return wrap(_next_seqno, _isn); }
    //!@}
};

#endif  // SPONGE_LIBSPONGE_TCP_SENDER_HH