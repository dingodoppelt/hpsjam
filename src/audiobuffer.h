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

#ifndef	_HPSJAM_AUDIOBUFFER_H_
#define	_HPSJAM_AUDIOBUFFER_H_

#include <math.h>
#include <assert.h>

#include "hpsjam.h"
#include "protocol.h"

#define	HPSJAM_MAX_SAMPLES \
	(HPSJAM_SEQ_MAX * 2 * HPSJAM_DEF_SAMPLES)	/* samples */

static inline float
level_encode(float value)
{
	float divisor = logf(1.0f + 255.0f);

	if (value == 0.0f)
		return (0);
	else if (value < 0.0f)
		return - (logf(1.0f - 255.0f * value) / divisor);
	else
		return (logf(1.0f + 255.0f * value) / divisor);
}

static inline float
level_decode(float value)
{
	constexpr float multiplier = (1.0f / 255.0f);

	if (value == 0.0f)
		return (0);
	else if (value < 0.0f)
		return - multiplier * (powf(1.0f + 255.0f, -value) - 1.0f);
	else
		return multiplier * (powf(1.0f + 255.0f, value) - 1.0f);
}

class hpsjam_audio_level {
public:
	float level;

	hpsjam_audio_level() {
		clear();
	};
	void clear() {
		level = 0;
	};
	void addSamples(const float *ptr, size_t num) {
		for (size_t x = 0; x != num; x++) {
			const float v = fabsf(ptr[x]);
			if (v > level)
				level = v;
		}
		if (level > 1.0f)
			level = 1.0f;
	};
	float getLevel() {
		float retval = level;
		level = retval / 2.0f;
		return (retval);
	};
};

class hpsjam_audio_buffer {
	enum { fadeSamples = HPSJAM_DEF_SAMPLES };
public:
	float samples[HPSJAM_MAX_SAMPLES];
	float stats[HPSJAM_SEQ_MAX * 2];
	float last_sample;
	size_t consumer;
	size_t total;
	uint16_t limit;
	uint16_t fade_in;

	void clear() {
		memset(samples, 0, sizeof(samples));
		memset(stats, 0, sizeof(stats));
		last_sample = 0;
		consumer = 0;
		total = 0;
		limit = 3;	/* minimum value for handling one packet loss */
		fade_in = fadeSamples;
	};
	void set_jitter_limit_in_ms(uint16_t _limit) {
		limit = _limit + 3;
	};

	hpsjam_audio_buffer() {
		clear();
	};

	/* getLowWater() returns one of 0,1 or 2. */
	uint8_t getLowWater() const {
		uint8_t x;
		for (x = 0; x != HPSJAM_SEQ_MAX * 2; x++) {
			if (stats[x] >= 0.5f)
				break;
		}
		/* try to keep the low water level around 2ms */
		if (x < 2)
			return (0);	/* low data - go slower */
		else if (x > 2)
			return (2);	/* high data - go faster */
		else
			return (1);	/* normal */
	};

	/* getHighWater() returns one of 0,1 or 2. */
	uint8_t getHighWater() const {
		uint8_t x;
		for (x = 0; x != HPSJAM_SEQ_MAX * 2; x++) {
			if (stats[x] >= 0.5f)
				break;
		}
		/* try to keep the high water level down */
		if (x < limit)
			return (0);
		else if (x > limit)
			return (2);
		else
			return (1);
	};

	/* remove samples from buffer, must be called periodically */
	void remSamples(float *dst, size_t num) {
		size_t fwd = HPSJAM_MAX_SAMPLES - consumer;
		bool underrun = (num > total);

		/* fill missing samples with last value */
		if (underrun) {
			for (size_t x = total; x != num; x++) {
				last_sample -= last_sample / HPSJAM_SAMPLE_RATE;
				dst[x] = last_sample;
			}
			fade_in = fadeSamples;
			num = total;
		}

		/* keep track of low water mark */
		uint8_t index = (total - num) / HPSJAM_DEF_SAMPLES;

		/* shouldn't trigger, but just in case */
		if (index > (HPSJAM_SEQ_MAX * 2 - 1))
			index = (HPSJAM_SEQ_MAX * 2 - 1);

		stats[index] += 1.0f;

		if (stats[index] >= 256.0f) {
			for (uint8_t x = 0; x != HPSJAM_SEQ_MAX * 2; x++)
				stats[x] /= 2.0f;

			const uint8_t high = getHighWater();

			/*
			 * Shrink the buffer depending on the amount
			 * of supplied data:
			 */
			if (total >= num + HPSJAM_DEF_SAMPLES && high > 1) {
				shrink();
				/* need to update FWD now consumer is changed */
				fwd = HPSJAM_MAX_SAMPLES - consumer;
			}
		}

		/* copy samples from ring-buffer */
		while (num != 0) {
			if (fwd > num)
				fwd = num;
			memcpy(dst, samples + consumer, sizeof(samples[0]) * fwd);
			dst += fwd;
			num -= fwd;
			consumer += fwd;
			total -= fwd;
			if (consumer == HPSJAM_MAX_SAMPLES) {
				consumer = 0;
				fwd = HPSJAM_MAX_SAMPLES;
			} else {
				assert(num == 0);
				break;
			}
		}

		/*
		 * Fill in some samples on underrun, to avoid
		 * multiple jitters:
		 */
		if (underrun) {
			while (total < HPSJAM_DEF_SAMPLES) {
				const size_t producer = (consumer + total) % HPSJAM_MAX_SAMPLES;
				last_sample -= last_sample / HPSJAM_SAMPLE_RATE;
				samples[producer] = last_sample;
				total++;
			}
		}
	};

	/* add samples to buffer */
	void addSamples(const float *src, size_t num) {
		size_t producer = (consumer + total) % HPSJAM_MAX_SAMPLES;
		size_t fwd = HPSJAM_MAX_SAMPLES - producer;
		size_t max = HPSJAM_MAX_SAMPLES - total;

		if (num > max)
			num = max;

		/* copy samples to ring-buffer */
		while (num != 0) {
			if (fwd > num)
				fwd = num;
			if (fwd != 0) {
				/* check if there was a discontinuity, and fade in audio */
				if (fade_in != 0) {
					for (size_t x = 0; x != fwd; x++) {
						const float f = (float)fade_in / (float)fadeSamples;
						last_sample -= last_sample / HPSJAM_SAMPLE_RATE;
						samples[producer + x] = src[x] - f * src[x] + last_sample * f;
						fade_in -= (fade_in != 0);
					}
				} else {
					memcpy(samples + producer, src, sizeof(samples[0]) * fwd);
				}
				/* update last sample */
				last_sample = samples[producer + fwd - 1];
				src += fwd;
				num -= fwd;
				total += fwd;
				producer += fwd;
			}
			if (producer == HPSJAM_MAX_SAMPLES) {
				producer = 0;
				fwd = HPSJAM_MAX_SAMPLES;
			} else {
				assert(num == 0);
				break;
			}
		}
	};

	/* add silence to buffer */
	void addSilence(size_t num) {
		size_t producer = (consumer + total) % HPSJAM_MAX_SAMPLES;
		size_t fwd = HPSJAM_MAX_SAMPLES - producer;
		size_t max = HPSJAM_MAX_SAMPLES - total;

		if (num > max)
			num = max;

		/* copy samples to ring-buffer */
		while (num != 0) {
			if (fwd > num)
				fwd = num;
			if (fwd != 0) {
				for (size_t x = 0; x != fwd; x++) {
					last_sample -= last_sample / HPSJAM_SAMPLE_RATE;
					samples[producer + x] = last_sample;
				}
				fade_in = fadeSamples;
				num -= fwd;
				total += fwd;
				producer += fwd;
			}
			if (producer == HPSJAM_MAX_SAMPLES) {
				producer = 0;
				fwd = HPSJAM_MAX_SAMPLES;
			} else {
				assert(num == 0);
				break;
			}
		}
	};

	/* grow ring-buffer */
	void grow() {
		const size_t p[2] =
		  { (consumer + total + HPSJAM_MAX_SAMPLES - 1) % HPSJAM_MAX_SAMPLES,
		    (consumer + total + HPSJAM_MAX_SAMPLES - 2) % HPSJAM_MAX_SAMPLES };
		if (total > 1) {
			const float append = samples[p[0]];
			samples[p[0]] = (samples[p[0]] + samples[p[1]]) / 2.0f;
			addSamples(&append, 1);
		}
	};

	/* shrink ring-buffer */
	void shrink() {
		if (total < HPSJAM_DEF_SAMPLES)
			return;
		/* merge two millisecond buffers */
		for (size_t x = 0; x != HPSJAM_DEF_SAMPLES; x++) {
			const float factor = x * (1.0f / HPSJAM_DEF_SAMPLES);
			const size_t p[2] = {
			    consumer,
			    (consumer + HPSJAM_DEF_SAMPLES) % HPSJAM_MAX_SAMPLES,
			};
			samples[p[1]] = samples[p[0]] * (1.0f - factor) +
			  samples[p[1]] * factor;
			/* reduce one sample */
			consumer++;
			total--;
			if (consumer == HPSJAM_MAX_SAMPLES)
				consumer = 0;
		}
		/* shift stats down */
		for (uint8_t x = 0; x != (HPSJAM_SEQ_MAX * 2 - 1); x++)
			stats[x] = stats[x + 1];
		stats[HPSJAM_SEQ_MAX * 2 - 1] = 0.0f;
	};
};

#endif		/* _HPSJAM_AUDIOBUFFER_H_ */
