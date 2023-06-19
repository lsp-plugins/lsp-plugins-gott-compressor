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

#ifndef PRIVATE_META_GOTT_COMPRESSOR_H_
#define PRIVATE_META_GOTT_COMPRESSOR_H_

#include <lsp-plug.in/plug-fw/meta/types.h>
#include <lsp-plug.in/plug-fw/const.h>

#include <lsp-plug.in/dsp-units/const.h>
#include <lsp-plug.in/dsp-units/misc/windows.h>

namespace lsp
{
    //-------------------------------------------------------------------------
    // Plugin metadata
    namespace meta
    {
        typedef struct gott_compressor
        {
            static constexpr float  IN_GAIN_DFL             = GAIN_AMP_0_DB;
            static constexpr float  OUT_GAIN_DFL            = GAIN_AMP_0_DB;

            static constexpr float  REACT_TIME_MIN          = 0.000;
            static constexpr float  REACT_TIME_MAX          = 1.000;
            static constexpr float  REACT_TIME_DFL          = 0.200;
            static constexpr float  REACT_TIME_STEP         = 0.001;

            static constexpr float  ZOOM_MIN                = GAIN_AMP_M_18_DB;
            static constexpr float  ZOOM_MAX                = GAIN_AMP_0_DB;
            static constexpr float  ZOOM_DFL                = GAIN_AMP_0_DB;
            static constexpr float  ZOOM_STEP               = 0.0125f;

            static constexpr float  SPLIT1_MIN              = 20.0f;
            static constexpr float  SPLIT1_MAX              = 200.0f;
            static constexpr float  SPLIT1_DFL              = 120.0f;
            static constexpr float  SPLIT1_STEP             = 0.0005f;

            static constexpr float  SPLIT2_MIN              = SPLIT1_MAX + 25;
            static constexpr float  SPLIT2_MAX              = 4000.0f;
            static constexpr float  SPLIT2_DFL              = 1500.0f;
            static constexpr float  SPLIT2_STEP             = SPLIT1_STEP;

            static constexpr float  SPLIT3_MIN              = SPLIT2_MAX + 250;
            static constexpr float  SPLIT3_MAX              = 20000.0f;
            static constexpr float  SPLIT3_DFL              = 7000.0f;
            static constexpr float  SPLIT3_STEP             = SPLIT1_STEP;

            static constexpr size_t FFT_MESH_POINTS         = 640;
            static constexpr size_t FFT_RANK                = 13;
            static constexpr size_t FFT_ITEMS               = 1 << FFT_RANK;
            static constexpr size_t FILTER_MESH_POINTS      = FFT_MESH_POINTS + 2;
            static constexpr size_t FFT_WINDOW              = dspu::windows::HANN;
            static constexpr size_t CURVE_MESH_SIZE         = 256;
            static constexpr float  CURVE_DB_MIN            = -72;
            static constexpr float  CURVE_DB_MAX            = +24;

            static constexpr size_t BANDS_MAX               = 4;
            static constexpr size_t BANDS_DFL               = 3;
            static constexpr size_t SC_MODE_DFL             = 1;

            static constexpr float  THRESH_MIN_MIN          = GAIN_AMP_M_72_DB;
            static constexpr float  THRESH_MIN_MAX          = GAIN_AMP_0_DB;
            static constexpr float  THRESH_MIN_DFL          = GAIN_AMP_M_60_DB;
            static constexpr float  THRESH_MIN_STEP         = 0.01f;

            static constexpr float  THRESH_UP_MIN           = GAIN_AMP_M_72_DB;
            static constexpr float  THRESH_UP_MAX           = GAIN_AMP_0_DB;
            static constexpr float  THRESH_UP_DFL           = GAIN_AMP_M_36_DB;
            static constexpr float  THRESH_UP_STEP          = 0.01f;

            static constexpr float  THRESH_DOWN_MIN         = GAIN_AMP_M_72_DB;
            static constexpr float  THRESH_DOWN_MAX         = GAIN_AMP_0_DB;
            static constexpr float  THRESH_DOWN_DFL         = GAIN_AMP_M_12_DB;
            static constexpr float  THRESH_DOWN_STEP        = 0.01f;

            static constexpr float  RATIO_MIN               = 1.0f;
            static constexpr float  RATIO_MAX               = 100.0f;
            static constexpr float  RATIO_DFL               = 50.0f;
            static constexpr float  RATIO_STEP              = 0.0025f;

            static constexpr float  KNEE_MIN                = GAIN_AMP_M_24_DB;
            static constexpr float  KNEE_MAX                = GAIN_AMP_0_DB;
            static constexpr float  KNEE_DFL                = GAIN_AMP_M_6_DB;
            static constexpr float  KNEE_STEP               = 0.01f;

            static constexpr float  ATTACK_TIME_MIN         = 0.0f;
            static constexpr float  ATTACK_TIME_MAX         = 2000.0f;
            static constexpr float  ATTACK_TIME_DFL         = 20.0f;
            static constexpr float  ATTACK_TIME_STEP        = 0.0025f;

            static constexpr float  RELEASE_TIME_MIN        = 0.0f;
            static constexpr float  RELEASE_TIME_MAX        = 5000.0f;
            static constexpr float  RELEASE_TIME_DFL        = 100.0f;
            static constexpr float  RELEASE_TIME_STEP       = 0.0025f;

            static constexpr float  LOOKAHEAD_MIN           = 0.0f;
            static constexpr float  LOOKAHEAD_MAX           = 20.0f;
            static constexpr float  LOOKAHEAD_DFL           = 0.0f;
            static constexpr float  LOOKAHEAD_STEP          = 0.01f;

            static constexpr float  MAKEUP_MIN              = GAIN_AMP_M_60_DB;
            static constexpr float  MAKEUP_MAX              = GAIN_AMP_P_60_DB;
            static constexpr float  MAKEUP_DFL              = GAIN_AMP_0_DB;
            static constexpr float  MAKEUP_STEP             = 0.01f;

            static constexpr float  SC_REACTIVITY_MIN       = 0.000;    // Sidechain: Minimum reactivity [ms]
            static constexpr float  SC_REACTIVITY_MAX       = 250;      // Sidechain: Maximum reactivity [ms]
            static constexpr float  SC_REACTIVITY_DFL       = 10;       // Sidechain: Default reactivity [ms]
            static constexpr float  SC_REACTIVITY_STEP      = 0.025;    // Sidechain: Reactivity step

            static constexpr float  FREQ_BOOST_MIN          = 10.0f;
            static constexpr float  FREQ_BOOST_MAX          = 20000.0f;

            static constexpr size_t REFRESH_RATE            = 20;

            enum boost_t
            {
                FB_OFF,
                FB_BT_3DB,
                FB_MT_3DB,
                FB_BT_6DB,
                FB_MT_6DB,

                FB_DEFAULT              = FB_BT_3DB
            };
        } gott_compressor;

        // Plugin type metadata
        extern const plugin_t gott_compressor_mono;
        extern const plugin_t gott_compressor_stereo;
        extern const plugin_t gott_compressor_lr;
        extern const plugin_t gott_compressor_ms;
        extern const plugin_t sc_gott_compressor_mono;
        extern const plugin_t sc_gott_compressor_stereo;
        extern const plugin_t sc_gott_compressor_lr;
        extern const plugin_t sc_gott_compressor_ms;

    } /* namespace meta */
} /* namespace lsp */

#endif /* PRIVATE_META_GOTT_COMPRESSOR_H_ */
