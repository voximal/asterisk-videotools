/*
 * SIPR / ACELP.NET decoder
 *
 * Copyright (c) 2008 Vladimir Voroshilov
 * Copyright (c) 2009 Vitor Sessak
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <math.h>
#include <stdint.h>

#include "libavutil/mathematics.h"
#include "avcodec.h"
#define ALT_BITSTREAM_READER_LE
#include "get_bits.h"
#include "dsputil.h"

#include "lsp.h"
#include "celp_math.h"
#include "acelp_vectors.h"
#include "acelp_pitch_delay.h"
#include "acelp_filters.h"
#include "celp_filters.h"

#define LSFQ_DIFF_MIN        (0.0125 * M_PI)

#define LP_FILTER_ORDER      10

/** Number of past samples needed for excitation interpolation */
#define L_INTERPOL           (LP_FILTER_ORDER + 1)

/**  Subframe size for all modes except 16k */
#define SUBFR_SIZE           48

#define MAX_SUBFRAME_COUNT   5

#include "siprdata.h"

typedef enum {
    MODE_16k,
    MODE_8k5,
    MODE_6k5,
    MODE_5k0,
    MODE_COUNT
} SiprMode;

typedef struct {
    const char *mode_name;
    uint16_t bits_per_frame;
    uint8_t subframe_count;
    uint8_t frames_per_packet;
    float pitch_sharp_factor;

    /* bitstream parameters */
    uint8_t number_of_fc_indexes;

    /** size in bits of the i-th stage vector of quantizer */
    uint8_t vq_indexes_bits[5];

    /** size in bits of the adaptive-codebook index for every subframe */
    uint8_t pitch_delay_bits[5];

    uint8_t gp_index_bits;
    uint8_t fc_index_bits[10]; ///< size in bits of the fixed codebook indexes
    uint8_t gc_index_bits;     ///< size in bits of the gain  codebook indexes
} SiprModeParam;

static const SiprModeParam modes[MODE_COUNT] = {
    [MODE_8k5] = {
        .mode_name          = "8k5",
        .bits_per_frame     = 152,
        .subframe_count     = 3,
        .frames_per_packet  = 1,
        .pitch_sharp_factor = 0.8,

        .number_of_fc_indexes = 3,
        .vq_indexes_bits      = {6, 7, 7, 7, 5},
        .pitch_delay_bits     = {8, 5, 5},
        .gp_index_bits        = 0,
        .fc_index_bits        = {9, 9, 9},
        .gc_index_bits        = 7
    },

    [MODE_6k5] = {
        .mode_name          = "6k5",
        .bits_per_frame     = 232,
        .subframe_count     = 3,
        .frames_per_packet  = 2,
        .pitch_sharp_factor = 0.8,

        .number_of_fc_indexes = 3,
        .vq_indexes_bits      = {6, 7, 7, 7, 5},
        .pitch_delay_bits     = {8, 5, 5},
        .gp_index_bits        = 0,
        .fc_index_bits        = {5, 5, 5},
        .gc_index_bits        = 7
    },

    [MODE_5k0] = {
        .mode_name          = "5k0",
        .bits_per_frame     = 296,
        .subframe_count     = 5,
        .frames_per_packet  = 2,
        .pitch_sharp_factor = 0.85,

        .number_of_fc_indexes = 1,
        .vq_indexes_bits      = {6, 7, 7, 7, 5},
        .pitch_delay_bits     = {8, 5, 8, 5, 5},
        .gp_index_bits        = 0,
        .fc_index_bits        = {10},
        .gc_index_bits        = 7
    }
};

typedef struct {
    AVCodecContext *avctx;
    DSPContext dsp;

    SiprMode mode;

    float past_pitch_gain;
    float lsf_history[LP_FILTER_ORDER];

    float excitation[L_INTERPOL + PITCH_DELAY_MAX + 5*SUBFR_SIZE];

    DECLARE_ALIGNED_16(float, synth_buf[LP_FILTER_ORDER + 5*SUBFR_SIZE + 6]);

    float lsp_history[LP_FILTER_ORDER];
    float gain_mem;
    float energy_history[4];
    float highpass_filt_mem[2];
    float postfilter_mem[PITCH_DELAY_MAX + LP_FILTER_ORDER];

    /* 5k0 */
    float tilt_mem;
    float postfilter_agc;
    float postfilter_mem5k0[PITCH_DELAY_MAX + LP_FILTER_ORDER];
    float postfilter_syn5k0[LP_FILTER_ORDER + SUBFR_SIZE*5];
} SiprContext;

typedef struct {
    int vq_indexes[5];
    int pitch_delay[5];        ///< pitch delay
    int gp_index[5];           ///< adaptive-codebook gain indexes
    int16_t fc_indexes[5][10]; ///< fixed-codebook indexes
    int gc_index[5];           ///< fixed-codebook gain indexes
} SiprParameters;


static void dequant(float *out, const int *idx, const float *cbs[])
{
    int i;
    int stride  = 2;
    int num_vec = 5;

    for (i = 0; i < num_vec; i++)
        memcpy(out + stride*i, cbs[i] + stride*idx[i], stride*sizeof(float));

}

static void lsf_decode_fp(float *lsfnew, float *lsf_history,
                          const SiprParameters *parm)
{
    int i;
    float lsf_tmp[LP_FILTER_ORDER];

    dequant(lsf_tmp, parm->vq_indexes, lsf_codebooks);

    for (i = 0; i < LP_FILTER_ORDER; i++)
        lsfnew[i] = lsf_history[i] * 0.33 + lsf_tmp[i] + mean_lsf[i];

    ff_sort_nearly_sorted_floats(lsfnew, LP_FILTER_ORDER - 1);

    /* Note that a minimum distance is not enforced between the last value and
       the previous one, contrary to what is done in ff_acelp_reorder_lsf() */
    ff_set_min_dist_lsf(lsfnew, LSFQ_DIFF_MIN, LP_FILTER_ORDER - 1);
    lsfnew[9] = FFMIN(lsfnew[LP_FILTER_ORDER - 1], 1.3 * M_PI);

    memcpy(lsf_history, lsf_tmp, LP_FILTER_ORDER * sizeof(*lsf_history));

    for (i = 0; i < LP_FILTER_ORDER - 1; i++)
        lsfnew[i] = cos(lsfnew[i]);
    lsfnew[LP_FILTER_ORDER - 1] *= 6.153848 / M_PI;
}

/** Apply pitch lag to the fixed vector (AMR section 6.1.2). */
static void pitch_sharpening(int pitch_lag_int, float beta,
                             float *fixed_vector)
{
    int i;

    for (i = pitch_lag_int; i < SUBFR_SIZE; i++)
        fixed_vector[i] += beta * fixed_vector[i - pitch_lag_int];
}

/**
 * Extracts decoding parameters from the input bitstream.
 * @param parms          parameters structure
 * @param pgb            pointer to initialized GetBitContext structure
 */
static void decode_parameters(SiprParameters* parms, GetBitContext *pgb,
                              const SiprModeParam *p)
{
    int i, j;

    for (i = 0; i < 5; i++)
        parms->vq_indexes[i]        = get_bits(pgb, p->vq_indexes_bits[i]);

    for (i = 0; i < p->subframe_count; i++) {
        parms->pitch_delay[i]       = get_bits(pgb, p->pitch_delay_bits[i]);
        parms->gp_index[i]          = get_bits(pgb, p->gp_index_bits);

        for (j = 0; j < p->number_of_fc_indexes; j++)
            parms->fc_indexes[i][j] = get_bits(pgb, p->fc_index_bits[j]);

        parms->gc_index[i]          = get_bits(pgb, p->gc_index_bits);
    }
}

static void lsp2lpc_sipr(const double *lsp, float *Az)
{
    int lp_half_order = LP_FILTER_ORDER >> 1;
    double buf[(LP_FILTER_ORDER >> 1) + 1];
    double pa[(LP_FILTER_ORDER >> 1) + 1];
    double *qa = buf + 1;
    int i,j;

    qa[-1] = 0.0;

    ff_lsp2polyf(lsp    , pa, lp_half_order    );
    ff_lsp2polyf(lsp + 1, qa, lp_half_order - 1);

    for (i = 1, j = LP_FILTER_ORDER - 1; i < lp_half_order; i++, j--) {
        double paf =  pa[i]            * (1 + lsp[LP_FILTER_ORDER - 1]);
        double qaf = (qa[i] - qa[i-2]) * (1 - lsp[LP_FILTER_ORDER - 1]);
        Az[i-1]  = (paf + qaf) * 0.5;
        Az[j-1]  = (paf - qaf) * 0.5;
    }

    Az[lp_half_order - 1] = (1.0 + lsp[LP_FILTER_ORDER - 1]) *
        pa[lp_half_order] * 0.5;

    Az[LP_FILTER_ORDER - 1] = lsp[LP_FILTER_ORDER - 1];
}

static void sipr_decode_lp(float *lsfnew, const float *lsfold, float *Az,
                           int num_subfr)
{
    double lsfint[LP_FILTER_ORDER];
    int i,j;
    float t, t0 = 1.0 / num_subfr;

    t = t0 * 0.5;
    for (i = 0; i < num_subfr; i++) {
        for (j = 0; j < LP_FILTER_ORDER; j++)
            lsfint[j] = lsfold[j] * (1 - t) + t * lsfnew[j];

        lsp2lpc_sipr(lsfint, Az);
        Az += LP_FILTER_ORDER;
        t += t0;
    }
}

/**
 * Evaluates the adaptative impulse response.
 */
static void eval_ir(const float *Az, int pitch_lag, float *freq,
                    float pitch_sharp_factor)
{
    float tmp1[SUBFR_SIZE+1], tmp2[LP_FILTER_ORDER+1];
    int i;

    tmp1[0] = 1.;
    for (i = 0; i < LP_FILTER_ORDER; i++) {
        tmp1[i+1] = Az[i] * ff_pow_0_55[i];
        tmp2[i  ] = Az[i] * ff_pow_0_7 [i];
    }
    memset(tmp1 + 11, 0, 37 * sizeof(float));

    ff_celp_lp_synthesis_filterf(freq, tmp2, tmp1, SUBFR_SIZE,
                                 LP_FILTER_ORDER);

    pitch_sharpening(pitch_lag, pitch_sharp_factor, freq);
}

/**
 * Evaluates the convolution of a vector with a sparse vector.
 */
static void convolute_with_sparse(float *out, const AMRFixed *pulses,
                                  const float *shape, int length)
{
    int i, j;

    memset(out, 0, length*sizeof(float));
    for (i = 0; i < pulses->n; i++)
        for (j = pulses->x[i]; j < length; j++)
            out[j] += pulses->y[i] * shape[j - pulses->x[i]];
}

/**
 * Apply postfilter, very similar to AMR one.
 */
static void postfilter_5k0(SiprContext *ctx, const float *lpc, float *samples)
{
    float buf[SUBFR_SIZE + LP_FILTER_ORDER];
    float *pole_out = buf + LP_FILTER_ORDER;
    float lpc_n[LP_FILTER_ORDER];
    float lpc_d[LP_FILTER_ORDER];
    int i;

    for (i = 0; i < LP_FILTER_ORDER; i++) {
        lpc_d[i] = lpc[i] * ff_pow_0_75[i];
        lpc_n[i] = lpc[i] *    pow_0_5 [i];
    };

    memcpy(pole_out - LP_FILTER_ORDER, ctx->postfilter_mem,
           LP_FILTER_ORDER*sizeof(float));

    ff_celp_lp_synthesis_filterf(pole_out, lpc_d, samples, SUBFR_SIZE,
                                 LP_FILTER_ORDER);

    memcpy(ctx->postfilter_mem, pole_out + SUBFR_SIZE - LP_FILTER_ORDER,
           LP_FILTER_ORDER*sizeof(float));

    ff_tilt_compensation(&ctx->tilt_mem, 0.4, pole_out, SUBFR_SIZE);

    memcpy(pole_out - LP_FILTER_ORDER, ctx->postfilter_mem5k0,
           LP_FILTER_ORDER*sizeof(*pole_out));

    memcpy(ctx->postfilter_mem5k0, pole_out + SUBFR_SIZE - LP_FILTER_ORDER,
           LP_FILTER_ORDER*sizeof(*pole_out));

    ff_celp_lp_zero_synthesis_filterf(samples, lpc_n, pole_out, SUBFR_SIZE,
                                      LP_FILTER_ORDER);

}

static void decode_fixed_sparse(AMRFixed *fixed_sparse, const int16_t *pulses,
                                SiprMode mode, int low_gain)
{
    int i;

    switch (mode) {
    case MODE_6k5:
        for (i = 0; i < 3; i++) {
            fixed_sparse->x[i] = 3 * (pulses[i] & 0xf) + i;
            fixed_sparse->y[i] = pulses[i] & 0x10 ? -1 : 1;
        }
        fixed_sparse->n = 3;
        break;
    case MODE_8k5:
        for (i = 0; i < 3; i++) {
            fixed_sparse->x[2*i    ] = 3 * ((pulses[i] >> 4) & 0xf) + i;
            fixed_sparse->x[2*i + 1] = 3 * ( pulses[i]       & 0xf) + i;

            fixed_sparse->y[2*i    ] = (pulses[i] & 0x100) ? -1.0: 1.0;

            fixed_sparse->y[2*i + 1] =
                (fixed_sparse->x[2*i + 1] < fixed_sparse->x[2*i]) ?
                -fixed_sparse->y[2*i    ] : fixed_sparse->y[2*i];
        }

        fixed_sparse->n = 6;
        break;
    case MODE_5k0:
    default:
        if (low_gain) {
            int offset = (pulses[0] & 0x200) ? 2 : 0;
            int val = pulses[0];

            for (i = 0; i < 3; i++) {
                int index = (val & 0x7) * 6 + 4 - i*2;

                fixed_sparse->y[i] = (offset + index) & 0x3 ? -1 : 1;
                fixed_sparse->x[i] = index;

                val >>= 3;
            }
            fixed_sparse->n = 3;
        } else {
            int pulse_subset = (pulses[0] >> 8) & 1;

            fixed_sparse->x[0] = ((pulses[0] >> 4) & 15) * 3 + pulse_subset;
            fixed_sparse->x[1] = ( pulses[0]       & 15) * 3 + pulse_subset + 1;

            fixed_sparse->y[0] = pulses[0] & 0x200 ? -1 : 1;
            fixed_sparse->y[1] = -fixed_sparse->y[0];
            fixed_sparse->n = 2;
        }
        break;
    }
}

static void decode_frame(SiprContext *ctx, SiprParameters *params,
                         float *out_data)
{
    int i, j;
    int subframe_count = modes[ctx->mode].subframe_count;
    int frame_size = subframe_count * SUBFR_SIZE;
    float Az[LP_FILTER_ORDER * MAX_SUBFRAME_COUNT];
    float *excitation;
    float ir_buf[SUBFR_SIZE + LP_FILTER_ORDER];
    float lsf_new[LP_FILTER_ORDER];
    float *impulse_response = ir_buf + LP_FILTER_ORDER;
    float *synth = ctx->synth_buf + 16; // 16 instead of LP_FILTER_ORDER for
                                        // memory alignment
    int t0_first = 0;
    AMRFixed fixed_cb;

    memset(ir_buf, 0, LP_FILTER_ORDER * sizeof(float));
    lsf_decode_fp(lsf_new, ctx->lsf_history, params);

    sipr_decode_lp(lsf_new, ctx->lsp_history, Az, subframe_count);

    memcpy(ctx->lsp_history, lsf_new, LP_FILTER_ORDER * sizeof(float));

    excitation = ctx->excitation + PITCH_DELAY_MAX + L_INTERPOL;

    for (i = 0; i < subframe_count; i++) {
        float *pAz = Az + i*LP_FILTER_ORDER;
        float fixed_vector[SUBFR_SIZE];
        int T0,T0_frac;
        float pitch_gain, gain_code, avg_energy;

        ff_decode_pitch_lag(&T0, &T0_frac, params->pitch_delay[i], t0_first, i,
                            ctx->mode == MODE_5k0, 6);

        if (i == 0 || (i == 2 && ctx->mode == MODE_5k0))
            t0_first = T0;

        ff_acelp_interpolatef(excitation, excitation - T0 + (T0_frac <= 0),
                              ff_b60_sinc, 6,
                              2 * ((2 + T0_frac)%3 + 1), LP_FILTER_ORDER,
                              SUBFR_SIZE);

        decode_fixed_sparse(&fixed_cb, params->fc_indexes[i], ctx->mode,
                            ctx->past_pitch_gain < 0.8);

        eval_ir(pAz, T0, impulse_response, modes[ctx->mode].pitch_sharp_factor);

        convolute_with_sparse(fixed_vector, &fixed_cb, impulse_response,
                              SUBFR_SIZE);

        avg_energy =
            (0.01 + ff_dot_productf(fixed_vector, fixed_vector, SUBFR_SIZE))/
                SUBFR_SIZE;

        ctx->past_pitch_gain = pitch_gain = gain_cb[params->gc_index[i]][0];

        gain_code = ff_amr_set_fixed_gain(gain_cb[params->gc_index[i]][1],
                                          avg_energy, ctx->energy_history,
                                          34 - 15.0/(0.05*M_LN10/M_LN2),
                                          pred);

        ff_weighted_vector_sumf(excitation, excitation, fixed_vector,
                                pitch_gain, gain_code, SUBFR_SIZE);

        pitch_gain *= 0.5 * pitch_gain;
        pitch_gain = FFMIN(pitch_gain, 0.4);

        ctx->gain_mem = 0.7 * ctx->gain_mem + 0.3 * pitch_gain;
        ctx->gain_mem = FFMIN(ctx->gain_mem, pitch_gain);
        gain_code *= ctx->gain_mem;

        for (j = 0; j < SUBFR_SIZE; j++)
            fixed_vector[j] = excitation[j] - gain_code * fixed_vector[j];

        if (ctx->mode == MODE_5k0) {
            postfilter_5k0(ctx, pAz, fixed_vector);

            ff_celp_lp_synthesis_filterf(ctx->postfilter_syn5k0 + LP_FILTER_ORDER + i*SUBFR_SIZE,
                                         pAz, excitation, SUBFR_SIZE,
                                         LP_FILTER_ORDER);
        }

        ff_celp_lp_synthesis_filterf(synth + i*SUBFR_SIZE, pAz, fixed_vector,
                                     SUBFR_SIZE, LP_FILTER_ORDER);

        excitation += SUBFR_SIZE;
    }

    memcpy(synth - LP_FILTER_ORDER, synth + frame_size - LP_FILTER_ORDER,
           LP_FILTER_ORDER * sizeof(float));

    if (ctx->mode == MODE_5k0) {
        for (i = 0; i < subframe_count; i++) {
            float energy = ff_dot_productf(ctx->postfilter_syn5k0 + LP_FILTER_ORDER + i*SUBFR_SIZE,
                                           ctx->postfilter_syn5k0 + LP_FILTER_ORDER + i*SUBFR_SIZE,
                                           SUBFR_SIZE);
            ff_adaptative_gain_control(&synth[i * SUBFR_SIZE], energy,
                                       SUBFR_SIZE, 0.9, &ctx->postfilter_agc);
        }

        memcpy(ctx->postfilter_syn5k0, ctx->postfilter_syn5k0 + frame_size,
               LP_FILTER_ORDER*sizeof(float));
    }
    memcpy(ctx->excitation, excitation - PITCH_DELAY_MAX - L_INTERPOL,
           (PITCH_DELAY_MAX + L_INTERPOL) * sizeof(float));

    ff_acelp_apply_order_2_transfer_function(synth,
                                             (const float[2]) {-1.99997   , 1.000000000},
                                             (const float[2]) {-1.93307352, 0.935891986},
                                             0.939805806,
                                             ctx->highpass_filt_mem,
                                             frame_size);

    ctx->dsp.vector_clipf(out_data, synth, -1, 32767./(1<<15), frame_size);

}

static av_cold int sipr_decoder_init(AVCodecContext * avctx)
{
    SiprContext *ctx = avctx->priv_data;
    int i;

    if      (avctx->bit_rate > 12200) ctx->mode = MODE_16k;
    else if (avctx->bit_rate > 7500 ) ctx->mode = MODE_8k5;
    else if (avctx->bit_rate > 5750 ) ctx->mode = MODE_6k5;
    else                              ctx->mode = MODE_5k0;

    av_log(avctx, AV_LOG_DEBUG, "Mode: %s\n", modes[ctx->mode].mode_name);

    for (i = 0; i < LP_FILTER_ORDER; i++)
        ctx->lsp_history[i] = cos((i+1) * M_PI / (LP_FILTER_ORDER + 1));

    for (i = 0; i < 4; i++)
        ctx->energy_history[i] = -14;

    avctx->sample_fmt = SAMPLE_FMT_FLT;

    if (ctx->mode == MODE_16k) {
        av_log(avctx, AV_LOG_ERROR, "decoding 16kbps SIPR files is not "
                                    "supported yet.\n");
        return -1;
    }

    dsputil_init(&ctx->dsp, avctx);

    return 0;
}

static int sipr_decode_frame(AVCodecContext *avctx, void *datap,
                             int *data_size, AVPacket *avpkt)
{
    SiprContext *ctx = avctx->priv_data;
    const uint8_t *buf=avpkt->data;
    SiprParameters parm;
    const SiprModeParam *mode_par = &modes[ctx->mode];
    GetBitContext gb;
    float *data = datap;
    int i;

    ctx->avctx = avctx;
    if (avpkt->size < (mode_par->bits_per_frame >> 3)) {
        av_log(avctx, AV_LOG_ERROR,
               "Error processing packet: packet size (%d) too small\n",
               avpkt->size);

        *data_size = 0;
        return -1;
    }
    if (*data_size < SUBFR_SIZE * mode_par->subframe_count * sizeof(float)) {
        av_log(avctx, AV_LOG_ERROR,
               "Error processing packet: output buffer (%d) too small\n",
               *data_size);

        *data_size = 0;
        return -1;
    }

    init_get_bits(&gb, buf, mode_par->bits_per_frame);

    for (i = 0; i < mode_par->frames_per_packet; i++) {
        decode_parameters(&parm, &gb, mode_par);
        decode_frame(ctx, &parm, data);

        data += SUBFR_SIZE * mode_par->subframe_count;
    }

    *data_size = mode_par->frames_per_packet * SUBFR_SIZE *
        mode_par->subframe_count * sizeof(float);

    return mode_par->bits_per_frame >> 3;
};

AVCodec sipr_decoder = {
    "sipr",
    CODEC_TYPE_AUDIO,
    CODEC_ID_SIPR,
    sizeof(SiprContext),
    sipr_decoder_init,
    NULL,
    NULL,
    sipr_decode_frame,
    .long_name = NULL_IF_CONFIG_SMALL("RealAudio SIPR / ACELP.NET"),
};
