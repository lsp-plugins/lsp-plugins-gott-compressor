/*
 * Copyright (C) 2023 Linux Studio Plugins Project <https://lsp-plug.in/>
 *           (C) 2023 Vladimir Sadovnikov <sadko4u@gmail.com>
 *
 * This file is part of lsp-plugins-gott-compressor
 * Created on: 19 июн. 2023 г.
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

#ifndef PRIVATE_UI_GOTT_COMPRESSOR_H_
#define PRIVATE_UI_GOTT_COMPRESSOR_H_

#include <lsp-plug.in/plug-fw/ui.h>
#include <lsp-plug.in/lltl/darray.h>

namespace lsp
{
    namespace plugui
    {
        /**
         * UI for GOTT Compressor plugin series
         */
        class gott_compressor: public ui::Module, public ui::IPortListener
        {
            protected:
                typedef struct band_t
                {
                    gott_compressor    *pUI;

                    ui::IPort          *pLoThresh;
                    ui::IPort          *pUpThresh;
                    ui::IPort          *pDownThresh;
                } filter_t;

            protected:
                const char * const     *fmtStrings;
                lltl::darray<filter_t>  vBands;

            protected:
                static void         make_value_greater_eq(ui::IPort *dst, ui::IPort *src);
                static void         make_value_less_eq(ui::IPort *dst, ui::IPort *src);

            protected:
                band_t             *find_band_by_port(const ui::IPort *port);
                void                init_bands();
                ui::IPort          *find_port(const char *fmt, const char *base, size_t id);
                ui::IPort          *bind_port(const char *fmt, const char *base, size_t id);
                void                process_band_port(band_t *band, ui::IPort *port);

            public:
                explicit            gott_compressor(const meta::plugin_t *meta);
                virtual            ~gott_compressor() override;

                virtual status_t    post_init() override;
                virtual status_t    pre_destroy() override;

            public:
                virtual void        notify(ui::IPort *port, size_t flags) override;
        };
    } /* namespace plugui */
} /* namespace lsp */




#endif /* PRIVATE_UI_GOTT_COMPRESSOR_H_ */
