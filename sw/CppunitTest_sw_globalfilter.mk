# -*- Mode: makefile-gmake; tab-width: 4; indent-tabs-mode: t -*-
#*************************************************************************
#
# This file is part of the LibreOffice project.
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
#*************************************************************************

$(eval $(call gb_CppunitTest_CppunitTest,sw_globalfilter))

$(eval $(call gb_CppunitTest_use_common_precompiled_header,sw_globalfilter))

$(eval $(call gb_CppunitTest_add_exception_objects,sw_globalfilter, \
    sw/qa/extras/globalfilter/globalfilter \
))

$(eval $(call gb_CppunitTest_use_libraries,sw_globalfilter, \
    comphelper \
    cppu \
    cppuhelper \
    sal \
    sfx \
    subsequenttest \
    svt \
    sw \
	swqahelper \
    test \
    tl \
    unotest \
    utl \
    vcl \
))

$(eval $(call gb_CppunitTest_use_externals,sw_globalfilter,\
    boost_headers \
    libxml2 \
))

$(eval $(call gb_CppunitTest_set_include,sw_globalfilter,\
    -I$(SRCDIR)/sw/inc \
    -I$(SRCDIR)/sw/source/core/inc \
    -I$(SRCDIR)/sw/source/uibase/inc \
    -I$(SRCDIR)/sw/qa/inc \
    $$(INCLUDE) \
))

$(eval $(call gb_CppunitTest_use_api,sw_globalfilter,\
	udkapi \
	offapi \
	oovbaapi \
))

$(eval $(call gb_CppunitTest_use_ure,sw_globalfilter))
$(eval $(call gb_CppunitTest_use_vcl,sw_globalfilter))

$(eval $(call gb_CppunitTest_use_rdb,sw_globalfilter,services))

$(eval $(call gb_CppunitTest_use_custom_headers,sw_globalfilter,\
	officecfg/registry \
))

$(eval $(call gb_CppunitTest_use_configuration,sw_globalfilter))

$(eval $(call gb_CppunitTest_add_arguments,sw_globalfilter, \
    -env:arg-env=$(gb_Helper_LIBRARY_PATH_VAR)"$$$${$(gb_Helper_LIBRARY_PATH_VAR)+=$$$$$(gb_Helper_LIBRARY_PATH_VAR)}" \
))

# vim: set noet sw=4 ts=4:
