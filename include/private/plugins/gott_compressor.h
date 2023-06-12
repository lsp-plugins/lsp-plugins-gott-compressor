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

#ifndef PRIVATE_PLUGINS_GOTT_COMPRESSOR_H_
#define PRIVATE_PLUGINS_GOTT_COMPRESSOR_H_

#include <lsp-plug.in/dsp-units/ctl/Bypass.h>
#include <lsp-plug.in/dsp-units/dynamics/DynamicProcessor.h>
#include <lsp-plug.in/dsp-units/filters/DynamicFilters.h>
#include <lsp-plug.in/dsp-units/filters/Equalizer.h>
#include <lsp-plug.in/dsp-units/filters/Filter.h>
#include <lsp-plug.in/dsp-units/util/Analyzer.h>
#include <lsp-plug.in/dsp-units/util/Delay.h>
#include <lsp-plug.in/dsp-units/util/Sidechain.h>
#include <lsp-plug.in/plug-fw/plug.h>

#include <private/meta/gott_compressor.h>

namespace lsp
{
    namespace plugins
    {
        /**
         * Base class for the latency compensation delay
         */
        class gott_compressor: public plug::Module
        {
            private:
                gott_compressor & operator = (const gott_compressor &);
                gott_compressor (const gott_compressor &);

            public:
                enum gott_mode_t
                {
                    GOTT_MONO,
                    GOTT_STEREO,
                    GOTT_LR,
                    GOTT_MS
                };

            protected:
                enum sync_t
                {
                    S_COMP_CURVE    = 1 << 0,
                    S_EQ_CURVE      = 1 << 2,

                    S_ALL           = S_COMP_CURVE | S_EQ_CURVE
                };

                typedef struct band_t
                {
                    dspu::Sidechain         sSC;                // Sidechain module
                    dspu::Equalizer         sEQ[2];             // Sidechain equalizers
                    dspu::DynamicProcessor  sProc;              // Dynamic Processor
                    dspu::Filter            sPassFilter;        // Passing filter for 'classic' mode
                    dspu::Filter            sRejFilter;         // Rejection filter for 'classic' mode
                    dspu::Filter            sAllFilter;         // All-pass filter for phase compensation

                    float                  *vVCA;               // Voltage-controlled amplification value for each band
                    float                  *vCurveBuffer;       // Compression curve
                    float                  *vFilterBuffer;      // Band Filter buffer

                    float                   fMinThresh;         // Minimum threshold
                    float                   fUpThresh;          // Upward threshold
                    float                   fDownThresh;        // Downward threshold
                    float                   fUpRatio;           // Upward ratio
                    float                   fDownRatio;         // Downward ratio
                    float                   fAttackTime;        // Attack time
                    float                   fReleaseTime;       // Release time
                    float                   fMakeup;            // Makeup gain
                    float                   fGainLevel;         // Measured gain adjustment level
                    size_t                  nSync;              // Mesh synchronization flags
                    size_t                  nFilterID;          // Filter ID in dynamic filters
                    bool                    bEnabled;           // Enabled flag
                    bool                    bSolo;              // Solo channel
                    bool                    bMute;              // Mute channel

                    plug::IPort            *pMinThresh;         // Minimum threshold
                    plug::IPort            *pUpThresh;          // Upward threshold
                    plug::IPort            *pDownThresh;        // Downward threshold
                    plug::IPort            *pUpRatio;           // Upward ratio
                    plug::IPort            *pDownRatio;         // Downward ratio
                    plug::IPort            *pKnee;              // Knee
                    plug::IPort            *pAttackTime;        // Attack time
                    plug::IPort            *pReleaseTime;       // Release time
                    plug::IPort            *pMakeup;            // Makeup gain

                    plug::IPort            *pEnabled;           // Enabled flag
                    plug::IPort            *pSolo;              // Solo channel
                    plug::IPort            *pMute;              // Mute channel
                    plug::IPort            *pCurveMesh;         // Curve mesh
                    plug::IPort            *pFreqMesh;          // Filter frequencymesh
                    plug::IPort            *pEnvLvl;            // Envelope level meter
                    plug::IPort            *pCurveLvl;          // Reduction curve level meter
                    plug::IPort            *pMeterGain;         // Reduction gain meter
                } band_t;

                typedef struct channel_t
                {
                    dspu::Bypass            sBypass;            // Bypass
                    dspu::Filter            sEnvBoost[2];       // Envelope boost filter
                    dspu::Equalizer         sDryEq;             // Dry equalizer
                    dspu::Delay             sDelay;             // Lookahead Delay

                    band_t                  vBands[meta::gott_compressor::BANDS_MAX];

                    float                  *vIn;                // Input data buffer
                    float                  *vOut;               // Output data buffer
                    float                  *vScIn;              // Sidechain data buffer (if present)
                    float                  *vInBuffer;          // Input buffer
                    float                  *vBuffer;            // Temporary buffer
                    float                  *vScBuffer;          // Sidechain buffer
                    float                  *vInAnalyze;         // Input signal analysis
                    float                  *vOutAnalyze;        // Output signal analysis
                    float                  *vTmpFilterBuffer;   // Filter transfer function of the channel (temporary)
                    float                  *vFilterBuffer;      // Filter transfer function of the channel

                    size_t                  nAnInChannel;       // Analyzer channel used for input signal analysis
                    size_t                  nAnOutChannel;      // Analyzer channel used for output signal analysis
                    bool                    bInFft;             // Input signal FFT enabled
                    bool                    bOutFft;            // Output signal FFT enabled
                    bool                    bRebuildFilers;     // Rebuild filter configuration

                    plug::IPort            *pIn;                // Input
                    plug::IPort            *pOut;               // Output
                    plug::IPort            *pScIn;              // Sidechain
                    plug::IPort            *pFftInSw;           // Pre-processing FFT analysis control port
                    plug::IPort            *pFftOutSw;          // Post-processing FFT analysis controlport
                    plug::IPort            *pFftIn;             // Pre-processing FFT analysis data
                    plug::IPort            *pFftOut;            // Post-processing FFT analysis data
                    plug::IPort            *pAmpGraph;          // Amplitude graph
                    plug::IPort            *pInLvl;             // Input level meter
                    plug::IPort            *pOutLvl;            // Output level meter
                } channel_t;

            protected:
                dspu::Analyzer          sAnalyzer;              // Analyzer
                dspu::DynamicFilters    sFilters;               // Dynamic filters for each band in 'modern' mode

                size_t                  nMode;                  // Processor mode
                bool                    bSidechain;             // External side chain
                bool                    bModern;                // Modern/Classic mode switch
                bool                    bEnvUpdate;             // Envelope filter update
                size_t                  nBands;                 // Number of bands
                bool                    bExtSidechain;          // External sidechain
                float                   fInGain;                // Input gain adjustment
                float                   fDryGain;               // Dry gain
                float                   fWetGain;               // Wet gain
                float                   fScPreamp;              // Sidechain pre-amplification
                size_t                  nEnvBoost;              // Envelope boost
                float                   vSplits[meta::gott_compressor::BANDS_MAX - 1];  // Split frequencies
                channel_t              *vChannels;              // Processor channels
                float                  *vAnalyze[4];            // Analysis buffer
                float                  *vBuffer;                // Temporary buffer
                float                  *vSC[2];                 // Sidechain pre-processing
                float                  *vEnv;                   // Envelope buffer
                float                  *vTr;                    // Transfer buffer
                float                  *vPFc;                   // Pass filter characteristics buffer
                float                  *vRFc;                   // Reject filter characteristics buffer
                float                  *vCurveBuffer;           // Compression curve (input values)
                float                  *vFreqBuffer;            // Frequencies (input values)
                uint32_t               *vFreqIndexes;           // Analyzer FFT indexes

                plug::IPort            *pBypass;                // Bypass port
                plug::IPort            *pMode;                  // Global mode
                plug::IPort            *pInGain;                // Input gain port
                plug::IPort            *pOutGain;               // Output gain port
                plug::IPort            *pDryGain;               // Dry gain port
                plug::IPort            *pWetGain;               // Wet gain port
                plug::IPort            *pScMode;                // Sidechain mode
                plug::IPort            *pScSource;              // Sidechain source
                plug::IPort            *pScPreamp;              // Sidechain preamp
                plug::IPort            *pScReact;               // Sidechain reactivity
                plug::IPort            *pLookahead;             // Lookahead time
                plug::IPort            *pReactivity;            // Reactivity
                plug::IPort            *pShiftGain;             // Shift gain port
                plug::IPort            *pZoom;                  // Zoom port
                plug::IPort            *pEnvBoost;              // Envelope adjust
                plug::IPort            *pSplits[meta::gott_compressor::BANDS_MAX - 1];  // Split frequencies
                plug::IPort            *pExtraBand;             // Extra band enable
                plug::IPort            *pExtSidechain;          // External sidechain enable

                uint8_t                *pData;                  // Aligned data pointer

            public:
                explicit gott_compressor(const meta::plugin_t *meta);
                virtual ~gott_compressor() override;

                virtual void        init(plug::IWrapper *wrapper, plug::IPort **ports) override;
                virtual void        destroy() override;

            public:
                virtual void        update_sample_rate(long sr) override;
                virtual void        update_settings() override;
                virtual void        ui_activated() override;
                virtual void        process(size_t samples) override;
                virtual void        dump(dspu::IStateDumper *v) const override;
        };

    } /* namespace plugins */
} /* namespace lsp */


#endif /* PRIVATE_PLUGINS_GOTT_COMPRESSOR_H_ */

