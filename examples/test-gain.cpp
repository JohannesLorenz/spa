/*************************************************************************/
/* osc-plugin.cpp - a minimal OSC example plugin                         */
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
	@file test-gain.cpp
	a test plugin for the gain plugin
*/

#include <cstring>
#include <iostream>
#include <vector>

#include <spa/audio.h>

class test_gain : public spa::plugin
{
	int time = 0;
	int time_end = 10;
public:
	void run() override
	{
		switch(static_cast<spa::test_mode>(test_mode))
		{
			case spa::test_mode::challenge:
				std::cout << "chal" << std::endl;
				for(unsigned i = 0; i < samplecount; ++i)
				{
					in.right[i] = (samplecount/3.f)/static_cast<float>(i);
					//out.right[i] = gain_port * in.right[i];
				}
				memset(in.left, '0', samplecount);

				gain_port.set(1.f * (time_end / (1+time)));
				break;
			case spa::test_mode::evaluate:
				std::cout << "eval" << std::endl;
				for(unsigned i = 0; i < samplecount; ++i)
				{
					if(in.right[i] != ((samplecount/2.f)/static_cast<float>(i)) * gain_port
						||
						out.right[i] != .0f)
					{
						test_feedback.set(spa::test_feedback::stop_failure);
					}
				}

				if(++time > time_end) // TODO: test_feedback might have failed already => fail func?
				{
					test_feedback.set(spa::test_feedback::stop_ok);
				}
				break;
			case spa::test_mode::none:
				// how can this even happen?
				test_feedback.set(spa::test_feedback::stop_failure);
				break;
		}
		// TODO: also change gain port
	}

public:	// FEATURE: make these private?
	~test_gain() override {}
	test_gain()
	{
		gain_port.min = 0.0f;
		gain_port.max = 1.0f;
//		gain_port.step = 0.02f;
//		gain_port.def = 1.0f;
	}

private:

	void activate() override {
		samplecount.set(10);
	}
	void deactivate() override {}

	spa::audio::stereo::out in;
	spa::audio::stereo::in out;
	spa::audio::control_out<float> gain_port;
	spa::audio::control_out<unsigned> samplecount;
	spa::test_mode_port test_mode;
	spa::test_feedback_port test_feedback;

	spa::port_ref_base& port(const char* path) override
	{
		switch(path[0])
		{
			using bas = spa::port_ref_base;
			case 'i': return in;
			case 'o': return out;
			case 's': return samplecount;
			case 'g': return gain_port;
			case 't': return (path[5] == 'm') ? (bas&)test_mode : (bas&)test_feedback;
			default: throw spa::port_not_found(path);
		}
	}
};

class test_gain_descriptor : public spa::descriptor
{
	SPA_DESCRIPTOR
public:
	test_gain_descriptor() { properties.hard_rt_capable = 1; }

	hoster_t hoster() const override { return hoster_t::github; }
	const char* organization_url() const override {
		return "JohannesLorenz"; /* TODO: create spa organisation? */ }
	const char* project_url() const override { return "spa"; }
	const char* label() const override { return "gain-test"; }

	const char* project() const override { return "spa"; }
	const char* name() const override { return "Gain test"; }
	const char* authors() const override { return "Johannes Lorenz"; }

	const char* description_full() const override {
		return description_line(); }
	const char* description_line() const override {
		return "Gain test plugin"; }

	license_type license() const override { return license_type::gpl_3_0; }

	struct port_names_t { const char** names; };
	spa::simple_vec<spa::simple_str> port_names() const override
	{
		return { "in", "out", "samplecount", "test-mode", "test-feedback", "gain" };
	}

	test_gain* instantiate() const override {
		return new test_gain; }
};

extern "C" {
//! the main entry point
const spa::descriptor* spa_descriptor(unsigned long )
{
	// we have only one plugin, ignore the number
	return new test_gain_descriptor;
}
}

