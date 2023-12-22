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

#include <lsp-plug.in/plug-fw/ui.h>
#include <lsp-plug.in/stdlib/stdio.h>

#include <private/plugins/gott_compressor.h>
#include <private/ui/gott_compressor.h>

#include <lsp-plug.in/dsp-units/units.h>
#include <lsp-plug.in/stdlib/string.h>
#include <lsp-plug.in/stdlib/locale.h>

namespace lsp
{
    namespace plugui
    {
        //---------------------------------------------------------------------
        // Plugin UI factory
        static const meta::plugin_t *plugin_uis[] =
        {
            &meta::gott_compressor_mono,
            &meta::gott_compressor_stereo,
            &meta::gott_compressor_ms,
            &meta::gott_compressor_lr,
            &meta::sc_gott_compressor_mono,
            &meta::sc_gott_compressor_stereo,
            &meta::sc_gott_compressor_ms,
            &meta::sc_gott_compressor_lr
        };

        static ui::Module *ui_factory(const meta::plugin_t *meta)
        {
            return new gott_compressor(meta);
        }

        static ui::Factory factory(ui_factory, plugin_uis, 8);


        //---------------------------------------------------------------------
        static const char *note_names[] =
        {
            "c", "c#", "d", "d#", "e", "f", "f#", "g", "g#", "a", "a#", "b"
        };

        static const char *fmt_strings[] =
        {
            "%s_%d",
            NULL
        };

        static const char *fmt_strings_lr[] =
        {
            "%s_%dl",
            "%s_%dr",
            NULL
        };

        static const char *fmt_strings_ms[] =
        {
            "%s_%dm",
            "%s_%ds",
            NULL
        };


        //---------------------------------------------------------------------
        // Plugin UI factory

        gott_compressor::gott_compressor(const meta::plugin_t *meta):
            ui::Module(meta)
        {
            if ((!strcmp(meta->uid, meta::gott_compressor_lr.uid)) ||
                (!strcmp(meta->uid, meta::sc_gott_compressor_lr.uid)))
                fmtStrings      = fmt_strings_lr;
            else if ((!strcmp(meta->uid, meta::gott_compressor_ms.uid)) ||
                    (!strcmp(meta->uid, meta::sc_gott_compressor_ms.uid)))
                fmtStrings      = fmt_strings_ms;
            else
                fmtStrings      = fmt_strings;
        }

        gott_compressor::~gott_compressor()
        {
        }

        template <class T>
        T *gott_compressor::find_split_widget(const char *fmt, const char *base, size_t id)
        {
            char widget_id[64];
            ::snprintf(widget_id, sizeof(widget_id)/sizeof(char), fmt, base, int(id));
            return pWrapper->controller()->widgets()->get<T>(widget_id);
        }

        status_t gott_compressor::slot_split_mouse_in(tk::Widget *sender, void *ptr, void *data)
        {
            // Fetch parameters
            gott_compressor *ui = static_cast<gott_compressor *>(ptr);
            if (ui == NULL)
                return STATUS_BAD_STATE;

            split_t *s = ui->find_split_by_widget(sender);
            if (s != NULL)
                ui->on_split_mouse_in(s);

            return STATUS_OK;
        }

        status_t gott_compressor::slot_split_mouse_out(tk::Widget *sender, void *ptr, void *data)
        {
            // Fetch parameters
            gott_compressor *ui = static_cast<gott_compressor *>(ptr);
            if (ui == NULL)
                return STATUS_BAD_STATE;

            ui->on_split_mouse_out();

            return STATUS_OK;
        }

        gott_compressor::split_t *gott_compressor::find_split_by_widget(tk::Widget *widget)
        {
            for (size_t i=0, n=vSplits.size(); i<n; ++i)
            {
                split_t *d = vSplits.uget(i);
                if ((d->wMarker == widget) ||
                    (d->wNote == widget))
                    return d;
            }
            return NULL;
        }

        void gott_compressor::on_split_mouse_in(split_t *s)
        {
            if (s->wNote != NULL)
            {
                s->wNote->visibility()->set(true);
                update_split_note_text(s);
            }
        }

        void gott_compressor::on_split_mouse_out()
        {
            for (size_t i=0, n=vSplits.size(); i<n; ++i)
            {
                split_t *d = vSplits.uget(i);
                if (d->wNote != NULL)
                    d->wNote->visibility()->set(false);
            }
        }

        void gott_compressor::add_splits()
        {
            static const char *fmt = "%s%d";

            for (size_t port_id=1; port_id<meta::gott_compressor::BANDS_MAX; ++port_id)
            {
                split_t s;

                s.pUI           = this;

                s.wMarker       = find_split_widget<tk::GraphMarker>(fmt, "split_marker", port_id);
                s.wNote         = find_split_widget<tk::GraphText>(fmt, "split_note", port_id);

                s.pFreq         = find_port(fmt, "sf", port_id);

                if (s.wMarker != NULL)
                {
                    s.wMarker->slots()->bind(tk::SLOT_MOUSE_IN, slot_split_mouse_in, this);
                    s.wMarker->slots()->bind(tk::SLOT_MOUSE_OUT, slot_split_mouse_out, this);
                }

                if (s.pFreq != NULL)
                    s.pFreq->bind(this);

                vSplits.add(&s);
            }
        }

        void gott_compressor::update_split_note_text(split_t *s)
        {
            // Get the frequency
            float freq = (s->pFreq != NULL) ? s->pFreq->value() : -1.0f;
            if (freq < 0.0f)
            {
                s->wNote->visibility()->set(false);
                return;
            }

            // Update the note name displayed in the text
            {
                // Fill the parameters
                expr::Parameters params;
                tk::prop::String lc_string;
                LSPString text;
                lc_string.bind(s->wNote->style(), pDisplay->dictionary());
                SET_LOCALE_SCOPED(LC_NUMERIC, "C");

                // Frequency
                text.fmt_ascii("%.2f", freq);
                params.set_string("frequency", &text);

                // Split number
                params.set_int("id", vSplits.index_of(s) + 1);

                // Process split note
                float note_full = dspu::frequency_to_note(freq);
                if (note_full != dspu::NOTE_OUT_OF_RANGE)
                {
                    note_full += 0.5f;
                    ssize_t note_number = ssize_t(note_full);

                    // Note name
                    ssize_t note        = note_number % 12;
                    text.fmt_ascii("lists.notes.names.%s", note_names[note]);
                    lc_string.set(&text);
                    lc_string.format(&text);
                    params.set_string("note", &text);

                    // Octave number
                    ssize_t octave      = (note_number / 12) - 1;
                    params.set_int("octave", octave);

                    // Cents
                    ssize_t note_cents  = (note_full - float(note_number)) * 100 - 50;
                    if (note_cents < 0)
                        text.fmt_ascii(" - %02d", -note_cents);
                    else
                        text.fmt_ascii(" + %02d", note_cents);
                    params.set_string("cents", &text);

                    s->wNote->text()->set("lists.gott_comp.notes.full", &params);
                }
                else
                    s->wNote->text()->set("lists.gott_comp.notes.unknown", &params);
            }

        }

        status_t gott_compressor::post_init()
        {
            status_t res = ui::Module::post_init();
            if (res != STATUS_OK)
                return res;

            init_bands();
            // Add splits widgets
            add_splits();

            return STATUS_OK;
        }

        status_t gott_compressor::pre_destroy()
        {
            return ui::Module::pre_destroy();
        }

        void gott_compressor::init_bands()
        {
            for (const char * const *fmt = fmtStrings; *fmt != NULL; ++fmt)
            {
                for (size_t port_id=1; port_id<=meta::gott_compressor::BANDS_MAX; ++port_id)
                {
                    band_t b;

                    b.pUI           = this;

                    b.pLoThresh     = bind_port(*fmt, "tm", port_id);
                    b.pUpThresh     = bind_port(*fmt, "tu", port_id);
                    b.pDownThresh   = bind_port(*fmt, "td", port_id);

                    // Add band to list
                    vBands.add(&b);
                }
            }
        }

        void gott_compressor::notify(ui::IPort *port, size_t flags)
        {
            if (flags & ui::PORT_USER_EDIT)
            {
                band_t *b = find_band_by_port(port);
                if (b!= NULL)
                    process_band_port(b, port);
            }

            for (size_t i=0, n=vSplits.size(); i<n; ++i)
            {
                split_t *d = vSplits.uget(i);
                if (d->pFreq == port)
                    update_split_note_text(d);
            }
        }

        void gott_compressor::process_band_port(band_t *b, ui::IPort *port)
        {
            // We always need to keep the lo <= up <= down condition
            if (port == b->pLoThresh)
            {
                make_value_greater_eq(b->pUpThresh, b->pLoThresh);
                make_value_greater_eq(b->pDownThresh, b->pUpThresh);
            }
            else if (port == b->pUpThresh)
            {
                make_value_less_eq(b->pLoThresh, b->pUpThresh);
                make_value_greater_eq(b->pDownThresh, b->pUpThresh);
            }
            else if (port == b->pDownThresh)
            {
                make_value_less_eq(b->pUpThresh, b->pDownThresh);
                make_value_less_eq(b->pLoThresh, b->pUpThresh);
            }
        }

        void gott_compressor::make_value_greater_eq(ui::IPort *dst, ui::IPort *src)
        {
            if ((src == NULL) || (dst == NULL))
                return;

            float v = src->value();
            if (dst->value() >= v)
                return;

            dst->set_value(v);
            dst->notify_all(ui::PORT_USER_EDIT);
        }

        void gott_compressor::make_value_less_eq(ui::IPort *dst, ui::IPort *src)
        {
            if ((src == NULL) || (dst == NULL))
                return;

            float v = src->value();
            if (dst->value() <= v)
                return;

            dst->set_value(v);
            dst->notify_all(ui::PORT_USER_EDIT);
        }

        gott_compressor::band_t *gott_compressor::find_band_by_port(const ui::IPort *port)
        {
            if (port == NULL)
                return NULL;

            for (lltl::iterator<band_t> it = vBands.values(); it; ++it)
            {
                band_t *b       = *it;
                if ((b->pLoThresh == port) ||
                    (b->pDownThresh == port) ||
                    (b->pUpThresh == port))
                    return b;
            }

            return NULL;
        }

        ui::IPort *gott_compressor::find_port(const char *fmt, const char *base, size_t id)
        {
            char port_id[32];
            ::snprintf(port_id, sizeof(port_id)/sizeof(char), fmt, base, int(id));
            return pWrapper->port(port_id);
        }

        ui::IPort *gott_compressor::bind_port(const char *fmt, const char *base, size_t id)
        {
            ui::IPort *p = find_port(fmt, base, id);
            if (p != NULL)
                p->bind(this);
            return p;
        }

    } /* namespace plugui */
} /* namespace lsp */
