/*-
 * Copyright (c) 2020-2021 Hans Petter Selasky. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef	_HPSJAM_PROTOCOL_H_
#define	_HPSJAM_PROTOCOL_H_

#include <QObject>

#include "hpsjam.h"
#include "socket.h"
#include "jitter.h"

#include <assert.h>

#include <stdint.h>
#include <string.h>

#include <sys/queue.h>

#define	HPSJAM_MAX_PKT (255 * 4)

enum {
	HPSJAM_TYPE_END,
	HPSJAM_TYPE_AUDIO_8_BIT_1CH,
	HPSJAM_TYPE_AUDIO_8_BIT_2CH,
	HPSJAM_TYPE_AUDIO_16_BIT_1CH,
	HPSJAM_TYPE_AUDIO_16_BIT_2CH,
	HPSJAM_TYPE_AUDIO_24_BIT_1CH,
	HPSJAM_TYPE_AUDIO_24_BIT_2CH,
	HPSJAM_TYPE_AUDIO_32_BIT_1CH,
	HPSJAM_TYPE_AUDIO_32_BIT_2CH,
	HPSJAM_TYPE_AUDIO_MAX = 60,
	HPSJAM_TYPE_MIDI_PACKET = 61,
	HPSJAM_TYPE_AUDIO_SILENCE = 62,
	HPSJAM_TYPE_ACK = 63,
	HPSJAM_TYPE_CONFIGURE_REQUEST,
	HPSJAM_TYPE_PING_REQUEST,
	HPSJAM_TYPE_PING_REPLY,
	HPSJAM_TYPE_ICON_REQUEST,
	HPSJAM_TYPE_NAME_REQUEST,
	HPSJAM_TYPE_LYRICS_REQUEST,
	HPSJAM_TYPE_LYRICS_REPLY,
	HPSJAM_TYPE_CHAT_REQUEST,
	HPSJAM_TYPE_CHAT_REPLY,
	HPSJAM_TYPE_FADER_GAIN_REQUEST,
	HPSJAM_TYPE_FADER_GAIN_REPLY,
	HPSJAM_TYPE_FADER_PAN_REQUEST,
	HPSJAM_TYPE_FADER_PAN_REPLY,
	HPSJAM_TYPE_FADER_BITS_REQUEST,
	HPSJAM_TYPE_FADER_BITS_REPLY,	/* unused */
	HPSJAM_TYPE_FADER_ICON_REPLY,
	HPSJAM_TYPE_FADER_NAME_REPLY,
	HPSJAM_TYPE_FADER_LEVEL_REPLY,
	HPSJAM_TYPE_FADER_EQ_REQUEST,
	HPSJAM_TYPE_FADER_EQ_REPLY,
	HPSJAM_TYPE_FADER_DISCONNECT_REPLY,
	HPSJAM_TYPE_LOCAL_GAIN_REPLY,
	HPSJAM_TYPE_LOCAL_PAN_REPLY,
	HPSJAM_TYPE_LOCAL_EQ_REPLY,
};

struct hpsjam_header {
	uint8_t sequence;
	void clear() {
		sequence = 0;
	};
	uint8_t getSeqNo() const {
		return (sequence % HPSJAM_SEQ_MAX);
	};
	uint8_t getRedNo() const {
		return ((sequence / HPSJAM_SEQ_MAX) % HPSJAM_SEQ_MAX);
	};
	void setSequence(uint8_t seq, uint8_t red) {
		sequence = (seq % HPSJAM_SEQ_MAX) +
		  ((red % HPSJAM_SEQ_MAX) * HPSJAM_SEQ_MAX);
	};
};

struct hpsjam_packet {
	uint8_t length;
	uint8_t type;
	uint8_t sequence[2];

	size_t getBytes() const {
		return (length * 4);
	};

	bool valid(const struct hpsjam_packet *end) const {
		const struct hpsjam_packet *ptr = this + length;
		if (type == HPSJAM_TYPE_END || length == 0 ||
		    (uintptr_t)ptr > (uintptr_t)end)
			return (false);
		else
			return (true);
	}; 

	const struct hpsjam_packet *next() const {
		return (this + length);
	};

	int8_t getS8(size_t offset) const {
		return (int8_t)sequence[2 + offset];
	};
	void putS8(size_t offset, int8_t value) {
		sequence[2 + offset] = value;
	};
	int16_t getS16(size_t offset) const {
		return (int16_t)(sequence[2 + offset] | (sequence[2 + offset + 1] << 8));
	};
	void putS16(size_t offset, int16_t value) {
		sequence[2 + offset] = (uint8_t)value;
		sequence[2 + offset + 1] = (uint8_t)(value >> 8);
	};
	int32_t getS24(size_t offset) const {
		int temp =
		  (sequence[2 + offset]) |
		  (sequence[2 + offset + 1] << 8) |
		  (sequence[2 + offset + 2] << 16);
		if (temp & (1 << 23))
			temp |= -(1 << 23);
		return (temp);
	};
	void putS24(size_t offset, int32_t value) {
		sequence[2 + offset] = (uint8_t)value;
		sequence[2 + offset + 1] = (uint8_t)(value >> 8);
		sequence[2 + offset + 2] = (uint8_t)(value >> 16);
	};
	int32_t getS32(size_t offset) const {
		return (
		    (sequence[2 + offset]) |
		    (sequence[2 + offset + 1] << 8) |
		    (sequence[2 + offset + 2] << 16) |
		    (sequence[2 + offset + 3] << 24));
	};
	void putS32(size_t offset, int32_t value) {
		sequence[2 + offset] = (uint8_t)value;
		sequence[2 + offset + 1] = (uint8_t)(value >> 8);
		sequence[2 + offset + 2] = (uint8_t)(value >> 16);
		sequence[2 + offset + 3] = (uint8_t)(value >> 24);
	};

	size_t get8Bit2ChSample(float *left, float *right) const;
	size_t get16Bit2ChSample(float *left, float *right) const;
	size_t get24Bit2ChSample(float *left, float *right) const;
	size_t get32Bit2ChSample(float *left, float *right) const;

	size_t get8Bit1ChSample(float *left) const;
	size_t get16Bit1ChSample(float *left) const;
	size_t get24Bit1ChSample(float *left) const;
	size_t get32Bit1ChSample(float *left) const;

	size_t getSilence() const;

	void put8Bit2ChSample(float *left, float *right, size_t samples);
	void put16Bit2ChSample(float *left, float *right, size_t samples);
	void put24Bit2ChSample(float *left, float *right, size_t samples);
	void put32Bit2ChSample(float *left, float *right, size_t samples);

	void put8Bit1ChSample(float *left, size_t samples);
	void put16Bit1ChSample(float *left, size_t samples);
	void put24Bit1ChSample(float *left, size_t samples);
	void put32Bit1ChSample(float *left, size_t samples);

	void putSilence(size_t samples);

	void putMidiData(const uint8_t *, size_t);
	bool getMidiData(uint8_t *, size_t *) const;

	uint8_t getLocalSeqNo() const {
		return (sequence[0]);
	};

	uint8_t getPeerSeqNo() const {
		return (sequence[1]);
	};

	void setLocalSeqNo(uint8_t seqno) {
		sequence[0] = seqno;
	};

	void setPeerSeqNo(uint8_t seqno) {
		sequence[1] = seqno;
	};

	bool getFaderValue(uint8_t &, uint8_t &, float *, size_t &) const;
	void setFaderValue(uint8_t, uint8_t, const float *, size_t);

	void setFaderData(uint8_t, uint8_t, const char *, size_t);
	bool getFaderData(uint8_t &, uint8_t &, const char **, size_t &) const;

	void setRawData(const char *, size_t, char pad = 0);
	bool getRawData(const char **, size_t &) const;

	bool getConfigure(uint8_t &out_format) const {
		if (length >= 2) {
			out_format = getS8(0);
			return (true);
		}
		return (false);
	};

	void setConfigure(uint8_t out_format) {
		length = 2;
		sequence[0] = 0;
		sequence[1] = 0;
		putS8(0, out_format);
		putS8(1, 0);
		putS8(2, 0);
		putS8(3, 0);
	};

	bool getPing(uint16_t &packets, uint16_t &time_ms, uint64_t &passwd) const {
		if (length >= 4) {
			packets = getS16(0);
			time_ms = getS16(2);
			passwd = ((uint64_t)(uint32_t)getS32(4)) | (((uint64_t)(uint32_t)getS32(8)) << 32);
			return (true);
		}
		return (false);
	};

	void setPing(uint16_t packets, uint16_t time_ms, uint64_t passwd) {
		length = 4;
		sequence[0] = 0;
		sequence[1] = 0;
		putS16(0, packets);
		putS16(2, time_ms);
		putS32(4, (uint32_t)passwd);
		putS32(8, (uint32_t)(passwd >> 32));
	};
};

struct hpsjam_packet_entry;
typedef TAILQ_HEAD(, hpsjam_packet_entry) hpsjam_packet_head_t;

struct hpsjam_packet_entry {
	TAILQ_ENTRY(hpsjam_packet_entry) entry;
	union {
		struct hpsjam_packet packet;
		uint8_t raw[HPSJAM_MAX_PKT];
	};
	struct hpsjam_packet_entry & insert_tail(hpsjam_packet_head_t *phead)
	{
		TAILQ_INSERT_TAIL(phead, this, entry);
		return (*this);
	};
	struct hpsjam_packet_entry & insert_head(hpsjam_packet_head_t *phead)
	{
		TAILQ_INSERT_HEAD(phead, this, entry);
		return (*this);
	};
	struct hpsjam_packet_entry & remove(hpsjam_packet_head_t *phead)
	{
		TAILQ_REMOVE(phead, this, entry);
		return (*this);
	};
};

union hpsjam_frame {
	uint8_t raw[HPSJAM_MAX_UDP];
	uint32_t raw32[HPSJAM_MAX_UDP / 4];
	uint64_t raw64[HPSJAM_MAX_UDP / 8];
#if (HPSJAM_MAX_UDP % 8)
#error "HPSJAM_MAX_UDP must be divisible by 8."
#endif
  	struct {
		struct hpsjam_header hdr;
		struct hpsjam_packet start[(HPSJAM_MAX_UDP - sizeof(hdr)) / sizeof(hpsjam_packet)];
		struct hpsjam_packet end[0];
	};
	void clear() {
		memset(this, 0, sizeof(*this));
	};
	void do_xor(const union hpsjam_frame &other) {
		for (size_t x = 0; x != (sizeof(*this) / 8); x++)
			raw64[x] ^= other.raw64[x];
	};
};

class hpsjam_output_packetizer : public QObject {
	Q_OBJECT;
public:
	union hpsjam_frame current;
	union hpsjam_frame mask;
	hpsjam_packet_head_t head;
	struct hpsjam_packet_entry *pending;
	uint16_t start_time; /* start time for message */
	uint16_t ping_time; /* response time in ticks */
	uint16_t pend_count; /* pending timeout counter */
	uint8_t pend_seqno; /* pending sequence number */
	uint8_t peer_seqno; /* peer sequence number */
	uint8_t d_cur;	/* current distance between XOR frames */
	uint8_t d_max;	/* maximum distance between XOR frames */
	uint8_t seqno;	/* current sequence number */
	bool send_ack;
	size_t offset;	/* current data offset */
	size_t d_len;	/* maximum XOR frame length */

	hpsjam_output_packetizer() {
		TAILQ_INIT(&head);
		pending = 0;
		init();
	};

	bool empty() const {
		return (TAILQ_FIRST(&head) == 0);
	};

	struct hpsjam_packet_entry *find(uint8_t type) const {
		struct hpsjam_packet_entry *pkt;
		TAILQ_FOREACH(pkt, &head, entry) {
			if (pkt->packet.type == type)
				return (pkt);
		}
		return (0);
	};

	void init(uint8_t distance = 2) {
		struct hpsjam_packet_entry *pkt;
		d_cur = 0;
		d_max = distance % HPSJAM_SEQ_MAX;
		start_time = 0;
		ping_time = 0;
		pend_count = 65535;
		pend_seqno = 0;
		peer_seqno = 0;
		seqno = 0;
		send_ack = false;
		offset = 0;
		current.clear();
		mask.clear();

		while ((pkt = TAILQ_FIRST(&head))) {
			pkt->remove(&head);
			delete pkt;
		}

		delete pending;
		pending = 0;
	};

	bool append_pkt(const struct hpsjam_packet_entry &entry)
	{
		size_t remainder = sizeof(current) - sizeof(current.hdr) - offset;
		size_t len = entry.packet.getBytes();

		if (len <= remainder) {
			memcpy(current.raw + sizeof(current.hdr) + offset, entry.raw, len);
			offset += len;
			return (true);
		}
		return (false);
	};

	bool append_ack()
	{
		const size_t remainder = sizeof(current) - sizeof(current.hdr) - offset;
		const size_t len = 4;

		if (len <= remainder) {
			uint8_t *ptr = current.raw + sizeof(current.hdr) + offset;
			ptr[0] = 1;
			ptr[1] = HPSJAM_TYPE_ACK;
			ptr[2] = 0;
			ptr[3] = peer_seqno;
			offset += len;
			return (true);
		}
		return (false);
	};

	void advance() {
		if (pending == 0)
			return;
		delete pending;
		pending = 0;
		ping_time = hpsjam_ticks - start_time;
	};

	bool isXorFrame() const {
		return (d_cur == d_max);
	};

	void send(const struct hpsjam_socket_address &addr) {
		if (d_cur == d_max) {
			/* finalize XOR packet */
			mask.hdr.setSequence(seqno, d_max);
			addr.sendto((const char *)&mask, d_len + sizeof(mask.hdr));
			mask.clear();
			d_cur = 0;
			d_len = 0;
		} else {
			/* add a control packet, if possible */
			if (pending == 0) {
				pending = TAILQ_FIRST(&head);
				if (pending != 0) {
					pending->remove(&head);
					pending->packet.setLocalSeqNo(pend_seqno);
					pending->packet.setPeerSeqNo(peer_seqno);
					start_time = hpsjam_ticks;
					pend_seqno++;
					if (append_pkt(*pending))
						send_ack = false;
					pend_count = 1;
				} else if (pend_count != 65535) {
					pend_count++;
				}
			} else {
				if ((pend_count % 64) == 0) {
					pending->packet.setPeerSeqNo(peer_seqno);
					if (append_pkt(*pending))
						send_ack = false;
					pend_count++;
				} else if (pend_count != 65535) {
					pend_count++;
				}
			}
			if (pend_count == 1000)
				emit pendingWatchdog();
			else if (pend_count == 2000)
				emit pendingTimeout();

			/* check if we need to send an ACK */
			if (send_ack && append_ack())
				send_ack = false;
			current.hdr.setSequence(seqno, 0);
			addr.sendto((const char *)&current, offset + sizeof(current.hdr));
			mask.do_xor(current);
			current.clear();
			seqno++;
			d_cur++;
			/* keep track of maximum XOR length */
			if (d_len < offset)
				d_len = offset;
			offset = 0;
		}
	};
signals:
	void pendingWatchdog();
	void pendingTimeout();
};

struct hpsjam_input_packetizer {
	struct hpsjam_jitter jitter;
	union hpsjam_frame current[HPSJAM_SEQ_MAX];
	union hpsjam_frame mask[HPSJAM_SEQ_MAX];
#define	HPSJAM_V_GOT_PACKET 1
#define	HPSJAM_V_GOT_XOR_MASK 2
#define	HPSJAM_V_GOT_RECEIVED 4
	uint8_t valid[HPSJAM_SEQ_MAX];
	uint8_t last_red;

	void init() {
		jitter.clear();
		for (size_t x = 0; x != HPSJAM_SEQ_MAX; x++) {
			current[x].clear();
			mask[x].clear();
		}
		memset(valid, 0, sizeof(valid));
		last_red = 2;
	};

	const union hpsjam_frame *first_pkt() {
		unsigned mask = 0;
		unsigned red;
		unsigned start;
		uint8_t min_x;

		for (uint8_t x = 0; x != HPSJAM_SEQ_MAX; x++)
			mask |= (valid[x] & HPSJAM_V_GOT_PACKET) << x;

		/*
		 * Figure out the rotation which gives the smallest
		 * value aligned to the redundancy packet:
		 */
		start = mask;
		min_x = 0;
		for (uint8_t x = 0; x != HPSJAM_SEQ_MAX; x++) {
			if (start > mask && (x % last_red) == 0) {
				start = mask;
				min_x = x;
			}
			if (mask & 1) {
				mask >>= 1;
				mask |= 1U << (HPSJAM_SEQ_MAX - 1);
			} else {
				mask >>= 1;
			}
		}

		/* compute the redundancy mask */
		red = (1U << last_red) - 1U;

		/*
		 * Start processing if all packets are present in a
		 * redundancy sequence or packets are received beyond
		 * the redundancy mask.
		 */
		while ((start & red) == red || (start & ~red) != 0) {
			/* account for RX loss */
			if ((valid[min_x] & HPSJAM_V_GOT_RECEIVED) == 0 &&
			    (valid[min_x] & HPSJAM_V_GOT_XOR_MASK) == 0) {
				/* account for RX loss */
				jitter.rx_loss();
			}

			/* check if we can consume packet(s) */
			for (uint8_t x = 0; x != last_red; x++) {
				const uint8_t z = (min_x + x) % HPSJAM_SEQ_MAX;
				if (valid[z] & HPSJAM_V_GOT_RECEIVED)
					continue;
				if (~valid[z] & HPSJAM_V_GOT_PACKET) {
					/* fill frame with silence */
					current[z].clear();
					current[z].start[0].putSilence(HPSJAM_NOM_SAMPLES);
					/* account for RX loss */
					jitter.rx_loss();
					/* account for RX damage */
					jitter.rx_damage();
				}
				/* mark this entry received */
				valid[z] |= HPSJAM_V_GOT_RECEIVED;
				/* clear received flag halfway through */
				valid[(z + (HPSJAM_SEQ_MAX / 2)) % HPSJAM_SEQ_MAX] &= ~HPSJAM_V_GOT_RECEIVED;
				return (current + z);
			}

			for (uint8_t x = 0; x != last_red; x++) {
				const uint8_t z = (min_x + x) % HPSJAM_SEQ_MAX;
				/* only keep the received flag */
				valid[z] &= HPSJAM_V_GOT_RECEIVED;
			}

			/* see if there is more data */
			min_x = (min_x + last_red) % HPSJAM_SEQ_MAX;
			start >>= last_red;
		}

		/* no packets can be received */
		return (0);
	};

	void recovery() {
		if (last_red <= 1)
			return;
		for (uint8_t x = 0; x != HPSJAM_SEQ_MAX; x += last_red) {
			if (~valid[x] & HPSJAM_V_GOT_XOR_MASK)
				continue;
			if (mask[x].hdr.getRedNo() != last_red)
				continue;
			uint8_t rx_missing = 0;
			for (uint8_t y = 0; y != last_red; y++) {
				const uint8_t z = (HPSJAM_SEQ_MAX + x - y - 1) % HPSJAM_SEQ_MAX;
				rx_missing += (~valid[z] & HPSJAM_V_GOT_PACKET);
			}
			if (rx_missing == 1) {
				/* one frame missing */
				for (uint8_t y = 0; y != last_red; y++) {
					const uint8_t z = (HPSJAM_SEQ_MAX + x - y - 1) % HPSJAM_SEQ_MAX;
					if (valid[z] & HPSJAM_V_GOT_PACKET)
						mask[x].do_xor(current[z]);
				}
				/* recover the missing frame */
				for (uint8_t y = 0; y != last_red; y++) {
					const uint8_t z = (HPSJAM_SEQ_MAX + x - y - 1) % HPSJAM_SEQ_MAX;
					if (~valid[z] & HPSJAM_V_GOT_PACKET) {
						current[z] = mask[x];
						/* invalidate headers */
						mask[x].hdr.clear();
						current[z].hdr.clear();
						/* set valid bit */
						valid[z] |= HPSJAM_V_GOT_PACKET;
						/* account for RX loss */
						jitter.rx_loss();
					}
				}
			}
		}
	};

	void receive(const union hpsjam_frame &frame) {
		const uint8_t rx_seqno = frame.hdr.getSeqNo();
		const uint8_t rx_red = frame.hdr.getRedNo();

		if (rx_red != 0) {
			/* check that the redundancy count is valid */
			if ((HPSJAM_SEQ_MAX % rx_red) == 0 && (rx_seqno % rx_red) == 0) {
				last_red = rx_red;
				mask[rx_seqno] = frame;
				valid[rx_seqno] |= HPSJAM_V_GOT_XOR_MASK;
			}
		} else {
			current[rx_seqno] = frame;
			valid[rx_seqno] |= HPSJAM_V_GOT_PACKET;
		}

		jitter.rx_packet();
	};
};

#endif		/* _HPSJAM_PROTOCOL_H_ */
