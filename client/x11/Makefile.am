# vim:set noet ts=4:
#
# ibus - The Input Bus
#
# Copyright (c) 2007-2009 Peng Huang <shawn.p.huang@gmail.com>
# Copyright (c) 2007-2009 Red Hat, Inc.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this program; if not, write to the
# Free Software Foundation, Inc., 59 Temple Place, Suite 330,
# Boston, MA  02111-1307  USA


libIMdkit = $(top_builddir)/util/IMdkit/libIMdkit.la
libibus = $(top_builddir)/src/libibus.la

libexec_PROGRAMS = ibus-x11

ibus_x11_SOURCES = \
	main.c \
	gdk-private.c \
	$(NULL)

ibus_x11_DEPENDENCIES = \
	$(libIMdkit) \
	$(libibus) \
	$(NULL)

ibus_x11_LDADD = \
	$(libIMdkit) \
	$(libibus) \
	$(NULL)

ibus_x11_LDFLAGS = \
	@GTK2_LIBS@ \
	$(NULL)

ibus_x11_CFLAGS = \
	@GTK2_CFLAGS@ \
	@DBUS_CFLAGS@ \
	-I$(top_srcdir)/util/IMdkit \
	-I$(top_srcdir)/src \
	$(NULL)

noinst_HEADERS = \
	gdk-private.h \
	locales.h \
	$(NULL)

$(IMdkit):
	(cd $(top_builddir)/util/IMdkit; make)

$(libibus):
	(cd $(top_builddir)/src; make)

locales.h:
	( \
		locale -a | \
		gawk -F _ '{ if (length($$1) <= 3) print $$1 }' | \
		uniq | sort | \
		xargs python -c 'import sys;print "#define LOCALES_STRING \"%s\"" % ",".join(sys.argv[1:])' \
	 ) > $@
