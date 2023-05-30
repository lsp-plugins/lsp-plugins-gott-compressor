/*
 * Copyright (C) 2020 Linux Studio Plugins Project <https://lsp-plug.in/>
 *           (C) 2020 Vladimir Sadovnikov <sadko4u@gmail.com>
 *
 * This file is part of lsp-plugins-gott-compressor
 * Created on: 25 нояб. 2020 г.
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

            bModern             = true;
            fInGain             = GAIN_AMP_0_DB;
            fDryGain            = GAIN_AMP_M_INF_DB;
            fWetGain            = GAIN_AMP_0_DB;

            vChannels           = NULL;
            vAnalyze[0]         = NULL;
            vAnalyze[1]         = NULL;
            vAnalyze[2]         = NULL;
            vAnalyze[3]         = NULL;
            vBuffer             = NULL;

            pBypass             = NULL;
            pMode               = NULL;
            pInGain             = NULL;
            pOutGain            = NULL;
            pDryGain            = NULL;
            pWetGain            = NULL;
            pReactivity         = NULL;
            pShiftGain          = NULL;
            pZoom               = NULL;
            pEnvBoost           = NULL;
            pSplits[0]          = NULL;
            pSplits[1]          = NULL;
            pSplits[2]          = NULL;
            pExtraBand          = NULL;

            pData               = NULL;
        }

        gott_compressor::~gott_compressor()
        {
        }

        void gott_compressor::init(plug::IWrapper *wrapper, plug::IPort **ports)
        {
            // Call parent class for initialization
            Module::init(wrapper, ports);

            // Initialize analyzer
            size_t channels         = (nMode == GOTT_MONO) ? 1 : 2;
            size_t an_cid           = 0;
            if (!sAnalyzer.init(2*channels, meta::gott_compressor::FFT_RANK,
                                MAX_SAMPLE_RATE, meta::gott_compressor::REFRESH_RATE))
                return;

            sAnalyzer.set_rank(meta::gott_compressor::FFT_RANK);
            sAnalyzer.set_activity(false);
            sAnalyzer.set_envelope(dspu::envelope::WHITE_NOISE);
            sAnalyzer.set_window(meta::gott_compressor::FFT_WINDOW);
            sAnalyzer.set_rate(meta::gott_compressor::REFRESH_RATE);

            // Compute amount of memory
            size_t szof_channels    = align_size(sizeof(channel_t) * channels, 0x20);
            size_t szof_buffer      = sizeof(float) * GOTT_BUFFER_SIZE;
            size_t to_alloc         =
                szof_channels +
                szof_buffer +       // vBuffer
                (
                    szof_buffer +   // vInBuffer for each channel
                    szof_buffer +   // vBuffer for each channel
                    szof_buffer +   // vScBuffer for each channel
                    ((bSidechain) ? szof_buffer : 0) + // vExtScBuffer for each channel
                    szof_buffer +   // vInAnalyze each channel
                    szof_buffer     // vOutAnalyze each channel
                ) * channels;

            // Allocate data
            uint8_t *ptr            = alloc_aligned<uint8_t>(pData, to_alloc);
            if (ptr == NULL)
                return;

            vChannels               = reinterpret_cast<channel_t *>(ptr);
            ptr                    += szof_channels;

            vBuffer                 = reinterpret_cast<float *>(ptr);
            ptr                    += szof_buffer;

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

                c->sDryEq.init(meta::gott_compressor::BANDS_MAX - 1, 0);
                c->sDryEq.set_mode(dspu::EQM_IIR);

                c->vIn                  = NULL;
                c->vOut                 = NULL;
                c->vScIn                = NULL;
                c->vInBuffer            = reinterpret_cast<float *>(ptr);
                ptr                    += szof_buffer;
                c->vBuffer              = reinterpret_cast<float *>(ptr);
                ptr                    += szof_buffer;
                c->vScBuffer            = reinterpret_cast<float *>(ptr);
                ptr                    += szof_buffer;
                c->vExtScBuffer         = NULL;
                if (bSidechain)
                {
                    c->vExtScBuffer         = reinterpret_cast<float *>(ptr);
                    ptr                    += szof_buffer;
                }
                c->vInAnalyze           = reinterpret_cast<float *>(ptr);
                ptr                    += szof_buffer;
                c->vOutAnalyze          = reinterpret_cast<float *>(ptr);
                ptr                    += szof_buffer;

                c->nAnInChannel         = an_cid++;
                c->nAnOutChannel        = an_cid++;
                vAnalyze[c->nAnInChannel]   = c->vInAnalyze;
                vAnalyze[c->nAnOutChannel]  = c->vOutAnalyze;

                c->bInFft               = false;
                c->bOutFft              = false;

                c->pIn                  = NULL;
                c->pOut                 = NULL;
                c->pScIn                = NULL;
                c->pFftInSw             = NULL;
                c->pFftOutSw            = NULL;
                c->pFftIn               = NULL;
                c->pFftOut              = NULL;
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
            pInGain                 = TRACE_PORT(ports[port_id++]);
            pOutGain                = TRACE_PORT(ports[port_id++]);
            pDryGain                = TRACE_PORT(ports[port_id++]);
            pWetGain                = TRACE_PORT(ports[port_id++]);
            pReactivity             = TRACE_PORT(ports[port_id++]);
            pShiftGain              = TRACE_PORT(ports[port_id++]);
            pZoom                   = TRACE_PORT(ports[port_id++]);
            pEnvBoost               = TRACE_PORT(ports[port_id++]);
            pSplits[0]              = TRACE_PORT(ports[port_id++]);
            pSplits[1]              = TRACE_PORT(ports[port_id++]);
            pSplits[2]              = TRACE_PORT(ports[port_id++]);
            pExtraBand              = TRACE_PORT(ports[port_id++]);

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
        }

        void gott_compressor::destroy()
        {
            Module::destroy();

            // Destroy analyzer
            sAnalyzer.destroy();

            // Destroy channels
            if (vChannels != NULL)
            {
                size_t channels         = (nMode == GOTT_MONO) ? 1 : 2;

                for (size_t i=0; i<channels; ++i)
                {
                    channel_t *c            = &vChannels[i];
                    c->sBypass.destroy();
                }

                vChannels               = NULL;
            }

            // Free allocated data
            if (pData != NULL)
            {
                free_aligned(pData);
                pData                   = NULL;
            }
        }

        void gott_compressor::update_sample_rate(long sr)
        {
        }

        void gott_compressor::update_settings()
        {
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
                if (bSidechain)
                {
                    if (nMode == GOTT_MS)
                    {
                        dsp::lr_to_ms(vChannels[0].vExtScBuffer, vChannels[1].vExtScBuffer, vChannels[0].vScIn, vChannels[1].vScIn, to_process);
                        dsp::mul_k2(vChannels[0].vExtScBuffer, fInGain, to_process);
                        dsp::mul_k2(vChannels[1].vExtScBuffer, fInGain, to_process);
                    }
                    else if (nMode == GOTT_MONO)
                        dsp::mul_k3(vChannels[0].vExtScBuffer, vChannels[0].vScIn, fInGain, to_process);
                    else
                    {
                        dsp::mul_k3(vChannels[0].vExtScBuffer, vChannels[0].vScIn, fInGain, to_process);
                        dsp::mul_k3(vChannels[1].vExtScBuffer, vChannels[1].vScIn, fInGain, to_process);
                    }
                }


                // Do frequency boost and input channel analysis
                for (size_t i=0; i<channels; ++i)
                {
                    channel_t *c        = &vChannels[i];
                    c->sEnvBoost[0].process(c->vScBuffer, c->vBuffer, to_process);
                    if (bSidechain)
                        c->sEnvBoost[1].process(c->vExtScBuffer, c->vExtScBuffer, to_process);

                    dsp::copy(c->vInAnalyze, c->vBuffer, to_process);
                }

            #if 0
                // MAIN PLUGIN STUFF
                for (size_t i=0; i<channels; ++i)
                {
                    channel_t *c        = &vChannels[i];

                    for (size_t j=0; j<c->nPlanSize; ++j)
                    {
                        dyna_band_t *b      = c->vPlan[j];

                        // Prepare sidechain signal with band equalizers
                        b->sEQ[0].process(vSc[0], (b->bExtSc) ? vChannels[0].vExtScBuffer : vChannels[0].vScBuffer, to_process);
                        if (channels > 1)
                            b->sEQ[1].process(vSc[1], (b->bExtSc) ? vChannels[1].vExtScBuffer : vChannels[1].vScBuffer, to_process);

                        // Preprocess VCA signal
                        b->sSC.process(vBuffer, const_cast<const float **>(vSc), to_process); // Band now contains processed by sidechain signal
                        b->sScDelay.process(vBuffer, vBuffer, b->fScPreamp, to_process); // Apply sidechain preamp and lookahead delay

                        if (b->bEnabled)
                        {
                            b->sProc.process(b->vVCA, vEnv, vBuffer, to_process); // Output
                            if (bModern)
                                dsp::limit1(b->vVCA, GAIN_AMP_M_72_DB, GAIN_AMP_P_72_DB, to_process);
                            dsp::mul_k2(b->vVCA, b->fMakeup, to_process); // Apply makeup gain

                            // Output curve level
                            float lvl = dsp::abs_max(vEnv, to_process);
                            b->pEnvLvl->set_value(lvl);
                            b->pMeterGain->set_value(dsp::abs_max(b->vVCA, to_process));
                            lvl = b->sProc.curve(lvl) * b->fMakeup;
                            b->pCurveLvl->set_value(lvl);

                            // Remember last envelope level and buffer level
                            b->fGainLevel   = b->vVCA[to_process-1];

                            // Check muting option
                            if (b->bMute)
                                dsp::fill(b->vVCA, GAIN_AMP_M_36_DB, to_process);
                        }
                        else
                        {
                            dsp::fill(b->vVCA, (b->bMute) ? GAIN_AMP_M_36_DB : GAIN_AMP_0_DB, to_process);
                            b->fGainLevel   = GAIN_AMP_0_DB;
                        }
                    }

                    // Output curve parameters for disabled expanders
                    for (size_t i=0; i<meta::mb_dyna_processor::BANDS_MAX; ++i)
                    {
                        dyna_band_t *b      = &c->vBands[i];
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

                        for (size_t j=0; j<c->nPlanSize; ++j)
                        {
                            dyna_band_t *b      = c->vPlan[j];
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

                        for (size_t j=0; j<c->nPlanSize; ++j)
                        {
                            dyna_band_t *b      = c->vPlan[j];

                            b->sAllFilter.process(c->vBuffer, c->vBuffer, to_process); // Process the signal with all-pass
                            b->sPassFilter.process(vEnv, vBuffer, to_process); // Filter frequencies from input
                            dsp::mul2(vEnv, b->vVCA, to_process); // Apply VCA gain
                            dsp::add2(c->vBuffer, vEnv, to_process); // Add signal to the channel buffer
                            b->sRejFilter.process(vBuffer, vBuffer, to_process); // Filter frequencies from input
                        }
                    }
                }

                // MAIN PLUGIN STUFF END
            #endif

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
                    c->sBypass.process(c->vOut, c->vInBuffer, c->vBuffer, to_process);

                    // Update pointers
                    c->vIn             += to_process;
                    c->vOut            += to_process;
                    if (c->vScIn != NULL)
                        c->vScIn           += to_process;
                }
                samples    -= to_process;
            } // while (samples > 0)

        #if 0
            // Output FFT curves for each channel
            for (size_t i=0; i<channels; ++i)
            {
                channel_t *c     = &vChannels[i];

                // Calculate transfer function for the expander
                if (bModern)
                {
                    dsp::pcomplex_fill_ri(c->vTr, 1.0f, 0.0f, meta::mb_dyna_processor::FFT_MESH_POINTS);

                    // Calculate transfer function
                    for (size_t j=0; j<c->nPlanSize; ++j)
                    {
                        dyna_band_t *b      = c->vPlan[j];
                        sFilters.freq_chart(b->nFilterID, vTr, vFreqs, b->fGainLevel, meta::mb_dyna_processor::FFT_MESH_POINTS);
                        dsp::pcomplex_mul2(c->vTr, vTr, meta::mb_dyna_processor::FFT_MESH_POINTS);
                    }
                }
                else
                {
                    dsp::pcomplex_fill_ri(vTr, 1.0f, 0.0f, meta::mb_dyna_processor::FFT_MESH_POINTS);   // vBuffer
                    dsp::fill_zero(c->vTr, meta::mb_dyna_processor::FFT_MESH_POINTS*2);                 // c->vBuffer

                    // Calculate transfer function
                    for (size_t j=0; j<c->nPlanSize; ++j)
                    {
                        dyna_band_t *b      = c->vPlan[j];

                        // Apply all-pass characteristics
                        b->sAllFilter.freq_chart(vPFc, vFreqs, meta::mb_dyna_processor::FFT_MESH_POINTS);
                        dsp::pcomplex_mul2(c->vTr, vPFc, meta::mb_dyna_processor::FFT_MESH_POINTS);

                        // Apply lo-pass filter characteristics
                        b->sPassFilter.freq_chart(vPFc, vFreqs, meta::mb_dyna_processor::FFT_MESH_POINTS);
                        dsp::pcomplex_mul2(vPFc, vTr, meta::mb_dyna_processor::FFT_MESH_POINTS);
                        dsp::fmadd_k3(c->vTr, vPFc, b->fGainLevel, meta::mb_dyna_processor::FFT_MESH_POINTS*2);

                        // Apply hi-pass filter characteristics
                        b->sRejFilter.freq_chart(vRFc, vFreqs, meta::mb_dyna_processor::FFT_MESH_POINTS);
                        dsp::pcomplex_mul2(vTr, vRFc, meta::mb_dyna_processor::FFT_MESH_POINTS);
                    }
                }
                dsp::pcomplex_mod(c->vTrMem, c->vTr, meta::mb_dyna_processor::FFT_MESH_POINTS);

                // Output FFT curve, compression curve and FFT spectrogram for each band
                for (size_t j=0; j<meta::mb_dyna_processor::BANDS_MAX; ++j)
                {
                    dyna_band_t *b      = &c->vBands[j];

                    // FFT spectrogram
                    plug::mesh_t *mesh        = NULL;

                    // FFT curve
                    if (b->nSync & S_EQ_CURVE)
                    {
                        mesh                = (b->pScFreqChart != NULL) ? b->pScFreqChart->buffer<plug::mesh_t>() : NULL;
                        if ((mesh != NULL) && (mesh->isEmpty()))
                        {
                            // Add extra points
                            mesh->pvData[0][0] = SPEC_FREQ_MIN*0.5f;
                            mesh->pvData[0][meta::mb_dyna_processor::MESH_POINTS+1] = SPEC_FREQ_MAX * 2.0f;
                            mesh->pvData[1][0] = 0.0f;
                            mesh->pvData[1][meta::mb_dyna_processor::MESH_POINTS+1] = 0.0f;

                            // Fill mesh
                            dsp::copy(&mesh->pvData[0][1], vFreqs, meta::mb_dyna_processor::MESH_POINTS);
                            dsp::mul_k3(&mesh->pvData[1][1], b->vTr, b->fScPreamp, meta::mb_dyna_processor::MESH_POINTS);
                            mesh->data(2, meta::mb_dyna_processor::FILTER_MESH_POINTS);

                            // Mark mesh as synchronized
                            b->nSync           &= ~S_EQ_CURVE;
                        }
                    }

                    // Compression curve
                    if (b->nSync & S_DP_CURVE)
                    {
                        mesh                = (b->pCurveGraph != NULL) ? b->pCurveGraph->buffer<plug::mesh_t>() : NULL;
                        if ((mesh != NULL) && (mesh->isEmpty()))
                        {
                            if (b->bEnabled)
                            {
                                // Copy frequency points
                                dsp::copy(mesh->pvData[0], vCurve, meta::mb_dyna_processor::CURVE_MESH_SIZE);
                                b->sProc.curve(mesh->pvData[1], vCurve, meta::mb_dyna_processor::CURVE_MESH_SIZE);
                                if (b->fMakeup != GAIN_AMP_0_DB)
                                    dsp::mul_k2(mesh->pvData[1], b->fMakeup, meta::mb_dyna_processor::CURVE_MESH_SIZE);

                                // Mark mesh containing data
                                mesh->data(2, meta::mb_dyna_processor::CURVE_MESH_SIZE);
                            }
                            else
                                mesh->data(2, 0);

                            // Mark mesh as synchronized
                            b->nSync           &= ~S_DP_CURVE;
                        }

                        // Model graph
                        mesh                = (b->pModelGraph != NULL) ? b->pModelGraph->buffer<plug::mesh_t>() : NULL;
                        if ((mesh != NULL) && (mesh->isEmpty()))
                        {
                            if (b->bEnabled)
                            {
                                // Copy frequency points
                                dsp::copy(mesh->pvData[0], vCurve, meta::mb_dyna_processor::CURVE_MESH_SIZE);
                                b->sProc.model(mesh->pvData[1], vCurve, meta::mb_dyna_processor::CURVE_MESH_SIZE);

                                // Mark mesh containing data
                                mesh->data(2, meta::mb_dyna_processor::CURVE_MESH_SIZE);
                            }
                            else
                                mesh->data(2, 0);

                            // Mark mesh as synchronized
                            b->nSync           &= ~S_DP_MODEL;
                        }
                    }
                }

                // Output FFT curve for input
                plug::mesh_t *mesh            = (c->pFftIn != NULL) ? c->pFftIn->buffer<plug::mesh_t>() : NULL;
                if ((mesh != NULL) && (mesh->isEmpty()))
                {
                    if (c->bInFft)
                    {
                        // Copy frequency points
                        dsp::copy(mesh->pvData[0], vFreqs, meta::mb_dyna_processor::FFT_MESH_POINTS);
                        sAnalyzer.get_spectrum(c->nAnInChannel, mesh->pvData[1], vIndexes, meta::mb_dyna_processor::FFT_MESH_POINTS);

                        // Mark mesh containing data
                        mesh->data(2, meta::mb_dyna_processor::FFT_MESH_POINTS);
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
                        dsp::copy(mesh->pvData[0], vFreqs, meta::mb_dyna_processor::FFT_MESH_POINTS);
                        sAnalyzer.get_spectrum(c->nAnOutChannel, mesh->pvData[1], vIndexes, meta::mb_dyna_processor::FFT_MESH_POINTS);

                        // Mark mesh containing data
                        mesh->data(2, meta::mb_dyna_processor::FFT_MESH_POINTS);
                    }
                    else
                        mesh->data(2, 0);
                }

                // Output Channel curve
                mesh            = (c->pAmpGraph != NULL) ? c->pAmpGraph->buffer<plug::mesh_t>() : NULL;
                if ((mesh != NULL) && (mesh->isEmpty()))
                {
                    // Calculate amplitude (modulo)
                    dsp::copy(mesh->pvData[0], vFreqs, meta::mb_dyna_processor::FFT_MESH_POINTS);
                    dsp::copy(mesh->pvData[1], c->vTrMem, meta::mb_dyna_processor::FFT_MESH_POINTS);
                    mesh->data(2, meta::mb_dyna_processor::FFT_MESH_POINTS);
                }
            } // for channel
        #endif

            // Request for redraw
            if (pWrapper != NULL)
                pWrapper->query_display_draw();
        }

        void gott_compressor::dump(dspu::IStateDumper *v) const
        {
        }

    } /* namespace plugins */
} /* namespace lsp */


