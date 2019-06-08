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

class reverser : public spa::plugin
{
	enum action_t
	{
		ac_record,
		ac_hold,
		ac_play_1,
		ac_play_2
	};

public:
	void run() override
	{
		switch(static_cast<int>(action))
		{
			case ac_play_1:
			case ac_play_2:
				{
					if(last_action != action)
					{
						play_head = record_head;
					}

					// last element of buffer we can go back to
					std::size_t last = (static_cast<unsigned>(samplecount)) <= play_head ? samplecount : 0;

					for(std::size_t i = play_head; i > last; --i)
					{
						out.left[i] = buffer[0][play_head - i];
						out.right[i] = buffer[1][play_head - i];
					}
					for(std::size_t i = last; i < play_head + samplecount; --i)
					{
						out.left[i] = out.right[i] = .0f;
					}
					play_head = last;
				}
				break;

			case ac_record:
			{
				if(last_action != action)
				{
					record_head = 0;
				}

				// note: record_head will always be <= buffer->size
				std::size_t last = std::min(static_cast<std::size_t>(static_cast<unsigned>(samplecount)),
									buffer->size() - record_head);

				for(std::size_t i = 0; i < last; ++i)
				{
					buffer[0][record_head + i] = in.left[i];
					buffer[1][record_head + i] = in.right[i];
				}
				// nothing more to record

				// output silence while recording
				for(std::size_t i = 0; i < samplecount; ++i)
				{
					out.left[i] = out.right[i] = .0f;
				}

				record_head += last;

			}
			[[clang::fallthrough]]; case ac_hold:
			{
				// output silence while recording/holding
				for(std::size_t i = 0; i < samplecount; ++i)
				{
					out.left[i] = out.right[i] = .0f;
				}
				break;
			}
		}

		last_action = action;
	}

public:	// FEATURE: make these private?
	~reverser() override {}
	reverser()
	{
		action.min = 0;
		action.max = 2;
		action.def = ac_hold;
		for(std::size_t i = 0; i < sizeof(buffer); ++i)
			std::fill(buffer[i].begin(), buffer[i].end(), .0f);
	}

private:

	void activate() override {}
	void deactivate() override {}

	std::vector<float> buffer[2];

	//bool is_recording = false;
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

class reverser_descriptor : public spa::descriptor
{
	SPA_DESCRIPTOR
public:
	reverser_descriptor() { properties.hard_rt_capable = 1; }

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

	reverser* instantiate() const override {
		return new reverser; }
};

extern "C" {
//! the main entry point
const spa::descriptor* spa_descriptor(unsigned long )
{
	// we have only one plugin, ignore the number
	return new reverser_descriptor;
}
}

