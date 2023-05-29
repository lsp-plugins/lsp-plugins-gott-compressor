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
        }

        gott_compressor::~gott_compressor()
        {
            destroy();
        }

        void gott_compressor::init(plug::IWrapper *wrapper, plug::IPort **ports)
        {
            // Call parent class for initialization
            Module::init(wrapper, ports);
        }

        void gott_compressor::destroy()
        {
            Module::destroy();
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


