/* CairoTSPlot.hh - a time-series plot widget
 *
 * Copyright (C) Javier Lopez-Gomez, 2016
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef CAIROTSPLOT_HH
#define CAIROTSPLOT_HH

#include <atomic>
#include <cairomm/context.h>
#include <chrono>
#include <cmath>
#include <functional>
#include <gtkmm.h>
#include <string>

#define CAIROTSPLOT_FONT_SIZE 14
#define MARGIN_LEFT 4
#define MARGIN_TOP 40
#define MARGIN_RIGHT 32
#define MARGIN_BOTTOM 32
#define _RGBA_DEFAULT                                                                                                  \
  Gdk::RGBA { "#2e7db3" }
#define _RGBA_ALERT                                                                                                    \
  Gdk::RGBA { "#cc0d29" }
//  NUM_POINTS should be a power-of-2
#define NUM_POINTS 512

class CairoTSPlot : public Gtk::DrawingArea {
protected:
  typedef std::function<double(void *, bool &)> sample_fn_t;
  struct value_struct {
    double value;
    bool is_alerted;
    bool has_timepoint;
    std::chrono::time_point<std::chrono::steady_clock> timepoint;
    value_struct() : value(HUGE_VAL), is_alerted(FALSE), has_timepoint(FALSE) {}
  };

  std::string _text;
  sample_fn_t _sample_fn;
  std::unique_ptr<value_struct[]> _data;
  std::atomic_uint _data_head;
  std::atomic_bool _data_changed;
  std::chrono::time_point<std::chrono::steady_clock> _lasttimepoint;
  double _value_min, _value_max, _tick_step, _data_height;
  Gdk::RGBA _text_rgba;
  Cairo::Matrix _transform_matrix;
  Cairo::RefPtr<Cairo::Surface> _background;

  void draw_background(void);

  bool on_draw(const Cairo::RefPtr<Cairo::Context> &context) override;
  void on_size_allocate(Gtk::Allocation &allocation) override {
    Gtk::Widget::on_size_allocate(allocation);
    _data_height = allocation.get_height() - MARGIN_TOP - MARGIN_BOTTOM;
    if (_background)
      draw_background();
  }

public:
  /** Construct a CairoTSPlot object
   * @param text Text rendered above the plot
   * @param fn std::function<double(void *, bool&)> that returns the
   *     next value; the second argument is used to change the _alert
   *     member of the current 'struct value_struct'
   * @param _m Minimum value of any sample
   * @param _M Maximum value of any sample
   * @param step Draw vertical axis scale using @a step increments
   */
  CairoTSPlot(std::string text, sample_fn_t fn, double _m, double _M, double step = 0)
      : _text(text), _sample_fn(fn), _data(new value_struct[NUM_POINTS]), _data_head(0), _data_changed(FALSE),
        _value_min(_m), _value_max(_M), _tick_step(step), _transform_matrix(Cairo::identity_matrix()) {
    get_style_context()->lookup_color("theme_text_color", _text_rgba);
  }
  CairoTSPlot(const CairoTSPlot &_o)
      : CairoTSPlot(_o._text, _o._sample_fn, _o._value_min, _o._value_max, _o._tick_step) {}
  virtual ~CairoTSPlot() {}

  void set_transform_matrix(Cairo::Matrix &_m) {
    _transform_matrix = _m;
    queue_draw();
  }

#define _circbuf_get(_b, _i) (_b[(_i) & (NUM_POINTS - 1)])
  /** Call the @a fn function (constructor argument) and rotate _data;
   * _data[_data_head] will be the new value.
   */
  void sample(void *arg,
              std::chrono::time_point<std::chrono::steady_clock> timepoint = std::chrono::steady_clock::now()) {
    std::chrono::duration<double> _diff = timepoint - _lasttimepoint;
    bool hastimepoint = (_diff.count() >= 5.0f);
    struct value_struct &_s = _circbuf_get(_data, _data_head.fetch_add(1));

    _s.value = _sample_fn(arg, _s.is_alerted);
    if ((_s.has_timepoint = hastimepoint))
      _lasttimepoint = _s.timepoint = timepoint;
    _data_changed = TRUE;
  }

  void update() {
    if (_data_changed) {
      _data_changed = FALSE;
      get_window()->invalidate_rect(Gdk::Rectangle(0, 0, get_allocation().get_width(), get_allocation().get_height()),
                                    FALSE);
    }
  }

protected:
  double yoffset_of(double value) {
    return static_cast<int>((value - _value_min) / (_value_max - _value_min) * _data_height);
  }
};

#endif /* CAIROTSPLOT_HH */
