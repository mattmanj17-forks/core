# -*- Mode: makefile-gmake; tab-width: 4; indent-tabs-mode: t -*-
#
# This file is part of the LibreOffice project.
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#

$(eval $(call gb_UnpackedTarball_UnpackedTarball,python3))

$(eval $(call gb_UnpackedTarball_set_tarball,python3,$(PYTHON_TARBALL),,python3))

# Since Python 3.11, _freeze_module.vcxproj needs python.exe to build deepfreeze.c on Windows
# Since a wheel file is just a zip file, unzil them to the Lib directory with the other libraries
$(eval $(call gb_UnpackedTarball_set_pre_action,python3,\
	$(if $(filter WNT,$(OS)), \
		mkdir -p externals/pythonx86 && \
		unzip -q -d externals/pythonx86 -o $(gb_UnpackedTarget_TARFILE_LOCATION)/$(PYTHON_BOOTSTRAP_TARBALL) && \
		chmod +x externals/pythonx86/tools/* && \
	) \
	unzip -q -d Lib/ -o Lib/test/wheeldata/setuptools-67.6.1-py3-none-any.whl && \
	unzip -q -d Lib/ -o Lib/ensurepip/_bundled/pip-25.0.1-py3-none-any.whl \
))

$(eval $(call gb_UnpackedTarball_fix_end_of_line,python3,\
	PCbuild/libffi.props \
	PCbuild/pcbuild.sln \
))

ifneq ($(MSYSTEM),)
# use binary flag so patch from git-bash won't choke on mixed line-endings in patches
$(eval $(call gb_UnpackedTarball_set_patchflags,python3,--binary))
endif

$(eval $(call gb_UnpackedTarball_add_patches,python3,\
	external/python3/i100492-freebsd.patch.1 \
	external/python3/python-3.3.0-darwin.patch.1 \
	external/python3/python-3.7.6-msvc-ssl.patch.1 \
	external/python3/python-3.5.4-msvc-disable.patch.1 \
	external/python3/ubsan.patch.0 \
	external/python3/init-sys-streams-cant-initialize-stdin.patch.0 \
))

ifneq ($(filter DRAGONFLY FREEBSD LINUX NETBSD OPENBSD SOLARIS,$(OS)),)
$(eval $(call gb_UnpackedTarball_add_patches,python3,\
	external/python3/python-3.3.3-elf-rpath.patch.1 \
))
endif

ifneq ($(ENABLE_RUNTIME_OPTIMIZATIONS),TRUE)
$(eval $(call gb_UnpackedTarball_add_patches,python3,\
	external/python3/python-3.3.3-disable-obmalloc.patch.0 \
))
endif

ifneq ($(SYSTEM_ZLIB),TRUE)
$(eval $(call gb_UnpackedTarball_add_patches,python3, \
    external/python3/internal-zlib.patch.0 \
))
endif

# vim: set noet sw=4 ts=4:
