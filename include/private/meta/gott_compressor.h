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

#ifndef PRIVATE_META_GOTT_COMPRESSOR_H_
#define PRIVATE_META_GOTT_COMPRESSOR_H_

#include <lsp-plug.in/plug-fw/meta/types.h>
#include <lsp-plug.in/plug-fw/const.h>
#include <lsp-plug.in/dsp-units/const.h>

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
            static constexpr float  SPLIT1_STEP             = 0.002f;

            static constexpr float  SPLIT2_MIN              = SPLIT1_MAX + 50;
            static constexpr float  SPLIT2_MAX              = 5000.0f;
            static constexpr float  SPLIT2_DFL              = 2000.0f;
            static constexpr float  SPLIT2_STEP             = 0.002f;

            static constexpr float  SPLIT3_MIN              = SPLIT2_MAX + 500;
            static constexpr float  SPLIT3_MAX              = 20000.0f;
            static constexpr float  SPLIT3_DFL              = 7000.0f;
            static constexpr float  SPLIT3_STEP             = 0.002f;

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
