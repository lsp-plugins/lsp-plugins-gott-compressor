/*
 * Copyright (C) 2025 Linux Studio Plugins Project <https://lsp-plug.in/>
 *           (C) 2025 Vladimir Sadovnikov <sadko4u@gmail.com>
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

#include <lsp-plug.in/plug-fw/meta/ports.h>
#include <lsp-plug.in/shared/meta/developers.h>
#include <private/meta/gott_compressor.h>

#define LSP_PLUGINS_GOTT_COMPRESSOR_VERSION_MAJOR       1
#define LSP_PLUGINS_GOTT_COMPRESSOR_VERSION_MINOR       0
#define LSP_PLUGINS_GOTT_COMPRESSOR_VERSION_MICRO       16

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
            { "None",           "sidechain.boost.none"      },
            { "Pink BT",        "sidechain.boost.pink_bt"   },
            { "Pink MT",        "sidechain.boost.pink_mt"   },
            { "Brown BT",       "sidechain.boost.brown_bt"  },
            { "Brown MT",       "sidechain.boost.brown_mt"  },
            { NULL, NULL }
        };

        static const port_item_t gott_global_dyna_modes[] =
        {
            { "Classic",        "multiband.classic"         },
            { "Modern",         "multiband.modern"          },
            { "Linear Phase",   "multiband.linear_phase"    },
            { NULL, NULL }
        };

        static const port_item_t gott_sc_modes[] =
        {
            { "Peak",           "sidechain.peak"            },
            { "RMS",            "sidechain.rms"             },
            { "LPF",            "sidechain.lpf"             },
            { "SMA",            "sidechain.sma"             },
            { NULL, NULL }
        };

        static const port_item_t gott_sc_sources[] =
        {
            { "Middle",         "sidechain.middle"          },
            { "Side",           "sidechain.side"            },
            { "Left",           "sidechain.left"            },
            { "Right",          "sidechain.right"           },
            { "Min",            "sidechain.min"             },
            { "Max",            "sidechain.max"             },
            { NULL, NULL }
        };

        static const port_item_t gott_sc_split_source[] =
        {
            { "Left/Right",     "sidechain.left_right"      },
            { "Right/Left",     "sidechain.right_left"      },
            { "Mid/Side",       "sidechain.mid_side"        },
            { "Side/Mid",       "sidechain.side_mid"        },
            { "Min",            "sidechain.min"             },
            { "Max",            "sidechain.max"             },
            { NULL, NULL }
        };

        static const port_item_t gott_lr_selectors[] =
        {
            { "Left",           "gott_comp.selectors.left"  },
            { "Right",          "gott_comp.selectors.right" },
            { NULL, NULL }
        };

        static const port_item_t gott_ms_selectors[] =
        {
            { "Mid",           "gott_comp.selectors.middle" },
            { "Side",          "gott_comp.selectors.side"   },
            { NULL, NULL }
        };

        static const port_item_t gott_sc_source[] =
        {
            { "Internal",       "sidechain.internal"        },
            { "Link",           "sidechain.link"            },
            { NULL, NULL }
        };

        static const port_item_t gott_sc_source_for_sc[] =
        {
            { "Internal",       "sidechain.internal"        },
            { "External",       "sidechain.external"        },
            { "Link",           "sidechain.link"            },
            { NULL, NULL }
        };

        #define GOTT_SHM_LINK_MONO \
            OPT_RETURN_MONO("link", "shml", "Side-chain shared memory link")

        #define GOTT_SHM_LINK_STEREO \
            OPT_RETURN_STEREO("link", "shml_", "Side-chain shared memory link")

        #define GOTT_BASE \
            BYPASS, \
            COMBO("mode", "Operating mode", "Mode", 1, gott_global_dyna_modes), \
            SWITCH("prot", "Surge protection", "Surge protect", 1.0f), \
            AMP_GAIN("g_in", "Input gain", "Input gain", gott_compressor::IN_GAIN_DFL, 10.0f), \
            AMP_GAIN("g_out", "Output gain", "Output gain", gott_compressor::OUT_GAIN_DFL, 10.0f), \
            AMP_GAIN("g_dry", "Dry gain", "Dry", 0.0f, 10.0f), \
            AMP_GAIN("g_wet", "Wet gain", "Wet", 1.0f, 10.0f), \
            PERCENTS("drywet", "Dry/Wet balance", "Dry/Wet", 100.0f, 0.1f), \
            COMBO("sc_mode", "Sidechain mode", "SC mode", gott_compressor::SC_MODE_DFL, gott_sc_modes), \
            COMBO("sc_src", "Sidechain source", "SC source", 0, gott_sc_sources), \
            AMP_GAIN("sc_pamp", "Sidechain pre-amplification", "SC preamp", 1.0f, 10.0f), \
            LOG_CONTROL("sc_rea", "Sidechain reactivity", "SC react", U_MSEC, gott_compressor::SC_REACTIVITY), \
            CONTROL("lkahead", "Lookahead", "Lookahead", U_MSEC, gott_compressor::LOOKAHEAD), \
            LOG_CONTROL("react", "FFT reactivity", "Reactivity", U_MSEC, gott_compressor::REACT_TIME), \
            AMP_GAIN("shift", "Shift gain", "Shift", 1.0f, 100.0f), \
            LOG_CONTROL("zoom", "Graph zoom", "Zoom", U_GAIN_AMP, gott_compressor::ZOOM), \
            COMBO("envb", "Envelope boost", "Env boost", gott_compressor::FB_DEFAULT, gott_dyna_sc_boost), \
            LOG_CONTROL("sf1", "Split frequency 1", "Split 1", U_HZ, gott_compressor::SPLIT1), \
            LOG_CONTROL("sf2", "Split frequency 2", "Split 2", U_HZ, gott_compressor::SPLIT2), \
            LOG_CONTROL("sf3", "Split frequency 3", "Split 3", U_HZ, gott_compressor::SPLIT3), \
            SWITCH("flt", "Band filter curves", "Show filters", 1.0f), \
            SWITCH("ebe", "Enable extra band", "Extra band on", 0)

        #define GOTT_PREMIX \
            SWITCH("showpmx", "Show pre-mix overlay", "Show premix bar", 0.0f), \
            AMP_GAIN10("in2lk", "Input to Link mix", "In to Link mix", GAIN_AMP_M_INF_DB), \
            AMP_GAIN10("lk2in", "Link to Input mix", "Link to In mix", GAIN_AMP_M_INF_DB), \
            AMP_GAIN10("lk2sc", "Link to Sidechain mix", "Link to SC mix", GAIN_AMP_M_INF_DB)

        #define GOTT_SC_PREMIX \
            GOTT_PREMIX, \
            AMP_GAIN10("in2sc", "Input to Sidechain mix", "In to SC mix", GAIN_AMP_M_INF_DB), \
            AMP_GAIN10("sc2in", "Sidechain to Input mix", "SC to In mix", GAIN_AMP_M_INF_DB), \
            AMP_GAIN10("sc2lk", "Sidechain to Link mix", "SC to Link mix", GAIN_AMP_M_INF_DB)

        #define GOTT_COMMON \
            GOTT_BASE, \
            COMBO("sc_ext", "External sidechain source", "Ext SC source", 0, gott_sc_source)

        #define GOTT_SC_COMMON \
            GOTT_BASE, \
            COMBO("sc_ext", "External sidechain source", "Ext SC source", 0, gott_sc_source_for_sc)

        #define GOTT_SPLIT_COMMON \
            SWITCH("ssplit", "Stereo split", "Stereo split", 0.0f), \
            COMBO("sp_src", "Split sidechain source", "Split SC source", 0, gott_sc_split_source)

        #define GOTT_LINK(id, label, alias) \
            SWITCH(id, label, alias, 0.0f)

        #define GOTT_ANALYSIS(id, label, alias) \
            SWITCH("ife" id, "Input FFT graph enable" label, "FFT In" alias, 1.0f), \
            SWITCH("ofe" id, "Output FFT graph enable" label, "FFT Out" alias, 1.0f), \
            MESH("ifg" id, "Input FFT graph" label, 2, gott_compressor::FFT_MESH_POINTS + 2), \
            MESH("ofg" id, "Output FFT graph" label, 2, gott_compressor::FFT_MESH_POINTS)

        #define GOTT_AMP_CURVE(id, label) \
            MESH("ag" id, "Compressor amplitude graph " label, 2, gott_compressor::FFT_MESH_POINTS)

        #define GOTT_METERS(id, label) \
            METER_GAIN("ilm" id, "Input level meter" label, GAIN_AMP_P_24_DB), \
            METER_GAIN("olm" id, "Output level meter" label, GAIN_AMP_P_24_DB)

        #define GOTT_BAND(id, label, alias) \
            LOG_CONTROL("tm" id, "Minimum threshold" label, "Min thresh" alias, U_GAIN_AMP, gott_compressor::THRESH_MIN), \
            LOG_CONTROL("tu" id, "Upward threshold" label, "Up thresh" alias, U_GAIN_AMP, gott_compressor::THRESH_UP), \
            LOG_CONTROL("td" id, "Downward threshold" label, "Down thresh" alias, U_GAIN_AMP, gott_compressor::THRESH_DOWN), \
            LOG_CONTROL("ru" id, "Upward ratio" label, "Up ratio" alias, U_NONE, gott_compressor::RATIO_UP), \
            LOG_CONTROL("rd" id, "Downward ratio" label, "Down ratio" alias, U_NONE, gott_compressor::RATIO_DOWN), \
            LOG_CONTROL("kn" id, "Knee" label, "Knee" alias, U_GAIN_AMP, gott_compressor::KNEE), \
            LOG_CONTROL("ta" id, "Attack time" label, "Att time" alias, U_MSEC, gott_compressor::ATTACK_TIME), \
            LOG_CONTROL("tr" id, "Release time" label, "Rel time" alias, U_MSEC, gott_compressor::RELEASE_TIME), \
            LOG_CONTROL("mk" id, "Makeup gain" label, "Makeup" alias, U_GAIN_AMP, gott_compressor::MAKEUP), \
            SWITCH("be" id, "Enable compressor on the band" label, "On " alias, 1.0f), \
            SWITCH("bs" id, "Solo band" label, "Solo " alias, 0.0f), \
            SWITCH("bm" id, "Mute band" label, "Mute " alias, 0.0f), \
            MESH("ccg" id, "Compression curve graph" label, 2, gott_compressor::CURVE_MESH_SIZE), \
            MESH("bfc" id, "Band frequency chart" label, 2, gott_compressor::FILTER_MESH_POINTS)

        #define GOTT_BAND_METERS(id, label) \
            METER_OUT_GAIN("elm" id, "Envelope level meter" label, GAIN_AMP_P_36_DB), \
            METER_OUT_GAIN("clm" id, "Curve level meter" label, GAIN_AMP_P_36_DB), \
            METER_OUT_GAIN("rlm" id, "Reduction level meter" label, GAIN_AMP_P_72_DB)

        static const port_t gott_compressor_mono_ports[] =
        {
            PORTS_MONO_PLUGIN,
            GOTT_SHM_LINK_MONO,
            GOTT_PREMIX,
            GOTT_COMMON,

            GOTT_BAND("_1", " 1", " 1"),
            GOTT_BAND("_2", " 2", " 2"),
            GOTT_BAND("_3", " 3", " 3"),
            GOTT_BAND("_4", " 4", " 4"),

            GOTT_BAND_METERS("_1", " 1"),
            GOTT_BAND_METERS("_2", " 2"),
            GOTT_BAND_METERS("_3", " 3"),
            GOTT_BAND_METERS("_4", " 4"),

            GOTT_ANALYSIS("", "", ""),
            GOTT_METERS("", ""),
            GOTT_AMP_CURVE("", ""),
            PORTS_END
        };

        static const port_t gott_compressor_stereo_ports[] =
        {
            PORTS_STEREO_PLUGIN,
            GOTT_SHM_LINK_STEREO,
            GOTT_PREMIX,
            GOTT_COMMON,
            GOTT_SPLIT_COMMON,

            GOTT_BAND("_1", " 1", " 1"),
            GOTT_BAND("_2", " 2", " 2"),
            GOTT_BAND("_3", " 3", " 3"),
            GOTT_BAND("_4", " 4", " 4"),

            GOTT_BAND_METERS("_1l", " 1 Left"),
            GOTT_BAND_METERS("_2l", " 2 Left"),
            GOTT_BAND_METERS("_3l", " 3 Left"),
            GOTT_BAND_METERS("_4l", " 4 Left"),
            GOTT_BAND_METERS("_1r", " 1 Right"),
            GOTT_BAND_METERS("_2r", " 2 Right"),
            GOTT_BAND_METERS("_3r", " 3 Right"),
            GOTT_BAND_METERS("_4r", " 4 Right"),

            GOTT_ANALYSIS("_l", " Left", " L"),
            GOTT_METERS("_l", " Left"),
            GOTT_ANALYSIS("_r", " Right", " R"),
            GOTT_METERS("_r", " Right"),
            GOTT_AMP_CURVE("_l", " Left"),
            GOTT_AMP_CURVE("_r", " Right"),
            PORTS_END
        };

        static const port_t gott_compressor_lr_ports[] =
        {
            PORTS_STEREO_PLUGIN,
            GOTT_SHM_LINK_STEREO,
            GOTT_PREMIX,
            GOTT_COMMON,
            COMBO("csel", "Channel selector", "Channel select", 0, gott_lr_selectors),
            GOTT_LINK("clink", "Left/Right controls link", "L/R link"),

            GOTT_BAND("_1l", " 1 Left", " 1 L"),
            GOTT_BAND("_2l", " 2 Left", " 2 L"),
            GOTT_BAND("_3l", " 3 Left", " 3 L"),
            GOTT_BAND("_4l", " 4 Left", " 4 L"),
            GOTT_BAND("_1r", " 1 Right", " 1 R"),
            GOTT_BAND("_2r", " 2 Right", " 2 R"),
            GOTT_BAND("_3r", " 3 Right", " 3 R"),
            GOTT_BAND("_4r", " 4 Right", " 4 R"),

            GOTT_BAND_METERS("_1l", " 1 Left"),
            GOTT_BAND_METERS("_2l", " 2 Left"),
            GOTT_BAND_METERS("_3l", " 3 Left"),
            GOTT_BAND_METERS("_4l", " 4 Left"),
            GOTT_BAND_METERS("_1r", " 1 Right"),
            GOTT_BAND_METERS("_2r", " 2 Right"),
            GOTT_BAND_METERS("_3r", " 3 Right"),
            GOTT_BAND_METERS("_4r", " 4 Right"),

            GOTT_ANALYSIS("_l", " Left", " L"),
            GOTT_METERS("_l", " Left"),
            GOTT_ANALYSIS("_r", " Right", " R"),
            GOTT_METERS("_r", " Right"),
            GOTT_AMP_CURVE("_l", " Left"),
            GOTT_AMP_CURVE("_r", " Right"),
            PORTS_END
        };

        static const port_t gott_compressor_ms_ports[] =
        {
            PORTS_STEREO_PLUGIN,
            GOTT_SHM_LINK_STEREO,
            GOTT_PREMIX,
            GOTT_COMMON,
            COMBO("csel", "Channel selector", "Channel select", 0, gott_ms_selectors),
            GOTT_LINK("clink", "Mid/Sidde controls link", "M/S link"),

            GOTT_BAND("_1m", " 1 Mid", " 1 M"),
            GOTT_BAND("_2m", " 2 Mid", " 2 M"),
            GOTT_BAND("_3m", " 3 Mid", " 3 M"),
            GOTT_BAND("_4m", " 4 Mid", " 4 M"),
            GOTT_BAND("_1s", " 1 Side", " 1 S"),
            GOTT_BAND("_2s", " 2 Side", " 2 S"),
            GOTT_BAND("_3s", " 3 Side", " 3 S"),
            GOTT_BAND("_4s", " 4 Side", " 4 S"),

            GOTT_BAND_METERS("_1m", " 1 Mid"),
            GOTT_BAND_METERS("_2m", " 2 Mid"),
            GOTT_BAND_METERS("_3m", " 3 Mid"),
            GOTT_BAND_METERS("_4m", " 4 Mid"),
            GOTT_BAND_METERS("_1s", " 1 Side"),
            GOTT_BAND_METERS("_2s", " 2 Side"),
            GOTT_BAND_METERS("_3s", " 3 Side"),
            GOTT_BAND_METERS("_4s", " 4 Side"),

            GOTT_ANALYSIS("_m", " Mid", " M"),
            GOTT_METERS("_l", " Left"),
            GOTT_ANALYSIS("_s", " Side", " S"),
            GOTT_METERS("_r", " Right"),
            GOTT_AMP_CURVE("_m", " Mid"),
            GOTT_AMP_CURVE("_s", " Side"),
            PORTS_END
        };

        static const port_t sc_gott_compressor_mono_ports[] =
        {
            PORTS_MONO_PLUGIN,
            PORTS_MONO_SIDECHAIN,
            GOTT_SHM_LINK_MONO,
            GOTT_SC_PREMIX,
            GOTT_SC_COMMON,

            GOTT_BAND("_1", " 1", " 1"),
            GOTT_BAND("_2", " 2", " 2"),
            GOTT_BAND("_3", " 3", " 3"),
            GOTT_BAND("_4", " 4", " 4"),

            GOTT_BAND_METERS("_1", " 1"),
            GOTT_BAND_METERS("_2", " 2"),
            GOTT_BAND_METERS("_3", " 3"),
            GOTT_BAND_METERS("_4", " 4"),

            GOTT_ANALYSIS("", "", ""),
            GOTT_METERS("", ""),
            GOTT_AMP_CURVE("", ""),
            PORTS_END
        };

        static const port_t sc_gott_compressor_stereo_ports[] =
        {
            PORTS_STEREO_PLUGIN,
            PORTS_STEREO_SIDECHAIN,
            GOTT_SHM_LINK_STEREO,
            GOTT_SC_PREMIX,
            GOTT_SC_COMMON,
            GOTT_SPLIT_COMMON,

            GOTT_BAND("_1", " 1", " 1"),
            GOTT_BAND("_2", " 2", " 2"),
            GOTT_BAND("_3", " 3", " 3"),
            GOTT_BAND("_4", " 4", " 4"),

            GOTT_BAND_METERS("_1l", " 1 Left"),
            GOTT_BAND_METERS("_2l", " 2 Left"),
            GOTT_BAND_METERS("_3l", " 3 Left"),
            GOTT_BAND_METERS("_4l", " 4 Left"),
            GOTT_BAND_METERS("_1r", " 1 Right"),
            GOTT_BAND_METERS("_2r", " 2 Right"),
            GOTT_BAND_METERS("_3r", " 3 Right"),
            GOTT_BAND_METERS("_4r", " 4 Right"),

            GOTT_ANALYSIS("_l", " Left", " L"),
            GOTT_METERS("_l", " Left"),
            GOTT_ANALYSIS("_r", " Right", " R"),
            GOTT_METERS("_r", " Right"),
            GOTT_AMP_CURVE("_l", " Left"),
            GOTT_AMP_CURVE("_r", " Right"),
            PORTS_END
        };

        static const port_t sc_gott_compressor_lr_ports[] =
        {
            PORTS_STEREO_PLUGIN,
            PORTS_STEREO_SIDECHAIN,
            GOTT_SHM_LINK_STEREO,
            GOTT_SC_PREMIX,
            GOTT_SC_COMMON,
            COMBO("csel", "Channel selector", "Channel select", 0, gott_lr_selectors),
            GOTT_LINK("clink", "Left/Right controls link", "L/R link"),

            GOTT_BAND("_1l", " 1 Left", "1 L"),
            GOTT_BAND("_2l", " 2 Left", "2 L"),
            GOTT_BAND("_3l", " 3 Left", "3 L"),
            GOTT_BAND("_4l", " 4 Left", "4 L"),
            GOTT_BAND("_1r", " 1 Right", "1 R"),
            GOTT_BAND("_2r", " 2 Right", "2 R"),
            GOTT_BAND("_3r", " 3 Right", "3 R"),
            GOTT_BAND("_4r", " 4 Right", "4 R"),

            GOTT_BAND_METERS("_1l", " 1 Left"),
            GOTT_BAND_METERS("_2l", " 2 Left"),
            GOTT_BAND_METERS("_3l", " 3 Left"),
            GOTT_BAND_METERS("_4l", " 4 Left"),
            GOTT_BAND_METERS("_1r", " 1 Right"),
            GOTT_BAND_METERS("_2r", " 2 Right"),
            GOTT_BAND_METERS("_3r", " 3 Right"),
            GOTT_BAND_METERS("_4r", " 4 Right"),

            GOTT_ANALYSIS("_l", " Left", " L"),
            GOTT_METERS("_l", " Left"),
            GOTT_ANALYSIS("_r", " Right", " R"),
            GOTT_METERS("_r", " Right"),
            GOTT_AMP_CURVE("_l", " Left"),
            GOTT_AMP_CURVE("_r", " Right"),
            PORTS_END
        };

        static const port_t sc_gott_compressor_ms_ports[] =
        {
            PORTS_STEREO_PLUGIN,
            PORTS_STEREO_SIDECHAIN,
            GOTT_SHM_LINK_STEREO,
            GOTT_SC_PREMIX,
            GOTT_SC_COMMON,
            COMBO("csel", "Channel selector", "Channel select", 0, gott_ms_selectors),
            GOTT_LINK("clink", "Mid/Side controls link", "M/S link"),

            GOTT_BAND("_1m", " 1 Mid", "1 M"),
            GOTT_BAND("_2m", " 2 Mid", "2 M"),
            GOTT_BAND("_3m", " 3 Mid", "3 M"),
            GOTT_BAND("_4m", " 4 Mid", "4 M"),
            GOTT_BAND("_1s", " 1 Side", "1 S"),
            GOTT_BAND("_2s", " 2 Side", "2 S"),
            GOTT_BAND("_3s", " 3 Side", "3 S"),
            GOTT_BAND("_4s", " 4 Side", "4 S"),

            GOTT_BAND_METERS("_1m", " 1 Mid"),
            GOTT_BAND_METERS("_2m", " 2 Mid"),
            GOTT_BAND_METERS("_3m", " 3 Mid"),
            GOTT_BAND_METERS("_4m", " 4 Mid"),
            GOTT_BAND_METERS("_1s", " 1 Side"),
            GOTT_BAND_METERS("_2s", " 2 Side"),
            GOTT_BAND_METERS("_3s", " 3 Side"),
            GOTT_BAND_METERS("_4s", " 4 Side"),

            GOTT_ANALYSIS("_m", " Mid", " M"),
            GOTT_METERS("_l", " Left"),
            GOTT_ANALYSIS("_s", " Side", " S"),
            GOTT_METERS("_r", " Right"),
            GOTT_AMP_CURVE("_m", " Mid"),
            GOTT_AMP_CURVE("_s", " Side"),
            PORTS_END
        };

        static const int plugin_classes[]       = { C_COMPRESSOR, -1 };
        static const int clap_features_mono[]   = { CF_AUDIO_EFFECT, CF_COMPRESSOR, CF_MONO, -1 };
        static const int clap_features_stereo[] = { CF_AUDIO_EFFECT, CF_COMPRESSOR, CF_STEREO, -1 };

        const meta::bundle_t gott_compressor_bundle =
        {
            "gott_compressor",
            "GOTT Comressor",
            B_MB_DYNAMICS,
            "wb8QnU4yRF0",
            "Performs Grand Over-The-Top multiband compression of audio signal"
        };

        const plugin_t gott_compressor_mono =
        {
            "GOTT Kompressor Mono",
            "GOTT Compressor Mono",
            "GOTT Compressor Mono",
            "GC1M",
            &developers::v_sadovnikov,
            "gott_compressor_mono",
            {
                LSP_LV2_URI("gott_compressor_mono"),
                LSP_LV2UI_URI("gott_compressor_mono"),
                "ngcm",
                LSP_VST3_UID("gc1m    ngcm"),
                LSP_VST3UI_UID("gc1m    ngcm"),
                LSP_LADSPA_GOTT_COMPRESSOR_BASE + 0,
                LSP_LADSPA_URI("gott_compressor_mono"),
                LSP_CLAP_URI("gott_compressor_mono"),
                LSP_GST_UID("gott_compressor_mono"),
            },
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
            "GOTT Compressor Stereo",
            "GC1S",
            &developers::v_sadovnikov,
            "gott_compressor_stereo",
            {
                LSP_LV2_URI("gott_compressor_stereo"),
                LSP_LV2UI_URI("gott_compressor_stereo"),
                "ngcs",
                LSP_VST3_UID("gc1s    ngcs"),
                LSP_VST3UI_UID("gc1s    ngcs"),
                LSP_LADSPA_GOTT_COMPRESSOR_BASE + 1,
                LSP_LADSPA_URI("gott_compressor_stereo"),
                LSP_CLAP_URI("gott_compressor_stereo"),
                LSP_GST_UID("gott_compressor_stereo"),
            },
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
            "GOTT Compressor L/R",
            "GC1LR",
            &developers::v_sadovnikov,
            "gott_compressor_lr",
            {
                LSP_LV2_URI("gott_compressor_lr"),
                LSP_LV2UI_URI("gott_compressor_lr"),
                "ngcl",
                LSP_VST3_UID("gc1lr   ngcl"),
                LSP_VST3UI_UID("gc1lr   ngcl"),
                LSP_LADSPA_GOTT_COMPRESSOR_BASE + 2,
                LSP_LADSPA_URI("gott_compressor_lr"),
                LSP_CLAP_URI("gott_compressor_lr"),
                LSP_GST_UID("gott_compressor_lr"),
            },
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
            "GOTT Compressor M/S",
            "GC1MS",
            &developers::v_sadovnikov,
            "gott_compressor_ms",
            {
                LSP_LV2_URI("gott_compressor_ms"),
                LSP_LV2UI_URI("gott_compressor_ms"),
                "ngcM",
                LSP_VST3_UID("gc1ms   ngcM"),
                LSP_VST3UI_UID("gc1ms   ngcM"),
                LSP_LADSPA_GOTT_COMPRESSOR_BASE + 3,
                LSP_LADSPA_URI("gott_compressor_ms"),
                LSP_CLAP_URI("gott_compressor_ms"),
                LSP_GST_UID("gott_compressor_ms"),
            },
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
            "SC GOTT Compressor Mono",
            "SCGC1M",
            &developers::v_sadovnikov,
            "sc_gott_compressor_mono",
            {
                LSP_LV2_URI("sc_gott_compressor_mono"),
                LSP_LV2UI_URI("sc_gott_compressor_mono"),
                "sgcm",
                LSP_VST3_UID("scgc1m  sgcm"),
                LSP_VST3UI_UID("scgc1m  sgcm"),
                LSP_LADSPA_GOTT_COMPRESSOR_BASE + 4,
                LSP_LADSPA_URI("sc_gott_compressor_mono"),
                LSP_CLAP_URI("sc_gott_compressor_mono"),
                LSP_GST_UID("sc_gott_compressor_mono"),
            },
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
            "SC GOTT Compressor Stereo",
            "SCGC1S",
            &developers::v_sadovnikov,
            "sc_gott_compressor_stereo",
            {
                LSP_LV2_URI("sc_gott_compressor_stereo"),
                LSP_LV2UI_URI("sc_gott_compressor_stereo"),
                "sgcs",
                LSP_VST3_UID("scgc1s  sgcs"),
                LSP_VST3UI_UID("scgc1s  sgcs"),
                LSP_LADSPA_GOTT_COMPRESSOR_BASE + 5,
                LSP_LADSPA_URI("sc_gott_compressor_stereo"),
                LSP_CLAP_URI("sc_gott_compressor_stereo"),
                LSP_GST_UID("sc_gott_compressor_stereo"),
            },
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
            "SC GOTT Compressor L/R",
            "SCGC1LR",
            &developers::v_sadovnikov,
            "sc_gott_compressor_lr",
            {
                LSP_LV2_URI("sc_gott_compressor_lr"),
                LSP_LV2UI_URI("sc_gott_compressor_lr"),
                "sgcl",
                LSP_VST3_UID("scgc1lr sgcl"),
                LSP_VST3UI_UID("scgc1lr sgcl"),
                LSP_LADSPA_GOTT_COMPRESSOR_BASE + 6,
                LSP_LADSPA_URI("sc_gott_compressor_lr"),
                LSP_CLAP_URI("sc_gott_compressor_lr"),
                LSP_GST_UID("sc_gott_compressor_lr"),
            },
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
            "SC GOTT Compressor M/S",
            "SCGC1MS",
            &developers::v_sadovnikov,
            "sc_gott_compressor_ms",
            {
                LSP_LV2_URI("sc_gott_compressor_ms"),
                LSP_LV2UI_URI("sc_gott_compressor_ms"),
                "sgcM",
                LSP_VST3_UID("scgc1ms sgcM"),
                LSP_VST3UI_UID("scgc1ms sgcM"),
                LSP_LADSPA_GOTT_COMPRESSOR_BASE + 7,
                LSP_LADSPA_URI("sc_gott_compressor_ms"),
                LSP_CLAP_URI("sc_gott_compressor_ms"),
                LSP_GST_UID("sc_gott_compressor_ms"),
            },
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



