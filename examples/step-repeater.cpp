/*************************************************************************/
/* step-repeater.cpp - a step-wise repeater                              */
/* Copyright (C) 2019                                                    */
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
	@file step-repeater.cpp
	a step-wise repeater plugin
*/

#include <cstring>
#include <iostream>
#include <vector>

#include <spa/audio.h>

class step_repeater : public spa::plugin
{
public:
	void run() override
	{
	}

public:	// FEATURE: make these private?
	~step_repeater() override {}
	step_repeater()
	{

	}

private:

	void activate() override {}
	void deactivate() override {}

	std::vector<float> buffer[2];

	spa::audio::stereo::in in;
	spa::audio::stereo::out out;
	spa::audio::samplecount samplecount;
	spa::audio::measure<int> repeat_rate;
	spa::audio::control_in<float>

	spa::port_ref_base& port(const char* path) override
	{
		switch(path[0])
		{
			case 'i': return in;
			case 'o': return out;
			case 's': return samplecount;
			default: throw spa::port_not_found(path);
		}
	}
};

class step_repeater_descriptor : public spa::descriptor
{
	SPA_DESCRIPTOR
public:
	step_repeater_descriptor() { properties.hard_rt_capable = 1; }

	hoster_t hoster() const override { return hoster_t::github; }
	const char* organization_url() const override {
		return "JohannesLorenz"; /* TODO: create spa organisation? */ }
	const char* project_url() const override { return "spa"; }
	const char* label() const override { return "step-repeater"; }

	const char* project() const override { return "spa"; }
	const char* name() const override { return "Step Repeater"; }
	const char* authors() const override { return "Johannes Lorenz"; }

	const char* description_full() const override {
		return description_line(); }
	const char* description_line() const override {
		return "Reverser plugin"; }

	license_type license() const override { return license_type::gpl_3_0; }

	struct port_names_t { const char** names; };
	spa::simple_vec<spa::simple_str> port_names() const override {
		return { "in", "out", "samplecount", "action" };
	}

	step_repeater* instantiate() const override {
		return new step_repeater; }
};

extern "C" {
//! the main entry point
const spa::descriptor* spa_descriptor(unsigned long )
{
	// we have only one plugin, ignore the number
	return new step_repeater_descriptor;
}
}

