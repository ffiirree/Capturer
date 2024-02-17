#include "libcap/sonic.h"

#include <algorithm>
#include <cassert>
#include <climits>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <numbers>

/*
The following code was used to generate the following sinc lookup table.

#include <limits.h>
#include <math.h>
#include <stdio.h>

double findHannWeight(int N, double x) { return 0.5 * (1.0 - cos(2 * M_PI * x / N)); }

double findSincCoefficient(int N, double x)
{
    double hannWindowWeight = findHannWeight(N, x);
    double sincWeight;
    x -= N / 2.0;
    if (x > 1e-9 || x < -1e-9) {
        sincWeight = sin(M_PI * x) / (M_PI * x);
    }
    else {
        sincWeight = 1.0;
    }
    return hannWindowWeight * sincWeight;
}

int main()
{
    double x;
    int    i;
    int    N = 12;
    for (i = 0, x = 0.0; x <= N; x += 0.02, i++) {
        printf("%u %d\n", i, (int)(SHRT_MAX * findSincCoefficient(N, x)));
    }
    return 0;
}
*/

/* The number of points to use in the sinc FIR filter for resampling. */
#define SINC_FILTER_POINTS 12 /* I am not able to hear improvement with higher N. */
#define SINC_TABLE_SIZE    601

/* Lookup table for windowed sinc function of SINC_FILTER_POINTS points. */
static short sincTable[SINC_TABLE_SIZE] = {
    0,     0,     0,     0,     0,     0,     0,     -1,    -1,    -2,    -2,    -3,    -4,    -6,    -7,
    -9,    -10,   -12,   -14,   -17,   -19,   -21,   -24,   -26,   -29,   -32,   -34,   -37,   -40,   -42,
    -44,   -47,   -48,   -50,   -51,   -52,   -53,   -53,   -53,   -52,   -50,   -48,   -46,   -43,   -39,
    -34,   -29,   -22,   -16,   -8,    0,     9,     19,    29,    41,    53,    65,    79,    92,    107,
    121,   137,   152,   168,   184,   200,   215,   231,   247,   262,   276,   291,   304,   317,   328,
    339,   348,   357,   363,   369,   372,   374,   375,   373,   369,   363,   355,   345,   332,   318,
    300,   281,   259,   234,   208,   178,   147,   113,   77,    39,    0,     -41,   -85,   -130,  -177,
    -225,  -274,  -324,  -375,  -426,  -478,  -530,  -581,  -632,  -682,  -731,  -779,  -825,  -870,  -912,
    -951,  -989,  -1023, -1053, -1080, -1104, -1123, -1138, -1149, -1154, -1155, -1151, -1141, -1125, -1105,
    -1078, -1046, -1007, -963,  -913,  -857,  -796,  -728,  -655,  -576,  -492,  -403,  -309,  -210,  -107,
    0,     111,   225,   342,   462,   584,   708,   833,   958,   1084,  1209,  1333,  1455,  1575,  1693,
    1807,  1916,  2022,  2122,  2216,  2304,  2384,  2457,  2522,  2579,  2625,  2663,  2689,  2706,  2711,
    2705,  2687,  2657,  2614,  2559,  2491,  2411,  2317,  2211,  2092,  1960,  1815,  1658,  1489,  1308,
    1115,  912,   698,   474,   241,   0,     -249,  -506,  -769,  -1037, -1310, -1586, -1864, -2144, -2424,
    -2703, -2980, -3254, -3523, -3787, -4043, -4291, -4529, -4757, -4972, -5174, -5360, -5531, -5685, -5819,
    -5935, -6029, -6101, -6150, -6175, -6175, -6149, -6096, -6015, -5905, -5767, -5599, -5401, -5172, -4912,
    -4621, -4298, -3944, -3558, -3141, -2693, -2214, -1705, -1166, -597,  0,     625,   1277,  1955,  2658,
    3386,  4135,  4906,  5697,  6506,  7332,  8173,  9027,  9893,  10769, 11654, 12544, 13439, 14335, 15232,
    16128, 17019, 17904, 18782, 19649, 20504, 21345, 22170, 22977, 23763, 24527, 25268, 25982, 26669, 27327,
    27953, 28547, 29107, 29632, 30119, 30569, 30979, 31349, 31678, 31964, 32208, 32408, 32565, 32677, 32744,
    32767, 32744, 32677, 32565, 32408, 32208, 31964, 31678, 31349, 30979, 30569, 30119, 29632, 29107, 28547,
    27953, 27327, 26669, 25982, 25268, 24527, 23763, 22977, 22170, 21345, 20504, 19649, 18782, 17904, 17019,
    16128, 15232, 14335, 13439, 12544, 11654, 10769, 9893,  9027,  8173,  7332,  6506,  5697,  4906,  4135,
    3386,  2658,  1955,  1277,  625,   0,     -597,  -1166, -1705, -2214, -2693, -3141, -3558, -3944, -4298,
    -4621, -4912, -5172, -5401, -5599, -5767, -5905, -6015, -6096, -6149, -6175, -6175, -6150, -6101, -6029,
    -5935, -5819, -5685, -5531, -5360, -5174, -4972, -4757, -4529, -4291, -4043, -3787, -3523, -3254, -2980,
    -2703, -2424, -2144, -1864, -1586, -1310, -1037, -769,  -506,  -249,  0,     241,   474,   698,   912,
    1115,  1308,  1489,  1658,  1815,  1960,  2092,  2211,  2317,  2411,  2491,  2559,  2614,  2657,  2687,
    2705,  2711,  2706,  2689,  2663,  2625,  2579,  2522,  2457,  2384,  2304,  2216,  2122,  2022,  1916,
    1807,  1693,  1575,  1455,  1333,  1209,  1084,  958,   833,   708,   584,   462,   342,   225,   111,
    0,     -107,  -210,  -309,  -403,  -492,  -576,  -655,  -728,  -796,  -857,  -913,  -963,  -1007, -1046,
    -1078, -1105, -1125, -1141, -1151, -1155, -1154, -1149, -1138, -1123, -1104, -1080, -1053, -1023, -989,
    -951,  -912,  -870,  -825,  -779,  -731,  -682,  -632,  -581,  -530,  -478,  -426,  -375,  -324,  -274,
    -225,  -177,  -130,  -85,   -41,   0,     39,    77,    113,   147,   178,   208,   234,   259,   281,
    300,   318,   332,   345,   355,   363,   369,   373,   375,   374,   372,   369,   363,   357,   348,
    339,   328,   317,   304,   291,   276,   262,   247,   231,   215,   200,   184,   168,   152,   137,
    121,   107,   92,    79,    65,    53,    41,    29,    19,    9,     0,     -8,    -16,   -22,   -29,
    -34,   -39,   -43,   -46,   -48,   -50,   -52,   -53,   -53,   -53,   -52,   -51,   -50,   -48,   -47,
    -44,   -42,   -40,   -37,   -34,   -32,   -29,   -26,   -24,   -21,   -19,   -17,   -14,   -12,   -10,
    -9,    -7,    -6,    -4,    -3,    -2,    -2,    -1,    -1,    0,     0,     0,     0,     0,     0,
    0
};

struct sonic_stream
{
    short *input_buffer;
    int    input_buffer_size;
    int    input_nb_samples;

    short *output_buffer;
    int    output_buffer_size;
    int    output_nb_samples;

    short *pitch_buffer;
    int    pitch_buffer_size;
    int    pitch_nb_samples;

    short *down_sample_buffer;

    int sample_fmt; // ffmpeg AVSampleFormat

    int sample_rate;

    int nb_channels;

    float speed;

    float pitch;

    float rate;

    int quality;

    /**
     * The point of the following 3 new variables is to gracefully handle rapidly
     * changing input speed.
     *
     * `samplePeriod` is just 1.0/sampleRate.  It is used in accumulating `inputPlayTime`, which is how long
     * we expect the total time should be to play the current input samples in the input buffer. `timeError`
     * keeps track of the error in play time created when playing < 2.0X speed, where we either insert or
     * delete a whole pitch period.  This can cause the output generated from the input to be off in play
     * time by up to a pitch period.  `timeError` replaces PICOLA's concept of the number of samples to play
     * unmodified after a pitch period insertion or deletion.  If speeding up, and the error is >= 0.0, then
     * remove a pitch period, and play samples unmodified until `timeError` is >= 0 again.  If slowing down,
     * and the error is <= 0.0, then add a pitch period, and play samples unmodified until `timeError` is <=
     * 0 again.
     */
    /* How long each output sample takes to play. */
    float sample_period;
    /* How long we expect the entire input buffer to take to play. */
    float input_play_time;
    /* The difference in when the latest output sample was played vs when we wanted.  */
    float time_error;

    int oldRatePosition;
    int newRatePosition;

    int minPeriod;
    int maxPeriod;

    int maxRequired;

    int remainingInputToCopy;

    int prevPeriod;
    int prevMinDiff;

    mutable std::mutex mtx{};
};

/* Free stream buffers. */
static void freeStreamBuffers(sonic_stream *stream)
{
    if (stream->input_buffer != nullptr) {
        free(stream->input_buffer);
    }

    if (stream->output_buffer != nullptr) {
        free(stream->output_buffer);
    }

    if (stream->pitch_buffer != nullptr) {
        free(stream->pitch_buffer);
    }

    if (stream->down_sample_buffer != nullptr) {
        free(stream->down_sample_buffer);
    }
}

/* Compute the number of samples to skip to down-sample the input. */
static int computeSkip(const sonic_stream *stream)
{
    int skip = 1;
    if (stream->sample_rate > SONIC_AMDF_FREQ && stream->quality == 0) {
        skip = stream->sample_rate / SONIC_AMDF_FREQ;
    }
    return skip;
}

/* Allocate stream buffers. */
static int allocateStreamBuffers(sonic_stream *stream, int sample_rate, int nb_channels)
{
    const int minPeriod = sample_rate / SONIC_MAX_PITCH;
    const int maxPeriod = sample_rate / SONIC_MIN_PITCH;

    const int maxRequired = 2 * maxPeriod;

    const int skip = computeSkip(stream);

    /* Allocate 25% more than needed so we hopefully won't grow. */
    stream->input_buffer_size = maxRequired + (maxRequired >> 2);

    stream->input_buffer = (short *)calloc(stream->input_buffer_size, sizeof(short) * nb_channels);
    if (stream->input_buffer == nullptr) {
        sonic_stream_destroy(stream);
        return 0;
    }

    /* Allocate 25% more than needed so we hopefully won't grow. */
    stream->output_buffer_size = maxRequired + (maxRequired >> 2);
    stream->output_buffer      = (short *)calloc(stream->output_buffer_size, sizeof(short) * nb_channels);
    if (stream->output_buffer == nullptr) {
        sonic_stream_destroy(stream);
        return 0;
    }

    /* Allocate 25% more than needed so we hopefully won't grow. */
    stream->pitch_buffer_size = maxRequired + (maxRequired >> 2);
    stream->pitch_buffer      = (short *)calloc(maxRequired, sizeof(short) * nb_channels);
    if (stream->pitch_buffer == nullptr) {
        sonic_stream_destroy(stream);
        return 0;
    }

    int downSampleBufferSize   = (maxRequired + skip - 1) / skip;
    stream->down_sample_buffer = (short *)calloc(downSampleBufferSize, sizeof(short));
    if (stream->down_sample_buffer == nullptr) {
        sonic_stream_destroy(stream);
        return 0;
    }

    stream->sample_rate     = sample_rate;
    stream->sample_period   = 1.0f / static_cast<float>(sample_rate);
    stream->nb_channels     = nb_channels;
    stream->oldRatePosition = 0;
    stream->newRatePosition = 0;
    stream->minPeriod       = minPeriod;
    stream->maxPeriod       = maxPeriod;
    stream->maxRequired     = maxRequired;
    stream->prevPeriod      = 0;

    return 1;
}

/* Enlarge the output buffer if needed. */
static int enlargeOutputBufferIfNeeded(sonic_stream *stream, int nb_samples)
{
    const int output_buffer_size = stream->output_buffer_size;

    if (stream->output_nb_samples + nb_samples > output_buffer_size) {
        stream->output_buffer_size += (output_buffer_size >> 1) + nb_samples;
        stream->output_buffer       = (short *)realloc(
            stream->output_buffer, stream->output_buffer_size * sizeof(short) * stream->nb_channels);
        if (stream->output_buffer == nullptr) {
            return 0;
        }
    }
    return 1;
}

/* Enlarge the input buffer if needed. */
static int enlargeInputBufferIfNeeded(sonic_stream *stream, int nb_samples)
{
    int inputBufferSize = stream->input_buffer_size;

    if (stream->input_nb_samples + nb_samples > inputBufferSize) {
        stream->input_buffer_size += (inputBufferSize >> 1) + nb_samples;
        stream->input_buffer       = (short *)realloc(
            stream->input_buffer, stream->input_buffer_size * sizeof(short) * stream->nb_channels);
        if (stream->input_buffer == nullptr) {
            return 0;
        }
    }
    return 1;
}

/* Update stream->numInputSamples, and update stream->inputPlayTime.  Call this whenever adding samples to
   the input buffer, to keep track of total expected input play time accounting. */
static void updateNumInputSamples(sonic_stream *stream, int nb_samples)
{
    float speed = stream->speed / stream->pitch;

    stream->input_nb_samples += nb_samples;
    stream->input_play_time  += nb_samples * stream->sample_period / speed;
}

/* Add the input samples to the input buffer. */
static int addFloatSamplesToInputBuffer(sonic_stream *stream, const float *data, int nb_samples)
{
    assert(stream);

    if (nb_samples == 0) {
        return 1;
    }

    if (!enlargeInputBufferIfNeeded(stream, nb_samples)) {
        return 0;
    }

    int    count  = nb_samples * stream->nb_channels;
    short *buffer = stream->input_buffer + stream->input_nb_samples * stream->nb_channels;
    while (count--) {
        *buffer++ = static_cast<short>(std::clamp<long>(std::lround((*data++) * 32767.0f), -32767, 32767));
    }

    updateNumInputSamples(stream, nb_samples);

    return 1;
}

/* Add the input samples to the input buffer. */
static int addShortSamplesToInputBuffer(sonic_stream *stream, const short *data, int nb_samples)
{
    if (nb_samples == 0) {
        return 1;
    }

    if (!enlargeInputBufferIfNeeded(stream, nb_samples)) {
        return 0;
    }

    const auto buffer = stream->input_buffer + stream->input_nb_samples * stream->nb_channels;
    memcpy(buffer, data, nb_samples * sizeof(short) * stream->nb_channels);

    updateNumInputSamples(stream, nb_samples);

    return 1;
}

/* Add the input samples to the input buffer. */
static int addUnsignedCharSamplesToInputBuffer(sonic_stream *stream, const unsigned char *samples,
                                               int numSamples)
{
    int count = numSamples * stream->nb_channels;

    if (numSamples == 0) {
        return 1;
    }
    if (!enlargeInputBufferIfNeeded(stream, numSamples)) {
        return 0;
    }
    short *buffer = stream->input_buffer + stream->input_nb_samples * stream->nb_channels;
    while (count--) {
        *buffer++ = (*samples++ - 128) << 8;
    }
    updateNumInputSamples(stream, numSamples);
    return 1;
}

/* Remove input samples that we have already processed. */
static void removeInputSamples(sonic_stream *stream, int position)
{
    int remainingSamples = stream->input_nb_samples - position;

    if (remainingSamples > 0) {
        memmove(stream->input_buffer, stream->input_buffer + position * stream->nb_channels,
                remainingSamples * sizeof(short) * stream->nb_channels);
    }
    /* If we play 3/4ths of the samples, then the expected play time of the remaining samples is 1/4th of
     * the original expected play time. */
    stream->input_play_time  = (stream->input_play_time * remainingSamples) / stream->input_nb_samples;
    stream->input_nb_samples = remainingSamples;
}

/* Copy from the input buffer to the output buffer, and remove the samples from the input buffer. */
static int copyInputToOutput(sonic_stream *stream, int numSamples)
{
    if (!enlargeOutputBufferIfNeeded(stream, numSamples)) {
        return 0;
    }
    memcpy(stream->output_buffer + stream->output_nb_samples * stream->nb_channels, stream->input_buffer,
           numSamples * sizeof(short) * stream->nb_channels);
    stream->output_nb_samples += numSamples;
    removeInputSamples(stream, numSamples);
    return 1;
}

/* Copy from samples to the output buffer */
static int copyToOutput(sonic_stream *stream, short *samples, int numSamples)
{
    if (!enlargeOutputBufferIfNeeded(stream, numSamples)) {
        return 0;
    }
    memcpy(stream->output_buffer + stream->output_nb_samples * stream->nb_channels, samples,
           numSamples * sizeof(short) * stream->nb_channels);
    stream->output_nb_samples += numSamples;
    return 1;
}

/* If skip is greater than one, average skip samples together and write them to the down-sample buffer.  If
   numChannels is greater than one, mix the channels together as we down sample. */
static void downSampleInput(sonic_stream *stream, short *samples, int skip)
{
    const int nb_samples        = stream->maxRequired / skip;
    const int samples_per_value = stream->nb_channels * skip;
    short    *down_buffer       = stream->down_sample_buffer;

    for (int i = 0; i < nb_samples; i++) {
        int value = 0;
        for (int j = 0; j < samples_per_value; j++) {
            value += *samples++;
        }
        value /= samples_per_value;

        *down_buffer++ = static_cast<short>(std::clamp<int>(value, -32767, 32767));
    }
}

/* Find the best frequency match in the range, and given a sample skip multiple.
   For now, just find the pitch of the first channel. */
static int findPitchPeriodInRange(short *samples, int minPeriod, int maxPeriod, int *retMinDiff,
                                  int *retMaxDiff)
{
    int           bestPeriod  = 0;
    int           worstPeriod = 255;
    unsigned long minDiff     = 1;
    unsigned long maxDiff     = 0;

    for (int period = minPeriod; period <= maxPeriod; period++) {
        unsigned long diff = 0;
        short        *s    = samples;
        short        *p    = samples + period;
        for (int i = 0; i < period; i++) {
            short sVal  = *s++;
            short pVal  = *p++;
            diff       += sVal >= pVal ? (unsigned short)(sVal - pVal) : (unsigned short)(pVal - sVal);
        }
        /* Note that the highest number of samples we add into diff will be less than 256, since we skip
           samples.  Thus, diff is a 24 bit number, and we can safely multiply by numSamples without
           overflow */
        if (bestPeriod == 0 || diff * bestPeriod < minDiff * period) {
            minDiff    = diff;
            bestPeriod = period;
        }
        if (diff * worstPeriod > maxDiff * period) {
            maxDiff     = diff;
            worstPeriod = period;
        }
    }

    *retMinDiff = minDiff / bestPeriod;
    *retMaxDiff = maxDiff / worstPeriod;

    return bestPeriod;
}

/* At abrupt ends of voiced words, we can have pitch periods that are better approximated by the previous
 * pitch period estimate.  Try to detect this case. */
static int prevPeriodBetter(sonic_stream *stream, int minDiff, int maxDiff, int preferNewPeriod)
{
    if (minDiff == 0 || stream->prevPeriod == 0) {
        return 0;
    }

    if (preferNewPeriod) {
        if (maxDiff > minDiff * 3) {
            /* Got a reasonable match this period */
            return 0;
        }
        if (minDiff * 2 <= stream->prevMinDiff * 3) {
            /* Mismatch is not that much greater this period */
            return 0;
        }
    }
    else {
        if (minDiff <= stream->prevMinDiff) {
            return 0;
        }
    }

    return 1;
}

/* Find the pitch period.  This is a critical step, and we may have to try multiple ways to get a good
   answer.  This version uses Average Magnitude Difference Function (AMDF).  To improve speed, we down
   sample by an integer factor get in the 11KHz range, and then do it again with a narrower frequency range
   without down sampling */
static int findPitchPeriod(sonic_stream *stream, short *samples, int preferNewPeriod)
{
    int minPeriod = stream->minPeriod;
    int maxPeriod = stream->maxPeriod;
    int minDiff, maxDiff, retPeriod;
    int skip = computeSkip(stream);
    int period;

    if (stream->nb_channels == 1 && skip == 1) {
        period = findPitchPeriodInRange(samples, minPeriod, maxPeriod, &minDiff, &maxDiff);
    }
    else {
        downSampleInput(stream, samples, skip);
        period = findPitchPeriodInRange(stream->down_sample_buffer, minPeriod / skip, maxPeriod / skip,
                                        &minDiff, &maxDiff);
        if (skip != 1) {
            period    *= skip;
            minPeriod  = period - (skip << 2);
            maxPeriod  = period + (skip << 2);
            if (minPeriod < stream->minPeriod) {
                minPeriod = stream->minPeriod;
            }
            if (maxPeriod > stream->maxPeriod) {
                maxPeriod = stream->maxPeriod;
            }
            if (stream->nb_channels == 1) {
                period = findPitchPeriodInRange(samples, minPeriod, maxPeriod, &minDiff, &maxDiff);
            }
            else {
                downSampleInput(stream, samples, 1);
                period = findPitchPeriodInRange(stream->down_sample_buffer, minPeriod, maxPeriod, &minDiff,
                                                &maxDiff);
            }
        }
    }
    if (prevPeriodBetter(stream, minDiff, maxDiff, preferNewPeriod)) {
        retPeriod = stream->prevPeriod;
    }
    else {
        retPeriod = period;
    }
    stream->prevMinDiff = minDiff;
    stream->prevPeriod  = period;
    return retPeriod;
}

/* Overlap two sound segments, ramp the volume of one down, while ramping the other one from zero up, and
 * add them, storing the result at the output. */
static void overlapAdd(int nb_samples, int nb_channels, short *out, short *ramp_down, short *ramp_up)
{
    for (int i = 0; i < nb_channels; i++) {
        short *o = out + i;
        short *u = ramp_up + i;
        short *d = ramp_down + i;

        for (int t = 0; t < nb_samples; t++) {
#ifdef SONIC_USE_SIN
            const double ratio = std::sin(t * std::numbers::pi / (2 * nb_samples));
            *o                 = static_cast<short>(
                std::clamp<long>(std::lround((*d) * (1.0f - ratio) + (*u) * ratio), -32767, 32767));
#else
            *o = ((*d) * (nb_samples - t) + (*u) * t) / nb_samples;
#endif

            o += nb_channels;
            d += nb_channels;
            u += nb_channels;
        }
    }
}

/* Just move the new samples in the output buffer to the pitch buffer */
static int moveNewSamplesToPitchBuffer(sonic_stream *stream, int originalNumOutputSamples)
{
    const int nb_samples  = stream->output_nb_samples - originalNumOutputSamples;
    const int nb_channels = stream->nb_channels;

    if (stream->pitch_nb_samples + nb_samples > stream->pitch_buffer_size) {
        const int pitchBufferSize  = stream->pitch_buffer_size;
        stream->pitch_buffer_size += (pitchBufferSize >> 1) + nb_samples;
        stream->pitch_buffer =
            (short *)realloc(stream->pitch_buffer, stream->pitch_buffer_size * sizeof(short) * nb_channels);
    }
    memcpy(stream->pitch_buffer + stream->pitch_nb_samples * nb_channels,
           stream->output_buffer + originalNumOutputSamples * nb_channels,
           nb_samples * sizeof(short) * nb_channels);
    stream->output_nb_samples  = originalNumOutputSamples;
    stream->pitch_nb_samples  += nb_samples;
    return 1;
}

/* Remove processed samples from the pitch buffer. */
static void removePitchSamples(sonic_stream *stream, int numSamples)
{
    int    numChannels = stream->nb_channels;
    short *source      = stream->pitch_buffer + numSamples * numChannels;

    if (numSamples == 0) {
        return;
    }
    if (numSamples != stream->pitch_nb_samples) {
        memmove(stream->pitch_buffer, source,
                (stream->pitch_nb_samples - numSamples) * sizeof(short) * numChannels);
    }
    stream->pitch_nb_samples -= numSamples;
}

/* Approximate the sinc function times a Hann window from the sinc table. */
static int findSincCoefficient(int i, int ratio, int width)
{
    int lobePoints = (SINC_TABLE_SIZE - 1) / SINC_FILTER_POINTS;
    int left       = i * lobePoints + (ratio * lobePoints) / width;
    int right      = left + 1;
    int position   = i * lobePoints * width + ratio * lobePoints - left * width;
    int leftVal    = sincTable[left];
    int rightVal   = sincTable[right];

    return ((leftVal * (width - position) + rightVal * position) << 1) / width;
}

/* Return 1 if value >= 0, else -1.  This represents the sign of value. */
static int getSign(int value) { return value >= 0 ? 1 : -1; }

/* Interpolate the new output sample. */
static short interpolate(sonic_stream *stream, short *in, int oldSampleRate, int newSampleRate)
{
    /* Compute N-point sinc FIR-filter here.  Clip rather than overflow. */
    int total         = 0;
    int position      = stream->newRatePosition * oldSampleRate;
    int leftPosition  = stream->oldRatePosition * newSampleRate;
    int rightPosition = (stream->oldRatePosition + 1) * newSampleRate;
    int ratio         = rightPosition - position - 1;
    int width         = rightPosition - leftPosition;
    int overflowCount = 0;

    for (int i = 0; i < SINC_FILTER_POINTS; i++) {
        int weight   = findSincCoefficient(i, ratio, width);
        int value    = in[i * stream->nb_channels] * weight;
        int oldSign  = getSign(total);
        total       += value;
        if (oldSign != getSign(total) && getSign(value) == oldSign) {
            /* We must have overflowed.  This can happen with a sinc filter. */
            overflowCount += oldSign;
        }
    }
    /* It is better to clip than to wrap if there was a overflow. */
    if (overflowCount > 0) {
        return SHRT_MAX;
    }
    else if (overflowCount < 0) {
        return SHRT_MIN;
    }
    return total >> 16;
}

/* Change the rate.  Interpolate with a sinc FIR filter using a Hann window. */
static int adjustRate(sonic_stream *stream, float rate, int originalNumOutputSamples)
{
    int    newSampleRate = static_cast<int>(stream->sample_rate / rate);
    int    oldSampleRate = stream->sample_rate;
    int    numChannels   = stream->nb_channels;
    int    position;
    short *in, *out;
    int    i;
    int    N = SINC_FILTER_POINTS;

    /* Set these values to help with the integer math */
    while (newSampleRate > (1 << 14) || oldSampleRate > (1 << 14)) {
        newSampleRate >>= 1;
        oldSampleRate >>= 1;
    }
    if (stream->output_nb_samples == originalNumOutputSamples) {
        return 1;
    }
    if (!moveNewSamplesToPitchBuffer(stream, originalNumOutputSamples)) {
        return 0;
    }
    /* Leave at least N pitch sample in the buffer */
    for (position = 0; position < stream->pitch_nb_samples - N; position++) {
        while ((stream->oldRatePosition + 1) * newSampleRate > stream->newRatePosition * oldSampleRate) {
            if (!enlargeOutputBufferIfNeeded(stream, 1)) {
                return 0;
            }
            out = stream->output_buffer + stream->output_nb_samples * numChannels;
            in  = stream->pitch_buffer + position * numChannels;
            for (i = 0; i < numChannels; i++) {
                *out++ = interpolate(stream, in, oldSampleRate, newSampleRate);
                in++;
            }
            stream->newRatePosition++;
            stream->output_nb_samples++;
        }
        stream->oldRatePosition++;
        if (stream->oldRatePosition == oldSampleRate) {
            stream->oldRatePosition = 0;
            stream->newRatePosition = 0;
        }
    }
    removePitchSamples(stream, position);
    return 1;
}

/* Skip over a pitch period.  Return the number of output samples. */
static int skipPitchPeriod(sonic_stream *stream, short *data, float speed, int period)
{
    const int nb_channels = stream->nb_channels;

    // speeds >= 2.0, we skip over a portion of each pitch period rather than dropping whole pitch periods
    const int new_samples = speed >= 2.0f ? static_cast<int>(period / (speed - 1.0f)) : period;

    if (!enlargeOutputBufferIfNeeded(stream, new_samples)) {
        return 0;
    }

    overlapAdd(new_samples, nb_channels, stream->output_buffer + stream->output_nb_samples * nb_channels,
               data, data + period * nb_channels);

    stream->output_nb_samples += new_samples;

    return new_samples;
}

/* Insert a pitch period, and determine how much input to copy directly. */
static int insertPitchPeriod(sonic_stream *stream, short *data, float speed, int period)
{
    const int nb_channels = stream->nb_channels;
    const int new_samples = speed <= 0.5f ? static_cast<int>(period * speed / (1.0f - speed)) : period;

    if (!enlargeOutputBufferIfNeeded(stream, period + new_samples)) {
        return 0;
    }

    short *out = stream->output_buffer + stream->output_nb_samples * nb_channels;
    memcpy(out, data, period * sizeof(short) * nb_channels);
    out = stream->output_buffer + (stream->output_nb_samples + period) * nb_channels;
    overlapAdd(new_samples, nb_channels, out, data + period * nb_channels, data);

    stream->output_nb_samples += period + new_samples;

    return new_samples;
}

/* PICOLA copies input to output until the total output samples == consumed input samples * speed. */
static int copyUnmodifiedSamples(sonic_stream *stream, short *samples, float speed, int position,
                                 int *newSamples)
{
    int   availableSamples = stream->input_nb_samples - position;
    float inputToCopyFloat = 1 - stream->time_error * speed / (stream->sample_period * (speed - 1.0f));

    *newSamples = inputToCopyFloat > availableSamples ? availableSamples : (int)inputToCopyFloat;
    if (!copyToOutput(stream, samples, *newSamples)) {
        return 0;
    }

    stream->time_error += *newSamples * stream->sample_period * (speed - 1.0f) / speed;

    return 1;
}

/* Resample as many pitch periods as we have buffered on the input.
   Return 0 if we fail to resize an input or output buffer. */
static int changeSpeed(sonic_stream *stream, float speed)
{
    int nb_samples = stream->input_nb_samples;

    int position       = 0;
    int new_nb_samples = 0;
    int maxRequired    = stream->maxRequired;

    if (stream->input_nb_samples < maxRequired) {
        return 1;
    }

    do {
        short *samples = stream->input_buffer + position * stream->nb_channels;
        if ((speed > 1.0f && speed < 2.0f && stream->time_error < 0.0f) ||
            (speed < 1.0f && speed > 0.5f && stream->time_error > 0.0f)) {
            /* Deal with the case where PICOLA is still copying input samples to output unmodified, */
            if (!copyUnmodifiedSamples(stream, samples, speed, position, &new_nb_samples)) {
                return 0;
            }
            position += new_nb_samples;
        }
        else {
            /* We are in the remaining cases, either inserting/removing a pitch period
               for speed < 2.0X, or a portion of one for speed >= 2.0X. */
            int period = findPitchPeriod(stream, samples, 1);
            if (speed > 1.0) {
                new_nb_samples  = skipPitchPeriod(stream, samples, speed, period);
                position       += period + new_nb_samples;
                if (speed < 2.0) {
                    stream->time_error +=
                        (new_nb_samples * stream->sample_period) -
                        ((period + new_nb_samples) * stream->input_play_time / stream->input_nb_samples);
                }
            }
            else {
                new_nb_samples  = insertPitchPeriod(stream, samples, speed, period);
                position       += new_nb_samples;
                if (speed > 0.5) {
                    stream->time_error +=
                        (period + new_nb_samples) * stream->sample_period -
                        (new_nb_samples * stream->input_play_time / stream->input_nb_samples);
                }
            }

            if (new_nb_samples == 0) {
                return 0; /* Failed to resize output buffer */
            }
        }
    } while (position + maxRequired <= nb_samples);

    removeInputSamples(stream, position);

    return 1;
}

/**
 * Resample as many pitch periods as we have buffered on the input.
 *
 * @return Return 0 if we fail to resize an input or output buffer.
 */
static int processStreamInput(sonic_stream *stream)
{
    const int   originalNumOutputSamples = stream->output_nb_samples;
    const float rate                     = stream->rate * stream->pitch;

    if (stream->input_nb_samples == 0) {
        return 1;
    }

    float localSpeed = stream->input_nb_samples * stream->sample_period / stream->input_play_time;
    if (localSpeed > 1.00001 || localSpeed < 0.99999) {
        changeSpeed(stream, localSpeed);
    }
    else {
        if (!copyInputToOutput(stream, stream->input_nb_samples)) {
            return 0;
        }
    }

    if (rate != 1.0f) {
        if (!adjustRate(stream, rate, originalNumOutputSamples)) {
            return 0;
        }
    }

    return 1;
}

/* Create a sonic stream.  Return NULL only if we are out of memory and cannot allocate the stream. */
sonic_stream *sonic_stream_create(int sample_fmt, int sample_rate, int nb_channels)
{
    if (sample_fmt != 0 && sample_fmt != 1 && sample_fmt != 3) return nullptr;

    const auto stream = (sonic_stream *)calloc(1, sizeof(sonic_stream));

    if (!stream || !allocateStreamBuffers(stream, sample_rate, nb_channels)) return nullptr;

    stream->sample_fmt      = sample_fmt;
    stream->speed           = 1.0f;
    stream->pitch           = 1.0f;
    stream->rate            = 1.0f;
    stream->oldRatePosition = 0;
    stream->newRatePosition = 0;
    stream->quality         = 0;

    return stream;
}

/* Destroy the sonic stream. */
void sonic_stream_destroy(sonic_stream *stream)
{
    freeStreamBuffers(stream);
    free(stream);
}

/* Simple wrapper around sonicWriteFloatToStream that does the unsigned char to float conversion for you. */
int sonic_stream_write_u8(sonic_stream *stream, const unsigned char *data, int nb_samples)
{
    std::scoped_lock lock(stream->mtx);

    if (!addUnsignedCharSamplesToInputBuffer(stream, data, nb_samples)) {
        return 0;
    }
    return processStreamInput(stream);
}

int sonic_stream_write(sonic_stream *stream, const void *data, int nb_samples)
{
    std::scoped_lock lock(stream->mtx);

    // clang-format off
    switch (stream->sample_fmt) {
    case 0 /* AV_SAMPLE_FMT_U8  */: if (!addUnsignedCharSamplesToInputBuffer(stream, static_cast<const unsigned char *>(data), nb_samples)) { return 0; }  break;
    case 1 /* AV_SAMPLE_FMT_S16 */: if (!addShortSamplesToInputBuffer(stream, static_cast<const short *>(data), nb_samples)) { return 0; }                 break;
    case 3 /* AV_SAMPLE_FMT_FLT */: if (!addFloatSamplesToInputBuffer(stream, static_cast<const float *>(data), nb_samples)) { return 0; }                 break;
    default:                        return 0;
    }
    // clang-format on

    return processStreamInput(stream);
}

int sonic_stream_read(sonic_stream *stream, void *data, int nb_samples)
{
    // clang-format off
    switch (stream->sample_fmt) {
    case 0 /* AV_SAMPLE_FMT_U8  */: return sonic_stream_read_u8(stream, static_cast<unsigned char *>(data), nb_samples);
    case 1 /* AV_SAMPLE_FMT_S16 */: return sonic_stream_read_s16(stream, static_cast<short *>(data), nb_samples);
    case 3 /* AV_SAMPLE_FMT_FLT */: return sonic_stream_read_f32(stream, static_cast<float *>(data), nb_samples);
    default:                        return 0;
    }
    // clang-format on
}

/* Read unsigned char data out of the stream.  Sometimes no data will be available, and zero is returned,
 * which is not an error condition. */
int sonic_stream_read_u8(sonic_stream *stream, unsigned char *data, int maxSamples)
{
    std::scoped_lock lock(stream->mtx);

    int numSamples        = stream->output_nb_samples;
    int remaining_samples = 0;

    if (numSamples == 0) {
        return 0;
    }

    if (numSamples > maxSamples) {
        remaining_samples = numSamples - maxSamples;
        numSamples        = maxSamples;
    }

    short *buffer = stream->output_buffer;
    int    count  = numSamples * stream->nb_channels;
    while (count--) {
        *data++ = (char)((*buffer++) >> 8) + 128;
    }

    if (remaining_samples > 0) {
        memmove(stream->output_buffer, stream->output_buffer + numSamples * stream->nb_channels,
                remaining_samples * sizeof(short) * stream->nb_channels);
    }

    stream->output_nb_samples = remaining_samples;

    return numSamples;
}

/* Simple wrapper around sonicWriteFloatToStream that does the short to float conversion for you. */
int sonic_stream_write_s16(sonic_stream *stream, const short *data, int nb_samples)
{
    std::scoped_lock lock(stream->mtx);

    if (!addShortSamplesToInputBuffer(stream, data, nb_samples)) {
        return 0;
    }
    return processStreamInput(stream);
}

/* Read short data out of the stream.  Sometimes no data will be available, and zero is returned, which is
 * not an error condition. */
int sonic_stream_read_s16(sonic_stream *stream, short *samples, int maxSamples)
{
    std::scoped_lock lock(stream->mtx);

    if (stream->output_nb_samples == 0) {
        return 0;
    }

    int numSamples       = stream->output_nb_samples;
    int remainingSamples = 0;

    if (numSamples > maxSamples) {
        remainingSamples = numSamples - maxSamples;
        numSamples       = maxSamples;
    }

    memcpy(samples, stream->output_buffer, numSamples * sizeof(short) * stream->nb_channels);
    if (remainingSamples > 0) {
        memmove(stream->output_buffer, stream->output_buffer + numSamples * stream->nb_channels,
                remainingSamples * sizeof(short) * stream->nb_channels);
    }

    stream->output_nb_samples = remainingSamples;

    return numSamples;
}

/* Write floating point data to the input buffer and process it. */
int sonic_stream_write_f32(sonic_stream *stream, const float *data, int nb_samples)
{
    std::scoped_lock lock(stream->mtx);

    if (!addFloatSamplesToInputBuffer(stream, data, nb_samples)) {
        return 0;
    }
    return processStreamInput(stream);
}

/* Read data out of the stream.  Sometimes no data will be available, and zero is returned, which is not an
 * error condition. */
int sonic_stream_read_f32(sonic_stream *stream, float *data, int maxSamples)
{
    std::scoped_lock lock(stream->mtx);

    int numSamples       = stream->output_nb_samples;
    int remainingSamples = 0;

    if (numSamples == 0) {
        return 0;
    }
    if (numSamples > maxSamples) {
        remainingSamples = numSamples - maxSamples;
        numSamples       = maxSamples;
    }

    const short *buffer = stream->output_buffer;
    int          count  = numSamples * stream->nb_channels;
    while (count--) {
        *data++ = (*buffer++) / 32767.0f;
    }

    if (remainingSamples > 0) {
        memmove(stream->output_buffer, stream->output_buffer + numSamples * stream->nb_channels,
                remainingSamples * sizeof(short) * stream->nb_channels);
    }

    stream->output_nb_samples = remainingSamples;

    return numSamples;
}

/* Force the sonic stream to generate output using whatever data it currently has.  No extra delay will be
   added to the output, but flushing in the middle of words could introduce distortion. */
int sonic_stream_flush(sonic_stream *stream)
{
    std::scoped_lock lock(stream->mtx);

    const int   maxRequired       = stream->maxRequired;
    const int   remaining_samples = stream->input_nb_samples;
    const float speed             = stream->speed / stream->pitch;
    const float rate              = stream->rate * stream->pitch;
    const int   expected_samples =
        stream->output_nb_samples +
        static_cast<int>(std::lround((remaining_samples / speed + stream->pitch_nb_samples) / rate));

    /* Add enough silence to flush both input and pitch buffers. */
    if (!enlargeInputBufferIfNeeded(stream, remaining_samples + 2 * maxRequired)) {
        return 0;
    }
    memset(stream->input_buffer + remaining_samples * stream->nb_channels, 0,
           2 * maxRequired * sizeof(short) * stream->nb_channels);
    stream->input_nb_samples += 2 * maxRequired;

    // sonic_stream_write_s16
    if (!processStreamInput(stream)) {
        return 0;
    }

    /* Throw away any extra samples we generated due to the silence we added */
    if (stream->output_nb_samples > expected_samples) {
        stream->output_nb_samples = expected_samples;
    }

    /* Empty input and pitch buffers */
    stream->input_nb_samples = 0;
    stream->input_play_time  = 0.0f;
    stream->time_error       = 0.0f;
    stream->pitch_nb_samples = 0;

    return 1;
}

int sonic_stream_drain(sonic_stream *stream)
{
    if (!sonic_stream_flush(stream)) return 0;

    std::scoped_lock lock(stream->mtx);
    stream->output_nb_samples = 0;

    return 1;
}

/* Return the number of expected output samples */
int sonic_stream_expected_samples(sonic_stream *stream)
{
    std::scoped_lock lock(stream->mtx);

    const float speed = stream->speed / stream->pitch;
    const float rate  = stream->rate * stream->pitch;

    return stream->output_nb_samples +
           static_cast<int>(
               std::lround((stream->input_nb_samples / speed + stream->pitch_nb_samples) / rate));
}

/* Return the number of samples in the output buffer */
int sonic_stream_available_samples(sonic_stream *stream)
{
    std::scoped_lock lock(stream->mtx);

    return stream->output_nb_samples;
}

int sonic_stream_remaining_samples(sonic_stream *stream)
{
    std::scoped_lock lock(stream->mtx);

    return stream->input_nb_samples;
}

/* Get the sample rate of the stream. */
int sonic_stream_get_sample_rate(const sonic_stream *stream)
{
    std::scoped_lock lock(stream->mtx);

    return stream->sample_rate;
}

/* Set the sample rate of the stream.  This will cause samples buffered in the stream to be lost. */
void sonic_stream_set_sample_rate(sonic_stream *stream, int sample_rate)
{
    std::scoped_lock lock(stream->mtx);

    freeStreamBuffers(stream);
    allocateStreamBuffers(stream, sample_rate, stream->nb_channels);
}

int sonic_stream_get_sample_fmt(const sonic_stream *stream)
{
    std::scoped_lock lock(stream->mtx);

    return stream->sample_fmt;
}

/* Get the number of channels. */
int sonic_stream_get_nb_channels(const sonic_stream *stream)
{
    std::scoped_lock lock(stream->mtx);

    return stream->nb_channels;
}

/* Set the num channels of the stream.  This will cause samples buffered in the stream to be lost. */
void sonic_stream_set_nb_channels(sonic_stream *stream, int numChannels)
{
    std::scoped_lock lock(stream->mtx);

    freeStreamBuffers(stream);
    allocateStreamBuffers(stream, stream->sample_rate, numChannels);
}

/* Get the speed of the stream. */
float sonic_stream_get_speed(const sonic_stream *stream)
{
    std::scoped_lock lock(stream->mtx);

    return stream->speed;
}

/* Set the speed of the stream. */
void sonic_stream_set_speed(sonic_stream *stream, float speed)
{
    std::scoped_lock lock(stream->mtx);

    stream->speed = speed;
}

/* Get the pitch of the stream. */
float sonic_stream_get_pitch(const sonic_stream *stream)
{
    std::scoped_lock lock(stream->mtx);

    return stream->pitch;
}

/* Set the pitch of the stream. */
void sonic_stream_set_pitch(sonic_stream *stream, float pitch)
{
    std::scoped_lock lock(stream->mtx);

    stream->pitch = pitch;
}

/* Get the rate of the stream. */
float sonic_stream_get_rate(const sonic_stream *stream)
{
    std::scoped_lock lock(stream->mtx);

    return stream->rate;
}

/* Set the playback rate of the stream. This scales pitch and speed at the same time. */
void sonic_stream_set_rate(sonic_stream *stream, float rate)
{
    std::scoped_lock lock(stream->mtx);

    stream->rate = rate;

    stream->oldRatePosition = 0;
    stream->newRatePosition = 0;
}

/* Get the quality setting. */
int sonic_stream_get_quality(const sonic_stream *stream)
{
    std::scoped_lock lock(stream->mtx);

    return stream->quality;
}

/* Set the "quality".  Default 0 is virtually as good as 1, but very much faster. */
void sonic_stream_set_quality(sonic_stream *stream, int quality)
{
    std::scoped_lock lock(stream->mtx);

    stream->quality = quality;
}
