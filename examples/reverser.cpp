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
	struct dist_t
	{
		std::size_t val;
		float to_float() const { return static_cast<float>(val); }
		dist_t() = default;
		constexpr dist_t(const dist_t& other) = default;
		constexpr dist_t operator=(const dist_t& other) { val = other.val; return *this; }
		constexpr explicit dist_t(std::size_t val) : val(val) {}
		bool operator<(const dist_t& rhs) const { return val < rhs.val; }
		bool operator>(const dist_t& rhs) const { return val > rhs.val; }
		bool operator<=(const dist_t& rhs) const { return val <= rhs.val; }
		bool operator>=(const dist_t& rhs) const { return val >= rhs.val; }
		dist_t& operator++() { val++; return *this; }
		dist_t operator*(std::size_t fac) const { dist_t res; res.val = val * fac; return res; }
	};

	struct pos_t
	{
		std::size_t val;
		bool operator==(const pos_t& rhs) const { return val == rhs.val; }
		bool operator!=(const pos_t& rhs) const { return val != rhs.val; }
		pos_t& operator=(const pos_t& rhs) { val = rhs.val; return *this; }
		pos_t& operator++() { val++; return *this; }
		pos_t() = default;
		explicit pos_t(std::size_t val) : val(val) {}
		pos_t(const pos_t& rhs) = default;
	};

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

	dist_t fade_calc_length = dist_t(256); // works great in zyn, so...
	constexpr const static float fadein_adjustment = 20.0f;

//	std::size_t fade_length_front, fade_length_end;

	//std::size_t blend_size, blend_progress;

	// time for fade in + fade out, so that both never can occur at the same time
	const dist_t minimal_action_change = fade_calc_length * std::size_t(2);
	dist_t last_action_change = dist_t(0); //! how many samples is last action change ago?

#if 0
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
#endif

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

#if 0
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
#endif
	enum fade_type
	{
		no_fade,
		fade_in,
		fade_out
	};



	// higher pos is known to be higher, i.e. behind lower pos in the
	// readable range
	dist_t minus (pos_t higher, pos_t lower) // TODO: use operator-
	{
		return dist_t((higher.val + bs() - lower.val) % bs());
	}

	pos_t minus(pos_t higher, dist_t lower)
	{
		return pos_t((higher.val + bs() - lower.val) % bs());
	}

	dist_t minus (dist_t a, dist_t b) // TODO: use operator-
	{
		return dist_t(a.val - b.val);
	}

	pos_t plus(pos_t a, dist_t b)
	{
		return pos_t((a.val + b.val) % bs());
	}

	dist_t plus(dist_t a, dist_t b)
	{
		return dist_t(a.val + b.val);
	}


#if 0
	// higher pos is known to be higher, i.e. behind lower pos in the
	// readable range
	std::size_t minus(std::size_t higher_pos, std::size_t lower_pos_or_delta)
	{
		return (higher_pos + bs() - lower_pos_or_delta) % bs();
	}

	std::size_t plus(std::size_t a, std::size_t b)
	{
		return (a + b) % bs();
	}
#endif
	template<class GetWritePos>
	void write_buffers(pos_t play_left_pos,
		pos_t play_right_pos,
		pos_t play_left_peak_pos,
		pos_t play_right_peak_pos)
	{
		const float fade_delta = 1.f / fade_calc_length.to_float();

		//float fade_in_mult = ;
		pos_t i;

		// use multiple for loops for speedups


		std::size_t iout = forward ? 0 : minus(play_right_pos, play_left_pos).val;

		GetWritePos get_write_pos;

		// i is the ringbuffer index, starting at from_this_buf
		// iout is the output index, starting at 0
		for(i = play_left_pos;
			i != play_left_peak_pos;
			++i, iout = forward ? (iout + 1) : (iout - 1))
		{
			float fade = 1.f - minus(play_left_peak_pos, i).to_float() * fade_delta;
			out.left[get_write_pos(iout)] += fade * buffer[0][i.val];
			out.right[get_write_pos(iout)] += fade * buffer[1][i.val];
		}

		for(i = play_left_peak_pos;
			i != play_right_peak_pos;
			++i, iout = forward ? (iout + 1) : (iout - 1))
		{
			out.left[get_write_pos(iout)] += buffer[0][i.val];
			out.right[get_write_pos(iout)] += buffer[1][i.val];
		}

		for(i = play_right_peak_pos;
			i != play_right_pos;
			++i, iout = forward ? (iout + 1) : (iout - 1))
		{
			float fade = 1.f - (plus(minus(i, play_right_peak_pos), dist_t(1))).to_float() * fade_delta;
			out.left[get_write_pos(iout)] += fade * buffer[0][i.val];
			out.right[get_write_pos(iout)] += fade * buffer[1][i.val];
		}
	}

	// play buffer, increases from_this_buf
	// TODO: capture from, too
	void play(pos_t& from_this_buf, fade_type ft,
		pos_t artificial_fade_out_start = pos_t(std::numeric_limits<std::size_t>::max()))
	{
		// how many samples can we still play backwards?
		// TODO: use std::size_t for such ports?

		const dist_t samples(static_cast<unsigned>(samplecount));

#if 0
		// find last sample we need to play
		// TODO: simplify this
		std::size_t end = from_this_buf +
			(forward ? std::min(minus(record_head, from_this_buf), samples)
				: std::min(from_this_buf, samples));
#endif

#if 0
		std::size_t max =
			std::min(forward ? read_space_right : read_space_left,
				samples);
#endif
		// a fade-in means that
		//  - before the fade, all is 0
		//  - in the fade, we have 0/n, 1/n ... (n-1)/n
		// markers for the 3 for loops
#if 0
		const std::size_t fade_in_begin = from_this_buf;
		std::size_t fade_in_end = from_this_buf;
		std::size_t fade_out_begin = from_this_buf + max;
		const std::size_t fade_out_end = from_this_buf + max;
#endif

		switch (ft)
		{
			case no_fade:
				break;
			case fade_in:
				assert(false);
//				fade_start = (minus(from_this_buf, from) / static_cast<float>(fade_calc_length));
//				fade_delta = 1.f / fade_calc_length;
				break;
			case fade_out:
//				fade_out_start = 1.f - (minus(from_this_buf, from) / static_cast<float>(fade_calc_length));
//				fade_delta = -1.f / fade_calc_length;
				break;
		}

		auto is_greater = [this](pos_t lhs, pos_t rhs) -> bool {
			return (minus(lhs, record_start) >
				minus(rhs, record_start));
		};

		auto is_smaller = [this](pos_t lhs, pos_t rhs) {
			return (minus(lhs, record_start) <
				minus(rhs, record_start));
		};

		auto max = [is_greater](pos_t pos1, pos_t pos2) {
			return is_greater(pos1, pos2) ? pos1 : pos2;
		};

		auto min = [is_smaller](pos_t pos1, pos_t pos2) {
			return is_smaller(pos1, pos2) ? pos1 : pos2;
		};

/*		// are there enough samples to play this buffer + a fadeout?
		if(read_space_right >= fade_calc_length + samplecount)
		{
			play_right_peak_pos = fade_right_end;
		}
		// fade starts before this buffer?
		else if(read_space_right < fade_calc_length)
		{
			fade_right_peak = fade_left_begin;
		}
		else { // fade starts in this buffer
			fade_right_peak = minus(record_head, fade_calc_length);
		}
*/

/*
    fade_left_peak        fade_right_peak / artificial_fade_out_start
	  +----------------------+
	 /                        \
	/                          \   env_end
       /                            \ record_end / ...
      +           <-------+          +
    record_start          +-------->
 (=env_start)             |
		    from_this_buf

		  +       +   <- play left pos
			  +        +   <- play right pos
		  +       +   <- play left peak pos
			  +      +     <- play right peak pos
*/

		// 1. calculate envelope independent of current position

		pos_t env_start = record_start;
		pos_t env_end =
			(artificial_fade_out_start.val == std::numeric_limits<std::size_t>::max())
			? record_head
			: plus(artificial_fade_out_start, fade_calc_length);

		pos_t env_fade_left_peak = plus(env_start, fade_calc_length);
		pos_t env_fade_right_peak = minus(env_end, fade_calc_length);

//		std::size_t play_left_pos = plus(from_this_buf, );

		pos_t play_left_pos, play_right_pos;

		{
			dist_t read_space_right = minus(env_end, from_this_buf);
			dist_t read_space_left = minus(from_this_buf, env_start);
			if (read_space_right < fade_calc_length && read_space_left < fade_calc_length)
			{
				// not yet at full volume, plus already not at full volume?
				assert(false); // impossible
			}

			if(forward)
			{
				play_left_pos = from_this_buf;
				play_right_pos = plus(play_left_pos, std::min(read_space_right, samples));
			}
			else {
				play_right_pos = from_this_buf;
				play_left_pos = minus(play_right_pos, std::min(read_space_left, samples));
			}
		}

		pos_t play_left_peak_pos, play_right_peak_pos;
		play_right_peak_pos = min(env_fade_right_peak, play_right_pos);
		play_left_peak_pos = max(env_fade_left_peak, play_left_pos);


		if(forward)
		{
			write_buffers(0, 1);
		}
		else {

		}


/*

		if(forward)
		{
			struct funcs
			{
				static void inc(std::size_t& a, std::size_t bs) { a = plus(a, 1, bs); }
				static std::size_t distance(std::size_t& a, std::size_t& b, std::size_t bs) { return minus(a, b, bs); }
			};
			write_buffers<funcs>(fade_in_begin, fade_in_end, fade_out_begin,
				fade_out_end);
		}
		else
		{
			struct funcs
			{
				static void inc(std::size_t& a, std::size_t bs) { a = minus(a, 1, bs); }
				static std::size_t distance(std::size_t& a, std::size_t& b, std::size_t bs) { return minus(b, a, bs); }
			};
			write_buffers<funcs>(fade_in_begin, fade_in_end, fade_out_begin,
				fade_out_end);
		}
*/
	}

	std::size_t bs() const { return buffer->size(); }

public:


	void run() override
	{
		// update last action lazy
		// buffer switching, start of fade etc all goes lazy
		if(last_action_change >= minimal_action_change)
		{
			last_action_change = dist_t(0);

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
			// action change marks beginning of fade out
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

				dist_t samples_dist(static_cast<std::size_t>(
					static_cast<unsigned>(samplecount)));
				dist_t record_space = minus(record_start, record_head);
				--record_space.val;
				dist_t max = std::min(samples_dist, record_space);
							// -1 because play_head = record_head would mean
							// that we have not recorded anything

				// TODO: this could be optimized to get rid of the mod
				for(dist_t i = dist_t(0); i < max; ++i)
				{
					buffer[0][plus(record_head, i).val] = in.left[i.val];
					buffer[1][plus(record_head, i).val] = in.right[i.val];
				}

				record_head = plus(record_head, max);
				break;
			}
			case ac_hold:
				break;
		}

		last_action_change = plus(last_action_change, dist_t(samplecount));
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
	pos_t record_head = pos_t(0);
	//! index into each buffer, where the next samples need to be played
	pos_t play_head = pos_t(0);

	pos_t record_start; //!< where the current recording started
	pos_t last_play_head; //!< play_head from previous action, still moving
	pos_t last_play_head_at_action_change;

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

