# -*- Mode: makefile-gmake; tab-width: 4; indent-tabs-mode: t -*-
#
# This file is part of the LibreOffice project.
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#

$(eval $(call gb_CustomTarget_CustomTarget,postprocess/signing))


$(eval $(call gb_CustomTarget_register_targets,postprocess/signing,\
	signing.done \
))

# PFXFILE and PFXPASSWORD should be set in environment
TIMESTAMPURL ?= "http://timestamp.globalsign.com/scripts/timestamp.dll"

$(gb_CustomTarget_workdir)/postprocess/signing/signing.done: \
	$(SRCDIR)/postprocess/signing/signing.pl \
	$(call gb_Module_get_target,extras) \
	$(call gb_Postprocess_get_target,AllLibraries) \
	$(call gb_Postprocess_get_target,AllExecutables) \
	$(call gb_Postprocess_get_target,AllModuleTests) \
	$(call gb_Postprocess_get_target,AllModuleSlowtests)

$(gb_CustomTarget_workdir)/postprocess/signing/signing.done:
	$(call gb_Output_announce,$(subst $(WORKDIR)/,,$@),$(true),PRL,2)
	$(call gb_Trace_StartRange,$(subst $(WORKDIR)/,,$@),PRL)
ifeq ($(COM),MSC)
	EXCLUDELIST=$(shell $(gb_MKTEMP)) && \
	echo "$(foreach lib,$(gb_MERGEDLIBS),$(call gb_Library_get_filename,$(lib)))" | tr ' ' '\n' > $$EXCLUDELIST && \
	$(PERL) $(SRCDIR)/postprocess/signing/signing.pl \
			-e $$EXCLUDELIST \
			-l $(subst .done,_log.txt,$@) \
			$(if $(verbose),-v) \
			$(if $(PFXFILE),-f $(PFXFILE)) \
			$(if $(PFXPASSWORD),-p $(PFXPASSWORD)) \
			$(if $(TIMESTAMPURL),-t $(TIMESTAMPURL)) \
			$(INSTDIR)/URE/bin/*.dll \
			$(INSTDIR)/URE/bin/*.exe \
			$(INSTDIR)/program/*.dll \
			$(INSTDIR)/program/*.exe \
			$(INSTDIR)/program/*.com \
			$(INSTDIR)/program/soffice.bin \
			$(INSTDIR)/program/unopkg.bin \
			$(INSTDIR)/program/pyuno$(if $(MSVC_USE_DEBUG_RUNTIME),_d).pyd \
			$(INSTDIR)/$(LIBO_BIN_FOLDER)/python-core-$(PYTHON_VERSION)/bin/*.exe \
			$(INSTDIR)/$(LIBO_BIN_FOLDER)/python-core-$(PYTHON_VERSION)/lib/*.dll \
			$(INSTDIR)/$(LIBO_BIN_FOLDER)/python-core-$(PYTHON_VERSION)/lib/*.pyd \
			$(INSTDIR)/$(LIBO_BIN_FOLDER)/python-core-$(PYTHON_VERSION)/lib/distutils/command/*.exe \
			$(INSTDIR)/program/shlxthdl/*.dll \
			$(INSTDIR)/intl/*.dll \
			$(INSTDIR)/sdk/cli/*.dll \
			$(INSTDIR)/sdk/bin/*.exe \
	&& rm $$EXCLUDELIST && touch $@
else
	@echo "Nothing to do, signing is Windows (MSC) only."
endif
	$(call gb_Trace_EndRange,$(subst $(WORKDIR)/,,$@),PRL)

# vim: set noet sw=4 ts=4:
