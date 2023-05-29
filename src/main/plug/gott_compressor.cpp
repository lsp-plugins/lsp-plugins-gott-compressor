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
#include <lsp-plug.in/plug-fw/meta/func.h>
#include <lsp-plug.in/stdlib/string.h>

#include <private/plugins/gott_compressor.h>

/* The size of temporary buffer for audio processing */
#define BUFFER_SIZE         0x1000U

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

            vChannels           = NULL;

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

            size_t channels         = (nMode == GOTT_MONO) ? 1 : 2;
            size_t szof_channel     = sizeof(channel_t);
            size_t to_alloc         = szof_channel * channels;

            // Allocate data
            uint8_t *ptr            = alloc_aligned<uint8_t>(pData, to_alloc);
            if (ptr == NULL)
                return;

            vChannels               = reinterpret_cast<channel_t *>(ptr);
            ptr                    += szof_channel * channels;

            // Initialize channels
            for (size_t i=0; i<channels; ++i)
            {
                channel_t *c            = &vChannels[i];

                c->sBypass.construct();

                c->vIn                  = NULL;
                c->vOut                 = NULL;
                c->vScIn                = NULL;

                c->pIn                  = NULL;
                c->pOut                 = NULL;
                c->pScIn                = NULL;
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
        }

        void gott_compressor::destroy()
        {
            Module::destroy();

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
        }

        void gott_compressor::dump(dspu::IStateDumper *v) const
        {
        }

    } /* namespace plugins */
} /* namespace lsp */


