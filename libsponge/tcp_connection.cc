#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _time_since_last_segment_received_counter; }

bool TCPConnection::real_send() {
    bool isSend = false;
    while (!_sender.segments_out().empty()) {
        isSend = true;
        TCPSegment seg = _sender.segments_out().front();
        _sender.segments_out().pop();
        set_ack_and_windowsize(seg);
        _segments_out.push(seg);
    }
    return isSend;
}

void TCPConnection::segment_received(const TCPSegment &seg) {
    _time_since_last_segment_received_counter = 0;
    // Check if the RST has been set
    if (seg.header().rst) {
        _sender.stream_in().set_error();
        _receiver.stream_out().set_error();
        _active = false;
        return;
    }

    // Give the segment to reveicer
    _receiver.segment_received(seg);

    // Check if need to linger
    if (check_inbound_ended() && !_sender.stream_in().eof()) {
        _linger_after_streams_finish = false;
    }

    // Check if the ACK has been set
    if (seg.header().ack) {
        _sender.ack_received(seg.header().ackno, seg.header().win);
        real_send();
    }

    // Send ACK
    if (seg.length_in_sequence_space() > 0) {
        // Handle the SYN/ACK case
        _sender.fill_window();
        bool isSend = real_send();
        // Send at least one ACK message
        if (!isSend) {
            _sender.send_empty_segment();
            TCPSegment ACKSeg = _sender.segments_out().front();
            _sender.segments_out().pop();
            set_ack_and_windowsize(ACKSeg);
            _segments_out.push(ACKSeg);
        }
    }
}

void TCPConnection::set_ack_and_windowsize(TCPSegment &seg) {
    // Ask receiver for ack and window size
    optional<WrappingInt32> ackno = _receiver.ackno();
    if (ackno.has_value()) {
        seg.header().ack = true;
        seg.header().ackno = ackno.value();
    }

    size_t window_size = _receiver.window_size();
    seg.header().win = static_cast<uint16_t>(window_size);
    return;
}

void TCPConnection::connect() {
    _sender.fill_window();
    real_send();
}

size_t TCPConnection::write(const std::string &data) {
    if (!data.size())
        return 0;
    size_t actually_write = _sender.stream_in().write(data);
    _sender.fill_window();
    real_send();
    return actually_write;
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    // May send FIN
    _sender.fill_window();
    real_send();
}

void TCPConnection::send_RST() {
    _sender.send_empty_segment();
    TCPSegment RSTSeg = _sender.segments_out().front();
    _sender.segments_out().pop();
    set_ack_and_windowsize(RSTSeg);
    RSTSeg.header().rst = true;
    _segments_out.push(RSTSeg);
}

// prereqs1 : The inbound stream has been fully assembled and has ended.
bool TCPConnection::check_inbound_ended() {
    return _receiver.unassembled_bytes() == 0 && _receiver.stream_out().input_ended();
}

// prereqs2 : The outbound stream has been ended by the local application and fully sent (including
// the fact that it ended, i.e. a segment with fin ) to the remote peer.
// prereqs3 : The outbound stream has been fully acknowledged by the remote peer.
bool TCPConnection::check_outbound_ended() {
    return _sender.stream_in().eof() && _sender.next_seqno_absolute() == _sender.stream_in().bytes_written() + 2 &&
           _sender.bytes_in_flight() == 0;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    _time_since_last_segment_received_counter += ms_since_last_tick;
    // Tick the sender to do the retransmit
    _sender.tick(ms_since_last_tick);
    if (_sender.segments_out().size() > 0) {
        TCPSegment retxSeg = _sender.segments_out().front();
        _sender.segments_out().pop();
        set_ack_and_windowsize(retxSeg);
        // Abort the connection
        if (_sender.consecutive_retransmissions() > _cfg.MAX_RETX_ATTEMPTS) {
            _sender.stream_in().set_error();
            _receiver.stream_out().set_error();
            retxSeg.header().rst = true;
            _active = false;
        }
        _segments_out.push(retxSeg);
    }

    if (check_inbound_ended() && check_outbound_ended()) {
        if (!_linger_after_streams_finish) {
            _active = false;
        } else if (_time_since_last_segment_received_counter >= 10 * _cfg.rt_timeout) {
            _active = false;
        }
    }
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";
            // Your code here: need to send a RST segment to the peer
            _sender.stream_in().set_error();
            _receiver.stream_out().set_error();
            send_RST();
            _active = false;
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}
