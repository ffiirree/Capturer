#ifndef CAPTURER_SONIC_H
#define CAPTURER_SONIC_H

/**
 * [Sonic Library](https://github.com/waywardgeek/sonic.git)
 * Copyright (c) 2010
 * Bill Cox
 * This file is part of the Sonic Library.
 *
 * This file is licensed under the Apache 2.0 license.
 *
 * @brief
 * The Sonic Library implements a new algorithm invented by Bill Cox for the specific purpose of speeding up
 * speech by high factors at high quality.  It generates smooth speech at speed up factors as high as 6X,
 * possibly more.  It is also capable of slowing down speech, and generates high quality results regardless
 * of the speed up or slow down factor.
 *
 * For speeding up speech by 2X or more, the following equation is used:
 *
 *     new_samples = period / (speed - 1.0)
 *     scale = 1.0 / new_samples;
 *
 * where `period` is the current pitch period, determined using AMDF or any other pitch estimator, and
 * `speed` is the speedup factor.  If the current position in the input stream is pointed to by `samples`,
 * and the current output stream position is pointed to by `out`, then `new_samples` number of samples can
 * be generated with:
 *
 *     out[t] = (samples[t] * (new_samples - t) + samples[t + period] * t) / new_samples;
 *
 * where t = 0 to new_samples - 1.
 *
 * For speed factors < 2X, the PICOLA algorithm is used.  The above algorithm is first used to double the
 * speed of one pitch period.  Then, enough input is directly copied from the input to the output to achieve
 * the desired speed up factor, where 1.0 < speed < 2.0.  The amount of data copied is derived:
 *
 *     speed = (2 * period + length) / (period + length)
 *     speed * length + speed * period = 2 * period + length
 *     length(speed - 1) = 2 * period - speed * period
 *     length = period * (2 - speed) / (speed - 1)
 *
 * For slowing down speech where 0.5 < speed < 1.0, a pitch period is inserted into the output twice, and
 * length of input is copied from the input to the output until the output desired speed is reached.  The
 * length of data copied is:
 *
 *     length = period * (speed - 0.5) / (1 - speed)
 *
 * For slow down factors below 0.5, no data is copied, and an algorithm similar to high speed factors is
 * used.
 */

/* Uncomment this to use sin-wav based overlap add which in theory can improve
   sound quality slightly, at the expense of lots of floating point math. */
#define SONIC_USE_SIN

/* This specifies the range of voice pitches we try to match.
   Note that if we go lower than 65, we could overflow in findPitchInRange */
#ifndef SONIC_MIN_PITCH
#define SONIC_MIN_PITCH 65
#endif

#ifndef SONIC_MAX_PITCH
#define SONIC_MAX_PITCH 400
#endif

/* These are used to down-sample some inputs to improve speed */
#define SONIC_AMDF_FREQ 4000

struct sonic_stream;

/* For all of the following functions, nb_channels is multiplied by numSamples
   to determine the actual number of values read or returned. */

/**
 * Create a sonic stream.
 *
 * @param sample_rate
 * @param nb_channels   1 for mono, and 2 for stereo
 * @return              Return NULL only if we are out of memory and cannot allocate the stream.
 */
sonic_stream *sonic_stream_create(int sample_fmt, int sample_rate, int nb_channels);

/**
 * Destroy the sonic stream.
 */
void sonic_stream_destroy(sonic_stream *stream);

/**
 * Use this to write data to be speed up or down into the stream.
 *
 * @date    AV_SAMPLE_FMT_U8, AV_SAMPLE_FMT_S16 or AV_SAMPLE_FMT_FLT
 * @return  Return 0 if memory realloc failed, otherwise 1
 */
int sonic_stream_write(sonic_stream *stream, const void *data, int nb_samples);

/**
 * Use this to read data out of the stream.
 *
 * @attention Sometimes no data will be available, and zero is returned, which is not an error condition.
 */
int sonic_stream_read(sonic_stream *stream, void *data, int nb_samples);

/**
 * Use this to read 8-bit unsigned data out of the stream.
 *
 * @attention Sometimes no data will be available, and zero is returned, which is not an error condition.
 */
int sonic_stream_read_u8(sonic_stream *stream, unsigned char *data, int maxSamples);

/**
 * Use this to read 16-bit data out of the stream.
 *
 * @attention Sometimes no data will be available, and zero is returned, which is not an error condition.
 */
int sonic_stream_read_s16(sonic_stream *stream, short *samples, int maxSamples);

/**
 * Use this to read floating point data out of the stream.
 *
 * @attention Sometimes no data will be available, and zero is returned, which is not an error condition.
 */
int sonic_stream_read_f32(sonic_stream *stream, float *data, int maxSamples);

/**
 * Force the sonic stream to generate output using whatever data it currently has.
 *
 * @attention No extra delay will be added to the output, but flushing in the middle of words could
 * introduce distortion.
 */
int sonic_stream_flush(sonic_stream *stream);

int sonic_stream_drain(sonic_stream *stream);

/* Return the number of expected output samples */
int sonic_stream_expected_samples(sonic_stream *stream);

/* Return the number of samples in the output buffer */
int sonic_stream_available_samples(sonic_stream *stream);

/* Return the number of samples in the input buffer */
int sonic_stream_remaining_samples(sonic_stream *stream);

/* Get the speed of the stream. */
float sonic_stream_get_speed(const sonic_stream *stream);

/* Set the speed of the stream. */
void sonic_stream_set_speed(sonic_stream *stream, float speed);

/* Get the pitch of the stream. */
float sonic_stream_get_pitch(const sonic_stream *stream);

/* Set the pitch of the stream. */
void sonic_stream_set_pitch(sonic_stream *stream, float pitch);

/* Get the rate of the stream. */
float sonic_stream_get_rate(const sonic_stream *stream);

/* Set the rate of the stream. */
void sonic_stream_set_rate(sonic_stream *stream, float rate);

/* Get the quality setting. */
int sonic_stream_get_quality(const sonic_stream *stream);

/**
 * Set the "quality".
 *
 * Default 0 is virtually as good as 1, but very much faster.
 */
void sonic_stream_set_quality(sonic_stream *stream, int quality);

/* Get the sample rate of the stream. */
int sonic_stream_get_sample_rate(const sonic_stream *stream);

/**
 * Set the sample rate of the stream.
 *
 * @attention This will drop any samples that have not been read.
 */
void sonic_stream_set_sample_rate(sonic_stream *stream, int sample_rate);

/* Get the sample format of the stream. */
int sonic_stream_get_sample_fmt(const sonic_stream *stream);

/* Get the number of channels. */
int sonic_stream_get_nb_channels(const sonic_stream *stream);

/**
 * Set the number of channels.
 *
 * @attention This will drop any samples that have not been read.
 */
void sonic_stream_set_nb_channels(sonic_stream *stream, int nb_channels);

#endif //! CAPTURER_SONIC_H
