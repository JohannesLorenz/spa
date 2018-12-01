/*************************************************************************/
/* osc-plugin.cpp - a minimal OSC example plugin                         */
/* Copyright (C) 2018                                                    */
/* Johannes Lorenz (j.git$$$lorenz-ho.me, $$$=@)                         */
/*                                                                       */
/* This program is free software; you can redistribute it and/or modify  */
/* it under the terms of the GNU General Public License as published by  */
/* the Free Software Foundation; either version 3 of the License, or (at */
/* your option) any later version.                                       */
/* This program is distributed in the hope that it will be useful, but   */
/* WITHOUT ANY WARRANTY; without even the implied warranty of            */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU      */
/* General Public License for more details.                              */
/*                                                                       */
/* You should have received a copy of the GNU General Public License     */
/* along with this program; if not, write to the Free Software           */
/* Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110, USA  */
/*************************************************************************/

/**
	@file gain.cpp
	a gain plugin for practical use
*/

#include <cstring>
#include <iostream>
#include <vector>

#include <spa/audio.h>

class gain_effect : public spa::plugin
{
public:
	void run() override
	{
		for(unsigned i = 0; i < samplecount; ++i)
		{
			out.left[i] = gain * in.left[i];
			out.right[i] = gain * in.right[i];
		}
	}

public:	// FEATURE: make these private?
	~gain_effect() override {}
	gain_effect()
	{
		gain.min = 0.0f;
		gain.max = 1.0f;
		gain.step = 0.02f;
		gain.def = 1.0f;
	}

private:

	void activate() override {}
	void deactivate() override {}

	spa::audio::stereo::in in;
	spa::audio::stereo::out out;
	spa::audio::control_in<float> gain;
	spa::audio::samplecount samplecount;

	spa::port_ref_base& port(const char* path) override
	{
		switch(path[0])
		{
			case 'i': return in;
			case 'o': return out;
			case 's': return samplecount;
			case 'g': return gain;
			default: throw spa::port_not_found(path);
		}
	}
};

class gain_effect_descriptor : public spa::descriptor
{
	SPA_DESCRIPTOR
public:
	gain_effect_descriptor() { properties.hard_rt_capable = 1; }

	hoster_t hoster() const override { return hoster_t::github; }
	const char* organization_url() const override {
		return "JohannesLorenz"; /* TODO: create spa organisation? */ }
	const char* project_url() const override { return "spa"; }
	const char* label() const override { return "gain"; }

	const char* project() const override { return "spa"; }
	const char* name() const override { return "Gain"; }
	const char* authors() const override { return "Johannes Lorenz"; }

	const char* description_full() const override {
		return description_line(); }
	const char* description_line() const override {
		return "Gain plugin"; }

	license_type license() const override { return license_type::gpl_3_0; }

	struct port_names_t { const char** names; };
	spa::simple_vec<spa::simple_str> port_names() const override {
		return { "in", "out", "samplecount", "gain" };
	}

	gain_effect* instantiate() const override {
		return new gain_effect; }
};

extern "C" {
//! the main entry point
const spa::descriptor* spa_descriptor(unsigned long )
{
	// we have only one plugin, ignore the number
	return new gain_effect_descriptor;
}
}

