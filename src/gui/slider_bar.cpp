/*
** Copyright (C) 2004 Jesse Chappell <jesse@essej.net>
**  
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**  
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**  
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
**  
*/

#include <wx/wx.h>

#include <iostream>
#include <cmath>

#include "slider_bar.hpp"



using namespace SooperLooperGui;
using namespace std;

// Convert a value in dB's to a coefficent
#define DB_CO(g) ((g) > -144.0f ? powf(10.0f, (g) * 0.05f) : 0.0f)
#define CO_DB(v) (20.0f * log10f(v))

static inline double 
gain_to_slider_position (double g)
{
	if (g == 0) return 0;
	//return pow((6.0*log(g)/log(2.0)+192.0)/198.0, 8.0);
	return pow((6.0*log(g)/log(2.0)+198.0)/198.0, 8.0);

}

static inline double 
slider_position_to_gain (double pos)
{
	if (pos == 0) {
		return 0.0;
	}
	/* XXX Marcus writes: this doesn't seem right to me. but i don't have a better answer ... */
	//return pow (2.0,(sqrt(sqrt(sqrt(pos)))*198.0-192.0)/6.0);
	return pow (2.0,(sqrt(sqrt(sqrt(pos)))*198.0-198.0)/6.0);
}


enum {
	ID_TextCtrl = 8000

};

BEGIN_EVENT_TABLE(SliderBar, wxWindow)

	EVT_SIZE(SliderBar::OnSize)
	EVT_PAINT(SliderBar::OnPaint)
	EVT_MOUSE_EVENTS(SliderBar::OnMouseEvents)
	EVT_MOUSEWHEEL (SliderBar::OnMouseEvents)
	//EVT_TEXT (ID_TextCtrl, SliderBar::on_text_event)
	EVT_TEXT_ENTER (ID_TextCtrl, SliderBar::on_text_event)
	
END_EVENT_TABLE()

SliderBar::SliderBar(wxWindow * parent, wxWindowID id,  float lb, float ub, float val,  const wxPoint& pos, const wxSize& size)
	: wxWindow(parent, id, pos, size)
{
	_lower_bound = lb;
	_upper_bound = ub;
	_value = val;
	_backing_store = 0;
	_dragging = false;
	_decimal_digits = 1;
	_text_ctrl = 0;
	_ignoretext = false;
	
	_bgcolor.Set(30,30,30);
	_bgbrush.SetColour (_bgcolor);
	SetBackgroundColour (_bgcolor);
	SetThemeEnabled(false);

	_valuecolor.Set(244, 255, 178);

	_textcolor = *wxWHITE;
	_barcolor.Set(14, 50, 89);
	_overbarcolor.Set(20, 65, 104);
	_barbrush.SetColour(_barcolor);
	
	_bordercolor.Set(67, 83, 103);
	_borderpen.SetColour(_bordercolor);
	_borderpen.SetWidth(1);
	_borderbrush.SetColour(_bgcolor);

	_linebrush.SetColour(wxColour(154, 245, 168));
	
	_bar_style = FromLeftStyle;
	_scale_mode = LinearMode;
	_snap_mode = NoSnap;

}

SliderBar::~SliderBar()
{

}


void
SliderBar::set_style (BarStyle md)
{
	if (md != _bar_style) {
		_bar_style = md;
		Refresh(false);
	}
}

void
SliderBar::set_snap_mode (SnapMode md)
{
	if (md != _snap_mode) {
		_snap_mode = md;
	}
}

void
SliderBar::set_scale_mode (ScaleMode mode)
{
	if (mode != _scale_mode) {
		_scale_mode = mode;
		update_value_str();
		Refresh(false);
	}
}

void
SliderBar::set_bounds (float lb, float ub)
{
	if (_lower_bound != lb || _upper_bound != ub) {
		_lower_bound = lb;
		_upper_bound = ub;

		// force value to within
		if (_value < _lower_bound) {
			_value = _lower_bound;
			update_value_str();
			Refresh(false);
		}
		else if (_value > _upper_bound) {
			_value = _upper_bound;
			update_value_str();
			Refresh(false);
		}
	}
}

void
SliderBar::set_label (const wxString & label)
{
	_label_str = label;
	Refresh(false);	
}

void
SliderBar::set_units (const wxString & units)
{
	_units_str = units;
	update_value_str();
	Refresh(false);	
}

void
SliderBar::set_decimal_digits (int val)
{
	_decimal_digits = val;
	update_value_str();
	Refresh(false);	
}

void
SliderBar::set_value (float val)
{
	float newval = val;
	
	if (_scale_mode == ZeroGainMode) {
		newval = gain_to_slider_position (val);
	}

// 	if (_snap_mode == IntegerSnap) {
// 		newval = nearbyintf (newval);
// 	}

	newval = min (newval, _upper_bound);
	newval = max (newval, _lower_bound);
	
	if (newval != _value) {
		_value = newval;
		update_value_str();
		Refresh(false);
	}
}

float
SliderBar::get_value ()
{
	if (_scale_mode == ZeroGainMode) {
		return slider_position_to_gain(_value);
	}
	else {
		return _value;
	}
}
	

void
SliderBar::update_value_str()
{
	if (_scale_mode == ZeroGainMode) {
		float gain = slider_position_to_gain(_value);
		if (gain == 0) {
			_value_str.Printf(wxT("-inf %s"), _units_str.c_str());
		}
		else {
			_value_str.Printf(wxT("%.*f %s"), _decimal_digits, CO_DB(gain),  _units_str.c_str());
		}
	}
	else {
		_value_str.Printf(wxT("%.*f %s"), _decimal_digits, _value, _units_str.c_str());
	}
}


wxString SliderBar::get_precise_value_str()
{
	wxString valstr;
	
	if (_scale_mode == ZeroGainMode) {
		float gain = slider_position_to_gain(_value);
		if (gain == 0) {
			valstr.Printf(wxT("-inf"));
		}
		else if (_snap_mode == IntegerSnap) {
			valstr.Printf(wxT("%g"), CO_DB(gain));
		}
		else {
			valstr.Printf(wxT("%.8f"), CO_DB(gain));
		}
	}
	else {
		if (_snap_mode == IntegerSnap) {
			valstr.Printf(wxT("%g"), _value);
		}
		else {
			valstr.Printf(wxT("%.8f"), _value);
		}
	}

	return valstr;
}


void SliderBar::set_bg_color (const wxColour & col)
{
	_bgcolor = col;
	_bgbrush.SetColour (col);
	SetBackgroundColour (col);
	Refresh(false);
}

void SliderBar::set_text_color (const wxColour & col)
{
	_textcolor = col;
	Refresh(false);
}

void SliderBar::set_border_color (const wxColour & col)
{
	_bordercolor = col;
	_borderbrush.SetColour (col);
	Refresh(false);
}

void SliderBar::set_bar_color (const wxColour & col)
{
	_barcolor = col;
	_barbrush.SetColour (col);
	Refresh(false);
}

void
SliderBar::OnSize(wxSizeEvent & event)
{
	GetClientSize(&_width, &_height);

	_val_scale = (_upper_bound - _lower_bound) / (_width);

	_memdc.SelectObject (wxNullBitmap);
	if (_backing_store) {
		delete _backing_store;
	}
	_backing_store = new wxBitmap(_width, _height);

 	_memdc.SelectObject(*_backing_store);
	_memdc.SetFont(GetFont());
	
	event.Skip();
}

void SliderBar::OnPaint(wxPaintEvent & event)
{
 	wxPaintDC pdc(this);

	if (!_backing_store) {
		return;
	}
	
 	draw_area(_memdc);

 	pdc.Blit(0, 0, _width, _height, &_memdc, 0, 0);
}


void
SliderBar::OnMouseEvents (wxMouseEvent &ev)
{
	if (!IsEnabled()) {
		ev.Skip();
		return;
	}

	if (ev.Entering() && !_dragging) {
		_barbrush.SetColour(_overbarcolor);
		Refresh(false);
	}
	else if (ev.Leaving() && !_dragging) {
		_barbrush.SetColour(_barcolor);
		Refresh(false);
	}
	

	
	if (ev.Dragging() && _dragging)
	{
		int delta = ev.GetX() - _last_x;
		float fdelta = delta * _val_scale;

		if (ev.ControlDown()) {
			fdelta *= 0.5f;
			if (ev.ShiftDown()) {
				fdelta *= 0.5f;
			}
		}

		float newval = _value + fdelta;

		if (newval > _upper_bound) {
			newval = _upper_bound;
		}
		else if (newval < _lower_bound) {
			newval = _lower_bound;
		}
		//cerr << "dragging: " << delta << "  " << fdelta << "  "  << newval << endl;

		if (_snap_mode == IntegerSnap) {
			newval = nearbyintf (newval);
		}
		
		if (newval != _value) {
			_value = newval;
			
			value_changed (get_value()); // emit

			update_value_str();
			Refresh(false);
			//cerr << "new val is: " << _value << endl;
		}

		
		_last_x = ev.GetX();
	}
	else if (ev.Moving()) {
		// do nothing
	}
	else if (ev.GetEventType() == wxEVT_MOUSEWHEEL)
	{
		// don't get the events right now
		
		float fscale = 0.05f * (ev.ControlDown() ? 0.5f: 1.0f);
		float newval;
		
		if (ev.GetWheelRotation() > 0) {
			newval = _value + (_upper_bound - _lower_bound) * fscale;			
		}
		else {
			newval = _value - (_upper_bound - _lower_bound) * fscale;			
		}

		if (newval > _upper_bound) {
			newval = _upper_bound;
		}
		else if (newval < _lower_bound) {
			newval = _lower_bound;
		}

		if (_snap_mode == IntegerSnap) {
			newval = nearbyintf (newval);
		}
		
		_value = newval;
		
		value_changed (get_value()); // emit
		
		update_value_str();
		Refresh(false);
	}
	else if (ev.ButtonDown())
	{
		CaptureMouse();
		_dragging = true;
		_last_x = ev.GetX();

		if (ev.MiddleDown()) {
			// set immediately
			float newval = (ev.GetX() * _val_scale) + _lower_bound;

			if (_snap_mode == IntegerSnap) {
				newval = nearbyintf (newval);
			}

			_value = newval;
			
			value_changed (get_value()); // emit

			update_value_str();
			Refresh(false);
			
		}
	}
	else if (ev.ButtonUp())
	{
		_dragging = false;
		ReleaseMouse();

		if (ev.GetX() >= _width || ev.GetX() < 0
		    || ev.GetY() < 0 || ev.GetY() > _height) {
			_barbrush.SetColour(_barcolor);
			Refresh(false);
		}
		else {
			_barbrush.SetColour(_overbarcolor);
			Refresh(false);
		}
		
	}
	else if (ev.ButtonDClick()) {
		// todo editor
		show_text_ctrl ();
	}

	ev.Skip();
}


void SliderBar::draw_area(wxDC & dc)
{
	wxCoord w,h;
	int pixw;

	dc.SetBackground(_bgbrush);
	dc.Clear();

	dc.SetBrush(_borderbrush);
	dc.SetPen(_borderpen);
	dc.DrawRectangle (0, 0, _width, _height);
	
	
	dc.SetPen(*wxTRANSPARENT_PEN);
	dc.SetBrush(_barbrush);

	if (_bar_style == FromLeftStyle)
	{
		pixw = (int) ((_value - _lower_bound) / _val_scale);
		dc.DrawRectangle (1, 1, pixw-1, _height-2);
	}
	else if (_bar_style == FromRightStyle)
	{
		pixw = (int) ((_upper_bound - _value) / _val_scale);
		dc.DrawRectangle (pixw, 1, _width - pixw - 1, _height-2);
	}

	dc.SetBrush(_linebrush);
	pixw = (int) ((_value - _lower_bound) / _val_scale);
	dc.DrawRectangle (pixw - 1, 1, 2, _height-2);
	


	
	dc.SetTextForeground(_textcolor);
	dc.GetTextExtent(_label_str, &w, &h);
	dc.DrawText (_label_str, 3, _height - h - 3);

	dc.SetTextForeground(_valuecolor);
	dc.GetTextExtent(_value_str, &w, &h);
	dc.DrawText (_value_str, _width - w - 3, _height - h - 3);
	

}


void SliderBar::show_text_ctrl ()
{
	wxString valstr = get_precise_value_str();
	
	if (!_text_ctrl) {
		_text_ctrl = new HidingTextCtrl(this, ID_TextCtrl, valstr, wxPoint(1,1), wxSize(_width - 2, _height - 2),
						wxTE_PROCESS_ENTER|wxTE_RIGHT);
		_text_ctrl->SetName (wxT("KeyAware"));
		_text_ctrl->SetFont(GetFont());
	}

	_text_ctrl->SetValue (valstr);
	
	_text_ctrl->SetSelection (-1, -1);
	
	_text_ctrl->SetSize (_width - 2, _height - 2);
	_text_ctrl->Show(true);
	_text_ctrl->SetFocus();
}

void SliderBar::hide_text_ctrl ()
{
	if (_text_ctrl && _text_ctrl->IsShown()) {
		_text_ctrl->Show(false);

		SetFocus();
	}
}

void SliderBar::on_text_event (wxCommandEvent &ev)
{
	if (ev.GetEventType() == wxEVT_COMMAND_TEXT_ENTER) {
		
		// commit change
		bool good = false;
		bool neginf = false;
		double newval = 0.0;
		
		if (_scale_mode == ZeroGainMode && _text_ctrl->GetValue().Strip(wxString::both) == wxT("-inf")) {
			newval = 0.0;
			good = neginf = true;
		}
		else if (_text_ctrl->GetValue().ToDouble(&newval)) {
			good = true;
		}
		
		if (good) {
			if (_scale_mode == ZeroGainMode && !neginf) {
				newval = DB_CO(newval);
			}

			set_value ((float) newval);
			value_changed (get_value()); // emit
		}
		
		hide_text_ctrl();
	}
}

BEGIN_EVENT_TABLE(SliderBar::HidingTextCtrl, wxTextCtrl)
	EVT_KILL_FOCUS (SliderBar::HidingTextCtrl::on_focus_event)
END_EVENT_TABLE()
	
void SliderBar::HidingTextCtrl::on_focus_event (wxFocusEvent & ev)
{
	if (ev.GetEventType() == wxEVT_KILL_FOCUS) {
		Show(false);
	}
}