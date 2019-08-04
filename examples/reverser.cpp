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

#include <cassert>
#include <cmath>
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

	static const constexpr std::size_t buffer_alloc_sz = 44100 * 8;

	//! Save the next bytes after the current sample
	//! In case the ring buffer is flushed, we still have enough samples
	//! to fade out
	//std::vector<std::vector<float>> next_bytes[2];

	constexpr const static std::size_t fade_calc_length = 256; // works great in zyn, so...
	constexpr const static float fadein_adjustment = 20.0f;

//	std::size_t fade_length_front, fade_length_end;

	//std::size_t blend_size, blend_progress;

	const std::size_t minimal_action_change = 256;
	std::size_t last_action_change = 0; //! how many samples is last action change ago?

	//! multiply with an increasing S curve
	//! @param smps begin of array, must have fadein_calc_length elements
	//! @note copied from zyn ...
	std::size_t fadein_length(const float *smps) const
	{
		int zerocrossings = 0;
		for(std::size_t i = 1; i < fade_calc_length; ++i)
		if((smps[i - 1] < 0.0f) && (smps[i] > 0.0f))
			zerocrossings++;  // these are only the possitive crossings

		float tmp = (fade_calc_length - 1.0f) / (zerocrossings + 1) / 3.0f;
		if(tmp < 8.0f)
			tmp = 8.0f;
		tmp *= fadein_adjustment;

		std::size_t n = static_cast<std::size_t>(tmp);
		if(n > fade_calc_length)
			n = fade_calc_length;
		return n;
	}

	void apply_fade(float* smps, std::size_t length)
	{
		for(std::size_t i = 0; i < length; ++i)
		{ //fade-in
			float tmp = 0.5f -
				cosf(static_cast<float>(i) /
					static_cast<float>(length) *
					static_cast<float>(M_PI))
					* 0.5f;
			smps[i] *= tmp;
		}
	}


	//! multiply with an increasing S curve
	//! @note copied from zyn ...
	static void fadein(float *smps, std::size_t begin, std::size_t size)
	{
		float delta = size / static_cast<float>(fade_calc_length);
		float initial = begin / static_cast<float>(fade_calc_length);

		for (std::size_t i = 0; i < size; ++i)
			smps[i] = initial + i * delta;
#if 0
		int zerocrossings = 0;
		for(std::size_t i = start; i < end; ++i)
		if((smps[i - 1] < 0.0f) && (smps[i] > 0.0f))
			zerocrossings++;  // these are only the possitive crossings

		float tmp = (samplecount - 1.0f) / (zerocrossings + 1) / 3.0f;
		if(tmp < 8.0f)
			tmp = 8.0f;
		tmp *= fadein_adjustment;

		std::size_t n = static_cast<std::size_t>(tmp);
		if(n > samplecount)
			n = samplecount;
		for(std::size_t i = 0; i < n; ++i)
		{ //fade-in
			float tmp = 0.5f - cosf((float)i / (float) n * M_PI) * 0.5f;
			smps[i] *= tmp;
		}
#endif
	}

	static void fadeout(float *smps, std::size_t begin, std::size_t size)
	{
		float delta = size / static_cast<float>(fade_calc_length);
		float initial = begin / static_cast<float>(fade_calc_length);

		for (std::size_t i = 0; i < size; ++i)
			smps[i] = initial - i * delta;
	}

	enum fade_type
	{
		no_fade,
		fade_in,
		fade_out
	};

	std::size_t minus(std::size_t higher_pos, std::size_t lower_pos)
	{
		// higher pos is known to be higher
		return (higher_pos + bs() - lower_pos) % bs();
	}

	// play buffer, increases from_this_buf
	void play(std::size_t& from_this_buf, fade_type ft, std::size_t from = 0)
	{
		// how many samples can we still play backwards?
		// TODO: use std::size_t for such ports?

		const std::size_t samples = static_cast<unsigned>(samplecount);

#if 0
		// find last sample we need to play
		// TODO: simplify this
		std::size_t end = from_this_buf +
			(forward ? std::min(minus(record_head, from_this_buf), samples)
				: std::min(from_this_buf, samples));
#endif
		std::size_t max = std::min
			(forward ? minus(record_head, from_this_buf) : minus(from_this_buf, record_start), samples);



		float fade_start, fade_delta;
		switch (ft)
		{
			case no_fade:
				fade_start = 1.0f;
				fade_delta = 0.0f;
				break;
			case fade_in:
				assert(false);
				fade_start = (minus(from_this_buf, from) / static_cast<float>(fade_calc_length));
				fade_delta = 1.f / fade_calc_length;
				break;
			case fade_out:
				fade_start = 1.f - (minus(from_this_buf, from) / static_cast<float>(fade_calc_length));
				fade_delta = -1.f / fade_calc_length;
				break;
		}


		if(forward)
		{
			for(std::size_t i = 0; i < max; ++i)
			{
				float fade = (fade_start + i * fade_delta);
				out.left[i] += fade * buffer[0][(from_this_buf + i) % bs()];
				out.right[i] += fade * buffer[1][(from_this_buf + i) % bs()];
			}
			from_this_buf += max;
		}
		else {
			for(std::size_t i = 0; i < max; ++i)
			{
				float fade = (fade_start + i * fade_delta);
				out.left[i] += fade * buffer[0][(from_this_buf + bs() - i) % bs()];
				out.right[i] += fade * buffer[1][(from_this_buf + bs() - i) % bs()];
			}
			from_this_buf -= max;
		}
	}

	std::size_t bs() const { return buffer->size(); }

public:


	void run() override
	{
		// update last action lazy
		// buffer switching, start of fade etc all goes lazy
		if(last_action_change >= minimal_action_change)
		{
			last_action_change = 0;

			last_action = action;
			last_play_head_at_action_change = last_play_head = play_head;
			action = static_cast<action_t>(static_cast<int>(action_ctrl));

			if(action != last_action)
			switch (action)
			{
				case ac_record:
					record_start = record_head;
					break;

				case ac_play_1:
				case ac_play_2:
					// i.e. start of play

					if(last_action == ac_record)
					{
						play_head = forward ? record_start : record_head;
					}

				break;

				case ac_hold:
					break;
			}
		}

		// initialize with silence
		for(std::size_t i = 0; i < samplecount; ++i)
		{
			out.left[i] = out.right[i] = .0f;
		}

		if(last_action != action && (last_action == ac_play_1 || last_action == ac_play_2))
		{
			play(last_play_head, fade_out, last_play_head_at_action_change);
		}

		switch(action)
		{
			case ac_play_1:
			case ac_play_2:
				play(play_head, (last_action == action) ? no_fade : fade_in);
				break;

			case ac_record:
			{
				// check how many bytes we can still record
				// note: record_head will always be <= bs

				// how many samples can we record?
				std::size_t max = std::min(
							static_cast<std::size_t>(
								static_cast<unsigned>(samplecount)),
							minus(record_start, record_head) - 1);
							// -1 because play_head = record_head would mean
							// that we have not recorded anything

				// TODO: this could be optimized to get rid of the mod
				for(std::size_t i = 0; i < max; ++i)
				{
					buffer[0][record_head + i % bs()] = in.left[i];
					buffer[1][record_head + i % bs()] = in.right[i];
				}

				record_head += max;
				break;
			}
			case ac_hold:
				break;
		}

		last_action_change += samplecount;
	}

public:	// FEATURE: make these private?
	~reverser() override {}
	reverser()
	{
		action_ctrl.min = 0;
		action_ctrl.max = 3;
		action_ctrl.def = ac_hold;
		forward.def = false;

		for(std::size_t i = 0; i < sizeof(buffer); ++i)
		{
			// TODO: size must be at least 2*256, or so
			buffer[i].resize(buffer_alloc_sz, .0f);
		}
/*		std::size_t max_next_buffer = (buffersize > 256) ? 1 : (256/buffersize);
		for(std::size_t i = 0; i < sizeof(buffer); ++i)
			std::fill((*buffer)[i].begin(), (*buffer)[i].end(), .0f);*/
	}

private:
	void activate() override {}
	void deactivate() override {}

#if 0
	// buffers recalling audio for reversal
	// should never be used directly, except for buffer swapping
	std::vector<float> buffer_arr[2];
	std::vector<float> buffer_arr_2[2];
#endif
	//! buffer to store recorded samples
	std::vector<float> buffer[2];
#if 0
	//! if last_repeat_remain > 0, this is the previous valid buffer
	std::vector<float> (*prev_buffer)[2] = &buffer_arr_2;
#endif
	//std::vector<float> fadeout_save_buffers[2];

	//bool is_recording = false;
	action_t last_action = action_t::ac_hold; //  TODO: sync with control

	//! index into each buffer, where the next samples need to be recorded
	std::size_t record_head = 0;
	//! index into each buffer, where the next samples need to be played
	std::size_t play_head = 0;

	std::size_t record_start; //!< where the current recording started
	std::size_t last_play_head; //!< play_head from previous action, still moving
	std::size_t last_play_head_at_action_change;

	spa::audio::stereo::in in;
	spa::audio::stereo::out out;
	spa::audio::control_in<int> action_ctrl;
	action_t action; // "lazy" action
	spa::audio::samplecount samplecount;
	spa::audio::control_in<bool> forward;

	spa::port_ref_base& port(const char* path) override
	{
		switch(path[0])
		{
			case 'i': return in;
			case 'o': return out;
			case 's': return samplecount;
			case 'a': return action_ctrl;
			case 'd': return forward;
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
		return { "in", "out", "samplecount", "action", "forward" };
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

