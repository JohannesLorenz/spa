#include <spa/spa.h>

namespace spa {

/*
 * give those classes at least one translation unit with out-of-line
 * definitions, so compilers know a place to store their vtables
 */

visitor::~visitor() {}
port_ref_base::~port_ref_base() {}
plugin::~plugin() {}
descriptor::~descriptor() {}

/*
 * accept definitions
 */

ACCEPT(port_ref_base, spa::visitor)

}



