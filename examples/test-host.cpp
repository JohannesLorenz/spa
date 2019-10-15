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

#include <map>
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
#include <linux/limits.h>

template<class T>
struct cast_to : public spa::audio::data_visitor
{
	T* res = nullptr;
	using spa::audio::data_visitor::visit;
	void visit(spa::data& ) override { throw std::bad_cast(); }
	void visit(T& x) override { res = &x; }
};

class test_host
{
	friend struct host_visitor;
public:
	test_host(const std::vector<std::string>& lib_names);
	~test_host();

	//! play the @p time'th time, i.e. 0, 1, 2...
	void play();

	//! All tests passed by now?
	bool ok() const { return all_ok; }
	bool wants_to_run() const {
		return tester->test_feedback == spa::test_feedback::go_on; }
	bool failed() const {
		return tester->test_feedback == spa::test_feedback::stop_failure; }
private:
	bool all_ok = true;

	void shutdown_plugin();

	struct data_info
	{
		spa::data* data;
		bool has_input = false, has_output = false;
		data_info(spa::data* d) : data(d) {}
	};

	std::map<std::string, data_info> port_data;

	struct plugin_data_t
	{
		bool init(const char *library_name, std::map<std::string, data_info> *port_data);


		using dlopen_handle_t = void*;

		dlopen_handle_t lib = nullptr;

		const class spa::descriptor* descriptor = nullptr;
		class spa::plugin* plugin = nullptr;

		constexpr static int buffersize_fix = 10;
		unsigned buffersize;
#if 0
		std::vector<float> unprocessed_l, unprocessed_r,
			processed_l, processed_r;


		// for controls where we do not know the meaning (but the user will)
		std::vector<float> unknown_controls;
		std::unique_ptr<spa::audio::osc_ringbuffer> rb;
#else
		spa::test_mode test_mode = spa::test_mode::none;
		spa::test_feedback test_feedback = spa::test_feedback::none;
#endif
		template<class T, class ...Args>
		T* append_data(std::string id, spa::direction_t direction,
			std::map<std::string, data_info>* port_data, Args... args)
		{
			T* result;
			auto itr = port_data->find(id);
			if(itr == port_data->end())
			{
				data_info inf(result = new T(args...));
				// direction turns around: output for plugin = input for host
				inf.has_input = inf.has_input || direction == spa::direction_t::output;
				inf.has_output = inf.has_output || direction == spa::direction_t::input;
				port_data->emplace(id, inf);
			}
			else
			{
				cast_to<T> caster;
				itr->second.data->accept(caster);
				itr->second.has_input = itr->second.has_input || direction == spa::direction_t::output;
				itr->second.has_output = itr->second.has_output || direction == spa::direction_t::input;
				result = caster.res;
			}
			return result;
		}

		//	std::map<std::string, port_base*> ports;

		void shutdown();
	};

	plugin_data_t *plugin, *tester;

	std::vector<plugin_data_t> plugin_data;
};

test_host::test_host(const std::vector<std::string>& lib_names)
{
	plugin_data.reserve(lib_names.size());
	for(const std::string& cur_lib : lib_names)
	{
		plugin_data.emplace_back();
		if(!plugin_data.back().init(cur_lib.c_str(), &port_data))
		{
			// in most apps, don't abort the app on failure
			all_ok = false;
			break;
		}
	}

	if(plugin_data.size() != 2)
		throw std::runtime_error("Expect exactly two plugins");
	if(!((plugin_data[0].test_mode == spa::test_mode::none) ^
		(plugin_data[1].test_mode == spa::test_mode::none)))
		throw std::runtime_error("Expect exactly one test plugin");
	tester = (plugin_data[0].test_mode != spa::test_mode::none) ? &plugin_data[0] : &plugin_data[1];
	plugin = (plugin_data[0].test_mode == spa::test_mode::none) ? &plugin_data[0] : &plugin_data[1];

	for(auto& pr : port_data)
	if(pr.second.has_input && !pr.second.has_output)
	{
		std::cout << "info: port " << pr.first << " is not checked" << std::endl;
	}

	for(auto& pr : port_data)
	if(pr.second.has_output && !pr.second.has_input)
	{
		std::string msg = "port " + pr.first + " is being read from a plugin, "
			"but never written to";
		throw std::runtime_error(msg);
	}
}

test_host::~test_host()
{
	for(plugin_data_t& p : plugin_data)
		p.shutdown();
}

void test_host::play()
{
	if(!ok())
		return;

	(void)time;

	tester->test_mode = spa::test_mode::challenge;
	tester->plugin->run(); // TODO: set challenge mode

	plugin->plugin->run();

	tester->test_mode = spa::test_mode::evaluate;
	tester->plugin->run(); // TODO: set check mode


#if 0
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
#endif
}

struct host_visitor : public virtual spa::audio::visitor
{
	bool ok = true;
	test_host::plugin_data_t* plu;
	const spa::simple_str* port_name;
	std::map<std::string, test_host::data_info>* data;

	using spa::audio::visitor::visit;
/*
	template<class T>
	void set_ref(spa::port_ref<T>& p, test_host::plugin_data_t::data_info& info) {
		p.set_ref()
	}*/

	using dir = spa::direction_t;

	// TODO: forbid channel.operator= x ? (use channel class with set method)
	void visit(spa::audio::in& p) override {
		std::cout << "audio in" << std::endl;
		p.set_ref(plu->append_data<spa::data_vector<float>>(
				port_name->data(), dir::input, data)->value.data());
	}
	void visit(spa::audio::out& p) override {
		std::cout << "audio out" << std::endl;
		p.set_ref(plu->append_data<spa::data_vector<float>>(
				port_name->data(), dir::output, data)->value.data());
	}
	void visit(spa::audio::stereo::in& p) override {
		std::cout << "in, stereo" << std::endl;
		p.left = plu->append_data<spa::data_vector<float>>(
			port_name->data() + std::string("-left"), dir::input, data)->value.data();
		p.right = plu->append_data<spa::data_vector<float>>(
			port_name->data() + std::string("-right"), dir::input, data)->value.data();
	}
	void visit(spa::audio::stereo::out& p) override {
		std::cout << "out, stereo" << std::endl;
		p.left = plu->append_data<spa::data_vector<float>>(
			port_name->data() + std::string("-left"), dir::output, data)->value.data();
		p.right = plu->append_data<spa::data_vector<float>>(
			port_name->data() + std::string("-right"), dir::output, data)->value.data();
	}
	void visit(spa::port_ref<unsigned>& p) override {
		std::cout << "unsigned" << std::endl;
		p.set_ref(&plu->append_data<spa::data_simple<unsigned>>(
			port_name->data(), dir::output, data)->value); }
	void visit(spa::port_ref<const unsigned>& p) override {
		std::cout << "const unsigned" << std::endl;
		p.set_ref(&plu->append_data<spa::data_simple<unsigned>>(
			port_name->data(), dir::input, data)->value); }
	void visit(spa::port_ref<std::size_t>& p) override {
		std::cout << "size_t" << std::endl;
		p.set_ref(&plu->append_data<spa::data_simple<std::size_t>>(
			port_name->data(), dir::output, data)->value); }
	void visit(spa::port_ref<const std::size_t>& p) override {
		std::cout << "const size_t" << std::endl;
		p.set_ref(&plu->append_data<spa::data_simple<std::size_t>>(
			port_name->data(), dir::input, data)->value); }
	void visit(spa::port_ref<float>& p) override {
		std::cout << "unsigned" << std::endl;
		p.set_ref(&plu->append_data<spa::data_simple<float>>(
			port_name->data(), dir::output, data)->value); }
	void visit(spa::port_ref<const float>& p) override {
		std::cout << "const unsigned" << std::endl;
		p.set_ref(&plu->append_data<spa::data_simple<float>>(
			port_name->data(), dir::input, data)->value); }
	void visit(spa::audio::osc_ringbuffer_in& p) override {
		std::cout << "ringbuffer input" << std::endl;
		p.connect(plu->append_data<spa::audio::data_osc_ringbuffer>(
			port_name->data(), dir::input, data, 8192)->value);
	}

	void visit(spa::test_mode_port& p) override {
		plu->test_mode = spa::test_mode::challenge;
		p.set_ref(&plu->test_mode);
		// TODO assign plugin and tester
	}

	void visit(spa::test_feedback_port& p) override {
		plu->test_feedback = spa::test_feedback::go_on;
		p.set_ref(&plu->test_feedback);
		// TODO assign plugin and tester
	}

	void visit(spa::port_ref_base& ref) override {
		if(ref.compulsory() || ref.initial())
			ok = false;
		else
			std::cout << "port of unknown type, ignoring" << std::endl; }

	~host_visitor() override;
};

host_visitor::~host_visitor() {}

bool test_host::plugin_data_t::init(const char* library_name, std::map<std::string, data_info>* port_data)
{
	spa::descriptor_loader_t descriptor_loader;
	lib = dlopen(library_name, RTLD_LAZY | RTLD_LOCAL);
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
			v.plu = this;
			v.data = port_data;
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


	struct initializer : public spa::audio::data_visitor
	{
		std::size_t buffersize;
		using data_visitor::visit;
		void visit(spa::data_vector<float>& v)
		{
			v.value.resize(buffersize);
		}
	} m_initialize;
	m_initialize.buffersize = buffersize;

	// initialize the buffers..., check input/output
	for(auto& pr : *port_data)
	{
		pr.second.data->accept(m_initialize);
	}


	// now, that all initially required ports (like buffersize) are
	// connected, do allocations (like resizing buffer)
	plugin->init();

	plugin->activate();

	return true;
}

void test_host::plugin_data_t::shutdown()
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


/*[[noreturn]] void usage()
{
	std::cout << "usage: osc-host [<shared object library>]\n" << std::endl;
	exit(0);
}*/

int main(int argc, char** argv)
{
	int rc = EXIT_SUCCESS;
//	const char* library_name = nullptr;
/*	switch(argc)
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
		usage();*/
	if(argc != 3)
	{
		std::cerr << "usage: " << argv[0] << " <plugin 1> <plugin 2>" << std::endl;
	}

	try
	{
		std::vector<std::string> lib_paths;
		for(int i = 1; i < argc; ++i)
		{
			char abs_path[PATH_MAX];
			if(realpath(argv[i], abs_path))
			{
				lib_paths.emplace_back(abs_path);
			}
			else {
				perror("Getting absolute path of plugin");
				throw std::runtime_error("Plugin path resolve error");
			}
		}

		test_host host(lib_paths);
		if(!host.ok())
			throw std::runtime_error("Error while starting host");

		while(host.wants_to_run())
			host.play();

		if(host.failed())
			rc = EXIT_FAILURE;
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

