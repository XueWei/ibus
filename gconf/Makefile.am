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

libibus = $(top_builddir)/src/libibus.la

INCLUDES = \
	-I$(top_srcdir)/src \
	$(NULL)

AM_CFLAGS = \
	@GLIB2_CFLAGS@ \
	@GCONF_CFLAGS@ \
	@DBUS_CFLAGS@ \
	-DG_LOG_DOMAIN=\"IBUS\" \
	$(INCLUDES) \
	$(NULL)
AM_LDFLAGS = \
	@GLIB2_LIBS@ \
	@GCONF_LIBS@ \
	@DBUS_LIBS@ \
	$(libibus) \
	$(NULL)

libexec_PROGRAMS = \
	ibus-gconf \
	$(NULL)

ibus_gconf_SOURCES = \
	main.c \
	config.c \
	config.h \
	$(NULL)
ibus_gconf_CFLAGS = \
	$(AM_CFLAGS) \
	$(NULL)
ibus_gconf_LDFLAGS = \
	$(AM_LDFLAGS) \
	$(NULL)

component_DATA = \
	gconf.xml \
	$(NULL)
componentdir = $(pkgdatadir)/component

CLEANFILES = \
	gconf.xml \
	*.pyc \
	$(NULL)

EXTRA_DIST = \
	gconf.xml.in.in \
	$(NULL)

gconf.xml: gconf.xml.in
	( \
		libexecdir=${libexecdir}; \
		s=`cat $<`; \
		eval "echo \"$${s}\""; \
	) > $@

$(libibus):
	$(MAKE) -C $(top_builddir)/src
