/*************************************************************************/
/* spa - simple plugin API                                               */
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

/*
	!WARNING!

	This plugin library is currently under development. It may change
	a lot, even without announcement.

	At this unstable state, port contents may still change, and as a result,
	different versions of the library may not be compatible.
*/

/**
	@file spa.h
	generic spa utils
*/

#ifndef SPA_SPA_H
#define SPA_SPA_H

// Note: Any shared data, i.e.
//       * member variables
//       * return types from non-inline functions
//       * thrown errors that reach plugin and host
//       must be in your own (version) control, i.e. no STL, boost, libXYZ...
#include <cstdarg> // only functions for varargs

#include <string> // used for host functions only (see bottom of file)
#include <cassert>

// fix "major" defined in GNU libraries
#ifdef major
#undef major
#endif

// fix "minor" defined in GNU libraries
#ifdef minor
#undef minor
#endif

#include "spa_fwd.h"

namespace spa {

//! api version to check whether plugin API and host API are compatible
//! @todo this may not work, needs some better idea
class version_t
{
	unsigned _major, _minor, _patch;
public:
	//! major spa version, change means API break
	constexpr unsigned major() const { return _major; }
	//! minor spa version, change means API break
	constexpr unsigned minor() const { return _minor; }
	//! patch spa version, change guarantees that API does not break
	constexpr unsigned patch() const { return _patch; }
	constexpr version_t(unsigned _major, unsigned _minor, unsigned _patch)
		: _major(_major), _minor(_minor), _patch(_patch) {}
};

inline bool operator<(const version_t& lhs, const version_t& rhs) {
	return lhs.major() < rhs.major()
		|| (lhs.major() == rhs.major() &&
			( lhs.minor() < rhs.minor()
				|| ( lhs.minor() == rhs.minor() &&
					lhs.patch() < rhs.patch()
		)));
}

inline bool operator>(const version_t& lhs, const version_t& rhs) {
	return operator<(rhs, lhs);
}

inline bool operator<=(const version_t& lhs, const version_t& rhs) {
	return !operator>(lhs, rhs);
}

inline bool operator>=(const version_t& lhs, const version_t& rhs) {
	return !operator<(lhs, rhs);
}

//! current version of this spa API
constexpr const version_t api_version(0, 0, 3);
//! least spa API version that is compatible
constexpr const version_t least_api_version(0, 0, 2);

namespace detail {

inline unsigned m_strlen(const char* str)
{
	unsigned sz = 0;
	for(; *str; ++str, ++sz) ;
	return sz;
}

inline void m_memcpy(char* dest, const char* src, std::size_t sz)
{
	for(; sz; --sz, ++dest, ++src) *dest = *src;
}

inline bool m_streq(const char* s1, const char* s2)
{
	for(; (*s1 == *s2) && *s1; ++s1, ++s2) ;
	return *s1 == *s2;
}

}

//! base class for all exceptions that the API introduces
class exception
{
	const char* _what; //!< simple explenation of the issue
public:
	const char* what() const noexcept { return _what; }
	exception(const char* what) : _what(what) {}
};

//! host asks for a port using plugin::port , but no port with such a name
class port_not_found : public exception
{
public:
	const char* portname;
	port_not_found(const char* portname = nullptr) :
		exception("no port with that name"),
	portname(portname) {}
	// TODO: iostream operator<< for all classes
};

//! exception thrown by spa containers if an element out of range is being
//! requested
class out_of_range : public exception
{
public:
	std::size_t accessed;
	std::size_t size;
	out_of_range(std::size_t accessed, std::size_t size) :
		exception("accessed an element out of range"),
		accessed(accessed),
		size(size) {}
};

class version_mismatch : public exception
{
public:
	version_t version, least_version;
protected:
	version_mismatch(const char* text,
				version_t version,
				version_t least_version) :
		exception(text),
		version(version), least_version(least_version)
	{
	}
};

class plugin_too_old : public version_mismatch
{
public:
	plugin_too_old(version_t plugin_version) :
		version_mismatch("plugin spa version too old for host",
				 plugin_version, least_api_version) {}
};

class host_too_old : public version_mismatch
{
public:
	host_too_old(version_t plugin_expects) :
		version_mismatch("host spa version too old for plugin",
				 api_version, plugin_expects) {}
};

//! name of the entry function that a host must resolve
constexpr const char* descriptor_name = "spa_descriptor";

//! Simple vector on heap, without library dependencies
//! @todo untested
template<class T>
class simple_vec
{
protected:
	T* _data = nullptr;
	unsigned len = 0;

	void mdelete() { delete[] _data; _data = nullptr; }

	static void init(const T* ) noexcept {}
	template<class First, class ...More>
	static void init(T* dest, const First& first, const More& ...args)
		noexcept
	{
		*dest = first;
		init(dest + 1, args...);
	}
public:
	class const_iterator
	{
		friend class simple_vec;
		const T* ptr;
		const_iterator(const T* ptr) noexcept : ptr(ptr) {}
	public:
		bool operator!=(const const_iterator& other) const noexcept {
			return ptr != other.ptr;
		}
		const_iterator& operator++() noexcept { ++ptr; return *this; }
		const T& operator*() noexcept { return *ptr; }
		const T& operator->() noexcept { return *ptr; }
	};

	const_iterator begin() const noexcept { return const_iterator{_data}; }
	const_iterator end() const noexcept {
		return const_iterator{_data + len}; }

	//! Return a pointer to the internal byte array, representing the data
	//! For empty strings, this currently returns nullptr
	const T* data() const noexcept { return _data; }
	//! Return the number of elements
	unsigned size() const noexcept { return len; }
	//! Return whether the vector is empty
	bool empty() const noexcept { return !len; }
	//! Clear the vector
	void clear() noexcept { mdelete(); len = 0; }
	// TODO: implement push_back, pop_back
	//! Return element at position @p idx
	T& operator[](int idx) noexcept { return _data[idx]; }
	//! Return element at position @p idx
	const T& operator[](int idx) const noexcept { return _data[idx]; }
	//! Return safely element at position @p idx
	T& at(unsigned idx) noexcept(false)
	{
		if(idx > len)
			throw out_of_range(idx, 1+len);
		else
			return _data[idx];
	}
	//! Return safely element at position @p idx
	const T& at(unsigned idx) const noexcept(false)
	{
		if(idx > len)
			throw out_of_range(idx, 1+len);
		else
			return _data[idx];
	}
	//! Construct an empty vector. Guaranteed not to alloc
	simple_vec() noexcept(false) {}
	template<class ...Args>
	simple_vec(const Args& ...args) {
		len = sizeof...(args);
		_data = new T[len];
		init(_data, args...);
	}
	simple_vec(simple_vec&& other) noexcept {
		len = other.len;
		_data = other._data;
		other.len = 0;
		other._data = nullptr;
	}
	simple_vec(const simple_vec<T>& other) = delete;
	~simple_vec() noexcept { mdelete(); }
};

//! Simple string on heap, without library dependencies
//! @todo untested
class simple_str : public simple_vec<char>
{
public:
	//! Return the number of chars before the final 0 byte
	unsigned length() const noexcept { return size() - 1; }
	//! Construct an empty string
	simple_str() noexcept(false) {}
	//! Construct a string, copying @p initial
	simple_str(const char* initial) noexcept(false) { operator=(initial); }
	//! Assign this string to be a copy of @p newstr
	simple_str& operator=(const char* newstr) noexcept(false)
	{
		mdelete();
		len = detail::m_strlen(newstr);
		unsigned sz = len + 1;
		_data = new char[sz];
		detail::m_memcpy(_data, newstr, sz);
		return *this;
	}
	//! Append @p rhs.
	//! @note The internal pointer can change
	simple_str& operator+=(const char* rhs) noexcept(false) {
		unsigned rhslen = detail::m_strlen(rhs);
		char* newdata = new char[len + rhslen + 1];
		detail::m_memcpy(_data, newdata, len);
		detail::m_memcpy(_data + len, rhs, rhslen + 1);
		mdelete();
		_data = newdata;
		len = len + rhslen;
		return *this;
	}
	simple_str(simple_str&& other) noexcept :
		simple_vec(std::move(static_cast<simple_vec&>(other))) {}
	simple_str(const simple_str& other) = delete;
};

//! direction, as seen from the plugin
enum direction_t {
	input = 1, //!< data from host to plugin
	output = 2 //!< data from plugin to host
};

class port_ref_base
{
public:
	//! Whether this parameter must be known before all others,
	//! e.g. to initialize buffers
	//! This means that the host must provide useful data where it
	//! connects the plugin's ports before it calls plugin::init()
	//! Implicits that the port is compulsory
	virtual bool initial() const { return false; }

	//! Whether the host must connect the knob for sane functionality
	//! Initial ports are always compulsory, regardless of this function
	virtual bool compulsory() const { return false; }

	simple_str label;
public:
	//! combination of direction_t (TODO: use bitmask?)
	virtual int directions() const = 0;

	//! accept function conforming to the visitor pattern
	//! equivalent to writing "SPA_OBJECT"
	virtual void accept(class visitor& v);

	virtual ~port_ref_base();
};

//! use this in every class that you want to make visitable
#define SPA_OBJECT void accept(class spa::visitor& v) override;

//! class for simple types
//! TODO: use atomics
template<class T>
class port_ref : public virtual port_ref_base
{
	T* ref;
public:
	SPA_OBJECT

	// TODO: some of those functions may be useless

	operator T&() { return *ref; }
	operator const T&() const { return *ref; }

	const port_ref<T>& operator=(const T& value) { return set(value); }
	const port_ref<T>& set(const T& value) { *ref = value; return *this; }

	//! array operator - only to use if the target is an array
	T& operator[](int i) { return ref[i]; }
	//! array operator - only to use if the target is an array
	const T& operator[](int i) const { return ref[i]; }

	void set_ref(T* pointer) { ref = pointer; }
//	void set_ref(const T* pointer) { ref = pointer; }
};

class counted : public virtual port_ref_base {
public:
	int channel;
};

// not sure if the input output classes make any sense, and whether they should
// have an accept function...

class input : public virtual port_ref_base {
	int directions() const override { return direction_t::input; }
	void accept(class visitor& ) override {}
};

class output : public virtual port_ref_base {
	int directions() const override { return direction_t::output; }
	void accept(class visitor& ) override {}
};

/*
 * ringbuffers on the host side
 */

template<class T>
class ringbuffer_in_base;

//! base class for different templates of ringbuffer - don't use directly
template<class T>
class ringbuffer_base
{
	friend class ringbuffer_in_base<T>;

	T* buffer;
	std::size_t size, pos;
public:
	std::size_t write_space() const { return size - pos; }
	void write(const T* data, std::size_t n) {
		for(std::size_t i = 0; i < n; ++i)
			buffer[pos++] = data[i];
	}

	void reset() { pos = 0; }

	ringbuffer_base(std::size_t size) :
		buffer(new T[size]), size(size), pos(0) {}
	~ringbuffer_base() { delete[] buffer; }
};

//! generic ringbuffer class
template<class T>
class ringbuffer : public ringbuffer_base<T>
{
public:
	using ringbuffer_base<T>::ringbuffer_base;
};

//! char ringbuffer specialization, which supports a special write function
template<>
class ringbuffer<char> : public ringbuffer_base<char>
{
	using base = ringbuffer_base<char>;
public:
	void write_with_length(const char* data, std::size_t len)
	{
		if(write_space() >= len + 4)
		{
			uint32_t len32 = static_cast<uint32_t>(len);
			//printf("vmessage length: %u\n", len32);

			char lenc[4] = {static_cast<char>((len32 >> 24) & 0xFF),
				static_cast<char>((len32 >> 16) & 0xFF),
				static_cast<char>((len32 >> 8) & 0xFF),
				static_cast<char>((len32) & 0xFF)};
			//printf("length array: %x %x %x %x\n",
			//	+lenc[0], +lenc[1], +lenc[2], +lenc[3]);
			base::write(lenc, 4);
			base::write(data, len);
		}
	}

	ringbuffer(std::size_t size) : base(size) {}
};

/*
 * ringbuffers refs on the host side
 */
//! base class, don't use
template<class T>
class ringbuffer_in_base
{
	std::size_t size;
	ringbuffer_base<T>* ref = nullptr;
	std::size_t pos;
public:
	struct read_sequence_t
	{
		const T* buf;
		std::size_t pos, range;
		const T& operator[](std::size_t idx) const { return buf[pos+idx]; }
		bool copy(char* res, std::size_t n) const {
			if(n <= range) {
				for(std::size_t i = 0; i < n; ++i) {
					res[i] = buf[pos + i];
				}
				return true;
			}
			else return false;
		}
	};
public:
	std::size_t read_space() const { return ref->pos - pos; }

	read_sequence_t read(std::size_t n) {
		assert(n <= read_space());
		read_sequence_t res { ref->buffer, pos, n };
		pos += n;
		return res;
	}

	ringbuffer_in_base(std::size_t s) : size(s), pos(0) {}

	void connect(ringbuffer_base<T>& _ref)
	{
		if(size != _ref.size)
		 throw exception("connecting ringbuffers of incompatible sizes");
		else
		 ref = &_ref;
	}

	std::size_t get_size() const { return size; }

	void reset() { pos = 0; }
};

//! ringbuffer in port for plugins to reference a host ringbuffer
template<class T>
class base_ringbuffer_in : public ringbuffer_in_base<T>
{
public:
	using ringbuffer_in_base<T>::ringbuffer_in_base;
};

//! ringbuffer in port for plugins to reference a host ringbuffer
template<>
class base_ringbuffer_in<char> : public ringbuffer_in_base<char>
{
	uint32_t length = 0, lastlength = 0; // length of the next string
public:
	using base = ringbuffer_in_base<char>;
	using base::ringbuffer_in_base;

	//! read the next message into temporary buffer
	//! @return true iff there was a next message;
	bool read_msg(char* read_buffer, std::size_t max)
	{
		if(read_space() > 0)
		{
			if(!length) {
				uint32_t clength;
				{
					auto rd = read(4);
					clength = static_cast<uint32_t>(+rd[3])
					 + static_cast<uint32_t>(+rd[2] << 8)
					 + static_cast<uint32_t>(+rd[1] << 16)
					 + static_cast<uint32_t>(+rd[0] << 24);
					if(clength > 256)
					    printf("read length array: %x%x%x%x\n",
					    +rd[0], +rd[1], +rd[2], +rd[3]);
				}
				//assert(clength < 256); // TODO: remove me
				//printf("read length: %d\n", +clength);
				std::size_t rs = read_space();
				// if anything, then at least the matching message
				assert(rs == 0 || rs >= clength);
				length = clength;
			}

			if(read_space())
			{

				if(length && read_space() < length)
					throw exception("char ringbuffer "
						"contains corrupted data");
				if(max < length)
					throw out_of_range(length, max);
				else
				{
					auto rd = read(length);
					bool ok = rd.copy(read_buffer, length);
					assert(ok); // the "length" int told us it was OK
					lastlength = length;
					length = 0;
				}
				std::size_t rs = read_space();
				// if anything, then at least one number
				assert(rs == 0 || rs >= 4);

				//printf("READ MSG: %s\n", read_buffer);
				return true;
			}
			else
			{
				//puts("Read nothing");
				return false;
			}
		}
		else
			return false;
	}
};

template<class T>
class ringbuffer_in : public base_ringbuffer_in<T>, public input
{
public:
	SPA_OBJECT
	using base_ringbuffer_in<T>::base_ringbuffer_in;
};

//! ringbuffer out port for plugins to reference a host ringbuffer
template<class T>
class ringbuffer_out : public output
{
public:
	SPA_OBJECT
	ringbuffer<T>* ref;
};

//! make a visitor function for type @p type, which will default to
//! visiting a base class @p base of it
#define SPA_MK_VISIT(type, base) \
	virtual void visit(type& p) { visit(static_cast<base&>(p)); }

class visitor
{
public:
	virtual void visit(port_ref_base& ) {}

#define SPA_MK_VISIT_PR(type) \
	SPA_MK_VISIT(port_ref<type>, port_ref_base) \
	SPA_MK_VISIT(port_ref<const type>, port_ref_base) \
	SPA_MK_VISIT(ringbuffer_in<type>, port_ref_base)

#define SPA_MK_VISIT_PR2(type) SPA_MK_VISIT_PR(type) \
	SPA_MK_VISIT_PR(unsigned type)

	SPA_MK_VISIT_PR(bool)

	SPA_MK_VISIT_PR2(char)
	SPA_MK_VISIT_PR2(short)
	SPA_MK_VISIT_PR2(int)
	SPA_MK_VISIT_PR2(long)
	SPA_MK_VISIT_PR2(long long)

	SPA_MK_VISIT_PR(float)
	SPA_MK_VISIT_PR(double)

#undef SPA_MK_VISIT_PR2
#undef SPA_MK_VISIT_PR

	virtual ~visitor();
};

//! define an accept function for a (non-template) class
#define ACCEPT(classname, visitor_type)\
	void classname::accept(class spa::visitor& v) {\
		dynamic_cast<visitor_type&>(v).visit(*this);\
	}

//! define an accept function for a template class
#define ACCEPT_T(classname, visitor_type)\
	template<class T>\
	void classname<T>::accept(class spa::visitor& v) {\
		dynamic_cast<visitor_type&>(v).visit(*this);\
	}

ACCEPT_T(port_ref, spa::visitor)
ACCEPT_T(ringbuffer_in, spa::visitor)

//! Base class for the spa plugin
class plugin
{
public:
	/*
		TODO: specify threads
	*/
	//! Must do one computation, depending on however this is defined
	//! E.g. if it's an audio plugin and has a sample count port, this
	//! should compute as many new floats for the out buffers
	virtual void run() = 0;

	//! The plugin must initiate all heavy variables
	virtual void init() {}
	//! Fast function to activate a plugin (RT)
	virtual void activate() {}
	virtual void deactivate() {}

	//! Let the plugin dump a savefile. The success of the operation will
	//! need to be checked later by check_save()
	//! The audio thread must be stopped
	//! @param savefile The destination to dump the savefile to
	//! @param ticket A value that, combined with @p savefile, identifies
	//!   that operation, e.g. an increasing counter or an OSC timestamp
	//! @return true iff the saving request could be sent
	virtual bool save(const char* savefile, uint64_t ticket) {
		(void)savefile;
		(void)ticket;
		return false; }

	//! Let the plugin load a savefile. The success of the operation will
	//! need to be checked later by check_load()
	//! The audio thread must be stopped
	//! @param savefile The path to load the savefile from
	//! @param ticket A value that, combined with @p savefile, identifies
	//!   that operation, e.g. an increasing counter or an OSC timestamp
	//! @return true iff the loading request could be sent
	virtual bool load(const char* savefile, uint64_t ticket) {
		(void)savefile;
		(void)ticket;
		return false; }

	//! Let the plugin restore, e.g. reload all savefile variables
	//! unchanged, but recognize changed ports
	virtual void restore(uint64_t ticket) { (void)ticket; }

	//! Check if a requested save operation succeeded
	virtual bool save_check(const char* savefile, uint64_t ticket) {
		(void)savefile;
		(void)ticket;
		return false; }

	//! Check if a requested load operation succeeded
	virtual bool load_check(const char* savefile, uint64_t ticket) {
		(void)savefile;
		(void)ticket;
		return false; }

	//! Check if a requested restore operation succeeded
	virtual bool restore_check(uint64_t ticket) {
		(void)ticket;
		return false; }

	//! Destructor, must clean up any allocated memory
	virtual ~plugin();

	//! Return the port with name @p path (or throw port_not_found)
	//! If a port at path is offered, but no memory has been allocated
	//! yet, it *must* be allocated now, and exist until the end
	//! the host is responsible for freeing (TODO: can it know?)
	virtual port_ref_base& port(const char* path) = 0;

	//! show or hide the external UI
	virtual void ui_ext_show(bool show) { (void)show; }

	/*
	 * plugin instance identification inside host
	 * if none of this is given, a host could still try to use the plugin
	 * instance pointer for identification
	 */
	//! Port for communication, if any. If none, try to provide window_id
	virtual unsigned net_port() const { return 0; }
	//! Window ID of the main window of the plugin. Must be unique inside
	//! your window server environment. For X11: Use the X Window ID.
	virtual const char* window_id() const { return nullptr; }
};

#define SPA_DESCRIPTOR \
	spa::version_t spa_version() const override final {\
		return spa::api_version; }\
	spa::version_t least_spa_version() const override final { \
		return spa::least_api_version; } \
	void override_me_using_spa_descriptor_macro() const override final {}

//! Base class to let the host provide information without
//! it requiring to be started
class descriptor
{
public:
	//! License possibilities
	enum class license_type
	{
		gpl_3_0,  //!< GPL 3.0 or any later
		gpl_2_0,  //!< GPL 2.0 or any later
		lgpl_3_0, //!< LGPL 3.0 or any later
		lgpl_2_1  //!< LGPL 2.1 or any later
	};

	enum class hoster_t
	{
		localhost, //!< if you have not published it yet
		// alphabetic order from here:
		github,
		gitlab,
		sourceforge,
		other
	};

	/*
	 * Trick to find out what version the plugin uses
	 * these functions should never be explicitly overridden, but implicitly
	 * using the SPA_DESCRIPTOR macro
	 */
	//!< must return spa::api_version
	virtual version_t spa_version() const = 0;
	//!< must return spa::least_api_version
	virtual version_t least_spa_version() const = 0;
	//!< this function shall force the user to use the SPA_DESCRIPTOR macro,
	//!< forcing some default implementations of virtuals
	//!< NEVER override this function without using the SPA_DESCRIPTOR macro
	virtual void override_me_using_spa_descriptor_macro() const = 0;

	/*
	 * IDENTIFICATION OF THIS PLUGIN
	 * the comination of the following functions identify your plugin
	 * uniquely in the world. plugins should use them as identification
	 * for e.g. savefiles
	 */
	//! main hoster of your source (no mirrored one)
	virtual hoster_t hoster() const = 0;

	//! full url of you hoster, e.g. "https://github.com"
	//! @note only if hoster() returns "other"
	virtual const char* hoster_other() const { return nullptr; }

	//! organisation or user shortcut for hosters, if any
	//! (e.g. github organisation or user)
	virtual const char* organization_url() const = 0;

	//! project for this plugin
	//! @note if multiple plugins share this plugin, they should have
	//!   the same project (and maybe the same descriptor subclass as
	//!   base class)
	virtual const char* project_url() const = 0;

	//! The version control branch of this project
	virtual const char* branch() const { return "master"; }


	//! Plugin descriptor which will not change over time
	//! Should be unique inside your project
	//! (e.g. "sweep-filter-3")
	virtual const char* label() const = 0;

	/*
	 * END OF IDENTIFICATION FOR THIS PLUGIN
	 */

	//! Project name, not abbreviated
	virtual const char* project() const = 0;

	//! Full name, not abbreviated (e.g. "Resonant sweep filter")
	virtual const char* name() const = 0;

	//! Author(s), comma separated, e.g.
	//! "firstname1 lastname1, firstname2 lastname2 <mail>"
	virtual const char* authors() const { return nullptr; }

	//! Organization(s), comma separated
	virtual const char* organizations() const { return nullptr; }

	//! License that the plugin is coded in
	virtual license_type license() const = 0;

	//! Describe in one line (<= 80 chars) what the plugin does
	virtual const char* description_line() const { return nullptr; }

	//! Describe in detail what the plugin does
	virtual const char* description_full() const { return nullptr; }

	//! Function that must return a plugin
	//! @note by default, the plugin will be freed using delete
	//!   This can be changed by overriding delete_plugin()
	virtual plugin* instantiate() const = 0;

	//! Should return an XPM array for a preview logo, or nullptr
	virtual const char** xpm_load() const { return nullptr; }

	//! Desctructor, must clean up any allocated memory
	virtual ~descriptor();

/*	// Will probably deleted, since dtor can do the same
	//! Function that must delete a plugin instantiated by instantiate()
	virtual void delete_plugin(plugin* ptr) const { delete ptr; }

	//! Function that should clean up the memory of this descriptor
	virtual void delete_self() { delete this; } */

	//! Return all port names the plugin wants to expose. Must be
	//! nullptr-terminated.
	//! There can still be other ports. The host can find them using
	//! dnd, or if they are in an old-versioned savefile.
	virtual simple_vec<simple_str> port_names() const = 0;

	//! csv-list of files that can be loaded, e.g. "xmz, xiz"
	virtual const char* save_formats() const { return nullptr; }

	virtual bool save_has() const { return false; }
	virtual bool load_has() const { return false; }
	virtual bool restore_has() const { return false; }

	//! return whether the plugin has an external UI
	virtual bool ui_ext() const { return false; }

	virtual int version_major() const { return 0; }
	virtual int version_minor() const { return 0; }
	virtual int version_patch() const { return 0; }

	/*
	 * properties
	 */
	struct properties
	{
		//! plugin has realtime dependency (e.g. hardware device),
		//! so its output may not be cached or subject to significant
		//! latency
		unsigned realtime_dependency:1;
		//! plugin makes no syscalls and uses no "slow algorithms"
		unsigned hard_rt_capable:1;
	} properties;
};

//! Function that must return a spa descriptor.
//! The argument must currently be 0 (TODO).
//! Entry point for any plugin.
//! @note by default, the descriptor will be freed using delete
//!   This can be changed by overriding descriptor::delete_self()
typedef descriptor* (*descriptor_loader_t) (unsigned long);

inline void assert_versions_match(const spa::descriptor& descriptor)
	noexcept(false)
{
	if(descriptor.spa_version() < least_api_version)
		throw spa::plugin_too_old(descriptor.spa_version());
	if(api_version < descriptor.least_spa_version())
		throw spa::host_too_old(descriptor.least_spa_version());
}

// TODO: move to host only file

inline std::string unique_name(const spa::descriptor& desc,
				const char* sep = "::")
{
	std::string res;
	switch(desc.hoster())
	{
		case spa::descriptor::hoster_t::localhost:
			res = "github.com";
			break;
		case spa::descriptor::hoster_t::github:
			res = "github.com";
			break;
		case spa::descriptor::hoster_t::gitlab:
			res = "gitlab.com";
			break;
		case spa::descriptor::hoster_t::sourceforge:
			res = "sourcefourge.net";
			break;
		case spa::descriptor::hoster_t::other:
			res = desc.hoster_other();
			break;
	}
	res += sep;
	if(desc.organization_url()) {
		res += desc.organization_url();
		res += sep;
	}
	res += desc.project_url();
	res += sep;
	res += desc.branch();
	res += sep;
	res += desc.label();

	return res;
}

} // namespace spa

#endif // SPA_SPA_H

