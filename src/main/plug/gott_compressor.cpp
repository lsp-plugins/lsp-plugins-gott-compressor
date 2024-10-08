/*
 * Copyright (C) 2024 Linux Studio Plugins Project <https://lsp-plug.in/>
 *           (C) 2024 Vladimir Sadovnikov <sadko4u@gmail.com>
 *
 * This file is part of lsp-plugins-gott-compressor
 * Created on: 29 мая 2023 г.
 *
 * lsp-plugins-gott-compressor is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * lsp-plugins-gott-compressor is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with lsp-plugins-gott-compressor. If not, see <https://www.gnu.org/licenses/>.
 */

#include <lsp-plug.in/common/alloc.h>
#include <lsp-plug.in/common/bits.h>
#include <lsp-plug.in/common/debug.h>
#include <lsp-plug.in/dsp/dsp.h>
#include <lsp-plug.in/dsp-units/units.h>
#include <lsp-plug.in/dsp-units/misc/envelope.h>
#include <lsp-plug.in/plug-fw/core/AudioBuffer.h>
#include <lsp-plug.in/plug-fw/meta/func.h>
#include <lsp-plug.in/shared/debug.h>
#include <lsp-plug.in/shared/id_colors.h>
#include <lsp-plug.in/stdlib/string.h>

#include <private/plugins/gott_compressor.h>

/* The size of temporary buffer for audio processing */
#define GOTT_BUFFER_SIZE        0x400U

namespace lsp
{
    namespace plugins
    {
        //---------------------------------------------------------------------
        // Plugin factory
        static const meta::plugin_t *plugins[] =
        {
            &meta::gott_compressor_mono,
            &meta::gott_compressor_stereo,
            &meta::gott_compressor_lr,
            &meta::gott_compressor_ms,
            &meta::sc_gott_compressor_mono,
            &meta::sc_gott_compressor_stereo,
            &meta::sc_gott_compressor_lr,
            &meta::sc_gott_compressor_ms
        };

        static plug::Module *plugin_factory(const meta::plugin_t *meta)
        {
            return new gott_compressor(meta);
        }

        static plug::Factory factory(plugin_factory, plugins, 8);

        //---------------------------------------------------------------------
        // Implementation
        gott_compressor::gott_compressor(const meta::plugin_t *meta):
            Module(meta)
        {
            nMode               = GOTT_MONO;
            bSidechain          = false;
            if (!strcmp(meta->uid, meta::gott_compressor_mono.uid))
            {
                nMode               = GOTT_MONO;
                bSidechain          = false;
            }
            else if (!strcmp(meta->uid, meta::gott_compressor_stereo.uid))
            {
                nMode               = GOTT_STEREO;
                bSidechain          = false;
            }
            else if (!strcmp(meta->uid, meta::gott_compressor_ms.uid))
            {
                nMode               = GOTT_MS;
                bSidechain          = false;
            }
            else if (!strcmp(meta->uid, meta::gott_compressor_lr.uid))
            {
                nMode               = GOTT_LR;
                bSidechain          = false;
            }
            else if (!strcmp(meta->uid, meta::sc_gott_compressor_mono.uid))
            {
                nMode               = GOTT_MONO;
                bSidechain          = true;
            }
            else if (!strcmp(meta->uid, meta::sc_gott_compressor_stereo.uid))
            {
                nMode               = GOTT_STEREO;
                bSidechain          = true;
            }
            else if (!strcmp(meta->uid, meta::sc_gott_compressor_ms.uid))
            {
                nMode               = GOTT_MS;
                bSidechain          = true;
            }
            else if (!strcmp(meta->uid, meta::sc_gott_compressor_lr.uid))
            {
                nMode               = GOTT_LR;
                bSidechain          = true;
            }

            enXOver             = XOVER_MODERN;
            nBands              = meta::gott_compressor::BANDS_MAX;
            nScType             = SCT_INTERNAL;
            bProt               = true;
            bEnvUpdate          = true;
            bStereoSplit        = false;
            fInGain             = GAIN_AMP_0_DB;
            fDryGain            = GAIN_AMP_M_INF_DB;
            fWetGain            = GAIN_AMP_0_DB;
            fScPreamp           = GAIN_AMP_0_DB;
            nEnvBoost           = 0;
            fZoom               = GAIN_AMP_0_DB;
            for (size_t i=0; i<meta::gott_compressor::BANDS_MAX-1; ++i)
                vSplits[i]          = 0.0f;

            vChannels           = NULL;
            vAnalyze[0]         = NULL;
            vAnalyze[1]         = NULL;
            vAnalyze[2]         = NULL;
            vAnalyze[3]         = NULL;
            vEmptyBuf           = NULL;
            vBuffer             = NULL;
            vProtBuffer         = NULL;
            vSCIn[0]            = NULL;
            vSCIn[1]            = NULL;
            vSC[0]              = NULL;
            vSC[1]              = NULL;
            vEnv                = NULL;
            vTr                 = NULL;
            vPFc                = NULL;
            vRFc                = NULL;
            vCurveBuffer        = NULL;
            vFreqBuffer         = NULL;
            vFreqIndexes        = NULL;
            pIDisplay           = NULL;

            pBypass             = NULL;
            pMode               = NULL;
            pProt               = NULL;
            pInGain             = NULL;
            pOutGain            = NULL;
            pDryGain            = NULL;
            pWetGain            = NULL;
            pDryWet             = NULL;
            pScMode             = NULL;
            pScSource           = NULL;
            pScSpSource         = NULL;
            pScPreamp           = NULL;
            pScReact            = NULL;
            pLookahead          = NULL;
            pReactivity         = NULL;
            pShiftGain          = NULL;
            pZoom               = NULL;
            pEnvBoost           = NULL;
            pSplits[0]          = NULL;
            pSplits[1]          = NULL;
            pSplits[2]          = NULL;
            pExtraBand          = NULL;
            pScType             = NULL;
            pStereoSplit        = NULL;

            pData               = NULL;
        }

        gott_compressor::~gott_compressor()
        {
            do_destroy();
        }

        void gott_compressor::init(plug::IWrapper *wrapper, plug::IPort **ports)
        {
            // Call parent class for initialization
            plug::Module::init(wrapper, ports);

            // Initialize analyzer
            size_t channels         = (nMode == GOTT_MONO) ? 1 : 2;
            size_t an_cid           = 0;
            size_t filter_cid       = 0;
            if (!sAnalyzer.init(2*channels, meta::gott_compressor::FFT_RANK,
                                MAX_SAMPLE_RATE, meta::gott_compressor::REFRESH_RATE))
                return;

            sAnalyzer.set_rank(meta::gott_compressor::FFT_RANK);
            sAnalyzer.set_activity(false);
            sAnalyzer.set_envelope(dspu::envelope::WHITE_NOISE);
            sAnalyzer.set_window(meta::gott_compressor::FFT_WINDOW);
            sAnalyzer.set_rate(meta::gott_compressor::REFRESH_RATE);

            sCounter.set_frequency(meta::gott_compressor::REFRESH_RATE, true);

            // Initialize filters according to number of bands
            if (sFilters.init(meta::gott_compressor::BANDS_MAX * channels) != STATUS_OK)
                return;

            // Initialize surge protection module
            if (!sProtSC.init(channels, meta::gott_compressor::SC_REACTIVITY_MAX))
                return;

            // Compute amount of memory
            size_t szof_channels    = align_size(sizeof(channel_t) * channels, OPTIMAL_ALIGN);
            size_t szof_buffer      = align_size(sizeof(float) * GOTT_BUFFER_SIZE, OPTIMAL_ALIGN);
            size_t szof_curve       = align_size(sizeof(float) * meta::gott_compressor::CURVE_MESH_SIZE, OPTIMAL_ALIGN);
            size_t szof_freq        = align_size(sizeof(float) * meta::gott_compressor::FFT_MESH_POINTS, OPTIMAL_ALIGN);
            size_t szof_indexes     = align_size(meta::gott_compressor::FFT_MESH_POINTS * sizeof(uint32_t), OPTIMAL_ALIGN);

            size_t to_alloc         =
                szof_channels +
                szof_buffer +       // vEmptyBuf
                szof_buffer +       // vBuffer
                szof_buffer +       // vProtBuffer
                szof_buffer*2 +     // vSC[2]
                szof_buffer +       // vEnv
                szof_freq*2 +       // vTr
                szof_freq*2 +       // vPFc
                szof_freq*2 +       // vRFc
                szof_curve +        // vCurveBuffer
                szof_freq +         // vFreqBuffer
                szof_indexes +      // vFreqIndexes
                (
                    szof_buffer +   // vInBuffer for each channel
                    szof_buffer +   // vBuffer for each channel
                    szof_buffer +   // vScBuffer for each channel
                    szof_buffer +   // vInAnalyze each channel
                    szof_freq*2 +   // vTmpFilterBuffer
                    szof_freq +     // vFilterBuffer
                    (
                        szof_buffer +   // vBuffer
                        szof_buffer +   // vVCA
                        szof_curve +    // vCurveBuffer
                        szof_freq*2 +   // vFilterBuffer
                        szof_freq*2     // vSidechainBuffer
                    ) * meta::gott_compressor::BANDS_MAX
                ) * channels;

            // Allocate data
            uint8_t *ptr            = alloc_aligned<uint8_t>(pData, to_alloc);
            if (ptr == NULL)
                return;

            vChannels               = advance_ptr_bytes<channel_t>(ptr, szof_channels);

            vEmptyBuf               = advance_ptr_bytes<float>(ptr, szof_buffer);
            vBuffer                 = advance_ptr_bytes<float>(ptr, szof_buffer);
            vProtBuffer             = advance_ptr_bytes<float>(ptr, szof_buffer);
            vSC[0]                  = advance_ptr_bytes<float>(ptr, szof_buffer);
            vSC[1]                  = advance_ptr_bytes<float>(ptr, szof_buffer);
            vEnv                    = advance_ptr_bytes<float>(ptr, szof_buffer);
            vTr                     = advance_ptr_bytes<float>(ptr, szof_freq*2);
            vPFc                    = advance_ptr_bytes<float>(ptr, szof_freq*2);
            vRFc                    = advance_ptr_bytes<float>(ptr, szof_freq*2);
            vCurveBuffer            = advance_ptr_bytes<float>(ptr, szof_curve);
            vFreqBuffer             = advance_ptr_bytes<float>(ptr, szof_freq);
            vFreqIndexes            = advance_ptr_bytes<uint32_t>(ptr, szof_indexes);

            // Initialize channels
            for (size_t i=0; i<channels; ++i)
            {
                channel_t *c            = &vChannels[i];

                c->sBypass.construct();
                c->sEnvBoost[0].construct();
                c->sEnvBoost[1].construct();

                c->sEnvBoost[0].init(NULL);
                if (bSidechain)
                    c->sEnvBoost[1].init(NULL);

                c->sDryEq.construct();
                c->sDryEq.init(meta::gott_compressor::BANDS_MAX - 1, 0);
                c->sDryEq.set_mode(dspu::EQM_IIR);
                c->sFFTXOver.construct();
                c->sDryDelay.construct();
                c->sAnDelay.construct();
                c->sScDelay.construct();
                c->sXOverDelay.construct();

                c->sDelay.construct();

                // Initialize bands
                for (size_t j=0; j<meta::gott_compressor::BANDS_MAX; ++j)
                {
                    band_t *b           = &c->vBands[j];

                    b->sSC.construct();
                    b->sEQ[0].construct();
                    b->sEQ[1].construct();
                    b->sProc.construct();
                    b->sPassFilter.construct();
                    b->sRejFilter.construct();
                    b->sAllFilter.construct();

                    if (!b->sSC.init(channels, meta::gott_compressor::SC_REACTIVITY_MAX))
                        return;
                    if (!b->sPassFilter.init(NULL))
                        return;
                    if (!b->sRejFilter.init(NULL))
                        return;
                    if (!b->sAllFilter.init(NULL))
                        return;

                    // Initialize sidechain equalizers
                    b->sEQ[0].init(2, 0);
                    b->sEQ[0].set_mode(dspu::EQM_IIR);
                    if (channels > 1)
                    {
                        b->sEQ[1].init(2, 0);
                        b->sEQ[1].set_mode(dspu::EQM_IIR);
                    }

                    // Initialize dynamic processor
                    for (size_t k=0; k<4; ++k)
                    {
                        b->sProc.set_attack_level(k, -1.0f);
                        b->sProc.set_release_level(k, -1.0f);
                    }

                    // Initialize oteher fields
                    b->vBuffer          = advance_ptr_bytes<float>(ptr, szof_buffer);
                    b->vVCA             = advance_ptr_bytes<float>(ptr, szof_buffer);
                    b->vCurveBuffer     = advance_ptr_bytes<float>(ptr, szof_curve);
                    b->vFilterBuffer    = advance_ptr_bytes<float>(ptr, szof_freq * 2);
                    b->vSidechainBuffer = advance_ptr_bytes<float>(ptr, szof_freq * 2);

                    b->fMinThresh       = GAIN_AMP_M_72_DB;
                    b->fUpThresh        = GAIN_AMP_M_48_DB;
                    b->fDownThresh      = GAIN_AMP_M_12_DB;
                    b->fUpRatio         = 100.0f;
                    b->fDownRatio       = 4.0f;
                    b->fAttackTime      = 10.0f;
                    b->fReleaseTime     = 10.0f;
                    b->fMakeup          = GAIN_AMP_0_DB;
                    b->fGainLevel       = 0.0f;
                    b->nSync            = S_ALL;

                    b->nFilterID        = filter_cid++;
                    b->bEnabled         = true;
                    b->bSolo            = false;
                    b->bMute            = false;

                    b->pMinThresh       = NULL;
                    b->pUpThresh        = NULL;
                    b->pDownThresh      = NULL;
                    b->pUpRatio         = NULL;
                    b->pDownRatio       = NULL;
                    b->pKnee            = NULL;
                    b->pAttackTime      = NULL;
                    b->pReleaseTime     = NULL;
                    b->pMakeup          = NULL;

                    b->pEnabled         = NULL;
                    b->pSolo            = NULL;
                    b->pMute            = NULL;
                    b->pCurveMesh       = NULL;
                    b->pFreqMesh        = NULL;
                    b->pEnvLvl          = NULL;
                    b->pCurveLvl        = NULL;
                    b->pMeterGain       = NULL;
                }

                c->vIn                  = NULL;
                c->vOut                 = NULL;
                c->vScIn                = NULL;
                c->vShmIn               = NULL;
                c->vInBuffer            = advance_ptr_bytes<float>(ptr, szof_buffer);
                c->vBuffer              = advance_ptr_bytes<float>(ptr, szof_buffer);
                c->vScBuffer            = advance_ptr_bytes<float>(ptr, szof_buffer);
                c->vInAnalyze           = advance_ptr_bytes<float>(ptr, szof_buffer);
                c->vTmpFilterBuffer     = advance_ptr_bytes<float>(ptr, szof_freq * 2);
                c->vFilterBuffer        = advance_ptr_bytes<float>(ptr, szof_freq);

                vSCIn[i]                = c->vScBuffer;

                c->nAnInChannel         = an_cid++;
                c->nAnOutChannel        = an_cid++;
                vAnalyze[c->nAnInChannel]   = NULL;
                vAnalyze[c->nAnOutChannel]  = NULL;

                c->bInFft               = false;
                c->bOutFft              = false;
                c->bRebuildFilers       = true;

                c->pIn                  = NULL;
                c->pOut                 = NULL;
                c->pScIn                = NULL;
                c->pShmIn               = NULL;
                c->pFftInSw             = NULL;
                c->pFftOutSw            = NULL;
                c->pFftIn               = NULL;
                c->pFftOut              = NULL;
                c->pAmpGraph            = NULL;
                c->pInLvl               = NULL;
                c->pOutLvl              = NULL;
            }

            // Bind ports
            size_t port_id          = 0;
            lsp_trace("Binding input ports");
            for (size_t i=0; i<channels; ++i)
                BIND_PORT(vChannels[i].pIn);
            lsp_trace("Binding output ports");
            for (size_t i=0; i<channels; ++i)
                BIND_PORT(vChannels[i].pOut);
            if (bSidechain)
            {
                lsp_trace("Binding sidechain ports");
                for (size_t i=0; i<channels; ++i)
                    BIND_PORT(vChannels[i].pScIn);
            }

            // Shared memory link
            lsp_trace("Binding shared memory link");
            SKIP_PORT("Shared memory link name");
            for (size_t i=0; i<channels; ++i)
                BIND_PORT(vChannels[i].pShmIn);

            lsp_trace("Binding common ports");
            BIND_PORT(pBypass);
            BIND_PORT(pMode);
            BIND_PORT(pProt);
            BIND_PORT(pInGain);
            BIND_PORT(pOutGain);
            BIND_PORT(pDryGain);
            BIND_PORT(pWetGain);
            BIND_PORT(pDryWet);
            BIND_PORT(pScMode);
            BIND_PORT(pScSource);
            BIND_PORT(pScPreamp);
            BIND_PORT(pScReact);
            BIND_PORT(pLookahead);
            BIND_PORT(pReactivity);
            BIND_PORT(pShiftGain);
            BIND_PORT(pZoom);
            BIND_PORT(pEnvBoost);
            BIND_PORT(pSplits[0]);
            BIND_PORT(pSplits[1]);
            BIND_PORT(pSplits[2]);
            SKIP_PORT("Filter curve enable switch"); // Skip filter curve enable button
            BIND_PORT(pExtraBand);
            BIND_PORT(pScMode);
            if (nMode == GOTT_STEREO)
            {
                BIND_PORT(pStereoSplit);
                BIND_PORT(pScSpSource);
            }
            if ((nMode == GOTT_LR) || (nMode == GOTT_MS))
                SKIP_PORT("Channel selector"); // Skip channel selector

            lsp_trace("Binding band ports");
            for (size_t i=0; i<channels; ++i)
            {
                channel_t *c            = &vChannels[i];

                if ((nMode == GOTT_STEREO) && (i > 0))
                {
                    for (size_t j=0; j<meta::gott_compressor::BANDS_MAX; ++j)
                    {
                        band_t *b               = &c->vBands[j];
                        band_t *sb              = &vChannels[0].vBands[j];

                        b->pMinThresh           = sb->pMinThresh;
                        b->pUpThresh            = sb->pUpThresh;
                        b->pDownThresh          = sb->pDownThresh;
                        b->pUpRatio             = sb->pUpRatio;
                        b->pDownRatio           = sb->pDownRatio;
                        b->pKnee                = sb->pKnee;
                        b->pAttackTime          = sb->pAttackTime;
                        b->pReleaseTime         = sb->pReleaseTime;
                        b->pMakeup              = sb->pMakeup;

                        b->pEnabled             = sb->pEnabled;
                        b->pSolo                = sb->pSolo;
                        b->pMute                = sb->pMute;

                        b->pCurveMesh           = sb->pCurveMesh;
                        b->pFreqMesh            = sb->pFreqMesh;
                    }
                }
                else
                {
                    for (size_t j=0; j<meta::gott_compressor::BANDS_MAX; ++j)
                    {
                        band_t *b               = &c->vBands[j];

                        BIND_PORT(b->pMinThresh);
                        BIND_PORT(b->pUpThresh);
                        BIND_PORT(b->pDownThresh);
                        BIND_PORT(b->pUpRatio);
                        BIND_PORT(b->pDownRatio);
                        BIND_PORT(b->pKnee);
                        BIND_PORT(b->pAttackTime);
                        BIND_PORT(b->pReleaseTime);
                        BIND_PORT(b->pMakeup);

                        BIND_PORT(b->pEnabled);
                        BIND_PORT(b->pSolo);
                        BIND_PORT(b->pMute);

                        BIND_PORT(b->pCurveMesh);
                        BIND_PORT(b->pFreqMesh);
                    }
                }
            }

            lsp_trace("Binding band meter ports");
            for (size_t i=0; i<channels; ++i)
            {
                channel_t *c            = &vChannels[i];
                for (size_t j=0; j<meta::gott_compressor::BANDS_MAX; ++j)
                {
                    band_t *b               = &c->vBands[j];

                    BIND_PORT(b->pEnvLvl);
                    BIND_PORT(b->pCurveLvl);
                    BIND_PORT(b->pMeterGain);
                }
            }

            lsp_trace("Binding channel meter ports");
            for (size_t i=0; i<channels; ++i)
            {
                channel_t *c            = &vChannels[i];
                BIND_PORT(c->pFftInSw);
                BIND_PORT(c->pFftOutSw);
                BIND_PORT(c->pFftIn);
                BIND_PORT(c->pFftOut);
                BIND_PORT(c->pInLvl);
                BIND_PORT(c->pOutLvl);
            }

            lsp_trace("Binding aplification curve ports");
            for (size_t i=0; i<channels; ++i)
            {
                channel_t *c            = &vChannels[i];
                BIND_PORT(c->pAmpGraph);
            }

            dsp::fill_zero(vEmptyBuf, GOTT_BUFFER_SIZE);

            // Initialize curve (logarithmic) in range of -72 .. +24 db
            float delta = (meta::gott_compressor::CURVE_DB_MAX - meta::gott_compressor::CURVE_DB_MIN) / (meta::gott_compressor::CURVE_MESH_SIZE-1);
            for (size_t i=0; i<meta::gott_compressor::CURVE_MESH_SIZE; ++i)
                vCurveBuffer[i]         = dspu::db_to_gain(meta::gott_compressor::CURVE_DB_MIN + delta * i);
        }

        void gott_compressor::destroy()
        {
            plug::Module::destroy();
            do_destroy();
        }

        void gott_compressor::do_destroy()
        {
            // Destroy analyzer
            sAnalyzer.destroy();

            // Destroy dynamic filters
            sFilters.destroy();

            // Destroy surge protection
            sProtSC.destroy();
            sProt.destroy();

            // Destroy channels
            if (vChannels != NULL)
            {
                size_t channels         = (nMode == GOTT_MONO) ? 1 : 2;

                for (size_t i=0; i<channels; ++i)
                {
                    channel_t *c            = &vChannels[i];

                    c->sBypass.destroy();
                    c->sEnvBoost[0].destroy();
                    c->sEnvBoost[1].destroy();
                    c->sDryEq.destroy();
                    c->sFFTXOver.destroy();
                    c->sDelay.destroy();
                    c->sDryDelay.destroy();
                    c->sAnDelay.destroy();
                    c->sScDelay.destroy();
                    c->sXOverDelay.destroy();

                    for (size_t j=0; j<meta::gott_compressor::BANDS_MAX; ++j)
                    {
                        band_t *b               = &c->vBands[j];

                        b->sSC.destroy();
                        b->sEQ[0].destroy();
                        b->sEQ[1].destroy();

                        b->sPassFilter.destroy();
                        b->sRejFilter.destroy();
                        b->sAllFilter.destroy();
                    }
                }

                vChannels               = NULL;
            }

            // Destroy inline display
            if (pIDisplay != NULL)
            {
                pIDisplay->destroy();
                pIDisplay   = NULL;
            }

            // Free allocated data
            if (pData != NULL)
            {
                free_aligned(pData);
                pData                   = NULL;
            }
        }

        void gott_compressor::ui_activated()
        {
            size_t channels     = (nMode == GOTT_MONO) ? 1 : 2;

            // Force meshes with the UI to synchronized
            for (size_t i=0; i<channels; ++i)
            {
                channel_t *c        = &vChannels[i];

                for (size_t j=0; j<meta::gott_compressor::BANDS_MAX; ++j)
                {
                    band_t *b           = &c->vBands[j];
                    b->nSync            = S_ALL;
                }
            }
        }

        size_t gott_compressor::select_fft_rank(size_t sample_rate)
        {
            const size_t k = (sample_rate + meta::gott_compressor::FFT_XOVER_FREQ_MIN/2) / meta::gott_compressor::FFT_XOVER_FREQ_MIN;
            const size_t n = int_log2(k);
            return meta::gott_compressor::FFT_XOVER_RANK_MIN + n;
        }

        void gott_compressor::update_sample_rate(long sr)
        {
            // Determine number of channels
            size_t channels     = (nMode == GOTT_MONO) ? 1 : 2;
            size_t fft_rank     = select_fft_rank(sr);
            size_t bins         = 1 << fft_rank;
            size_t max_delay    = bins + dspu::millis_to_samples(sr, meta::gott_compressor::LOOKAHEAD_MAX);

            // Update analyzer's sample rate
            sAnalyzer.set_sample_rate(sr);
            sFilters.set_sample_rate(sr);
            sProtSC.set_sample_rate(sr);
            sCounter.set_sample_rate(sr, true);
            bEnvUpdate          = true;

            // Update channels
            for (size_t i=0; i<channels; ++i)
            {
                channel_t *c = &vChannels[i];

                c->sBypass.init(sr);
                c->sDryEq.set_sample_rate(sr);
                c->sDelay.init(max_delay);
                c->sDryDelay.init(max_delay);
                c->sAnDelay.init(bins);
                c->sScDelay.init(bins);
                c->sXOverDelay.init(max_delay);

                // Need to re-initialize FFT crossover?
                if (fft_rank != c->sFFTXOver.rank())
                {
                    c->sFFTXOver.init(fft_rank, meta::gott_compressor::BANDS_MAX);
                    for (size_t j=0; j<meta::gott_compressor::BANDS_MAX; ++j)
                        c->sFFTXOver.set_handler(j, process_band, this, c);
                    c->sFFTXOver.set_rank(fft_rank);
                    c->sFFTXOver.set_phase(float(i) / float(channels));
                }
                c->sFFTXOver.set_sample_rate(sr);

                // Update bands
                for (size_t j=0; j<meta::gott_compressor::BANDS_MAX; ++j)
                {
                    band_t *b           = &c->vBands[j];

                    b->sSC.set_sample_rate(sr);
                    b->sProc.set_sample_rate(sr);

                    b->sPassFilter.set_sample_rate(sr);
                    b->sRejFilter.set_sample_rate(sr);
                    b->sAllFilter.set_sample_rate(sr);

                    b->sEQ[0].set_sample_rate(sr);
                    if (channels > 1)
                        b->sEQ[1].set_sample_rate(sr);
                }

                // Mark filters to rebuild
                c->bRebuildFilers       = true;
            }
        }

        dspu::sidechain_source_t gott_compressor::decode_sidechain_source(int source, bool split, size_t channel)
        {
            if (!split)
            {
                switch (source)
                {
                    case 0: return dspu::SCS_MIDDLE;
                    case 1: return dspu::SCS_SIDE;
                    case 2: return dspu::SCS_LEFT;
                    case 3: return dspu::SCS_RIGHT;
                    case 4: return dspu::SCS_AMIN;
                    case 5: return dspu::SCS_AMAX;
                    default: break;
                }
            }

            if (channel == 0)
            {
                switch (source)
                {
                    case 0: return dspu::SCS_LEFT;
                    case 1: return dspu::SCS_RIGHT;
                    case 2: return dspu::SCS_MIDDLE;
                    case 3: return dspu::SCS_SIDE;
                    case 4: return dspu::SCS_AMIN;
                    case 5: return dspu::SCS_AMAX;
                    default: break;
                }
            }
            else
            {
                switch (source)
                {
                    case 0: return dspu::SCS_RIGHT;
                    case 1: return dspu::SCS_LEFT;
                    case 2: return dspu::SCS_SIDE;
                    case 3: return dspu::SCS_MIDDLE;
                    case 4: return dspu::SCS_AMIN;
                    case 5: return dspu::SCS_AMAX;
                    default: break;
                }
            }

            return dspu::SCS_MIDDLE;
        }

        uint32_t gott_compressor::decode_sidechain_type(uint32_t sc) const
        {
            if (bSidechain)
            {
                switch (sc)
                {
                    case 0: return SCT_INTERNAL;
                    case 1: return SCT_EXTERNAL;
                    case 2: return SCT_LINK;
                    default: break;
                }
            }
            else
            {
                switch (sc)
                {
                    case 0: return SCT_INTERNAL;
                    case 1: return SCT_LINK;
                    default: break;
                }
            }

            return SCT_INTERNAL;
        }

        void gott_compressor::update_settings()
        {
            dspu::filter_params_t fp;
            size_t channels     = (nMode == GOTT_MONO) ? 1 : 2;
            bool solo_on        = false;
            bool prot_on        = pProt->value() >= 0.5f;
            bool rebuild_filters= false;
            int active_channels = 0;
            size_t env_boost    = pEnvBoost->value();
            size_t num_bands    = (pExtraBand->value() >= 0.5f) ? meta::gott_compressor::BANDS_MAX : meta::gott_compressor::BANDS_MAX - 1;
            float sc_preamp     = pScPreamp->value();
            size_t lookahead    = dspu::millis_to_samples(fSampleRate, pLookahead->value());

            // Determine work mode: classic, modern or linear phase
            xover_mode_t xover  = xover_mode_t(pMode->value());
            if (xover != enXOver)
            {
                enXOver             = xover;
                rebuild_filters     = true;
                for (size_t i=0; i<channels; ++i)
                    vChannels[i].sXOverDelay.clear();
            }

            // Check band and split configuration
            if (nBands != num_bands)
            {
                rebuild_filters     = true;
                nBands              = num_bands;
            }
            for (size_t i=0; i<(num_bands-1); ++i)
            {
                float freq          = pSplits[i]->value();
                if (freq != vSplits[i])
                {
                    rebuild_filters     = true;
                    vSplits[i]          = freq;
                }
            }
            bStereoSplit        = (pStereoSplit != NULL) ? pStereoSplit->value() >= 0.5f : false;

            // Store gain
            const float out_gain= pOutGain->value();
            const float drywet  = pDryWet->value() * 0.01f;
            const float dry_gain= pDryGain->value();
            const float wet_gain= pWetGain->value();

            fInGain             = pInGain->value();
            fDryGain            = (dry_gain * drywet + 1.0f - drywet) * out_gain;
            fWetGain            = wet_gain * drywet * out_gain;
            fZoom               = pZoom->value();

            nScType             = decode_sidechain_type(pScMode->value());
            plug::IPort *sc     = (bStereoSplit) ? pScSpSource : pScSource;
            size_t sc_src       = (sc != NULL) ? sc->value() : dspu::SCS_MIDDLE;

            float max_attack    = meta::gott_compressor::ATTACK_TIME_MIN;
            float sc_react      = pScReact->value();

            for (size_t i=0; i<channels; ++i)
            {
                channel_t *c = &vChannels[i];

                // Check configuration
                if (rebuild_filters)
                    c->bRebuildFilers       = true;

                // Update bypass settings
                c->sBypass.set_bypass(pBypass->value());

                // Update analyzer settings
                c->bInFft               = c->pFftInSw->value() >= 0.5f;
                c->bOutFft              = c->pFftOutSw->value() >= 0.5f;

                sAnalyzer.enable_channel(c->nAnInChannel, c->bInFft);
                sAnalyzer.enable_channel(c->nAnOutChannel, c->pFftOutSw->value()  >= 0.5f);

                if (sAnalyzer.channel_active(c->nAnInChannel))
                    active_channels ++;
                if (sAnalyzer.channel_active(c->nAnOutChannel))
                    active_channels ++;

                // Update bands
                for (size_t j=0; j<meta::gott_compressor::BANDS_MAX; ++j)
                {
                    band_t *b               = &c->vBands[j];

                    // Update solo/mute options
                    bool enabled            = (j < nBands) && (b->pEnabled->value() >= 0.5f);
                    bool mute               = (b->pMute->value() >= 0.5f);
                    bool solo               = (b->pSolo->value() >= 0.5f);

                    // Update sidechain settings
                    b->sSC.set_mode(pScMode->value());
                    b->sSC.set_reactivity(sc_react);
                    b->sSC.set_stereo_mode((nMode == GOTT_MS) ? dspu::SCSM_MIDSIDE : dspu::SCSM_STEREO);
                    b->sSC.set_source(decode_sidechain_source(sc_src, bStereoSplit, i));

                    if (sc_preamp != fScPreamp)
                        b->nSync       |= S_EQ_CURVE;

                    // Update processor settings
                    float attack            = b->pAttackTime->value();
                    float makeup            = b->pMakeup->value();
                    float up_ratio          = b->pUpRatio->value();
                    float down_ratio        = b->pDownRatio->value();
                    float f_down_gain       = b->pDownThresh->value();
                    float knee              = b->pKnee->value();
                    float f_up_gain         = lsp_min(b->pUpThresh->value(), f_down_gain * 0.999f);
                    float f_min_gain        = lsp_min(b->pMinThresh->value(), f_up_gain * 0.999f);
                    float f_min_value       = f_up_gain - (f_up_gain - f_min_gain) / up_ratio;

                    max_attack              = lsp_max(max_attack, attack);

                    // Update dynamics processor
                    b->sProc.set_attack_time(0, attack);
                    b->sProc.set_release_time(0, b->pReleaseTime->value());
                    b->sProc.set_dot(0, f_down_gain, f_down_gain, knee);
                    b->sProc.set_dot(1, f_up_gain, f_up_gain, knee);
                    b->sProc.set_dot(2, f_min_gain, f_min_value, knee);
                    b->sProc.set_dot(3, NULL);

                    b->sProc.set_in_ratio(1.0f);
                    b->sProc.set_out_ratio(down_ratio);

                    if ((b->sProc.modified()) || (b->fMakeup != makeup))
                    {
                        b->sProc.update_settings();
                        b->fMakeup      = makeup;
                        b->nSync       |= S_COMP_CURVE;
                    }
                    if ((b->bSolo != solo) || (b->bMute != mute) || (b->bEnabled != enabled))
                    {
                        b->bSolo        = solo;
                        b->bMute        = mute;
                        b->bEnabled     = enabled;
                        b->nSync       |= S_COMP_CURVE;
                    }
                    if (solo)
                        solo_on         = true;
                }

                // Update envelope boost filters
                if ((env_boost != nEnvBoost) || (bEnvUpdate))
                {
                    fp.fFreq        = meta::gott_compressor::FREQ_BOOST_MIN;
                    fp.fFreq2       = 0.0f;
                    fp.fQuality     = 0.0f;

                    switch (env_boost)
                    {
                        case meta::gott_compressor::FB_BT_3DB:
                            fp.nType        = dspu::FLT_BT_RLC_ENVELOPE;
                            fp.fGain        = GAIN_AMP_M_18_DB;
                            fp.nSlope       = 1;
                            break;
                        case meta::gott_compressor::FB_MT_3DB:
                            fp.nType        = dspu::FLT_MT_RLC_ENVELOPE;
                            fp.fGain        = GAIN_AMP_M_18_DB;
                            fp.nSlope       = 1;
                            break;
                        case meta::gott_compressor::FB_BT_6DB:
                            fp.nType        = dspu::FLT_BT_RLC_ENVELOPE;
                            fp.fGain        = GAIN_AMP_M_36_DB;
                            fp.nSlope       = 2;
                            break;
                        case meta::gott_compressor::FB_MT_6DB:
                            fp.nType        = dspu::FLT_MT_RLC_ENVELOPE;
                            fp.fGain        = GAIN_AMP_M_36_DB;
                            fp.nSlope       = 2;
                            break;
                        case meta::gott_compressor::FB_OFF:
                        default:
                            fp.nType        = dspu::FLT_NONE;
                            fp.fGain        = GAIN_AMP_0_DB;
                            fp.nSlope       = 1;
                            break;
                    }
                    c->sEnvBoost[0].update(fSampleRate, &fp);
                    if (bSidechain)
                        c->sEnvBoost[1].update(fSampleRate, &fp);
                }
            }

            // Update surge protection sidechain settings
            sProtSC.set_mode(pScMode->value());
            sProtSC.set_reactivity(sc_react);
            sProtSC.set_stereo_mode(dspu::SCSM_STEREO);
            sProtSC.set_source(dspu::SCS_AMAX);

            // Update surge protection
            sProt.set_on_threshold(GAIN_AMP_M_96_DB);
            sProt.set_off_threshold(meta::gott_compressor::THRESH_MIN_MIN * GAIN_AMP_M_12_DB);
            sProt.set_transition_time(dspu::millis_to_samples(fSampleRate, max_attack + sc_react) * meta::gott_compressor::PROT_ATTACK_MUL);
            sProt.set_shutdown_time(dspu::millis_to_samples(fSampleRate, meta::gott_compressor::PROT_SHUTDOWN_TIME));

            // Commit the envelope state
            fScPreamp       = sc_preamp;
            nEnvBoost       = env_boost;
            bEnvUpdate      = false;
            bProt           = prot_on;

            // Update analyzer parameters
            sAnalyzer.set_reactivity(pReactivity->value());
            if (pShiftGain != NULL)
                sAnalyzer.set_shift(pShiftGain->value() * 100.0f);
            sAnalyzer.set_activity(active_channels > 0);

            // Update analyzer
            if (sAnalyzer.needs_reconfiguration())
            {
                sAnalyzer.reconfigure();
                sAnalyzer.get_frequencies(
                    vFreqBuffer,
                    vFreqIndexes,
                    SPEC_FREQ_MIN,
                    SPEC_FREQ_MAX,
                    meta::gott_compressor::FFT_MESH_POINTS);
            }

            // Second pass over filter
            for (size_t i=0; i<channels; ++i)
            {
                channel_t *c    = &vChannels[i];

                // Check muting option
                for (size_t j=0; j<nBands; ++j)
                {
                    band_t *b       = &c->vBands[j];
                    if ((!b->bMute) && (solo_on))
                        b->bMute        = !b->bSolo;
                }

                // Rebuild compression plan
                if (c->bRebuildFilers)
                {
                    // Configure equalizers
                    for (size_t j=0; j<nBands; ++j)
                    {
                        band_t *b           = &c->vBands[j];
                        size_t band         = b - c->vBands;

                        float freq_start    = (j > 0) ? vSplits[j-1] : 0.0f;
                        float freq_end      = (j < (nBands - 1)) ? vSplits[j] : fSampleRate * 0.5f;

                        b->nSync           |= S_EQ_CURVE | S_BAND_CURVE;

                        lsp_trace("band[%d] start=%f, end=%f", int(j), freq_start, freq_end);

                        // Configure equalizer for the sidechain
                        for (size_t k=0; k<channels; ++k)
                        {
                            // Configure lo-pass filter
                            fp.nType        = (j != (nBands-1)) ? dspu::FLT_BT_LRX_LOPASS : dspu::FLT_NONE;
                            fp.fFreq        = freq_end;
                            fp.fFreq2       = fp.fFreq;
                            fp.fQuality     = 0.0f;
                            fp.fGain        = 1.0f;
                            fp.fQuality     = 0.0f;
                            fp.nSlope       = 2;

                            b->sEQ[k].set_params(0, &fp);

                            // Configure hi-pass filter
                            fp.nType        = (j != 0) ? dspu::FLT_BT_LRX_HIPASS : dspu::FLT_NONE;
                            fp.fFreq        = freq_start;
                            fp.fFreq2       = fp.fFreq;
                            fp.fQuality     = 0.0f;
                            fp.fGain        = 1.0f;
                            fp.fQuality     = 0.0f;
                            fp.nSlope       = 2;

                            b->sEQ[k].set_params(1, &fp);
                        }

                        // Update transfer function for equalizer
                        b->sEQ[0].freq_chart(b->vSidechainBuffer, vFreqBuffer, meta::gott_compressor::FFT_MESH_POINTS);
                        dsp::pcomplex_mod(b->vSidechainBuffer, b->vSidechainBuffer, meta::gott_compressor::FFT_MESH_POINTS);

                        // Update filter parameters, depending on operating mode
                        if (enXOver == XOVER_MODERN)
                        {
                            // Configure filter for band
                            if (j <= 0)
                            {
                                fp.nType        = dspu::FLT_BT_LRX_LOSHELF;
                                fp.fFreq        = freq_end;
                                fp.fFreq2       = freq_end;
                            }
                            else if (j >= (nBands - 1))
                            {
                                fp.nType        = dspu::FLT_BT_LRX_HISHELF;
                                fp.fFreq        = freq_start;
                                fp.fFreq2       = freq_start;
                            }
                            else
                            {
                                fp.nType        = dspu::FLT_BT_LRX_LADDERPASS;
                                fp.fFreq        = freq_start;
                                fp.fFreq2       = freq_end;
                            }

                            fp.fGain        = 1.0f;
                            fp.nSlope       = 2;
                            fp.fQuality     = 0.0;

                            lsp_trace("Filter type=%d, from=%f, to=%f", int(fp.nType), fp.fFreq, fp.fFreq2);

                            sFilters.set_params(b->nFilterID, &fp);
                            sFilters.set_filter_active(b->nFilterID, j < nBands);
                        }
                        else if (enXOver == XOVER_CLASSIC)
                        {
                            fp.fGain        = 1.0f;
                            fp.nSlope       = 2;
                            fp.fQuality     = 0.0;
                            fp.fFreq        = freq_end;
                            fp.fFreq2       = freq_end;

                            // We're going from low frequencies to high frequencies
                            if (j >= (nBands - 1))
                            {
                                fp.nType    = dspu::FLT_NONE;
                                b->sPassFilter.update(fSampleRate, &fp);
                                b->sRejFilter.update(fSampleRate, &fp);
                                b->sAllFilter.update(fSampleRate, &fp);
                            }
                            else
                            {
                                fp.nType    = dspu::FLT_BT_LRX_LOPASS;
                                b->sPassFilter.update(fSampleRate, &fp);
                                fp.nType    = dspu::FLT_BT_LRX_HIPASS;
                                b->sRejFilter.update(fSampleRate, &fp);
                                fp.nType    = (j == 0) ? dspu::FLT_NONE : dspu::FLT_BT_LRX_ALLPASS;
                                b->sAllFilter.update(fSampleRate, &fp);
                            }
                        }
                        else // enXOver == XOVER_LINEAR_PHASE
                        {
                            if (j > 0)
                            {
                                c->sFFTXOver.enable_hpf(band, true);
                                c->sFFTXOver.set_hpf_frequency(band, freq_start);
                                c->sFFTXOver.set_hpf_slope(band, -48.0f);
                            }
                            else
                                c->sFFTXOver.disable_hpf(band);

                            if (j < (nBands-1))
                            {
                                c->sFFTXOver.enable_lpf(band, true);
                                c->sFFTXOver.set_lpf_frequency(band, freq_end);
                                c->sFFTXOver.set_lpf_slope(band, -48.0f);
                            }
                            else
                                c->sFFTXOver.disable_lpf(band);
                        }
                    }

                    // Enable/disable bands in FFT crossover
                    for (size_t j=0; j<meta::gott_compressor::BANDS_MAX; ++j)
                    {
                        band_t *b       = &c->vBands[j];
                        size_t band     = b - c->vBands;
                        c->sFFTXOver.enable_band(band, j < nBands);
                    }

                    // Set-up all-pass filters for the 'dry' chain which can be mixed with the 'wet' chain.
                    for (size_t j=0; j<meta::gott_compressor::BANDS_MAX-1; ++j)
                    {
                        band_t *b       = (j < (nBands-1)) ? &c->vBands[j] : NULL;
                        fp.nType        = (b != NULL) ? dspu::FLT_BT_LRX_ALLPASS : dspu::FLT_NONE;
                        fp.fFreq        = (b != NULL) ? vSplits[j] : 0.0f;
                        fp.fFreq2       = fp.fFreq;
                        fp.fGain        = 1.0f;
                        fp.nSlope       = 2;
                        fp.fQuality     = 0.0f;

                        c->sDryEq.set_params(j, &fp);
                    }

                    // Cleanup flag indicating that filters should be rebuilt
                    c->bRebuildFilers   = false;
                }
            }

            // Report latency
            size_t xover_latency = (enXOver == XOVER_LINEAR_PHASE) ? vChannels[0].sFFTXOver.latency() : 0;

            set_latency(lookahead + xover_latency);
            for (size_t i=0; i<channels; ++i)
            {
                channel_t *c    = &vChannels[i];

                c->sDelay.set_delay(lookahead);
                c->sDryDelay.set_delay(lookahead + xover_latency);
                c->sAnDelay.set_delay(xover_latency);
                c->sScDelay.set_delay(xover_latency);
                c->sXOverDelay.set_delay(xover_latency);
            }
        }

        void gott_compressor::process_band(void *object, void *subject, size_t band, const float *data, size_t sample, size_t count)
        {
            channel_t *c            = static_cast<channel_t *>(subject);
            band_t *b               = &c->vBands[band];

            // Store data to band's buffer
            dsp::copy(&b->vBuffer[sample], data, count);
        }

        void gott_compressor::process_sidechain(size_t samples)
        {
            switch (nScType)
            {
                case SCT_EXTERNAL:
                {
                    const float *lc = (vChannels[0].vScIn != NULL) ? vChannels[0].vScIn : vEmptyBuf;
                    if (nMode == GOTT_MONO)
                    {
                        dsp::mul_k3(vChannels[0].vScBuffer, lc, fInGain, samples);
                        break;
                    }

                    const float *rc = (vChannels[1].vScIn != NULL) ? vChannels[1].vScIn : vEmptyBuf;
                    if (nMode == GOTT_MS)
                    {
                        dsp::lr_to_ms(vChannels[0].vScBuffer, vChannels[1].vScBuffer, lc, rc, samples);
                        dsp::mul_k2(vChannels[0].vScBuffer, fInGain, samples);
                        dsp::mul_k2(vChannels[1].vScBuffer, fInGain, samples);
                    }
                    else
                    {
                        dsp::mul_k3(vChannels[0].vScBuffer, lc, fInGain, samples);
                        dsp::mul_k3(vChannels[1].vScBuffer, rc, fInGain, samples);
                    }
                    break;
                }
                case SCT_LINK:
                {
                    const float *lc = (vChannels[0].vShmIn != NULL) ? vChannels[0].vShmIn: vEmptyBuf;
                    if (nMode == GOTT_MONO)
                    {
                        dsp::mul_k3(vChannels[0].vScBuffer, lc, fInGain, samples);
                        break;
                    }

                    const float *rc = (vChannels[1].vShmIn != NULL) ? vChannels[1].vShmIn : vEmptyBuf;
                    if (nMode == GOTT_MS)
                    {
                        dsp::lr_to_ms(vChannels[0].vScBuffer, vChannels[1].vScBuffer, lc, rc, samples);
                        dsp::mul_k2(vChannels[0].vScBuffer, fInGain, samples);
                        dsp::mul_k2(vChannels[1].vScBuffer, fInGain, samples);
                    }
                    else
                    {
                        dsp::mul_k3(vChannels[0].vScBuffer, lc, fInGain, samples);
                        dsp::mul_k3(vChannels[1].vScBuffer, rc, fInGain, samples);
                    }
                    break;
                }
                default:
                {
                    if (nMode != GOTT_MONO)
                    {
                        dsp::copy(vChannels[0].vScBuffer, vChannels[0].vBuffer, samples);
                        dsp::copy(vChannels[1].vScBuffer, vChannels[1].vBuffer, samples);
                    }
                    else
                        dsp::copy(vChannels[0].vScBuffer, vChannels[0].vBuffer, samples);
                    break;
                }
            }
        }

        void gott_compressor::process(size_t samples)
        {
            size_t channels     = (nMode == GOTT_MONO) ? 1 : 2;

            // Bind input signal
            for (size_t i=0; i<channels; ++i)
            {
                channel_t *c        = &vChannels[i];

                c->vIn              = c->pIn->buffer<float>();
                c->vOut             = c->pOut->buffer<float>();
                c->vScIn            = (c->pScIn != NULL) ? c->pScIn->buffer<float>() : NULL;
                c->vShmIn           = NULL;

                core::AudioBuffer *shm_buf  = (c->pShmIn != NULL) ? c->pShmIn->buffer<core::AudioBuffer>() : NULL;
                if ((shm_buf != NULL) && (shm_buf->active()))
                    c->vShmIn           = shm_buf->buffer();
            }

            // Do processing
            for (size_t offset = 0; offset < samples;)
            {
                // Determine buffer size for processing
                size_t to_process   = lsp_min(GOTT_BUFFER_SIZE, samples-offset);

                // Measure input signal level
                for (size_t i=0; i<channels; ++i)
                {
                    channel_t *c        = &vChannels[i];
                    float level         = dsp::abs_max(c->vIn, to_process) * fInGain;
                    c->pInLvl->set_value(level);
                }

                // Pre-process channel data
                if (nMode == GOTT_MS)
                {
                    dsp::lr_to_ms(vChannels[0].vBuffer, vChannels[1].vBuffer, vChannels[0].vIn, vChannels[1].vIn, to_process);
                    dsp::mul_k2(vChannels[0].vBuffer, fInGain, to_process);
                    dsp::mul_k2(vChannels[1].vBuffer, fInGain, to_process);
                }
                else if (nMode == GOTT_MONO)
                    dsp::mul_k3(vChannels[0].vBuffer, vChannels[0].vIn, fInGain, to_process);
                else
                {
                    dsp::mul_k3(vChannels[0].vBuffer, vChannels[0].vIn, fInGain, to_process);
                    dsp::mul_k3(vChannels[1].vBuffer, vChannels[1].vIn, fInGain, to_process);
                }

                // Process sidechain
                process_sidechain(to_process);

                // Do frequency boost and input channel analysis
                for (size_t i=0; i<channels; ++i)
                {
                    channel_t *c        = &vChannels[i];
                    c->sEnvBoost[0].process(c->vScBuffer, c->vScBuffer, to_process);
                    c->sAnDelay.process(c->vInAnalyze, c->vBuffer, to_process);
                    c->sScDelay.process(c->vScBuffer, c->vScBuffer, to_process);
                    vAnalyze[c->nAnInChannel] = c->vInAnalyze;
                }

                // Surge protection
                {
                    const float * in[2];
                    for (size_t i=0; i<channels; ++i)
                        in[i]           = vChannels[i].vIn;
                    sProtSC.process(vProtBuffer, in, to_process);
                    sProt.process(vProtBuffer, vProtBuffer, to_process);
                }

                // MAIN PLUGIN STUFF
                for (size_t i=0; i<channels; ++i)
                {
                    channel_t *c        = &vChannels[i];

                    for (size_t j=0; j<nBands; ++j)
                    {
                        band_t *b           = &c->vBands[j];

                        // Prepare sidechain signal with band equalizers
                        b->sEQ[0].process(vSC[0], vChannels[0].vScBuffer, to_process);
                        if (channels > 1)
                            b->sEQ[1].process(vSC[1], vChannels[1].vScBuffer, to_process);

                        // Preprocess VCA signal
                        b->sSC.process(vBuffer, const_cast<const float **>(vSC), to_process);   // Band now contains processed by sidechain signal
                        dsp::mul_k2(vBuffer, fScPreamp, to_process);

                        if (b->bEnabled)
                        {
                            b->sProc.process(b->vVCA, vEnv, vBuffer, to_process); // Output

                            // Output curve level
                            float lvl = dsp::abs_max(vEnv, to_process);
                            b->pEnvLvl->set_value(lvl);
                            float vca = dsp::abs_max(b->vVCA, to_process) * b->fMakeup;
                            b->pMeterGain->set_value(vca);
                            lvl = b->sProc.curve(lvl) * b->fMakeup;
                            b->pCurveLvl->set_value(lvl);

                            // Remember last envelope level and buffer level
                            b->fGainLevel   = b->vVCA[to_process-1] * b->fMakeup;

                            // Check muting option
                            if ((bProt) && (nScType == SCT_INTERNAL))
                                dsp::fmmul_k3(b->vVCA, vProtBuffer, b->fMakeup, to_process);
                            else
                                dsp::mul_k2(b->vVCA, b->fMakeup, to_process); // Apply makeup gain

                            // Patch the VCA signal
                            if (b->bMute)
                                dsp::fill(b->vVCA, GAIN_AMP_M_36_DB, to_process);
                            else if (enXOver == XOVER_MODERN) // 'Modern' mode
                                dsp::limit1(b->vVCA, GAIN_AMP_M_72_DB * b->fMakeup, GAIN_AMP_P_72_DB * b->fMakeup, to_process);
                        }
                        else
                        {
                            dsp::fill(b->vVCA, (b->bMute) ? GAIN_AMP_M_36_DB : GAIN_AMP_0_DB, to_process);
                            b->fGainLevel   = GAIN_AMP_0_DB;
                        }
                    }

                    // Output curve parameters for disabled bands
                    for (size_t i=0; i<meta::gott_compressor::BANDS_MAX; ++i)
                    {
                        band_t *b      = &c->vBands[i];
                        if (b->bEnabled)
                            continue;

                        b->pEnvLvl->set_value(0.0f);
                        b->pCurveLvl->set_value(0.0f);
                        b->pMeterGain->set_value(GAIN_AMP_0_DB);
                    }
                }

                // Here, we apply VCA to input signal dependent on the input
                if (enXOver == XOVER_MODERN) // 'Modern' mode
                {
                    // Apply VCA control
                    for (size_t i=0; i<channels; ++i)
                    {
                        channel_t *c        = &vChannels[i];

                        // Apply delay to compensate lookahead feature
                        c->sDelay.process(c->vInBuffer, c->vBuffer, to_process);

                        // Process first band
                        band_t *b           = &c->vBands[0];
                        sFilters.process(b->nFilterID, c->vBuffer, c->vInBuffer, b->vVCA, to_process);

                        // Process other bands
                        for (size_t j=1; j<nBands; ++j)
                        {
                            b                   = &c->vBands[j];
                            sFilters.process(b->nFilterID, c->vBuffer, c->vBuffer, b->vVCA, to_process);
                        }
                    }
                }
                else if (enXOver == XOVER_CLASSIC) // 'Classic' mode
                {
                    // Apply VCA control
                    for (size_t i=0; i<channels; ++i)
                    {
                        channel_t *c        = &vChannels[i];

                        // Originally, there is no signal
                        c->sDelay.process(c->vInBuffer, c->vBuffer, to_process); // Apply delay to compensate lookahead feature, store into vBuffer

                        // First band
                        band_t *b       = &c->vBands[0];
                        // Filter frequencies from input
                        b->sPassFilter.process(vEnv, c->vInBuffer, to_process);
                        // Apply VCA gain and add to the channel buffer
                        dsp::mul3(c->vBuffer, vEnv, b->vVCA, to_process);
                        // Filter frequencies from input
                        b->sRejFilter.process(vBuffer, c->vInBuffer, to_process);

                        // Other bands
                        for (size_t j=1; j<nBands; ++j)
                        {
                            b               = &c->vBands[j];

                            // Process the signal with all-pass
                            b->sAllFilter.process(c->vBuffer, c->vBuffer, to_process);
                            // Filter frequencies from input
                            b->sPassFilter.process(vEnv, vBuffer, to_process);
                            // Apply VCA gain and add to the channel buffer
                            dsp::fmadd3(c->vBuffer, vEnv, b->vVCA, to_process);
                            // Filter frequencies from input
                            b->sRejFilter.process(vBuffer, vBuffer, to_process);
                        }
                    }
                }
                else // enXOver == XOVER_LINEAR_PHASE
                {
                    // Apply VCA control
                    for (size_t i=0; i<channels; ++i)
                    {
                        channel_t *c        = &vChannels[i];

                        // Apply delay to compensate lookahead feature
                        c->sDelay.process(c->vBuffer, c->vBuffer, to_process);
                        // Apply delay to unprocessed signal to compensate lookahead + crossover delay
                        c->sXOverDelay.process(c->vInBuffer, c->vBuffer, to_process);
                        c->sFFTXOver.process(c->vBuffer, to_process);

                        // First band
                        band_t *b           = &c->vBands[0];
                        dsp::mul3(c->vBuffer, b->vVCA, b->vBuffer, to_process);

                        // Other bands
                        for (size_t j=1; j<nBands; ++j)
                        {
                            b                   = &c->vBands[j];
                            dsp::fmadd3(c->vBuffer, b->vVCA, b->vBuffer, to_process);
                        }
                    }
                }

                // MAIN PLUGIN STUFF END

                // Do output channel analysis
                for (size_t i=0; i<channels; ++i)
                {
                    channel_t *c        = &vChannels[i];
                    vAnalyze[c->nAnOutChannel]  = c->vBuffer;
                }

                if (sAnalyzer.activity())
                    sAnalyzer.process(vAnalyze, to_process);

                // Post-process data (if needed)
                if (nMode == GOTT_MS)
                {
                    dsp::ms_to_lr(vChannels[0].vBuffer, vChannels[1].vBuffer, vChannels[0].vBuffer, vChannels[1].vBuffer, to_process);
                    dsp::ms_to_lr(vChannels[0].vInBuffer, vChannels[1].vInBuffer, vChannels[0].vInBuffer, vChannels[1].vInBuffer, to_process);
                }

                // Final metering
                for (size_t i=0; i<channels; ++i)
                {
                    channel_t *c        = &vChannels[i];

                    // Apply dry/wet balance
                    if (enXOver == XOVER_MODERN)
                        dsp::mix2(c->vBuffer, c->vInBuffer, fWetGain, fDryGain, to_process);
                    else if (enXOver == XOVER_CLASSIC)
                    {
                        c->sDryEq.process(vBuffer, c->vInBuffer, to_process);
                        dsp::mix2(c->vBuffer, vBuffer, fWetGain, fDryGain, to_process);
                    }
                    else // enXOver == XOVER_LINEAR_PHASE
                        dsp::mix2(c->vBuffer, c->vInBuffer, fWetGain, fDryGain, to_process);

                    // Compute output level
                    float level         = dsp::abs_max(c->vBuffer, to_process);
                    c->pOutLvl->set_value(level);

                    // Apply bypass
                    c->sDryDelay.process(vBuffer, c->vIn, to_process);
                    c->sBypass.process(c->vOut, vBuffer, c->vBuffer, to_process);

                    // Update pointers
                    c->vIn             += to_process;
                    c->vOut            += to_process;
                    if (c->vScIn != NULL)
                        c->vScIn           += to_process;
                    if (c->vShmIn != NULL)
                        c->vShmIn          += to_process;
                }
                offset     += to_process;
            }

            sCounter.submit(samples);

            // Synchronize meshes with the UI
            plug::mesh_t *mesh = NULL;

            for (size_t i=0; i<channels; ++i)
            {
                channel_t *c        = &vChannels[i];

                // Calculate transfer function for the compressor
                if (sCounter.fired())
                {
                    if (enXOver == XOVER_MODERN)
                    {
                        // Calculate transfer function
                        band_t *b       = &c->vBands[0];
                        sFilters.freq_chart(b->nFilterID, c->vTmpFilterBuffer, vFreqBuffer, b->fGainLevel, meta::gott_compressor::FFT_MESH_POINTS);

                        for (size_t j=1; j<nBands; ++j)
                        {
                            b               = &c->vBands[j];
                            sFilters.freq_chart(b->nFilterID, vTr, vFreqBuffer, b->fGainLevel, meta::gott_compressor::FFT_MESH_POINTS);
                            dsp::pcomplex_mul2(c->vTmpFilterBuffer, vTr, meta::gott_compressor::FFT_MESH_POINTS);
                        }
                        dsp::pcomplex_mod(c->vFilterBuffer, c->vTmpFilterBuffer, meta::gott_compressor::FFT_MESH_POINTS);
                    }
                    else if (enXOver == XOVER_CLASSIC)
                    {
                        // Calculate transfer function for first band
                        band_t *b           = &c->vBands[0];

                        // Apply lo-pass filter characteristics
                        b->sPassFilter.freq_chart(vTr, vFreqBuffer, meta::gott_compressor::FFT_MESH_POINTS);
                        dsp::mul_k3(c->vTmpFilterBuffer, vTr, b->fGainLevel, meta::gott_compressor::FFT_MESH_POINTS*2);

                        // Apply hi-pass filter characteristics
                        b->sRejFilter.freq_chart(vRFc, vFreqBuffer, meta::gott_compressor::FFT_MESH_POINTS);
                        dsp::pcomplex_mul2(vTr, vRFc, meta::gott_compressor::FFT_MESH_POINTS);

                        // Calculate transfer function for other bands
                        for (size_t j=1; j<nBands; ++j)
                        {
                            b                   = &c->vBands[j];

                            // Apply all-pass characteristics
                            b->sAllFilter.freq_chart(vPFc, vFreqBuffer, meta::gott_compressor::FFT_MESH_POINTS);
                            dsp::pcomplex_mul2(c->vTmpFilterBuffer, vPFc, meta::gott_compressor::FFT_MESH_POINTS);

                            // Apply lo-pass filter characteristics
                            b->sPassFilter.freq_chart(vPFc, vFreqBuffer, meta::gott_compressor::FFT_MESH_POINTS);
                            dsp::pcomplex_mul2(vPFc, vTr, meta::gott_compressor::FFT_MESH_POINTS);
                            dsp::fmadd_k3(c->vTmpFilterBuffer, vPFc, b->fGainLevel, meta::gott_compressor::FFT_MESH_POINTS*2);

                            // Apply hi-pass filter characteristics
                            b->sRejFilter.freq_chart(vRFc, vFreqBuffer, meta::gott_compressor::FFT_MESH_POINTS);
                            dsp::pcomplex_mul2(vTr, vRFc, meta::gott_compressor::FFT_MESH_POINTS);
                        }
                        dsp::pcomplex_mod(c->vFilterBuffer, c->vTmpFilterBuffer, meta::gott_compressor::FFT_MESH_POINTS);

                        // Calculate transfer function
                        for (size_t j=0; j<nBands; ++j)
                        {
                            band_t *bp      = (j > 0) ? &c->vBands[j-1] : NULL;
                            band_t *b       = &c->vBands[j];

                            if (b->nSync & S_BAND_CURVE)
                            {
                                if (bp)
                                {
                                    bp->sRejFilter.freq_chart(vRFc, vFreqBuffer, meta::gott_compressor::FFT_MESH_POINTS);
                                    b->sPassFilter.freq_chart(vPFc, vFreqBuffer, meta::gott_compressor::FFT_MESH_POINTS);
                                    dsp::pcomplex_mul2(vPFc, vRFc, meta::gott_compressor::FFT_MESH_POINTS);
                                }
                                else
                                    b->sPassFilter.freq_chart(vPFc, vFreqBuffer, meta::gott_compressor::FFT_MESH_POINTS);

                                dsp::pcomplex_mod(b->vFilterBuffer, vPFc, meta::gott_compressor::FFT_MESH_POINTS);
                                b->nSync           &= ~size_t(S_BAND_CURVE);
                            }
                            if (j == 0)
                                dsp::mul_k3(c->vTmpFilterBuffer, b->vFilterBuffer, b->fGainLevel, meta::gott_compressor::FFT_MESH_POINTS);
                            else
                                dsp::fmadd_k3(c->vTmpFilterBuffer, b->vFilterBuffer, b->fGainLevel, meta::gott_compressor::FFT_MESH_POINTS);
                        }
                        dsp::copy(c->vFilterBuffer, c->vTmpFilterBuffer, meta::gott_compressor::FFT_MESH_POINTS);
                    }
                    else // enXOver == XOVER_LINEAR_PHASE
                    {
                        // Calculate transfer function
                        for (size_t j=0; j<nBands; ++j)
                        {
                            band_t *b           = &c->vBands[j];
                            size_t band         = b - c->vBands;
                            if (b->nSync & S_BAND_CURVE)
                            {
                                c->sFFTXOver.freq_chart(band, b->vFilterBuffer, vFreqBuffer, meta::gott_compressor::FFT_MESH_POINTS);
                                b->nSync           &= ~size_t(S_BAND_CURVE);
                            }
                            if (j == 0)
                                dsp::mul_k3(c->vTmpFilterBuffer, b->vFilterBuffer, b->fGainLevel, meta::gott_compressor::FFT_MESH_POINTS);
                            else
                                dsp::fmadd_k3(c->vTmpFilterBuffer, b->vFilterBuffer, b->fGainLevel, meta::gott_compressor::FFT_MESH_POINTS);
                        }

                        // Copy the result to the output buffer
                        dsp::copy(c->vFilterBuffer, c->vTmpFilterBuffer, meta::gott_compressor::FFT_MESH_POINTS);
                    }
                }

                // Output Channel curve
                mesh            = (c->pAmpGraph != NULL) ? c->pAmpGraph->buffer<plug::mesh_t>() : NULL;
                if ((mesh != NULL) && (mesh->isEmpty()))
                {
                    // Calculate amplitude (modulo)
                    dsp::copy(mesh->pvData[0], vFreqBuffer, meta::gott_compressor::FFT_MESH_POINTS);
                    dsp::copy(mesh->pvData[1], c->vFilterBuffer, meta::gott_compressor::FFT_MESH_POINTS);
                    mesh->data(2, meta::gott_compressor::FFT_MESH_POINTS);
                }

                // Output FFT curve for input
                mesh            = (c->pFftIn != NULL) ? c->pFftIn->buffer<plug::mesh_t>() : NULL;
                if ((mesh != NULL) && (mesh->isEmpty()))
                {
                    if (c->bInFft)
                    {
                        // Add extra points
                        mesh->pvData[0][0] = SPEC_FREQ_MIN * 0.5f;
                        mesh->pvData[0][meta::gott_compressor::FFT_MESH_POINTS+1] = SPEC_FREQ_MAX * 2.0f;
                        mesh->pvData[1][0] = 0.0f;
                        mesh->pvData[1][meta::gott_compressor::FFT_MESH_POINTS+1] = 0.0f;

                        // Copy frequency points
                        dsp::copy(&mesh->pvData[0][1], vFreqBuffer, meta::gott_compressor::FFT_MESH_POINTS);
                        sAnalyzer.get_spectrum(c->nAnInChannel, &mesh->pvData[1][1], vFreqIndexes, meta::gott_compressor::FFT_MESH_POINTS);

                        // Mark mesh containing data
                        mesh->data(2, meta::gott_compressor::FFT_MESH_POINTS + 2);
                    }
                    else
                        mesh->data(2, 0);
                }

                // Output FFT curve for output
                mesh            = (c->pFftOut != NULL) ? c->pFftOut->buffer<plug::mesh_t>() : NULL;
                if ((mesh != NULL) && (mesh->isEmpty()))
                {
                    if (sAnalyzer.channel_active(c->nAnOutChannel))
                    {
                        // Copy frequency points
                        dsp::copy(mesh->pvData[0], vFreqBuffer, meta::gott_compressor::FFT_MESH_POINTS);
                        sAnalyzer.get_spectrum(c->nAnOutChannel, mesh->pvData[1], vFreqIndexes, meta::gott_compressor::FFT_MESH_POINTS);

                        // Mark mesh containing data
                        mesh->data(2, meta::gott_compressor::FFT_MESH_POINTS);
                    }
                    else
                        mesh->data(2, 0);
                }

                // Output band curves
                for (size_t j=0; j<meta::gott_compressor::BANDS_MAX; ++j)
                {
                    band_t *b           = &c->vBands[j];

                    // Compressor band curve
                    if (b->nSync & S_EQ_CURVE)
                    {
                        mesh                = (b->pFreqMesh!= NULL) ? b->pFreqMesh->buffer<plug::mesh_t>() : NULL;
                        if ((mesh != NULL) && (mesh->isEmpty()))
                        {
                            // Add extra points
                            mesh->pvData[0][0] = SPEC_FREQ_MIN*0.5f;
                            mesh->pvData[0][meta::gott_compressor::FFT_MESH_POINTS+1] = SPEC_FREQ_MAX * 2.0f;
                            mesh->pvData[1][0] = 0.0f;
                            mesh->pvData[1][meta::gott_compressor::FFT_MESH_POINTS+1] = 0.0f;

                            // Fill mesh
                            dsp::copy(&mesh->pvData[0][1], vFreqBuffer, meta::gott_compressor::FFT_MESH_POINTS);
                            dsp::copy(&mesh->pvData[1][1], b->vSidechainBuffer, meta::gott_compressor::FFT_MESH_POINTS);
                            mesh->data(2, meta::gott_compressor::FILTER_MESH_POINTS);

                            // Mark mesh as synchronized
                            b->nSync           &= ~size_t(S_EQ_CURVE);
                        }
                    }

                    // Compressor function curve
                    if (b->nSync & S_COMP_CURVE)
                    {
                        mesh                = (b->pCurveMesh != NULL) ? b->pCurveMesh->buffer<plug::mesh_t>() : NULL;
                        if ((mesh != NULL) && (mesh->isEmpty()))
                        {
                            if (b->bEnabled)
                            {
                                // Copy frequency points
                                dsp::copy(mesh->pvData[0], vCurveBuffer, meta::gott_compressor::CURVE_MESH_SIZE);
                                b->sProc.curve(mesh->pvData[1], vCurveBuffer, meta::gott_compressor::CURVE_MESH_SIZE);
                                if (b->fMakeup != GAIN_AMP_0_DB)
                                    dsp::mul_k2(mesh->pvData[1], b->fMakeup, meta::gott_compressor::CURVE_MESH_SIZE);

                                // Mark mesh containing data
                                mesh->data(2, meta::gott_compressor::CURVE_MESH_SIZE);
                            }
                            else
                                mesh->data(2, 0);

                            // Mark mesh as synchronized
                            b->nSync           &= ~size_t(S_COMP_CURVE);
                        }
                    }
                }
            }

            // Request for redraw
            if ((pWrapper != NULL) && (sCounter.fired()))
                pWrapper->query_display_draw();

            sCounter.commit();
        }

        bool gott_compressor::inline_display(plug::ICanvas *cv, size_t width, size_t height)
        {
            // Check proportions
            if (height > (M_RGOLD_RATIO * width))
                height  = M_RGOLD_RATIO * width;

            // Init canvas
            if (!cv->init(width, height))
                return false;
            width   = cv->width();
            height  = cv->height();

            // Clear background
            bool bypassing = vChannels[0].sBypass.bypassing();
            cv->set_color_rgb((bypassing) ? CV_DISABLED : CV_BACKGROUND);
            cv->paint();

            // Draw axis
            cv->set_line_width(1.0);

            // "-72 db / (:zoom ** 3)" max="24 db * :zoom"

            float miny  = logf(GAIN_AMP_M_72_DB / dsp::ipowf(fZoom, 3));
            float maxy  = logf(GAIN_AMP_P_24_DB * fZoom);

            float zx    = 1.0f/SPEC_FREQ_MIN;
            float zy    = dsp::ipowf(fZoom, 3)/GAIN_AMP_M_72_DB;
            float dx    = width/(logf(SPEC_FREQ_MAX)-logf(SPEC_FREQ_MIN));
            float dy    = height/(miny-maxy);

            // Draw vertical lines
            cv->set_color_rgb(CV_YELLOW, 0.5f);
            for (float i=100.0f; i<SPEC_FREQ_MAX; i *= 10.0f)
            {
                float ax = dx*(logf(i*zx));
                cv->line(ax, 0, ax, height);
            }

            // Draw horizontal lines
            cv->set_color_rgb(CV_WHITE, 0.5f);
            for (float i=GAIN_AMP_M_72_DB; i<GAIN_AMP_P_24_DB; i *= GAIN_AMP_P_12_DB)
            {
                float ay = height + dy*(logf(i*zy));
                cv->line(0, ay, width, ay);
            }

            // Allocate buffer: f, x, y, tr
            pIDisplay           = core::IDBuffer::reuse(pIDisplay, 4, width+2);
            core::IDBuffer *b   = pIDisplay;
            if (b == NULL)
                return false;

            // Initialize mesh
            b->v[0][0]          = SPEC_FREQ_MIN*0.5f;
            b->v[0][width+1]    = SPEC_FREQ_MAX*2.0f;
            b->v[3][0]          = 1.0f;
            b->v[3][width+1]    = 1.0f;

            static const uint32_t c_colors[] = {
                CV_MIDDLE_CHANNEL,
                CV_LEFT_CHANNEL, CV_RIGHT_CHANNEL,
                CV_MIDDLE_CHANNEL, CV_SIDE_CHANNEL
            };

            size_t channels     = ((nMode == GOTT_MONO) || ((nMode == GOTT_STEREO) && (!bStereoSplit))) ? 1 : 2;
            const uint32_t *vc  = (channels == 1) ? &c_colors[0] :
                                  (nMode == GOTT_MS) ? &c_colors[3] :
                                  &c_colors[1];

            bool aa = cv->set_anti_aliasing(true);
            lsp_finally { cv->set_anti_aliasing(aa); };
            cv->set_line_width(2);

            for (size_t i=0; i<channels; ++i)
            {
                channel_t *c    = &vChannels[i];

                for (size_t j=0; j<width; ++j)
                {
                    size_t k        = (j*meta::gott_compressor::FFT_MESH_POINTS)/width;
                    b->v[0][j+1]    = vFreqBuffer[k];
                    b->v[3][j+1]    = c->vFilterBuffer[k];
                }

                dsp::fill(b->v[1], 0.0f, width+2);
                dsp::fill(b->v[2], height, width+2);
                dsp::axis_apply_log1(b->v[1], b->v[0], zx, dx, width+2);
                dsp::axis_apply_log1(b->v[2], b->v[3], zy, dy, width+2);

                // Draw mesh
                uint32_t color = (bypassing || !(active())) ? CV_SILVER : vc[i];
                Color stroke(color), fill(color, 0.5f);
                cv->draw_poly(b->v[1], b->v[2], width+2, stroke, fill);
            }

            return true;
        }

        void gott_compressor::dump(dspu::IStateDumper *v) const
        {
            plug::Module::dump(v);

            size_t channels     = (nMode == GOTT_MONO) ? 1 : 2;

            v->write_object("sAnalyzer", &sAnalyzer);
            v->write_object("sFilters", &sFilters);
            v->write_object("sProtSC", &sProtSC);
            v->write_object("sProt", &sProt);
            v->write_object("sCounter", &sCounter);

            v->write("nMode", nMode);
            v->write("nBands", nBands);
            v->write("enXOver", enXOver);
            v->write("nScType", nScType);
            v->write("bSidechain", bSidechain);
            v->write("bProt", bProt);
            v->write("bEnvUpdate", bEnvUpdate);
            v->write("bStereoSplit", bStereoSplit);
            v->write("fInGain", fInGain);
            v->write("fDryGain", fDryGain);
            v->write("fWetGain", fWetGain);
            v->write("fScPreamp", fScPreamp);
            v->write("nEnvBoost", nEnvBoost);
            v->write("fZoom", fZoom);
            v->writev("vSplits", vSplits, meta::gott_compressor::BANDS_MAX - 1);
            {
                v->begin_array("vChannels", vChannels, channels);
                lsp_finally { v->end_array(); };

                for (size_t i=0; i<channels; ++i)
                {
                    channel_t *c        = &vChannels[i];

                    v->write_object("sBypass", &c->sBypass);
                    v->write_object_array("sEnvBoost", c->sEnvBoost, 2);
                    v->write_object("sDryEq", &c->sBypass);
                    v->write_object("sFFTXOver", &c->sFFTXOver);
                    v->write_object("sDelay", &c->sBypass);
                    v->write_object("sDryDelay", &c->sDryDelay);
                    v->write_object("sAnDelay", &c->sAnDelay);
                    v->write_object("sScDelay", &c->sScDelay);
                    v->write_object("sXOverDelay", &c->sXOverDelay);

                    {
                        v->begin_array("vBands", c->vBands, meta::gott_compressor::BANDS_MAX);
                        lsp_finally { v->end_array(); };

                        for (size_t j=0; j<meta::gott_compressor::BANDS_MAX; ++j)
                        {
                            band_t *b       = &c->vBands[j];

                            v->begin_object(b, sizeof(band_t));
                            lsp_finally { v->end_object(); };

                            v->write_object("sSC", &b->sSC);
                            v->write_object_array("sEQ", b->sEQ, 2);
                            v->write_object("sProc", &b->sProc);
                            v->write_object("sPassFilter", &b->sPassFilter);
                            v->write_object("sRejFilter", &b->sRejFilter);
                            v->write_object("sAllFilter", &b->sAllFilter);

                            v->write("vVCA", b->vVCA);
                            v->write("vCurveBuffer", b->vCurveBuffer);
                            v->write("vFilterBuffer", b->vFilterBuffer);
                            v->write("vSidechainBuffer", b->vSidechainBuffer);

                            v->write("fMinThresh", b->fMinThresh);
                            v->write("fUpThresh", b->fUpThresh);
                            v->write("fDownThresh", b->fDownThresh);
                            v->write("fUpRatio", b->fUpRatio);
                            v->write("fDownRatio", b->fDownRatio);
                            v->write("fAttackTime", b->fAttackTime);
                            v->write("fReleaseTime", b->fReleaseTime);
                            v->write("fMakeup", b->fMakeup);
                            v->write("fGainLevel", b->fGainLevel);
                            v->write("nSync", b->nSync);
                            v->write("nFilterID", b->nFilterID);
                            v->write("bEnabled", b->bEnabled);
                            v->write("bSolo", b->bSolo);
                            v->write("bMute", b->bMute);

                            v->write("pMinThresh", b->pMinThresh);
                            v->write("pUpThresh", b->pUpThresh);
                            v->write("pDownThresh", b->pDownThresh);
                            v->write("pUpRatio", b->pUpRatio);
                            v->write("pDownRatio", b->pDownRatio);
                            v->write("pKnee", b->pKnee);
                            v->write("pAttackTime", b->pAttackTime);
                            v->write("pReleaseTime", b->pReleaseTime);
                            v->write("pMakeup", b->pMakeup);

                            v->write("pEnabled", b->pEnabled);
                            v->write("pSolo", b->pSolo);
                            v->write("pMute", b->pMute);
                            v->write("pCurveMesh", b->pCurveMesh);
                            v->write("pFreqMesh", b->pFreqMesh);
                            v->write("pEnvLvl", b->pEnvLvl);
                            v->write("pCurveLvl", b->pCurveLvl);
                            v->write("pMeterGain", b->pMeterGain);
                        }
                    }

                    v->write("vIn", c->vIn);
                    v->write("vOut", c->vOut);
                    v->write("vScIn", c->vScIn);
                    v->write("vShmIn", c->vScIn);
                    v->write("vInBuffer", c->vInBuffer);
                    v->write("vBuffer", c->vBuffer);
                    v->write("vScBuffer", c->vScBuffer);
                    v->write("vInAnalyze", c->vInAnalyze);
                    v->write("vTmpFilterBuffer", c->vTmpFilterBuffer);
                    v->write("vFilterBuffer", c->vFilterBuffer);

                    v->write("nAnInChannel", c->nAnInChannel);
                    v->write("nAnOutChannel", c->nAnOutChannel);
                    v->write("bInFft", c->bInFft);
                    v->write("bOutFft", c->bOutFft);
                    v->write("bRebuildFilers", c->bRebuildFilers);

                    v->write("pIn", c->pIn);
                    v->write("pOut", c->pOut);
                    v->write("pScIn", c->pScIn);
                    v->write("pShmIn", c->pScIn);
                    v->write("pFftInSw", c->pFftInSw);
                    v->write("pFftOutSw", c->pFftOutSw);
                    v->write("pFftIn", c->pFftIn);
                    v->write("pFftOut", c->pFftOut);
                    v->write("pAmpGraph", c->pAmpGraph);
                    v->write("pInLvl", c->pInLvl);
                    v->write("pOutLvl", c->pOutLvl);
                }
            }
            v->writev("vAnalyze", vAnalyze, 4);
            v->write("vEmptyBuf", vEmptyBuf);
            v->write("vBuffer", vBuffer);
            v->writev("vSC", vSC, 4);
            v->write("vEnv", vEnv);
            v->write("vTr", vTr);
            v->write("vPFc", vPFc);
            v->write("vRFc", vRFc);
            v->write("vCurveBuffer", vCurveBuffer);
            v->write("vFreqBuffer", vFreqBuffer);
            v->write("vFreqIndexes", vFreqIndexes);
            v->write("pIDisplay", pIDisplay);

            v->write("pBypass", pBypass);
            v->write("pMode", pMode);
            v->write("pInGain", pInGain);
            v->write("pOutGain", pOutGain);
            v->write("pDryGain", pDryGain);
            v->write("pWetGain", pWetGain);
            v->write("pDryWet", pDryWet);
            v->write("pScMode", pScMode);
            v->write("pScSource", pScSource);
            v->write("pScSpSource", pScSpSource);
            v->write("pScPreamp", pScPreamp);
            v->write("pScReact", pScReact);
            v->write("pLookahead", pLookahead);
            v->write("pReactivity", pReactivity);
            v->write("pShiftGain", pShiftGain);
            v->write("pZoom", pZoom);
            v->write("pEnvBoost", pEnvBoost);
            v->writev("pSplits", pSplits, meta::gott_compressor::BANDS_MAX - 1);
            v->write("pExtraBand", pExtraBand);
            v->write("pScType", pScType);
            v->write("pStereoSplit", pStereoSplit);

            v->write("pData", pData);
        }

    } /* namespace plugins */
} /* namespace lsp */


