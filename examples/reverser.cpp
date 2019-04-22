/*************************************************************************/
/* reverser.cpp - a reverser plugin                                      */
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
	@file reverser.cpp
	a reverser plugin
*/

#include <cstring>
#include <iostream>
#include <vector>

#include <spa/audio.h>

class reverser_effect : public spa::plugin
{
public:
	void run() override
	{
		if(action == 2) // play
		{
			if(last_action != action)
				play_head = 0;
			std::size_t end = std::max(play_head +
					samplecount, buffer->size());
			for(std::size_t i = play_head; i < end; ++i)
			{
				out.left[i] = buffer[0][play_head + i];
				out.right[i] = buffer[1][play_head + i];
			}
			for(std::size_t i = end; i < samplecount; ++i)
			{
				out.left[i] = out.right[i] = .0f;
			}
		}
		else
		{
			if(last_action != action)
			{
				is_recording = !is_recording;
				record_head = 0;
			}

			if(is_recording)
			{
				std::size_t end = std::max(record_head +
					samplecount, buffer->size());
				for(std::size_t i = record_head; i < end; ++i)
				{
					buffer[0][record_head + i] = in.left[i];
					buffer[1][record_head + i] = in.right[i];
					out.left[i] = out.right[i] = .0f;
				}
				for(std::size_t i = end; i < samplecount; ++i)
				{
					out.left[i] = out.right[i] = .0f;
				}
				record_head = end;
			}
		}

		last_action = action;
	}

public:	// FEATURE: make these private?
	~reverser_effect() override {}
	reverser_effect()
	{
		action.min = 0;
		action.max = 2;
		action.def = 0;
	}

private:

	void activate() override {}
	void deactivate() override {}

	std::vector<float> buffer[2];

	bool is_recording = false;
	int last_action = 2;
	std::size_t record_head = 0, play_head = 0;

	spa::audio::stereo::in in;
	spa::audio::stereo::out out;
	spa::audio::control_in<int> action;
	spa::audio::samplecount samplecount;

	spa::port_ref_base& port(const char* path) override
	{
		switch(path[0])
		{
			case 'i': return in;
			case 'o': return out;
			case 's': return samplecount;
			case 'a': return action;
			default: throw spa::port_not_found(path);
		}
	}
};

class reverser_effect_descriptor : public spa::descriptor
{
	SPA_DESCRIPTOR
public:
	reverser_effect_descriptor() { properties.hard_rt_capable = 1; }

	hoster_t hoster() const override { return hoster_t::github; }
	const char* organization_url() const override {
		return "JohannesLorenz"; /* TODO: create spa organisation? */ }
	const char* project_url() const override { return "spa"; }
	const char* label() const override { return "reverser"; }

	const char* project() const override { return "spa"; }
	const char* name() const override { return "Reverser"; }
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

	reverser_effect* instantiate() const override {
		return new reverser_effect; }
};

extern "C" {
//! the main entry point
const spa::descriptor* spa_descriptor(unsigned long )
{
	// we have only one plugin, ignore the number
	return new reverser_effect_descriptor;
}
}

