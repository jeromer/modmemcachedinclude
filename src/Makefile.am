mod_memcached_include_la_SOURCES = mod_memcached_include.c mod_memcached_include.h 
mod_memcached_include_la_CFLAGS = -Wall ${MODULE_CFLAGS}
mod_memcached_include_la_LDFLAGS = -rpath ${AP_LIBEXECDIR} -module -avoid-version ${MODULE_LDFLAGS}

mod_LTLIBRARIES = mod_memcached_include.la
moddir=${AP_LIBEXECDIR}

install: install-am
	rm -f $(DESTDIR)${AP_LIBEXECDIR}/mod_memcached_include.a
	rm -f $(DESTDIR)${AP_LIBEXECDIR}/mod_memcached_include.la
