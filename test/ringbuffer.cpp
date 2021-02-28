#include <cassert>
#include <cstring>
#include <iostream>
#include <spa/spa.h>

template<class T1, class T2>
void assert_eq(const T1& exp, const T2& cur)
{
	if(exp != cur)
	{
		std::cerr << "Expected " << exp << " got " << cur << std::endl;
		assert(exp == cur);
	}
}

int main()
{
	spa::ringbuffer<char> rb(16);
	spa::ringbuffer_in<char> reader(16);

	reader.connect(rb);

	rb.write("abc", 4);
	assert_eq(12u, rb.write_space());
	assert_eq(4u, reader.read_space());

	char buf[16];
	auto rd = reader.read(4);
	assert_eq(12u, rb.write_space());
	assert_eq(0u, reader.read_space());
	rd.copy(buf, 4);
	assert_eq(0, strcmp(buf, "abc"));

	rb.write("xy", 3);
	assert_eq(9u, rb.write_space());
	assert_eq(3u, reader.read_space());

	rd = reader.read(3);
	assert_eq(9u, rb.write_space());
	assert_eq(0u, reader.read_space());
	rd.copy(buf, 3);
	assert_eq(0, strcmp(buf, "xy"));

	rb.reset();
	reader.reset();
	assert_eq(16u, rb.write_space());
	assert_eq(0u, reader.read_space());

	return 0;
}
