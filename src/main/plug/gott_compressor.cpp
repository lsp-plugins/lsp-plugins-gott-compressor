/*
 * Copyright (C) 2023 Linux Studio Plugins Project <https://lsp-plug.in/>
 *           (C) 2023 Vladimir Sadovnikov <sadko4u@gmail.com>
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
#include <lsp-plug.in/common/debug.h>
#include <lsp-plug.in/dsp/dsp.h>
#include <lsp-plug.in/dsp-units/units.h>
#include <lsp-plug.in/dsp-units/misc/envelope.h>
#include <lsp-plug.in/plug-fw/meta/func.h>
#include <lsp-plug.in/shared/id_colors.h>
#include <lsp-plug.in/stdlib/string.h>

#include <private/plugins/gott_compressor.h>

/* The size of temporary buffer for audio processing */
#define GOTT_BUFFER_SIZE        0x1000U

namespace lsp
{
    [[maybe_unused]]
    static plug::IPort *TRACE_PORT(plug::IPort *p)
    {
        lsp_trace("  port id=%s", (p)->metadata()->id);
        return p;
    }

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

            bProt               = true;
            bModern             = true;
            bEnvUpdate          = true;
            nBands              = meta::gott_compressor::BANDS_MAX;
            bExtSidechain       = false;
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
            vBuffer             = NULL;
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
            pScMode             = NULL;
            pScSource           = NULL;
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
            pExtSidechain       = NULL;

            pData               = NULL;
        }

        gott_compressor::~gott_compressor()
        {
            destroy();
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

            // Initialize filters according to number of bands
            if (sFilters.init(meta::gott_compressor::BANDS_MAX * channels) != STATUS_OK)
                return;

            // Compute amount of memory
            size_t szof_channels    = align_size(sizeof(channel_t) * channels, OPTIMAL_ALIGN);
            size_t szof_buffer      = align_size(sizeof(float) * GOTT_BUFFER_SIZE, OPTIMAL_ALIGN);
            size_t szof_curve       = align_size(sizeof(float) * meta::gott_compressor::CURVE_MESH_SIZE, OPTIMAL_ALIGN);
            size_t szof_freq        = align_size(sizeof(float) * meta::gott_compressor::FFT_MESH_POINTS, OPTIMAL_ALIGN);
            size_t szof_indexes     = align_size(meta::gott_compressor::FFT_MESH_POINTS * sizeof(uint32_t), OPTIMAL_ALIGN);

            size_t to_alloc         =
                szof_channels +
                szof_buffer +       // vBuffer
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
                    szof_buffer +   // vOutAnalyze each channel
                    szof_freq*2 +   // vTmpFilterBuffer
                    szof_freq +     // vFilterBuffer
                    (
                        szof_buffer +   // vVCA
                        szof_freq*2 +   // vFilterBuffer
                        szof_curve      // vCurveBuffer
                    ) * meta::gott_compressor::BANDS_MAX
                ) * channels;

            // Allocate data
            uint8_t *ptr            = alloc_aligned<uint8_t>(pData, to_alloc);
            if (ptr == NULL)
                return;

            vChannels               = reinterpret_cast<channel_t *>(ptr);
            ptr                    += szof_channels;

            vBuffer                 = reinterpret_cast<float *>(ptr);
            ptr                    += szof_buffer;
            vSC[0]                  = reinterpret_cast<float *>(ptr);
            ptr                    += szof_buffer;
            vSC[1]                  = reinterpret_cast<float *>(ptr);
            ptr                    += szof_buffer;
            vEnv                    = reinterpret_cast<float *>(ptr);
            ptr                    += szof_buffer;
            vTr                     = reinterpret_cast<float *>(ptr);
            ptr                    += szof_freq*2;
            vPFc                    = reinterpret_cast<float *>(ptr);
            ptr                    += szof_freq*2;
            vRFc                    = reinterpret_cast<float *>(ptr);
            ptr                    += szof_freq*2;
            vCurveBuffer            = reinterpret_cast<float *>(ptr);
            ptr                    += szof_curve;
            vFreqBuffer             = reinterpret_cast<float *>(ptr);
            ptr                    += szof_freq;
            vFreqIndexes            = reinterpret_cast<uint32_t *>(ptr);
            ptr                    += szof_indexes;

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

                c->sDelay.construct();

                // Initialize bands
                for (size_t j=0; j<meta::gott_compressor::BANDS_MAX; ++j)
                {
                    band_t *b           = &c->vBands[j];

                    b->sSC.construct();
                    b->sEQ[0].construct();
                    b->sEQ[1].construct();
                    b->sProc.construct();
                    b->sProt.construct();
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
                    b->vVCA             = reinterpret_cast<float *>(ptr);
                    ptr                += szof_buffer;
                    b->vCurveBuffer     = reinterpret_cast<float *>(ptr);
                    ptr                += szof_curve;
                    b->vFilterBuffer    = reinterpret_cast<float *>(ptr);
                    ptr                += szof_freq * 2;

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
                c->vInBuffer            = reinterpret_cast<float *>(ptr);
                ptr                    += szof_buffer;
                c->vBuffer              = reinterpret_cast<float *>(ptr);
                ptr                    += szof_buffer;
                c->vScBuffer            = reinterpret_cast<float *>(ptr);
                ptr                    += szof_buffer;
                c->vInAnalyze           = reinterpret_cast<float *>(ptr);
                ptr                    += szof_buffer;
                c->vOutAnalyze          = reinterpret_cast<float *>(ptr);
                ptr                    += szof_buffer;
                c->vTmpFilterBuffer     = reinterpret_cast<float *>(ptr);
                ptr                    += szof_freq * 2;
                c->vFilterBuffer        = reinterpret_cast<float *>(ptr);
                ptr                    += szof_freq;

                c->nAnInChannel         = an_cid++;
                c->nAnOutChannel        = an_cid++;
                vAnalyze[c->nAnInChannel]   = c->vInAnalyze;
                vAnalyze[c->nAnOutChannel]  = c->vOutAnalyze;

                c->bInFft               = false;
                c->bOutFft              = false;
                c->bRebuildFilers       = true;

                c->pIn                  = NULL;
                c->pOut                 = NULL;
                c->pScIn                = NULL;
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
                vChannels[i].pIn            = TRACE_PORT(ports[port_id++]);
            lsp_trace("Binding output ports");
            for (size_t i=0; i<channels; ++i)
                vChannels[i].pOut           = TRACE_PORT(ports[port_id++]);
            if (bSidechain)
            {
                lsp_trace("Binding sidechain ports");
                for (size_t i=0; i<channels; ++i)
                    vChannels[i].pScIn          = TRACE_PORT(ports[port_id++]);
            }

            lsp_trace("Binding common ports");
            pBypass                 = TRACE_PORT(ports[port_id++]);
            pMode                   = TRACE_PORT(ports[port_id++]);
            pProt                   = TRACE_PORT(ports[port_id++]);
            pInGain                 = TRACE_PORT(ports[port_id++]);
            pOutGain                = TRACE_PORT(ports[port_id++]);
            pDryGain                = TRACE_PORT(ports[port_id++]);
            pWetGain                = TRACE_PORT(ports[port_id++]);
            pScMode                 = TRACE_PORT(ports[port_id++]);
            pScSource               = TRACE_PORT(ports[port_id++]);
            pScPreamp               = TRACE_PORT(ports[port_id++]);
            pScReact                = TRACE_PORT(ports[port_id++]);
            pLookahead              = TRACE_PORT(ports[port_id++]);
            pReactivity             = TRACE_PORT(ports[port_id++]);
            pShiftGain              = TRACE_PORT(ports[port_id++]);
            pZoom                   = TRACE_PORT(ports[port_id++]);
            pEnvBoost               = TRACE_PORT(ports[port_id++]);
            pSplits[0]              = TRACE_PORT(ports[port_id++]);
            pSplits[1]              = TRACE_PORT(ports[port_id++]);
            pSplits[2]              = TRACE_PORT(ports[port_id++]);
            TRACE_PORT(ports[port_id++]); // Skip filter curve enable button
            pExtraBand              = TRACE_PORT(ports[port_id++]);
            if (bSidechain)
                pExtSidechain           = TRACE_PORT(ports[port_id++]);
            if ((nMode == GOTT_LR) || (nMode == GOTT_MS))
                TRACE_PORT(ports[port_id++]); // Skip channel selector

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
                        b->pEnvLvl              = sb->pEnvLvl;
                        b->pCurveLvl            = sb->pCurveLvl;
                        b->pMeterGain           = sb->pMeterGain;
                    }
                }
                else
                {
                    for (size_t j=0; j<meta::gott_compressor::BANDS_MAX; ++j)
                    {
                        band_t *b               = &c->vBands[j];

                        b->pMinThresh           = TRACE_PORT(ports[port_id++]);
                        b->pUpThresh            = TRACE_PORT(ports[port_id++]);
                        b->pDownThresh          = TRACE_PORT(ports[port_id++]);
                        b->pUpRatio             = TRACE_PORT(ports[port_id++]);
                        b->pDownRatio           = TRACE_PORT(ports[port_id++]);
                        b->pKnee                = TRACE_PORT(ports[port_id++]);
                        b->pAttackTime          = TRACE_PORT(ports[port_id++]);
                        b->pReleaseTime         = TRACE_PORT(ports[port_id++]);
                        b->pMakeup              = TRACE_PORT(ports[port_id++]);

                        b->pEnabled             = TRACE_PORT(ports[port_id++]);
                        b->pSolo                = TRACE_PORT(ports[port_id++]);
                        b->pMute                = TRACE_PORT(ports[port_id++]);

                        b->pCurveMesh           = TRACE_PORT(ports[port_id++]);
                        b->pFreqMesh            = TRACE_PORT(ports[port_id++]);
                        b->pEnvLvl              = TRACE_PORT(ports[port_id++]);
                        b->pCurveLvl            = TRACE_PORT(ports[port_id++]);
                        b->pMeterGain           = TRACE_PORT(ports[port_id++]);
                    }
                }
            }

            lsp_trace("Binding channel meter ports");
            for (size_t i=0; i<channels; ++i)
            {
                channel_t *c            = &vChannels[i];
                c->pFftInSw             = TRACE_PORT(ports[port_id++]);
                c->pFftOutSw            = TRACE_PORT(ports[port_id++]);
                c->pFftIn               = TRACE_PORT(ports[port_id++]);
                c->pFftOut              = TRACE_PORT(ports[port_id++]);
                c->pInLvl               = TRACE_PORT(ports[port_id++]);
                c->pOutLvl              = TRACE_PORT(ports[port_id++]);
            }

            if ((nMode == GOTT_LR) || (nMode == GOTT_MS))
            {
                vChannels[0].pAmpGraph  = TRACE_PORT(ports[port_id++]);
                vChannels[1].pAmpGraph  = TRACE_PORT(ports[port_id++]);
            }
            else
                vChannels[0].pAmpGraph  = TRACE_PORT(ports[port_id++]);

            // Initialize curve (logarithmic) in range of -72 .. +24 db
            float delta = (meta::gott_compressor::CURVE_DB_MAX - meta::gott_compressor::CURVE_DB_MIN) / (meta::gott_compressor::CURVE_MESH_SIZE-1);
            for (size_t i=0; i<meta::gott_compressor::CURVE_MESH_SIZE; ++i)
                vCurveBuffer[i]         = dspu::db_to_gain(meta::gott_compressor::CURVE_DB_MIN + delta * i);
        }

        void gott_compressor::destroy()
        {
            plug::Module::destroy();

            // Destroy analyzer
            sAnalyzer.destroy();

            // Destroy dynamic filters
            sFilters.destroy();

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
                    c->sDelay.destroy();

                    for (size_t j=0; j<meta::gott_compressor::BANDS_MAX; ++j)
                    {
                        band_t *b               = &c->vBands[j];

                        b->sEQ[0].destroy();
                        b->sEQ[1].destroy();
                        b->sSC.destroy();

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

        void gott_compressor::update_sample_rate(long sr)
        {
            // Determine number of channels
            size_t channels     = (nMode == GOTT_MONO) ? 1 : 2;
            size_t max_delay    = dspu::millis_to_samples(sr, meta::gott_compressor::LOOKAHEAD_MAX);

            // Update analyzer's sample rate
            sAnalyzer.set_sample_rate(sr);
            sFilters.set_sample_rate(sr);
            bEnvUpdate          = true;

            // Update channels
            for (size_t i=0; i<channels; ++i)
            {
                channel_t *c = &vChannels[i];

                c->sBypass.init(sr);
                c->sDelay.init(max_delay);
                c->sDryEq.set_sample_rate(sr);

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

            // Store gain
            float out_gain      = pOutGain->value();
            fInGain             = pInGain->value();
            fDryGain            = out_gain * pDryGain->value();
            fWetGain            = out_gain * pWetGain->value();
            fZoom               = pZoom->value();

            for (size_t i=0; i<channels; ++i)
            {
                channel_t *c = &vChannels[i];

                // Check configuration
                if (rebuild_filters)
                    c->bRebuildFilers       = true;

                // Update bypass settings
                c->sBypass.set_bypass(pBypass->value());

                // Update analyzer settings
                c->bInFft           = c->pFftInSw->value() >= 0.5f;
                c->bOutFft          = c->pFftOutSw->value() >= 0.5f;

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
                    float sc_react          = pScReact->value();

                    // Update sidechain settings
                    b->sSC.set_mode(pScMode->value());
                    b->sSC.set_reactivity(sc_react);
                    b->sSC.set_stereo_mode((nMode == GOTT_MS) ? dspu::SCSM_MIDSIDE : dspu::SCSM_STEREO);
                    b->sSC.set_source((pScSource != NULL) ? pScSource->value() : dspu::SCS_MIDDLE);

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

                    // Update surge protection
                    b->sProt.set_on_threshold(f_min_gain);
                    b->sProt.set_off_threshold(meta::gott_compressor::THRESH_MIN_MIN * GAIN_AMP_M_12_DB);
                    b->sProt.set_transition_time(dspu::millis_to_samples(fSampleRate, attack + sc_react) * meta::gott_compressor::PROT_ATTACK_MUL);
                    b->sProt.set_shutdown_time(dspu::millis_to_samples(fSampleRate, meta::gott_compressor::PROT_SHUTDOWN_TIME));
                    if (prot_on && (!bProt))
                        b->sProt.reset();

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

                // Update lookahead delay
                c->sDelay.set_delay(lookahead);
            }

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
                        band_t *b   = &c->vBands[j];

                        float freq_start    = (j > 0) ? vSplits[j-1] : 0.0f;
                        float freq_end      = (j < (nBands - 1)) ? vSplits[j] : fSampleRate * 0.5f;

                        b->nSync           |= S_EQ_CURVE;

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
                        b->sEQ[0].freq_chart(b->vFilterBuffer, vFreqBuffer, meta::gott_compressor::FFT_MESH_POINTS);
                        dsp::pcomplex_mod(b->vFilterBuffer, b->vFilterBuffer, meta::gott_compressor::FFT_MESH_POINTS);

                        // Update filter parameters, depending on operating mode
                        if (bModern)
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
                        else
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
                    }

                    // Set-up all-pass filters for the 'dry' chain which can be mixed with the 'wet' chain.
                    for (size_t j=0; j<meta::gott_compressor::BANDS_MAX-1; ++j)
                    {
                        band_t *b       = (j < (nBands-1)) ? &c->vBands[j] : NULL;
                        fp.nType        = (b != NULL) ? dspu::FLT_BT_LRX_ALLPASS : dspu::FLT_NONE;
                        fp.fFreq        = (b != NULL) ? vSplits[j] : 0.0f;
                        fp.fFreq2       = fp.fFreq;
                        fp.fQuality     = 0.0f;
                        fp.fGain        = 1.0f;
                        fp.fQuality     = 0.0f;
                        fp.nSlope       = 2;

                        c->sDryEq.set_params(j, &fp);
                    }

                    // Cleanup flag indicating that filters should be rebuilt
                    c->bRebuildFilers   = false;
                }
            }

            // Report latency
            set_latency(lookahead);
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
            }

            // Do processing
            while (samples > 0)
            {
                // Determine buffer size for processing
                size_t to_process   = lsp_min(samples, GOTT_BUFFER_SIZE);

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
                if (bSidechain && bExtSidechain)
                {
                    if (nMode == GOTT_MS)
                    {
                        dsp::lr_to_ms(vChannels[0].vScBuffer, vChannels[1].vScBuffer, vChannels[0].vScIn, vChannels[1].vScIn, to_process);
                        dsp::mul_k2(vChannels[0].vScBuffer, fInGain, to_process);
                        dsp::mul_k2(vChannels[1].vScBuffer, fInGain, to_process);
                    }
                    else if (nMode == GOTT_MONO)
                        dsp::mul_k3(vChannels[0].vScBuffer, vChannels[0].vScIn, fInGain, to_process);
                    else
                    {
                        dsp::mul_k3(vChannels[0].vScBuffer, vChannels[0].vScIn, fInGain, to_process);
                        dsp::mul_k3(vChannels[1].vScBuffer, vChannels[1].vScIn, fInGain, to_process);
                    }
                }
                else
                {
                    if (nMode != GOTT_MONO)
                    {
                        dsp::copy(vChannels[0].vScBuffer, vChannels[0].vBuffer, to_process);
                        dsp::copy(vChannels[1].vScBuffer, vChannels[1].vBuffer, to_process);
                    }
                    else
                        dsp::copy(vChannels[0].vScBuffer, vChannels[0].vBuffer, to_process);
                }

                // Do frequency boost and input channel analysis
                for (size_t i=0; i<channels; ++i)
                {
                    channel_t *c        = &vChannels[i];
                    c->sEnvBoost[0].process(c->vScBuffer, c->vScBuffer, to_process);
                    dsp::copy(c->vInAnalyze, c->vBuffer, to_process);
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
                        b->sSC.process(vBuffer, const_cast<const float **>(vSC), to_process); // Band now contains processed by sidechain signal
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
                            b->fGainLevel   = b->vVCA[to_process-1];

                            // Check muting option
                            dsp::mul_k2(b->vVCA, b->fMakeup, to_process); // Apply makeup gain
                            if (bProt)
                                b->sProt.process_mul(b->vVCA, vBuffer, to_process);

                            // Patch the VCA signal
                            if (b->bMute)
                                dsp::fill(b->vVCA, GAIN_AMP_M_36_DB, to_process);
                            else if (bModern)
                                dsp::limit1(b->vVCA, GAIN_AMP_M_72_DB * b->fMakeup, GAIN_AMP_P_72_DB * b->fMakeup, to_process);
                        }
                        else
                        {
                            if (bProt)
                                b->sProt.process(vBuffer, to_process);
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
                if (bModern) // 'Modern' mode
                {
                    // Apply VCA control
                    for (size_t i=0; i<channels; ++i)
                    {
                        channel_t *c        = &vChannels[i];
                        c->sDelay.process(c->vBuffer, c->vBuffer, to_process); // Apply delay to compensate lookahead feature
                        dsp::copy(c->vInBuffer, c->vBuffer, to_process);

                        for (size_t j=0; j<nBands; ++j)
                        {
                            band_t *b          = &c->vBands[j];
                            sFilters.process(b->nFilterID, c->vBuffer, c->vBuffer, b->vVCA, to_process);
                        }
                    }
                }
                else // 'Classic' mode
                {
                    // Apply VCA control
                    for (size_t i=0; i<channels; ++i)
                    {
                        channel_t *c        = &vChannels[i];

                        // Originally, there is no signal
                        c->sDelay.process(c->vInBuffer, c->vBuffer, to_process); // Apply delay to compensate lookahead feature, store into vBuffer
                        dsp::copy(vBuffer, c->vInBuffer, to_process);
                        dsp::fill_zero(c->vBuffer, to_process);                 // Clear the channel buffer

                        for (size_t j=0; j<nBands; ++j)
                        {
                            band_t *b       = &c->vBands[j];

                            b->sAllFilter.process(c->vBuffer, c->vBuffer, to_process); // Process the signal with all-pass
                            b->sPassFilter.process(vEnv, vBuffer, to_process); // Filter frequencies from input
                            dsp::mul2(vEnv, b->vVCA, to_process); // Apply VCA gain
                            dsp::add2(c->vBuffer, vEnv, to_process); // Add signal to the channel buffer
                            b->sRejFilter.process(vBuffer, vBuffer, to_process); // Filter frequencies from input
                        }
                    }
                }

                // MAIN PLUGIN STUFF END

                // Do output channel analysis
                for (size_t i=0; i<channels; ++i)
                {
                    channel_t *c        = &vChannels[i];
                    dsp::copy(c->vOutAnalyze, c->vBuffer, to_process);
                }

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
                    if (bModern)
                        dsp::mix2(c->vBuffer, c->vInBuffer, fWetGain, fDryGain, to_process);
                    else
                    {
                        c->sDryEq.process(vBuffer, c->vInBuffer, to_process);
                        dsp::mix2(c->vBuffer, vBuffer, fWetGain, fDryGain, to_process);
                    }

                    // Compute output level
                    float level         = dsp::abs_max(c->vBuffer, to_process);
                    c->pOutLvl->set_value(level);

                    // Apply bypass
                    c->sBypass.process(c->vOut, c->vIn, c->vBuffer, to_process);

                    // Update pointers
                    c->vIn             += to_process;
                    c->vOut            += to_process;
                    if (c->vScIn != NULL)
                        c->vScIn           += to_process;
                }
                samples    -= to_process;
            } // while (samples > 0)


            // Synchronize meshes with the UI
            plug::mesh_t *mesh = NULL;

            for (size_t i=0; i<channels; ++i)
            {
                channel_t *c        = &vChannels[i];

                // Calculate transfer function for the compressor
                if (bModern)
                {
                    dsp::pcomplex_fill_ri(c->vTmpFilterBuffer, 1.0f, 0.0f, meta::gott_compressor::FFT_MESH_POINTS);

                    // Calculate transfer function
                    for (size_t j=0; j<nBands; ++j)
                    {
                        band_t *b       = &c->vBands[j];
                        sFilters.freq_chart(b->nFilterID, vTr, vFreqBuffer, b->fGainLevel, meta::gott_compressor::FFT_MESH_POINTS);
                        dsp::pcomplex_mul2(c->vTmpFilterBuffer, vTr, meta::gott_compressor::FFT_MESH_POINTS);
                    }
                }
                else
                {
                    dsp::pcomplex_fill_ri(vTr, 1.0f, 0.0f, meta::gott_compressor::FFT_MESH_POINTS);
                    dsp::fill_zero(c->vTmpFilterBuffer, meta::gott_compressor::FFT_MESH_POINTS*2);

                    // Calculate transfer function
                    for (size_t j=0; j<nBands; ++j)
                    {
                        band_t *b           = &c->vBands[j];

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
                }
                dsp::pcomplex_mod(c->vFilterBuffer, c->vTmpFilterBuffer, meta::gott_compressor::FFT_MESH_POINTS);

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
                            dsp::copy(&mesh->pvData[1][1], b->vFilterBuffer, meta::gott_compressor::FFT_MESH_POINTS);
                            mesh->data(2, meta::gott_compressor::FILTER_MESH_POINTS);

                            // Mark mesh as synchronized
                            b->nSync           &= ~S_EQ_CURVE;
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
                            b->nSync           &= ~S_COMP_CURVE;
                        }
                    }
                }
            }

            // Request for redraw
            if (pWrapper != NULL)
                pWrapper->query_display_draw();
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

            size_t channels = ((nMode == GOTT_MONO) || (nMode == GOTT_STEREO)) ? 1 : 2;
            static uint32_t c_colors[] = {
                    CV_MIDDLE_CHANNEL, CV_MIDDLE_CHANNEL,
                    CV_MIDDLE_CHANNEL, CV_MIDDLE_CHANNEL,
                    CV_LEFT_CHANNEL, CV_RIGHT_CHANNEL,
                    CV_MIDDLE_CHANNEL, CV_SIDE_CHANNEL
                   };

            bool aa = cv->set_anti_aliasing(true);
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
                uint32_t color = (bypassing || !(active())) ? CV_SILVER : c_colors[nMode*2 + i];
                Color stroke(color), fill(color, 0.5f);
                cv->draw_poly(b->v[1], b->v[2], width+2, stroke, fill);
            }
            cv->set_anti_aliasing(aa);

            return true;
        }

        void gott_compressor::dump(dspu::IStateDumper *v) const
        {
            size_t channels     = (nMode == GOTT_MONO) ? 1 : 2;

            v->write_object("sAnalyzer", &sAnalyzer);
            v->write_object("sFilters", &sFilters);

            v->write("nMode", nMode);
            v->write("bSidechain", bSidechain);
            v->write("bProt", bProt);
            v->write("bModern", bModern);
            v->write("bEnvUpdate", bEnvUpdate);
            v->write("nBands", nBands);
            v->write("bExtSidechain", bExtSidechain);
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
                    v->write_object("sDelay", &c->sBypass);

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
                            v->write_object("sProt", &b->sProt);
                            v->write_object("sPassFilter", &b->sPassFilter);
                            v->write_object("sRejFilter", &b->sRejFilter);
                            v->write_object("sAllFilter", &b->sAllFilter);

                            v->write("vVCA", b->vVCA);
                            v->write("vCurveBuffer", b->vCurveBuffer);
                            v->write("vFilterBuffer", b->vFilterBuffer);

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
                    v->write("vInBuffer", c->vInBuffer);
                    v->write("vBuffer", c->vBuffer);
                    v->write("vScBuffer", c->vScBuffer);
                    v->write("vInAnalyze", c->vInAnalyze);
                    v->write("vOutAnalyze", c->vOutAnalyze);
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
            v->write("pScMode", pScMode);
            v->write("pScSource", pScSource);
            v->write("pScPreamp", pScPreamp);
            v->write("pScReact", pScReact);
            v->write("pLookahead", pLookahead);
            v->write("pReactivity", pReactivity);
            v->write("pShiftGain", pShiftGain);
            v->write("pZoom", pZoom);
            v->write("pEnvBoost", pEnvBoost);
            v->writev("pSplits", pSplits, meta::gott_compressor::BANDS_MAX - 1);
            v->write("pExtraBand", pExtraBand);
            v->write("pExtSidechain", pExtSidechain);

            v->write("pData", pData);
        }

    } /* namespace plugins */
} /* namespace lsp */


