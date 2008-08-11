##
##  Makefile -- Build procedure for sample memcached_include Apache module
##  Autogenerated via ``apxs -n memcached_include -g''.
##

## apxs -c -lapr_memcache -L/opt/local/lib -I/opt/local/include/apr_memcache-0 -DDEBUG_INCLUDE mod_memcached_include.c

builddir=.
top_srcdir=/usr/local/apache-2.2.9
top_builddir=/usr/local/apache-2.2.9
include /usr/local/apache-2.2.9/build/special.mk

#   the used tools
APXS=apxs
APACHECTL=apache2ctl

#   additional defines, includes and libraries
#DEFS=-DDEBUG_MEMCACHED_INCLUDE
DEFS=
INCLUDES=-I/usr/local/apache-2.2.9/include/apr_memcache-0
LIBS=-L/usr/local/apache-2.2.9/lib -lapr_memcache

#   the default target
all: local-shared-build

#   install the shared object file into Apache 
install: install-modules-yes

#   cleanup
clean:
	-rm -f mod_memcached_include.o mod_memcached_include.lo mod_memcached_include.slo mod_memcached_include.la 

#   simple test
test: reload
	lynx -mime_header http://localhost/memcached_include

#   install and activate shared object by reloading Apache to
#   force a reload of the shared object file
reload: install restart

#   the general Apache start/restart/stop
#   procedures
start:
	$(APACHECTL) start
restart:
	$(APACHECTL) restart
stop:
	$(APACHECTL) stop

