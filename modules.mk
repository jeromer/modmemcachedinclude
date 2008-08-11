mod_memcached_include.la: mod_memcached_include.slo
	$(SH_LINK) -rpath $(libexecdir) -module -avoid-version  mod_memcached_include.lo
DISTCLEAN_TARGETS = modules.mk
shared =  mod_memcached_include.la
