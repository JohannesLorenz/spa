/*************************************************************************/
/* osc-host.cpp - a minimal osc-plugin host                              */
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
  @file osc-host.cpp
  example OSC host and test for an audio application
 */

#include <vector>
#include <cstring>
#include <string>
#include <cstdlib>
#include <climits>
#include <iostream>
#include <dlfcn.h>
#include <cmath>
#include <memory>
#include <spa/audio.h>

class osc_host
{
	friend struct host_visitor;
public:
	osc_host(const char* library_name);
	~osc_host();

	//! play the @p time'th time, i.e. 0, 1, 2...
	void play(int time);

	//! All tests passed by now?
	bool ok() const { return all_ok; }
private:
	bool all_ok = false;
	const class spa::descriptor* descriptor = nullptr;
	class spa::plugin* plugin = nullptr;

	bool init_plugin();
	void shutdown_plugin();

	using dlopen_handle_t = void*;
	dlopen_handle_t lib = nullptr;
	std::string library_name;

	constexpr static int buffersize_fix = 10;
	unsigned buffersize;
	std::vector<float> unprocessed_l, unprocessed_r,
		processed_l, processed_r;

	// for controls where we do not know the meaning (but the user will)
	std::vector<float> unknown_controls;
	std::unique_ptr<spa::audio::osc_ringbuffer> rb;

//	std::map<std::string, port_base*> ports;
};

osc_host::osc_host(const char* lib_name)
{
	library_name = lib_name;
	if(init_plugin())
		all_ok = true;
	else {
		// in most apps, don't abort the app on failure
		plugin = nullptr;
	}
}

osc_host::~osc_host()
{
	shutdown_plugin();
}

void osc_host::play(int time)
{
	if(!plugin)
		return;

	// simulate automation from the host
	rb->write("/gain", "f", fmodf(time/10.0f, 1.0f));

	// provide audio input
	for(unsigned i = 0; i < buffersize; ++i)
	{
		unprocessed_l[i] = unprocessed_r[i] = 0.1f;
	}

	// let the plugin work
	plugin->run();

	// check output
	for(unsigned i = 0; i < buffersize; ++i)
	{
		all_ok = all_ok &&
			(fabsf(processed_l[i] - 0.01f * time) < 0.0001f) &&
			(fabsf(processed_r[i] - 0.01f * time) < 0.0001f);
	}
}

struct host_visitor : public virtual spa::audio::visitor
{
	bool ok = true;
	osc_host* h;
	const spa::simple_str* port_name;

	using spa::audio::visitor::visit;

	// TODO: forbid channel.operator= x ? (use channel class with set method)
	virtual void visit(spa::audio::in& p) override {
		printf("in, c: %d\n", p.channel);
		p.set_ref((p.channel == spa::audio::stereo::left)
			? h->unprocessed_l.data()
			: h->unprocessed_r.data()); }
	virtual void visit(spa::audio::out& p) override {
		printf("out, c: %d\n", p.channel);
		p.set_ref((p.channel == spa::audio::stereo::left)
			? h->processed_l.data()
			: h->processed_r.data()); }
	virtual void visit(spa::audio::stereo::in& p) override {
		std::cout << "in, stereo" << std::endl;
		p.left = h->unprocessed_l.data();
		p.right = h->unprocessed_r.data(); }
	virtual void visit(spa::audio::stereo::out& p) override {
		std::cout << "out, stereo" << std::endl;
		p.left = h->processed_l.data();
		p.right = h->processed_r.data(); }
	virtual void visit(spa::audio::buffersize& p) override {
		std::cout << "buffersize" << std::endl;
		p.set_ref(&h->buffersize); }
	virtual void visit(spa::audio::osc_ringbuffer_in& p) override {
		std::cout << "ringbuffer input" << std::endl;
		if(h->rb)
			throw std::runtime_error("can not handle 2 OSC ports");
		else {
			h->rb.reset(
				new spa::audio::osc_ringbuffer(p.get_size()));
			p.connect(*h->rb);
		}
	}

	virtual void visit(spa::port_ref<const float>& p) override
	{
		if(p.initial())
		{
			// intial requires a sane value
			ok = false;
		}
		else
		{
			std::cout << "unknown control port" << std::endl;
			// .0f is a bad idea, we should be conform to (min,max)
			h->unknown_controls.push_back(.0f);
			p.set_ref(&h->unknown_controls.back());
		}
	}
	virtual void visit(spa::port_ref_base& ref) override {
		if(ref.compulsory() || ref.initial())
			ok = false;
		else
			std::cout << "port of unknown type, ignoring" << std::endl; }
};

bool osc_host::init_plugin()
{
	spa::descriptor_loader_t descriptor_loader;
	lib = dlopen(library_name.c_str(), RTLD_LAZY | RTLD_LOCAL);
	if(!lib) {
		std::cerr << "Warning: Could not load library " << library_name
			<< ": " << dlerror() << std::endl;
		return false;
	}

	// for the syntax, see the dlopen(3) manpage
	*(void **) (&descriptor_loader) = dlsym(lib, spa::descriptor_name);

	if(!descriptor_loader) {
		std::cerr << "Warning: Could not resolve \"osc_descriptor\" in "
			<< library_name << std::endl;
		return false;
	}

	if(descriptor_loader)
	{
		descriptor = (*descriptor_loader)(0 /* = plugin number, TODO */);
		// TODO: use collection of descriptors?
		spa::assert_versions_match(*descriptor);
		if(descriptor)
		 plugin = descriptor->instantiate();
	}

	// initialize all our port names before connecting...
	buffersize = buffersize_fix;
	// ... especially the buffers...
	unprocessed_l.resize(buffersize);
	unprocessed_r.resize(buffersize);
	processed_l.resize(buffersize);
	processed_r.resize(buffersize);


	const spa::simple_vec<spa::simple_str> port_names =
		descriptor->port_names();

	for(const spa::simple_str& port_name : port_names)
	{
		try
		{
			std::cout << "portname: " << port_name.data()
				  << std::endl;
			spa::port_ref_base& port_ref =
				plugin->port(port_name.data());

			// here comes the difficult part:
			// * what port type is in the plugin?
			// * how do we want to represent it

			host_visitor v;
			v.port_name = &port_name;
			v.h = this;
			port_ref.accept(v);
			if(!v.ok)
				throw spa::port_not_found(port_name.data());

		} catch(spa::port_not_found& e) {
			if(e.portname)
				std::cerr << "plugin specifies invalid port \"" << e.portname << "\", but does not provide it" << std::endl;
			else
				std::cerr << "plugin specifies invalid port, but does not provide it" << std::endl;
			return false;
		} catch(...) {
			throw;
		}
	}



	// now, that all initially required ports (like buffersize) are
	// connected, do allocations (like resizing buffer)
	plugin->init();

	plugin->activate();

	return true;
}

void osc_host::shutdown_plugin()
{
	if(!plugin)
		return;
	plugin->deactivate();

	delete plugin;
	plugin = nullptr;
	delete descriptor;
	descriptor = nullptr;

	if(lib) {
		dlclose(lib);
		lib = nullptr;
	}
}


void usage()
{
	std::cout << "usage: osc-host [<shared object library>]\n" << std::endl;
	exit(0);
}

int main(int argc, char** argv)
{
	int rc = EXIT_SUCCESS;
	const char* library_name = nullptr;
	switch(argc)
	{
		case 1:
			std::cout << "using example plugins"
				" \"libosc-plugin.so\"..." << std::endl;
			library_name = "libosc-plugin.so";
			break;
		case 2:
			library_name = argv[1];
			break;
	}
	if(!library_name)
		usage();

	try
	{
		char abs_path[PATH_MAX];
		if(realpath(library_name, abs_path))
		{
			library_name = abs_path;
			osc_host host(library_name);
			for(int i = 0; i < 10; ++i)
				host.play(i);
			if(!host.ok())
				throw std::runtime_error("Error while starting"
							" or running the host");
		}
		else {
			perror("Getting absolute path of plugin");
			throw std::runtime_error("Plugin path resolve error");
		}
	}
	catch (const std::exception& e) {
		std::cerr << "caught std::exception: " << e.what() << std::endl;
		rc = EXIT_FAILURE;
	}
	catch (...) {
		std::cerr << "caught unknown exception" << std::endl;
		rc = EXIT_FAILURE;
	}

	std::cout << "finished: "
		<< (rc == EXIT_SUCCESS ? "Success" : "Failure") << std::endl;
	return rc;
}

