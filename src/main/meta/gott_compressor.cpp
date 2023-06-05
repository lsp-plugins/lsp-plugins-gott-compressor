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

#include <lsp-plug.in/plug-fw/meta/ports.h>
#include <lsp-plug.in/shared/meta/developers.h>
#include <private/meta/gott_compressor.h>

#define LSP_PLUGINS_GOTT_COMPRESSOR_VERSION_MAJOR       1
#define LSP_PLUGINS_GOTT_COMPRESSOR_VERSION_MINOR       0
#define LSP_PLUGINS_GOTT_COMPRESSOR_VERSION_MICRO       0

#define LSP_PLUGINS_GOTT_COMPRESSOR_VERSION  \
    LSP_MODULE_VERSION( \
        LSP_PLUGINS_GOTT_COMPRESSOR_VERSION_MAJOR, \
        LSP_PLUGINS_GOTT_COMPRESSOR_VERSION_MINOR, \
        LSP_PLUGINS_GOTT_COMPRESSOR_VERSION_MICRO  \
    )

namespace lsp
{
    namespace meta
    {
        //-------------------------------------------------------------------------
        // Plugin metadata

        static const port_item_t gott_dyna_sc_boost[] =
        {
            { "None",           "sidechain.boost.none" },
            { "Pink BT",        "sidechain.boost.pink_bt" },
            { "Pink MT",        "sidechain.boost.pink_mt" },
            { "Brown BT",       "sidechain.boost.brown_bt" },
            { "Brown MT",       "sidechain.boost.brown_mt" },
            { NULL, NULL }
        };

        static const port_item_t gott_global_dyna_modes[] =
        {
            { "Classic",        "gott_comp.classic" },
            { "Modern",         "gott_comp.modern" },
            { NULL, NULL }
        };

        #define GOTT_COMMON \
            BYPASS, \
            COMBO("mode", "Compressor mode", 1, gott_global_dyna_modes), \
            AMP_GAIN("g_in", "Input gain", gott_compressor::IN_GAIN_DFL, 10.0f), \
            AMP_GAIN("g_out", "Output gain", gott_compressor::OUT_GAIN_DFL, 10.0f), \
            AMP_GAIN("g_dry", "Dry gain", 0.0f, 10.0f), \
            AMP_GAIN("g_wet", "Wet gain", 1.0f, 10.0f), \
            AMP_GAIN("g_sc", "Sidechain pre-amplification", 1.0f, 10.0f), \
            LOG_CONTROL("react", "FFT reactivity", U_MSEC, gott_compressor::REACT_TIME), \
            AMP_GAIN("shift", "Shift gain", 1.0f, 100.0f), \
            LOG_CONTROL("zoom", "Graph zoom", U_GAIN_AMP, gott_compressor::ZOOM), \
            COMBO("envb", "Envelope boost", gott_compressor::FB_DEFAULT, gott_dyna_sc_boost), \
            LOG_CONTROL("sf1", "Split frequency 1", U_GAIN_AMP, gott_compressor::SPLIT1), \
            LOG_CONTROL("sf2", "Split frequency 2", U_GAIN_AMP, gott_compressor::SPLIT2), \
            LOG_CONTROL("sf3", "Split frequency 3", U_GAIN_AMP, gott_compressor::SPLIT3), \
            SWITCH("ebe", "Enable extra band", 0)

        #define GOTT_SC_COMMON \
            SWITCH("esc", "Enable external sidechain", 0)

        #define GOTT_METERS(id, label) \
            SWITCH("ife" id, "Input FFT graph enable" label, 1.0f), \
            SWITCH("ofe" id, "Output FFT graph enable" label, 1.0f), \
            MESH("ifg" id, "Input FFT graph" label, 2, gott_compressor::FFT_MESH_POINTS), \
            MESH("ofg" id, "Output FFT graph" label, 2, gott_compressor::FFT_MESH_POINTS), \
            METER_GAIN("ilm" id, "Input level meter" label, GAIN_AMP_P_24_DB), \
            METER_GAIN("olm" id, "Output level meter" label, GAIN_AMP_P_24_DB)

        static const port_t gott_compressor_mono_ports[] =
        {
            PORTS_MONO_PLUGIN,
            GOTT_COMMON,
            GOTT_METERS("", ""),
            PORTS_END
        };

        static const port_t gott_compressor_stereo_ports[] =
        {
            PORTS_STEREO_PLUGIN,
            GOTT_COMMON,
            GOTT_METERS("_l", " Left"),
            GOTT_METERS("_r", " Right"),
            PORTS_END
        };

        static const port_t gott_compressor_lr_ports[] =
        {
            PORTS_STEREO_PLUGIN,
            GOTT_COMMON,
            GOTT_METERS("_l", " Left"),
            GOTT_METERS("_r", " Right"),
            PORTS_END
        };

        static const port_t gott_compressor_ms_ports[] =
        {
            PORTS_STEREO_PLUGIN,
            GOTT_COMMON,
            GOTT_METERS("_m", " Mid"),
            GOTT_METERS("_s", " Side"),
            PORTS_END
        };

        static const port_t sc_gott_compressor_mono_ports[] =
        {
            PORTS_MONO_PLUGIN,
            PORTS_MONO_SIDECHAIN,
            GOTT_COMMON,
            GOTT_SC_COMMON,
            GOTT_METERS("", ""),
            PORTS_END
        };

        static const port_t sc_gott_compressor_stereo_ports[] =
        {
            PORTS_STEREO_PLUGIN,
            PORTS_STEREO_SIDECHAIN,
            GOTT_COMMON,
            GOTT_SC_COMMON,
            GOTT_METERS("_l", " Left"),
            GOTT_METERS("_r", " Right"),
            PORTS_END
        };

        static const port_t sc_gott_compressor_lr_ports[] =
        {
            PORTS_STEREO_PLUGIN,
            PORTS_STEREO_SIDECHAIN,
            GOTT_COMMON,
            GOTT_SC_COMMON,
            GOTT_METERS("_l", " Left"),
            GOTT_METERS("_r", " Right"),
            PORTS_END
        };

        static const port_t sc_gott_compressor_ms_ports[] =
        {
            PORTS_STEREO_PLUGIN,
            PORTS_STEREO_SIDECHAIN,
            GOTT_COMMON,
            GOTT_SC_COMMON,
            GOTT_METERS("_m", " Mid"),
            GOTT_METERS("_s", " Side"),
            PORTS_END
        };

        static const int plugin_classes[]       = { C_COMPRESSOR, -1 };
        static const int clap_features_mono[]   = { CF_AUDIO_EFFECT, CF_COMPRESSOR, CF_MONO, -1 };
        static const int clap_features_stereo[] = { CF_AUDIO_EFFECT, CF_COMPRESSOR, CF_STEREO, -1 };

        const meta::bundle_t gott_compressor_bundle =
        {
            "gott_compressor",
            "GOTT Comressor",
            B_DYNAMICS,
            "", // TODO: provide ID of the video on YouTube
            "" // TODO: write plugin description, should be the same to the english version in 'bundles.json'
        };

        const plugin_t gott_compressor_mono =
        {
            "GOTT Kompressor Mono",
            "GOTT Compressor Mono",
            "GC1M",
            &developers::v_sadovnikov,
            "gott_compressor_mono",
            LSP_LV2_URI("gott_compressor_mono"),
            LSP_LV2UI_URI("gott_compressor_mono"),
            "ngcm",
            LSP_LADSPA_GOTT_COMPRESSOR_BASE + 0,
            LSP_LADSPA_URI("gott_compressor_mono"),
            LSP_CLAP_URI("gott_compressor_mono"),
            LSP_PLUGINS_GOTT_COMPRESSOR_VERSION,
            plugin_classes,
            clap_features_mono,
            E_DUMP_STATE | E_INLINE_DISPLAY,
            gott_compressor_mono_ports,
            "dynamics/gott_compressor/mono.xml",
            NULL,
            mono_plugin_port_groups,
            &gott_compressor_bundle
        };

        const plugin_t gott_compressor_stereo =
        {
            "GOTT Kompressor Stereo",
            "GOTT Compressor Stereo",
            "GC1S",
            &developers::v_sadovnikov,
            "gott_compressor_stereo",
            LSP_LV2_URI("gott_compressor_stereo"),
            LSP_LV2UI_URI("gott_compressor_stereo"),
            "ngcs",
            LSP_LADSPA_GOTT_COMPRESSOR_BASE + 1,
            LSP_LADSPA_URI("gott_compressor_stereo"),
            LSP_CLAP_URI("gott_compressor_stereo"),
            LSP_PLUGINS_GOTT_COMPRESSOR_VERSION,
            plugin_classes,
            clap_features_stereo,
            E_DUMP_STATE | E_INLINE_DISPLAY,
            gott_compressor_stereo_ports,
            "dynamics/gott_compressor/stereo.xml",
            NULL,
            stereo_plugin_port_groups,
            &gott_compressor_bundle
        };

        const plugin_t gott_compressor_lr =
        {
            "GOTT Kompressor LeftRight",
            "GOTT Compressor LeftRight",
            "GC1LR",
            &developers::v_sadovnikov,
            "gott_compressor_lr",
            LSP_LV2_URI("gott_compressor_lr"),
            LSP_LV2UI_URI("gott_compressor_lr"),
            "ngcl",
            LSP_LADSPA_GOTT_COMPRESSOR_BASE + 2,
            LSP_LADSPA_URI("gott_compressor_lr"),
            LSP_CLAP_URI("gott_compressor_lr"),
            LSP_PLUGINS_GOTT_COMPRESSOR_VERSION,
            plugin_classes,
            clap_features_stereo,
            E_DUMP_STATE | E_INLINE_DISPLAY,
            gott_compressor_lr_ports,
            "dynamics/gott_compressor/lr.xml",
            NULL,
            stereo_plugin_port_groups,
            &gott_compressor_bundle
        };

        const plugin_t gott_compressor_ms =
        {
            "GOTT Kompressor MidSide",
            "GOTT Compressor MidSide",
            "GC1MS",
            &developers::v_sadovnikov,
            "gott_compressor_ms",
            LSP_LV2_URI("gott_compressor_ms"),
            LSP_LV2UI_URI("gott_compressor_ms"),
            "ngcM",
            LSP_LADSPA_GOTT_COMPRESSOR_BASE + 3,
            LSP_LADSPA_URI("gott_compressor_ms"),
            LSP_CLAP_URI("gott_compressor_ms"),
            LSP_PLUGINS_GOTT_COMPRESSOR_VERSION,
            plugin_classes,
            clap_features_stereo,
            E_DUMP_STATE | E_INLINE_DISPLAY,
            gott_compressor_ms_ports,
            "dynamics/gott_compressor/ms.xml",
            NULL,
            stereo_plugin_port_groups,
            &gott_compressor_bundle
        };

        const plugin_t sc_gott_compressor_mono =
        {
            "Sidechain GOTT Kompressor Mono",
            "Sidechain GOTT Compressor Mono",
            "SCGC1M",
            &developers::v_sadovnikov,
            "sc_gott_compressor_mono",
            LSP_LV2_URI("sc_gott_compressor_mono"),
            LSP_LV2UI_URI("sc_gott_compressor_mono"),
            "sgcm",
            LSP_LADSPA_GOTT_COMPRESSOR_BASE + 4,
            LSP_LADSPA_URI("sc_gott_compressor_mono"),
            LSP_CLAP_URI("sc_gott_compressor_mono"),
            LSP_PLUGINS_GOTT_COMPRESSOR_VERSION,
            plugin_classes,
            clap_features_mono,
            E_DUMP_STATE | E_INLINE_DISPLAY,
            sc_gott_compressor_mono_ports,
            "dynamics/gott_compressor/mono.xml",
            NULL,
            mono_plugin_sidechain_port_groups,
            &gott_compressor_bundle
        };

        const plugin_t sc_gott_compressor_stereo =
        {
            "Sidechain GOTT Kompressor Stereo",
            "Sidechain GOTT Compressor Stereo",
            "SCGC1S",
            &developers::v_sadovnikov,
            "sc_gott_compressor_stereo",
            LSP_LV2_URI("sc_gott_compressor_stereo"),
            LSP_LV2UI_URI("sc_gott_compressor_stereo"),
            "sgcs",
            LSP_LADSPA_GOTT_COMPRESSOR_BASE + 5,
            LSP_LADSPA_URI("sc_gott_compressor_stereo"),
            LSP_CLAP_URI("sc_gott_compressor_stereo"),
            LSP_PLUGINS_GOTT_COMPRESSOR_VERSION,
            plugin_classes,
            clap_features_stereo,
            E_DUMP_STATE | E_INLINE_DISPLAY,
            sc_gott_compressor_stereo_ports,
            "dynamics/gott_compressor/stereo.xml",
            NULL,
            stereo_plugin_sidechain_port_groups,
            &gott_compressor_bundle
        };

        const plugin_t sc_gott_compressor_lr =
        {
            "Sidechain GOTT Kompressor LeftRight",
            "Sidechain GOTT Compressor LeftRight",
            "SCGC1LR",
            &developers::v_sadovnikov,
            "sc_gott_compressor_lr",
            LSP_LV2_URI("sc_gott_compressor_lr"),
            LSP_LV2UI_URI("sc_gott_compressor_lr"),
            "sgcl",
            LSP_LADSPA_GOTT_COMPRESSOR_BASE + 6,
            LSP_LADSPA_URI("sc_gott_compressor_lr"),
            LSP_CLAP_URI("sc_gott_compressor_lr"),
            LSP_PLUGINS_GOTT_COMPRESSOR_VERSION,
            plugin_classes,
            clap_features_stereo,
            E_DUMP_STATE | E_INLINE_DISPLAY,
            sc_gott_compressor_lr_ports,
            "dynamics/gott_compressor/lr.xml",
            NULL,
            stereo_plugin_sidechain_port_groups,
            &gott_compressor_bundle
        };

        const plugin_t sc_gott_compressor_ms =
        {
            "Sidechain GOTT Kompressor MidSide",
            "Sidechain GOTT Compressor MidSide",
            "SCGC1MS",
            &developers::v_sadovnikov,
            "sc_gott_compressor_ms",
            LSP_LV2_URI("sc_gott_compressor_ms"),
            LSP_LV2UI_URI("sc_gott_compressor_ms"),
            "sgcM",
            LSP_LADSPA_GOTT_COMPRESSOR_BASE + 7,
            LSP_LADSPA_URI("sc_gott_compressor_ms"),
            LSP_CLAP_URI("sc_gott_compressor_ms"),
            LSP_PLUGINS_GOTT_COMPRESSOR_VERSION,
            plugin_classes,
            clap_features_stereo,
            E_DUMP_STATE | E_INLINE_DISPLAY,
            sc_gott_compressor_ms_ports,
            "dynamics/gott_compressor/ms.xml",
            NULL,
            stereo_plugin_sidechain_port_groups,
            &gott_compressor_bundle
        };

    } /* namespace meta */
} /* namespace lsp */



