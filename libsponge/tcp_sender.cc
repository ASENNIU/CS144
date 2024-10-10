#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>
// #include <iostream>
#include <algorithm>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

/*
 * TCPSender Implementation
 *
 * This file contains the implementation of the TCPSender class methods.
 * It handles the core logic of the TCP sender, including:
 * - Segment creation and transmission
 * - Window management
 * - Acknowledgment processing
 * - Retransmission handling
 * - Timer management
 */

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout(retx_timeout)
    , _current_rto(retx_timeout)
    , _stream(capacity) {}

void TCPSender::fill_window() {
    // Send SYN if not sent yet
    if (_state == State::CLOSED) {
        TCPSegment seg;
        seg.header().syn = true;
        send_segment(seg);
        _state = State::SYN_SENT;
        return;
    }

    // Don't send if SYN hasn't been acknowledged
    if (_state == State::SYN_SENT) {
        return;
    }

    // Calculate available window size
    uint16_t window_size = _window_size ? _window_size : 1;  // Treat 0 as 1 for zero window probing
    uint64_t abs_ackno = _next_seqno - _bytes_in_flight;
    uint64_t window_right_edge = abs_ackno + window_size;

    // Send data segments
    while (_next_seqno < window_right_edge &&
           (!_stream.buffer_empty() || (_stream.eof() && _state != State::FIN_SENT))) {
        TCPSegment seg;
        size_t payload_size = min({_stream.buffer_size(),
                                   static_cast<size_t>(window_right_edge - _next_seqno),
                                   static_cast<size_t>(TCPConfig::MAX_PAYLOAD_SIZE)});

        // Read data from the stream
        seg.payload() = Buffer{_stream.read(payload_size)};

        // Set FIN flag if this is the last segment and there's room in the window
        if (_stream.eof() && _next_seqno + seg.length_in_sequence_space() < window_right_edge) {
            seg.header().fin = true;
            _state = State::FIN_SENT;
        }

        if (seg.length_in_sequence_space() == 0) {
            break;  // Avoid sending empty segments
        }

        send_segment(seg);

        if (seg.header().fin) {
            break;  // Stop after sending FIN
        }
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    uint64_t abs_ackno = unwrap(ackno, _isn, _next_seqno);
    if (!is_ack_valid(abs_ackno)) {
        return;
    }

    _window_size = window_size;

    // Process acknowledged segments
    bool segments_acked = false;
    while (!_segments_outstanding.empty()) {
        const auto &seg = _segments_outstanding.front();
        uint64_t seg_seqno = unwrap(seg.header().seqno, _isn, _next_seqno);
        if (seg_seqno + seg.length_in_sequence_space() <= abs_ackno) {
            _bytes_in_flight -= seg.length_in_sequence_space();
            _segments_outstanding.pop();
            segments_acked = true;
        } else {
            break;
        }
    }

    if (segments_acked) {
        // Reset retransmission parameters
        _current_rto = _initial_retransmission_timeout;
        _consecutive_retransmissions = 0;
        reset_timer();
    }

    // Update connection state
    if (_state == State::SYN_SENT && abs_ackno > 0) {
        _state = State::SYN_ACKED;
    }

    // Stop timer if all segments are acknowledged
    if (_segments_outstanding.empty()) {
        stop_timer();
    }

    // Always try to fill the window after receiving an ACK
    fill_window();
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    if (!_timer_running)
        return;
    _time_elapsed += ms_since_last_tick;
    if (_time_elapsed >= _current_rto) {
        _segments_out.push(_segments_outstanding.front());

        // Update retransmission parameter
        if (_window_size > 0 || _segments_outstanding.front().header().syn) {
            _consecutive_retransmissions++;
            _current_rto <<= 1;  // Double the RTO
        }

        reset_timer();
    }
}

void TCPSender::send_empty_segment() {
    TCPSegment seg;
    seg.header().seqno = wrap(_next_seqno, _isn);
    _segments_out.push(seg);
}

bool TCPSender::is_ack_valid(uint64_t abs_ackno) const {
    if (_segments_outstanding.empty()) {
        return abs_ackno <= _next_seqno;
    }
    uint64_t oldest_unacked = unwrap(_segments_outstanding.front().header().seqno, _isn, _next_seqno);
    return abs_ackno <= _next_seqno && abs_ackno >= oldest_unacked;
}

void TCPSender::send_segment(TCPSegment &seg) {
    seg.header().seqno = wrap(_next_seqno, _isn);
    _next_seqno += seg.length_in_sequence_space();
    _bytes_in_flight += seg.length_in_sequence_space();
    _segments_out.push(seg);
    _segments_outstanding.push(seg);
    start_timer();
}

void TCPSender::start_timer() {
    if (!_timer_running) {
        _timer_running = true;
        _time_elapsed = 0;
    }
}

void TCPSender::stop_timer() { _timer_running = false; }

void TCPSender::reset_timer() { _time_elapsed = 0; }