# -*- Mode: makefile-gmake; tab-width: 4; indent-tabs-mode: t -*-
#
# This file is part of the LibreOffice project.
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#

$(eval $(call gb_UnpackedTarball_UnpackedTarball,libxml2))

$(eval $(call gb_UnpackedTarball_set_tarball,libxml2,$(LIBXML_TARBALL),,libxml2))

$(eval $(call gb_UnpackedTarball_update_autoconf_configs,libxml2))

$(eval $(call gb_UnpackedTarball_set_patchlevel,libxml2,2))

# 0001-const-up-allowPCData.patch effort to upstream at:
# https://gitlab.gnome.org/GNOME/libxml2/-/merge_requests/333

$(eval $(call gb_UnpackedTarball_add_patches,libxml2,\
	$(if $(filter SOLARIS,$(OS)),external/libxml2/libxml2-global-symbols.patch) \
	external/libxml2/libxml2-vc10.patch \
	external/libxml2/libxml2-XMLCALL-redefine.patch.0 \
	external/libxml2/makefile.msvc-entry-point.patch.0 \
	external/libxml2/deprecated.patch.0 \
	external/libxml2/0001-const-up-allowPCData.patch.1 \
	$(if $(filter ANDROID,$(OS)),external/libxml2/libxml2-android.patch) \
	$(if $(gb_Module_CURRENTMODULE_SYMBOLS_ENABLED), \
		external/libxml2/libxml2-icu-sym.patch.0, \
		external/libxml2/libxml2-icu.patch.0) \
))

$(eval $(call gb_UnpackedTarball_add_file,libxml2,xml2-config.in,external/libxml2/xml2-config.in))

# vim: set noet sw=4 ts=4:
