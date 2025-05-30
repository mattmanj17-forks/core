# -*- Mode: makefile-gmake; tab-width: 4; indent-tabs-mode: t -*-
#
# This file is part of the LibreOffice project.
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#

$(eval $(call gb_Library_Library,pdfium))

$(eval $(call gb_Library_use_unpacked,pdfium,pdfium))

$(eval $(call gb_Library_set_warnings_disabled,pdfium))

$(eval $(call gb_Library_use_clang,pdfium))
#This currently results in all sorts of compile complaints
#$(eval $(call gb_Library_set_precompiled_header,pdfium,external/pdfium/inc/pch/precompiled_pdfium))
# The clang-cl provided with at least VS 2019 16.11.28 is known-broken with -std:c++20:
ifneq ($(filter -std:c++20,$(CXXFLAGS_CXX11)),)
ifeq ($(LO_CLANG_VERSION),120000)
$(eval $(call gb_Library_add_cxxflags,pdfium, \
    -std:c++17 \
))
endif
endif

$(eval $(call gb_Library_set_include,pdfium,\
    -I$(gb_UnpackedTarball_workdir)/pdfium \
    -I$(gb_UnpackedTarball_workdir)/pdfium/third_party \
    -I$(gb_UnpackedTarball_workdir)/pdfium/third_party/agg23 \
    $(if $(filter TRUE,$(SYSTEM_ABSEIL)),$(ABSEIL_CFLAGS),-I$(gb_UnpackedTarball_workdir)/pdfium/third_party/abseil-cpp) \
    $(if $(filter TRUE,$(SYSTEM_OPENJPEG2)),$(OPENJPEG2_CFLAGS)) \
    $$(INCLUDE) \
))

$(eval $(call gb_Library_add_defs,pdfium,\
    -DFPDF_IMPLEMENTATION \
    -DUSE_SYSTEM_LCMS2 \
    -DUSE_SYSTEM_LIBJPEG \
    -DUSE_SYSTEM_ZLIB \
    -DUSE_SYSTEM_ICUUC \
    -DMEMORY_TOOL_REPLACES_ALLOCATOR \
    -DUNICODE \
    -DWIN32_LEAN_AND_MEAN \
    -DCOMPONENT_BUILD \
))
ifeq ($(SYSTEM_OPENJPEG2),TRUE)
$(eval $(call gb_Library_add_defs,pdfium,\
    -DUSE_SYSTEM_LIBOPENJPEG2 \
))
endif
ifeq ($(SYSTEM_ABSEIL),TRUE)
$(eval $(call gb_Library_add_defs,pdfium,\
    -DUSE_SYSTEM_ABSEIL \
))
endif


$(eval $(call gb_Library_set_generated_cxx_suffix,pdfium,cpp))

# pdfium
$(eval $(call gb_Library_add_generated_exception_objects,pdfium,\
    UnpackedTarball/pdfium/fpdfsdk/cpdfsdk_annot \
    UnpackedTarball/pdfium/fpdfsdk/cpdfsdk_annotiteration \
    UnpackedTarball/pdfium/fpdfsdk/cpdfsdk_baannot \
    UnpackedTarball/pdfium/fpdfsdk/cpdfsdk_formfillenvironment \
    UnpackedTarball/pdfium/fpdfsdk/cpdfsdk_pageview \
    UnpackedTarball/pdfium/fpdfsdk/cpdfsdk_widget \
    UnpackedTarball/pdfium/fpdfsdk/fpdf_dataavail \
    UnpackedTarball/pdfium/fpdfsdk/fpdf_ext \
    UnpackedTarball/pdfium/fpdfsdk/fpdf_flatten \
    UnpackedTarball/pdfium/fpdfsdk/fpdf_progressive \
    UnpackedTarball/pdfium/fpdfsdk/fpdf_searchex \
    UnpackedTarball/pdfium/fpdfsdk/fpdf_structtree \
    UnpackedTarball/pdfium/fpdfsdk/fpdf_sysfontinfo \
    UnpackedTarball/pdfium/fpdfsdk/fpdf_transformpage \
    UnpackedTarball/pdfium/fpdfsdk/cpdfsdk_annotiterator \
    UnpackedTarball/pdfium/fpdfsdk/cpdfsdk_customaccess \
    UnpackedTarball/pdfium/fpdfsdk/cpdfsdk_filewriteadapter \
    UnpackedTarball/pdfium/fpdfsdk/cpdfsdk_helpers \
    UnpackedTarball/pdfium/fpdfsdk/fpdf_annot \
    UnpackedTarball/pdfium/fpdfsdk/fpdf_attachment \
    UnpackedTarball/pdfium/fpdfsdk/fpdf_catalog \
    UnpackedTarball/pdfium/fpdfsdk/fpdf_doc \
    UnpackedTarball/pdfium/fpdfsdk/fpdf_editimg \
    UnpackedTarball/pdfium/fpdfsdk/fpdf_editpage \
    UnpackedTarball/pdfium/fpdfsdk/fpdf_editpath \
    UnpackedTarball/pdfium/fpdfsdk/fpdf_edittext \
    UnpackedTarball/pdfium/fpdfsdk/fpdf_formfill \
    UnpackedTarball/pdfium/fpdfsdk/fpdf_save \
    UnpackedTarball/pdfium/fpdfsdk/fpdf_text \
    UnpackedTarball/pdfium/fpdfsdk/fpdf_view \
    UnpackedTarball/pdfium/fpdfsdk/cpdfsdk_pauseadapter \
    UnpackedTarball/pdfium/fpdfsdk/cpdfsdk_interactiveform \
    UnpackedTarball/pdfium/fpdfsdk/cpdfsdk_renderpage \
    UnpackedTarball/pdfium/fpdfsdk/fpdf_signature \
	UnpackedTarball/pdfium/constants/annotation_common \
	UnpackedTarball/pdfium/constants/appearance \
	UnpackedTarball/pdfium/constants/form_fields \
	UnpackedTarball/pdfium/constants/page_object \
	UnpackedTarball/pdfium/constants/stream_dict_common \
	UnpackedTarball/pdfium/constants/transparency \
	UnpackedTarball/pdfium/constants/font_encodings \
))

# fdrm
$(eval $(call gb_Library_add_generated_exception_objects,pdfium,\
    UnpackedTarball/pdfium/core/fdrm/fx_crypt \
    UnpackedTarball/pdfium/core/fdrm/fx_crypt_aes \
    UnpackedTarball/pdfium/core/fdrm/fx_crypt_sha \
))

# formfiller
$(eval $(call gb_Library_add_generated_exception_objects,pdfium,\
    UnpackedTarball/pdfium/fpdfsdk/formfiller/cffl_checkbox \
    UnpackedTarball/pdfium/fpdfsdk/formfiller/cffl_combobox \
    UnpackedTarball/pdfium/fpdfsdk/formfiller/cffl_formfield \
    UnpackedTarball/pdfium/fpdfsdk/formfiller/cffl_interactiveformfiller \
    UnpackedTarball/pdfium/fpdfsdk/formfiller/cffl_listbox \
    UnpackedTarball/pdfium/fpdfsdk/formfiller/cffl_pushbutton \
    UnpackedTarball/pdfium/fpdfsdk/formfiller/cffl_radiobutton \
    UnpackedTarball/pdfium/fpdfsdk/formfiller/cffl_textfield \
    UnpackedTarball/pdfium/fpdfsdk/formfiller/cffl_button \
    UnpackedTarball/pdfium/fpdfsdk/formfiller/cffl_textobject \
    UnpackedTarball/pdfium/fpdfsdk/formfiller/cffl_fieldaction \
    UnpackedTarball/pdfium/fpdfsdk/formfiller/cffl_perwindowdata \
))

# fpdfapi
$(eval $(call gb_Library_add_generated_exception_objects,pdfium,\
    UnpackedTarball/pdfium/core/fpdfapi/cmaps/CNS1/Adobe-CNS1-UCS2_5 \
    UnpackedTarball/pdfium/core/fpdfapi/cmaps/CNS1/B5pc-H_0 \
    UnpackedTarball/pdfium/core/fpdfapi/cmaps/CNS1/B5pc-V_0 \
    UnpackedTarball/pdfium/core/fpdfapi/cmaps/CNS1/CNS-EUC-H_0 \
    UnpackedTarball/pdfium/core/fpdfapi/cmaps/CNS1/CNS-EUC-V_0 \
    UnpackedTarball/pdfium/core/fpdfapi/cmaps/CNS1/ETen-B5-H_0 \
    UnpackedTarball/pdfium/core/fpdfapi/cmaps/CNS1/ETen-B5-V_0 \
    UnpackedTarball/pdfium/core/fpdfapi/cmaps/CNS1/ETenms-B5-H_0 \
    UnpackedTarball/pdfium/core/fpdfapi/cmaps/CNS1/ETenms-B5-V_0 \
    UnpackedTarball/pdfium/core/fpdfapi/cmaps/CNS1/HKscs-B5-H_5 \
    UnpackedTarball/pdfium/core/fpdfapi/cmaps/CNS1/HKscs-B5-V_5 \
    UnpackedTarball/pdfium/core/fpdfapi/cmaps/CNS1/UniCNS-UCS2-H_3 \
    UnpackedTarball/pdfium/core/fpdfapi/cmaps/CNS1/UniCNS-UCS2-V_3 \
    UnpackedTarball/pdfium/core/fpdfapi/cmaps/CNS1/UniCNS-UTF16-H_0 \
    UnpackedTarball/pdfium/core/fpdfapi/cmaps/CNS1/cmaps_cns1 \
    UnpackedTarball/pdfium/core/fpdfapi/cmaps/GB1/Adobe-GB1-UCS2_5 \
    UnpackedTarball/pdfium/core/fpdfapi/cmaps/GB1/GB-EUC-H_0 \
    UnpackedTarball/pdfium/core/fpdfapi/cmaps/GB1/GB-EUC-V_0 \
    UnpackedTarball/pdfium/core/fpdfapi/cmaps/GB1/GBK-EUC-H_2 \
    UnpackedTarball/pdfium/core/fpdfapi/cmaps/GB1/GBK-EUC-V_2 \
    UnpackedTarball/pdfium/core/fpdfapi/cmaps/GB1/GBK2K-H_5 \
    UnpackedTarball/pdfium/core/fpdfapi/cmaps/GB1/GBK2K-V_5 \
    UnpackedTarball/pdfium/core/fpdfapi/cmaps/GB1/GBKp-EUC-H_2 \
    UnpackedTarball/pdfium/core/fpdfapi/cmaps/GB1/GBKp-EUC-V_2 \
    UnpackedTarball/pdfium/core/fpdfapi/cmaps/GB1/GBpc-EUC-H_0 \
    UnpackedTarball/pdfium/core/fpdfapi/cmaps/GB1/GBpc-EUC-V_0 \
    UnpackedTarball/pdfium/core/fpdfapi/cmaps/GB1/UniGB-UCS2-H_4 \
    UnpackedTarball/pdfium/core/fpdfapi/cmaps/GB1/UniGB-UCS2-V_4 \
    UnpackedTarball/pdfium/core/fpdfapi/cmaps/GB1/cmaps_gb1 \
    UnpackedTarball/pdfium/core/fpdfapi/cmaps/Japan1/83pv-RKSJ-H_1 \
    UnpackedTarball/pdfium/core/fpdfapi/cmaps/Japan1/90ms-RKSJ-H_2 \
    UnpackedTarball/pdfium/core/fpdfapi/cmaps/Japan1/90ms-RKSJ-V_2 \
    UnpackedTarball/pdfium/core/fpdfapi/cmaps/Japan1/90msp-RKSJ-H_2 \
    UnpackedTarball/pdfium/core/fpdfapi/cmaps/Japan1/90msp-RKSJ-V_2 \
    UnpackedTarball/pdfium/core/fpdfapi/cmaps/Japan1/90pv-RKSJ-H_1 \
    UnpackedTarball/pdfium/core/fpdfapi/cmaps/Japan1/Add-RKSJ-H_1 \
    UnpackedTarball/pdfium/core/fpdfapi/cmaps/Japan1/Add-RKSJ-V_1 \
    UnpackedTarball/pdfium/core/fpdfapi/cmaps/Japan1/Adobe-Japan1-UCS2_4 \
    UnpackedTarball/pdfium/core/fpdfapi/cmaps/Japan1/EUC-H_1 \
    UnpackedTarball/pdfium/core/fpdfapi/cmaps/Japan1/EUC-V_1 \
    UnpackedTarball/pdfium/core/fpdfapi/cmaps/Japan1/Ext-RKSJ-H_2 \
    UnpackedTarball/pdfium/core/fpdfapi/cmaps/Japan1/Ext-RKSJ-V_2 \
    UnpackedTarball/pdfium/core/fpdfapi/cmaps/Japan1/H_1 \
    UnpackedTarball/pdfium/core/fpdfapi/cmaps/Japan1/UniJIS-UCS2-HW-H_4 \
    UnpackedTarball/pdfium/core/fpdfapi/cmaps/Japan1/UniJIS-UCS2-HW-V_4 \
    UnpackedTarball/pdfium/core/fpdfapi/cmaps/Japan1/UniJIS-UCS2-H_4 \
    UnpackedTarball/pdfium/core/fpdfapi/cmaps/Japan1/UniJIS-UCS2-V_4 \
    UnpackedTarball/pdfium/core/fpdfapi/cmaps/Japan1/V_1 \
    UnpackedTarball/pdfium/core/fpdfapi/cmaps/Japan1/cmaps_japan1 \
    UnpackedTarball/pdfium/core/fpdfapi/cmaps/Korea1/Adobe-Korea1-UCS2_2 \
    UnpackedTarball/pdfium/core/fpdfapi/cmaps/Korea1/KSC-EUC-H_0 \
    UnpackedTarball/pdfium/core/fpdfapi/cmaps/Korea1/KSC-EUC-V_0 \
    UnpackedTarball/pdfium/core/fpdfapi/cmaps/Korea1/KSCms-UHC-HW-H_1 \
    UnpackedTarball/pdfium/core/fpdfapi/cmaps/Korea1/KSCms-UHC-HW-V_1 \
    UnpackedTarball/pdfium/core/fpdfapi/cmaps/Korea1/KSCms-UHC-H_1 \
    UnpackedTarball/pdfium/core/fpdfapi/cmaps/Korea1/KSCms-UHC-V_1 \
    UnpackedTarball/pdfium/core/fpdfapi/cmaps/Korea1/KSCpc-EUC-H_0 \
    UnpackedTarball/pdfium/core/fpdfapi/cmaps/Korea1/UniKS-UCS2-H_1 \
    UnpackedTarball/pdfium/core/fpdfapi/cmaps/Korea1/UniKS-UCS2-V_1 \
    UnpackedTarball/pdfium/core/fpdfapi/cmaps/Korea1/UniKS-UTF16-H_0 \
    UnpackedTarball/pdfium/core/fpdfapi/cmaps/Korea1/cmaps_korea1 \
    UnpackedTarball/pdfium/core/fpdfapi/cmaps/fpdf_cmaps \
    UnpackedTarball/pdfium/core/fpdfapi/edit/cpdf_pagecontentgenerator \
    UnpackedTarball/pdfium/core/fpdfapi/edit/cpdf_npagetooneexporter \
    UnpackedTarball/pdfium/core/fpdfapi/edit/cpdf_pageexporter \
    UnpackedTarball/pdfium/core/fpdfapi/edit/cpdf_pageorganizer \
    UnpackedTarball/pdfium/core/fpdfapi/font/cpdf_cidfont \
    UnpackedTarball/pdfium/core/fpdfapi/font/cpdf_font \
    UnpackedTarball/pdfium/core/fpdfapi/font/cpdf_fontencoding \
    UnpackedTarball/pdfium/core/fpdfapi/font/cpdf_simplefont \
    UnpackedTarball/pdfium/core/fpdfapi/font/cpdf_truetypefont \
    UnpackedTarball/pdfium/core/fpdfapi/font/cpdf_type1font \
    UnpackedTarball/pdfium/core/fpdfapi/font/cpdf_type3char \
    UnpackedTarball/pdfium/core/fpdfapi/font/cpdf_type3font \
    UnpackedTarball/pdfium/core/fpdfapi/page/cpdf_allstates \
    UnpackedTarball/pdfium/core/fpdfapi/page/cpdf_clippath \
    UnpackedTarball/pdfium/core/fpdfapi/page/cpdf_color \
    UnpackedTarball/pdfium/core/fpdfapi/page/cpdf_colorspace \
    UnpackedTarball/pdfium/core/fpdfapi/page/cpdf_colorstate \
    UnpackedTarball/pdfium/core/fpdfapi/page/cpdf_contentmarks \
    UnpackedTarball/pdfium/core/fpdfapi/page/cpdf_contentmarkitem \
    UnpackedTarball/pdfium/core/fpdfapi/page/cpdf_contentparser \
    UnpackedTarball/pdfium/core/fpdfapi/page/cpdf_docpagedata \
    UnpackedTarball/pdfium/core/fpdfapi/page/cpdf_form \
    UnpackedTarball/pdfium/core/fpdfapi/page/cpdf_formobject \
    UnpackedTarball/pdfium/core/fpdfapi/page/cpdf_generalstate \
    UnpackedTarball/pdfium/core/fpdfapi/page/cpdf_graphicstates \
    UnpackedTarball/pdfium/core/fpdfapi/page/cpdf_image \
    UnpackedTarball/pdfium/core/fpdfapi/page/cpdf_imageobject \
    UnpackedTarball/pdfium/core/fpdfapi/page/cpdf_meshstream \
    UnpackedTarball/pdfium/core/fpdfapi/page/cpdf_page \
    UnpackedTarball/pdfium/core/fpdfapi/page/cpdf_pagemodule \
    UnpackedTarball/pdfium/core/fpdfapi/page/cpdf_pageobject \
    UnpackedTarball/pdfium/core/fpdfapi/page/cpdf_pageobjectholder \
    UnpackedTarball/pdfium/core/fpdfapi/page/cpdf_path \
    UnpackedTarball/pdfium/core/fpdfapi/page/cpdf_pathobject \
    UnpackedTarball/pdfium/core/fpdfapi/page/cpdf_pattern \
    UnpackedTarball/pdfium/core/fpdfapi/page/cpdf_shadingobject \
    UnpackedTarball/pdfium/core/fpdfapi/page/cpdf_shadingpattern \
    UnpackedTarball/pdfium/core/fpdfapi/page/cpdf_streamcontentparser \
    UnpackedTarball/pdfium/core/fpdfapi/page/cpdf_streamparser \
    UnpackedTarball/pdfium/core/fpdfapi/page/cpdf_textobject \
    UnpackedTarball/pdfium/core/fpdfapi/page/cpdf_textstate \
    UnpackedTarball/pdfium/core/fpdfapi/page/cpdf_tilingpattern \
    UnpackedTarball/pdfium/core/fpdfapi/parser/cfdf_document \
    UnpackedTarball/pdfium/core/fpdfapi/parser/cpdf_array \
    UnpackedTarball/pdfium/core/fpdfapi/parser/cpdf_boolean \
    UnpackedTarball/pdfium/core/fpdfapi/parser/cpdf_crypto_handler \
    UnpackedTarball/pdfium/core/fpdfapi/parser/cpdf_data_avail \
    UnpackedTarball/pdfium/core/fpdfapi/parser/cpdf_dictionary \
    UnpackedTarball/pdfium/core/fpdfapi/parser/cpdf_document \
    UnpackedTarball/pdfium/core/fpdfapi/parser/cpdf_hint_tables \
    UnpackedTarball/pdfium/core/fpdfapi/parser/cpdf_indirect_object_holder \
    UnpackedTarball/pdfium/core/fpdfapi/parser/cpdf_linearized_header \
    UnpackedTarball/pdfium/core/fpdfapi/parser/cpdf_name \
    UnpackedTarball/pdfium/core/fpdfapi/parser/cpdf_null \
    UnpackedTarball/pdfium/core/fpdfapi/parser/cpdf_number \
    UnpackedTarball/pdfium/core/fpdfapi/parser/cpdf_object \
    UnpackedTarball/pdfium/core/fpdfapi/parser/cpdf_parser \
    UnpackedTarball/pdfium/core/fpdfapi/parser/cpdf_reference \
    UnpackedTarball/pdfium/core/fpdfapi/parser/cpdf_security_handler \
    UnpackedTarball/pdfium/core/fpdfapi/parser/cpdf_simple_parser \
    UnpackedTarball/pdfium/core/fpdfapi/parser/cpdf_stream \
    UnpackedTarball/pdfium/core/fpdfapi/parser/cpdf_stream_acc \
    UnpackedTarball/pdfium/core/fpdfapi/parser/cpdf_string \
    UnpackedTarball/pdfium/core/fpdfapi/parser/cpdf_syntax_parser \
    UnpackedTarball/pdfium/core/fpdfapi/parser/fpdf_parser_decode \
    UnpackedTarball/pdfium/core/fpdfapi/parser/fpdf_parser_utility \
    UnpackedTarball/pdfium/core/fpdfapi/parser/cpdf_object_walker \
    UnpackedTarball/pdfium/core/fpdfapi/parser/cpdf_read_validator \
    UnpackedTarball/pdfium/core/fpdfapi/render/charposlist \
    UnpackedTarball/pdfium/core/fpdfapi/render/cpdf_devicebuffer \
    UnpackedTarball/pdfium/core/fpdfapi/render/cpdf_docrenderdata \
    UnpackedTarball/pdfium/core/fpdfapi/page/cpdf_imageloader \
    UnpackedTarball/pdfium/core/fpdfapi/render/cpdf_imagerenderer \
    UnpackedTarball/pdfium/core/fpdfapi/page/cpdf_pageimagecache \
    UnpackedTarball/pdfium/core/fpdfapi/render/cpdf_progressiverenderer \
    UnpackedTarball/pdfium/core/fpdfapi/render/cpdf_rendercontext \
    UnpackedTarball/pdfium/core/fpdfapi/render/cpdf_renderoptions \
    UnpackedTarball/pdfium/core/fpdfapi/render/cpdf_renderstatus \
    UnpackedTarball/pdfium/core/fpdfapi/render/cpdf_scaledrenderbuffer \
    UnpackedTarball/pdfium/core/fpdfapi/render/cpdf_textrenderer \
    UnpackedTarball/pdfium/core/fpdfapi/render/cpdf_type3cache \
    UnpackedTarball/pdfium/core/fpdfapi/render/cpdf_type3glyphmap \
    UnpackedTarball/pdfium/core/fpdfapi/render/cpdf_rendershading \
    UnpackedTarball/pdfium/core/fpdfapi/render/cpdf_rendertiling \
    UnpackedTarball/pdfium/core/fpdfapi/edit/cpdf_creator \
    UnpackedTarball/pdfium/core/fpdfapi/parser/cpdf_encryptor \
    UnpackedTarball/pdfium/core/fpdfapi/parser/cpdf_flateencoder \
    UnpackedTarball/pdfium/core/fpdfapi/font/cfx_cttgsubtable \
    UnpackedTarball/pdfium/core/fpdfapi/font/cfx_stockfontarray \
    UnpackedTarball/pdfium/core/fpdfapi/font/cpdf_cid2unicodemap \
    UnpackedTarball/pdfium/core/fpdfapi/font/cpdf_cmap \
    UnpackedTarball/pdfium/core/fpdfapi/font/cpdf_cmapparser \
    UnpackedTarball/pdfium/core/fpdfapi/font/cpdf_fontglobals \
    UnpackedTarball/pdfium/core/fpdfapi/font/cpdf_tounicodemap \
    UnpackedTarball/pdfium/core/fpdfapi/page/cpdf_devicecs \
    UnpackedTarball/pdfium/core/fpdfapi/page/cpdf_expintfunc \
    UnpackedTarball/pdfium/core/fpdfapi/page/cpdf_function \
    UnpackedTarball/pdfium/core/fpdfapi/page/cpdf_iccprofile \
    UnpackedTarball/pdfium/core/fpdfapi/page/cpdf_patterncs \
    UnpackedTarball/pdfium/core/fpdfapi/page/cpdf_psengine \
    UnpackedTarball/pdfium/core/fpdfapi/page/cpdf_psfunc \
    UnpackedTarball/pdfium/core/fpdfapi/page/cpdf_sampledfunc \
    UnpackedTarball/pdfium/core/fpdfapi/page/cpdf_stitchfunc \
    UnpackedTarball/pdfium/core/fpdfapi/parser/cpdf_object_avail \
    UnpackedTarball/pdfium/core/fpdfapi/parser/cpdf_page_object_avail \
    UnpackedTarball/pdfium/core/fpdfapi/parser/cpdf_cross_ref_avail \
    UnpackedTarball/pdfium/core/fpdfapi/edit/cpdf_pagecontentmanager \
    UnpackedTarball/pdfium/core/fpdfapi/page/cpdf_transparency \
    UnpackedTarball/pdfium/core/fpdfapi/page/cpdf_dib \
    UnpackedTarball/pdfium/core/fpdfapi/parser/cpdf_object_stream \
    UnpackedTarball/pdfium/core/fpdfapi/parser/cpdf_cross_ref_table \
    UnpackedTarball/pdfium/core/fpdfapi/edit/cpdf_stringarchivestream \
    UnpackedTarball/pdfium/core/fpdfapi/page/cpdf_occontext \
    UnpackedTarball/pdfium/core/fpdfapi/edit/cpdf_contentstream_write_utils \
    UnpackedTarball/pdfium/core/fpdfapi/page/cpdf_annotcontext \
    UnpackedTarball/pdfium/core/fpdfapi/render/cpdf_pagerendercontext \
    UnpackedTarball/pdfium/core/fpdfapi/page/cpdf_transferfuncdib \
    UnpackedTarball/pdfium/core/fpdfapi/page/cpdf_transferfunc \
    UnpackedTarball/pdfium/core/fpdfapi/page/cpdf_basedcs \
    UnpackedTarball/pdfium/core/fpdfapi/page/cpdf_indexedcs \
    UnpackedTarball/pdfium/core/fpdfapi/parser/object_tree_traversal_util \
))

# fpdfdoc
$(eval $(call gb_Library_add_generated_exception_objects,pdfium,\
    UnpackedTarball/pdfium/core/fpdfdoc/cpdf_aaction \
    UnpackedTarball/pdfium/core/fpdfdoc/cpdf_action \
    UnpackedTarball/pdfium/core/fpdfdoc/cpdf_annot \
    UnpackedTarball/pdfium/core/fpdfdoc/cpdf_annotlist \
    UnpackedTarball/pdfium/core/fpdfdoc/cpdf_apsettings \
    UnpackedTarball/pdfium/core/fpdfdoc/cpdf_bookmark \
    UnpackedTarball/pdfium/core/fpdfdoc/cpdf_bookmarktree \
    UnpackedTarball/pdfium/core/fpdfdoc/cpdf_defaultappearance \
    UnpackedTarball/pdfium/core/fpdfdoc/cpdf_dest \
    UnpackedTarball/pdfium/core/fpdfdoc/cpdf_filespec \
    UnpackedTarball/pdfium/core/fpdfdoc/cpdf_formcontrol \
    UnpackedTarball/pdfium/core/fpdfdoc/cpdf_formfield \
    UnpackedTarball/pdfium/core/fpdfdoc/cpdf_iconfit \
    UnpackedTarball/pdfium/core/fpdfdoc/cpdf_interactiveform \
    UnpackedTarball/pdfium/core/fpdfdoc/cpdf_link \
    UnpackedTarball/pdfium/core/fpdfdoc/cpdf_linklist \
    UnpackedTarball/pdfium/core/fpdfdoc/cpdf_metadata \
    UnpackedTarball/pdfium/core/fpdfdoc/cpdf_nametree \
    UnpackedTarball/pdfium/core/fpdfdoc/cpdf_numbertree \
    UnpackedTarball/pdfium/core/fpdfdoc/cpdf_pagelabel \
    UnpackedTarball/pdfium/core/fpdfdoc/cpdf_viewerpreferences \
    UnpackedTarball/pdfium/core/fpdfdoc/cpvt_fontmap \
    UnpackedTarball/pdfium/core/fpdfdoc/cpvt_wordinfo \
    UnpackedTarball/pdfium/core/fpdfdoc/cpdf_structelement \
    UnpackedTarball/pdfium/core/fpdfdoc/cpdf_structtree \
    UnpackedTarball/pdfium/core/fpdfdoc/cpdf_color_utils \
    UnpackedTarball/pdfium/core/fpdfdoc/cpdf_icon \
    UnpackedTarball/pdfium/core/fpdfdoc/cpdf_bafontmap \
    UnpackedTarball/pdfium/core/fpdfdoc/cpdf_generateap \
    UnpackedTarball/pdfium/core/fpdfdoc/cpvt_section \
    UnpackedTarball/pdfium/core/fpdfdoc/cpvt_variabletext \
))

# fpdftext
$(eval $(call gb_Library_add_generated_exception_objects,pdfium,\
    UnpackedTarball/pdfium/core/fpdftext/cpdf_linkextract \
    UnpackedTarball/pdfium/core/fpdftext/cpdf_textpage \
    UnpackedTarball/pdfium/core/fpdftext/cpdf_textpagefind \
    UnpackedTarball/pdfium/core/fpdftext/unicodenormalizationdata \
))

# fxcodec
$(eval $(call gb_Library_add_generated_exception_objects,pdfium,\
    UnpackedTarball/pdfium/core/fxcodec/fx_codec \
    UnpackedTarball/pdfium/core/fxcodec/jbig2/JBig2_ArithDecoder \
    UnpackedTarball/pdfium/core/fxcodec/jbig2/JBig2_ArithIntDecoder \
    UnpackedTarball/pdfium/core/fxcodec/jbig2/JBig2_BitStream \
    UnpackedTarball/pdfium/core/fxcodec/jbig2/JBig2_Context \
    UnpackedTarball/pdfium/core/fxcodec/jbig2/JBig2_GrdProc \
    UnpackedTarball/pdfium/core/fxcodec/jbig2/JBig2_GrrdProc \
    UnpackedTarball/pdfium/core/fxcodec/jbig2/JBig2_HtrdProc \
    UnpackedTarball/pdfium/core/fxcodec/jbig2/JBig2_HuffmanDecoder \
    UnpackedTarball/pdfium/core/fxcodec/jbig2/JBig2_HuffmanTable \
    UnpackedTarball/pdfium/core/fxcodec/jbig2/JBig2_Image \
    UnpackedTarball/pdfium/core/fxcodec/jbig2/JBig2_PatternDict \
    UnpackedTarball/pdfium/core/fxcodec/jbig2/JBig2_PddProc \
    UnpackedTarball/pdfium/core/fxcodec/jbig2/JBig2_SddProc \
    UnpackedTarball/pdfium/core/fxcodec/jbig2/JBig2_Segment \
    UnpackedTarball/pdfium/core/fxcodec/jbig2/JBig2_SymbolDict \
    UnpackedTarball/pdfium/core/fxcodec/jbig2/JBig2_TrdProc \
    UnpackedTarball/pdfium/core/fxcodec/gif/cfx_gif \
    UnpackedTarball/pdfium/core/fxcodec/gif/lzw_decompressor \
    UnpackedTarball/pdfium/core/fxcodec/cfx_codec_memory \
    UnpackedTarball/pdfium/core/fxcodec/fax/faxmodule \
    UnpackedTarball/pdfium/core/fxcodec/scanlinedecoder \
    UnpackedTarball/pdfium/core/fxcodec/jpeg/jpegmodule \
    UnpackedTarball/pdfium/core/fxcodec/jpx/cjpx_decoder \
    UnpackedTarball/pdfium/core/fxcodec/jpx/jpx_decode_utils \
    UnpackedTarball/pdfium/core/fxcodec/jbig2/JBig2_DocumentContext \
    UnpackedTarball/pdfium/core/fxcodec/basic/basicmodule \
    UnpackedTarball/pdfium/core/fxcodec/flate/flatemodule \
    UnpackedTarball/pdfium/core/fxcodec/icc/icc_transform \
    UnpackedTarball/pdfium/core/fxcodec/jbig2/jbig2_decoder \
    UnpackedTarball/pdfium/core/fxcodec/data_and_bytes_consumed \
))

$(eval $(call gb_Library_add_generated_cobjects,pdfium,\
    UnpackedTarball/pdfium/core/fxcodec/jpeg/jpeg_common \
))

# fxcrt
$(eval $(call gb_Library_add_generated_exception_objects,pdfium,\
    UnpackedTarball/pdfium/core/fxcrt/cfx_memorystream \
    UnpackedTarball/pdfium/core/fxcrt/cfx_seekablestreamproxy \
    UnpackedTarball/pdfium/core/fxcrt/fx_bidi \
    UnpackedTarball/pdfium/core/fxcrt/fx_coordinates \
    UnpackedTarball/pdfium/core/fxcrt/fx_extension \
    UnpackedTarball/pdfium/core/fxcrt/fx_memory \
    UnpackedTarball/pdfium/core/fxcrt/fx_stream \
    UnpackedTarball/pdfium/core/fxcrt/fx_system \
    UnpackedTarball/pdfium/core/fxcrt/fx_unicode \
    UnpackedTarball/pdfium/core/fxcrt/xml/cfx_xmldocument \
    UnpackedTarball/pdfium/core/fxcrt/xml/cfx_xmlelement \
    UnpackedTarball/pdfium/core/fxcrt/xml/cfx_xmlparser \
    UnpackedTarball/pdfium/core/fxcrt/xml/cfx_xmlnode \
    UnpackedTarball/pdfium/core/fxcrt/xml/cfx_xmlinstruction \
    UnpackedTarball/pdfium/core/fxcrt/xml/cfx_xmltext \
    UnpackedTarball/pdfium/core/fxcrt/xml/cfx_xmlchardata \
    UnpackedTarball/pdfium/core/fxcrt/css/cfx_csscolorvalue \
    UnpackedTarball/pdfium/core/fxcrt/css/cfx_csscomputedstyle \
    UnpackedTarball/pdfium/core/fxcrt/css/cfx_csscustomproperty \
    UnpackedTarball/pdfium/core/fxcrt/css/cfx_cssdeclaration \
    UnpackedTarball/pdfium/core/fxcrt/css/cfx_cssenumvalue \
    UnpackedTarball/pdfium/core/fxcrt/css/cfx_cssnumbervalue \
    UnpackedTarball/pdfium/core/fxcrt/css/cfx_csspropertyholder \
    UnpackedTarball/pdfium/core/fxcrt/css/cfx_cssrulecollection \
    UnpackedTarball/pdfium/core/fxcrt/css/cfx_cssselector \
    UnpackedTarball/pdfium/core/fxcrt/css/cfx_cssstringvalue \
    UnpackedTarball/pdfium/core/fxcrt/css/cfx_cssstylerule \
    UnpackedTarball/pdfium/core/fxcrt/css/cfx_cssstyleselector \
    UnpackedTarball/pdfium/core/fxcrt/css/cfx_cssstylesheet \
    UnpackedTarball/pdfium/core/fxcrt/css/cfx_csssyntaxparser \
    UnpackedTarball/pdfium/core/fxcrt/css/cfx_cssvalue \
    UnpackedTarball/pdfium/core/fxcrt/css/cfx_cssvaluelist \
    UnpackedTarball/pdfium/core/fxcrt/css/cfx_cssvaluelistparser \
    UnpackedTarball/pdfium/core/fxcrt/css/cfx_cssinputtextbuf \
    UnpackedTarball/pdfium/core/fxcrt/css/cfx_cssoutputtextbuf \
    UnpackedTarball/pdfium/core/fxcrt/cfx_datetime \
    UnpackedTarball/pdfium/core/fxcrt/bytestring \
    UnpackedTarball/pdfium/core/fxcrt/cfx_bitstream \
    UnpackedTarball/pdfium/core/fxcrt/fx_random \
    UnpackedTarball/pdfium/core/fxcrt/fx_string \
    UnpackedTarball/pdfium/core/fxcrt/widestring \
    UnpackedTarball/pdfium/core/fxcrt/css/cfx_cssdata \
    UnpackedTarball/pdfium/core/fxcrt/fx_codepage \
    UnpackedTarball/pdfium/core/fxcrt/fx_number \
    UnpackedTarball/pdfium/core/fxcrt/observed_ptr \
    UnpackedTarball/pdfium/core/fxcrt/string_data_template \
    UnpackedTarball/pdfium/core/fxcrt/binary_buffer \
    UnpackedTarball/pdfium/core/fxcrt/cfx_read_only_span_stream \
    UnpackedTarball/pdfium/core/fxcrt/cfx_read_only_string_stream \
    UnpackedTarball/pdfium/core/fxcrt/cfx_read_only_vector_stream \
    UnpackedTarball/pdfium/core/fxcrt/fx_memory_malloc \
    UnpackedTarball/pdfium/core/fxcrt/widetext_buffer \
    UnpackedTarball/pdfium/core/fxcrt/debug/alias \
    UnpackedTarball/pdfium/core/fxcrt/string_template \
))

# fxge
$(eval $(call gb_Library_add_generated_exception_objects,pdfium,\
    UnpackedTarball/pdfium/core/fxge/dib/cfx_bitmapstorer \
    UnpackedTarball/pdfium/core/fxge/dib/cfx_dibitmap \
    UnpackedTarball/pdfium/core/fxge/cfx_drawutils \
    UnpackedTarball/pdfium/core/fxge/dib/cfx_imagestretcher \
    UnpackedTarball/pdfium/core/fxge/dib/cfx_imagetransformer \
    UnpackedTarball/pdfium/core/fxge/dib/cfx_scanlinecompositor \
    UnpackedTarball/pdfium/core/fxge/dib/cstretchengine \
    UnpackedTarball/pdfium/core/fxge/dib/fx_dib \
    UnpackedTarball/pdfium/core/fxge/dib/blend \
    UnpackedTarball/pdfium/core/fxge/fontdata/chromefontdata/FoxitDingbats \
    UnpackedTarball/pdfium/core/fxge/fontdata/chromefontdata/FoxitFixed \
    UnpackedTarball/pdfium/core/fxge/fontdata/chromefontdata/FoxitFixedBold \
    UnpackedTarball/pdfium/core/fxge/fontdata/chromefontdata/FoxitFixedBoldItalic \
    UnpackedTarball/pdfium/core/fxge/fontdata/chromefontdata/FoxitFixedItalic \
    UnpackedTarball/pdfium/core/fxge/fontdata/chromefontdata/FoxitSans \
    UnpackedTarball/pdfium/core/fxge/fontdata/chromefontdata/FoxitSansBold \
    UnpackedTarball/pdfium/core/fxge/fontdata/chromefontdata/FoxitSansBoldItalic \
    UnpackedTarball/pdfium/core/fxge/fontdata/chromefontdata/FoxitSansItalic \
    UnpackedTarball/pdfium/core/fxge/fontdata/chromefontdata/FoxitSansMM \
    UnpackedTarball/pdfium/core/fxge/fontdata/chromefontdata/FoxitSerif \
    UnpackedTarball/pdfium/core/fxge/fontdata/chromefontdata/FoxitSerifBold \
    UnpackedTarball/pdfium/core/fxge/fontdata/chromefontdata/FoxitSerifBoldItalic \
    UnpackedTarball/pdfium/core/fxge/fontdata/chromefontdata/FoxitSerifItalic \
    UnpackedTarball/pdfium/core/fxge/fontdata/chromefontdata/FoxitSerifMM \
    UnpackedTarball/pdfium/core/fxge/fontdata/chromefontdata/FoxitSymbol \
    UnpackedTarball/pdfium/core/fxge/freetype/fx_freetype \
    UnpackedTarball/pdfium/core/fxge/renderdevicedriver_iface \
    UnpackedTarball/pdfium/core/fxge/agg/cfx_agg_bitmapcomposer \
    UnpackedTarball/pdfium/core/fxge/agg/cfx_agg_devicedriver \
    UnpackedTarball/pdfium/core/fxge/agg/cfx_agg_imagerenderer \
    UnpackedTarball/pdfium/core/fxge/agg/cfx_agg_cliprgn \
    UnpackedTarball/pdfium/core/fxge/cfx_color \
    UnpackedTarball/pdfium/core/fxge/cfx_glyphcache \
    UnpackedTarball/pdfium/core/fxge/cfx_folderfontinfo \
    UnpackedTarball/pdfium/core/fxge/cfx_font \
    UnpackedTarball/pdfium/core/fxge/cfx_fontcache \
    UnpackedTarball/pdfium/core/fxge/cfx_fontmapper \
    UnpackedTarball/pdfium/core/fxge/cfx_fontmgr \
    UnpackedTarball/pdfium/core/fxge/cfx_gemodule \
    UnpackedTarball/pdfium/core/fxge/cfx_graphstate \
    UnpackedTarball/pdfium/core/fxge/cfx_graphstatedata \
    UnpackedTarball/pdfium/core/fxge/cfx_path \
    UnpackedTarball/pdfium/core/fxge/cfx_renderdevice \
    UnpackedTarball/pdfium/core/fxge/cfx_substfont \
    UnpackedTarball/pdfium/core/fxge/cfx_unicodeencoding \
    UnpackedTarball/pdfium/core/fxge/cfx_glyphbitmap \
    UnpackedTarball/pdfium/core/fxge/scoped_font_transform \
    UnpackedTarball/pdfium/core/fxge/text_glyph_pos \
    UnpackedTarball/pdfium/core/fxge/fx_font \
    UnpackedTarball/pdfium/core/fxge/dib/cfx_dibbase \
    UnpackedTarball/pdfium/core/fxge/dib/cfx_cmyk_to_srgb \
    UnpackedTarball/pdfium/core/fxge/text_char_pos \
    UnpackedTarball/pdfium/core/fxge/cfx_face \
    UnpackedTarball/pdfium/core/fxge/cfx_defaultrenderdevice \
    UnpackedTarball/pdfium/core/fxge/calculate_pitch \
))

# javascript, build with pdf_enable_v8 disabled.
$(eval $(call gb_Library_add_generated_exception_objects,pdfium,\
    UnpackedTarball/pdfium/fxjs/cjs_event_context_stub \
    UnpackedTarball/pdfium/fxjs/cjs_runtimestub \
    UnpackedTarball/pdfium/fxjs/ijs_runtime \
))

# pwl
$(eval $(call gb_Library_add_generated_exception_objects,pdfium,\
    UnpackedTarball/pdfium/fpdfsdk/cpdfsdk_appstream \
    UnpackedTarball/pdfium/fpdfsdk/pwl/cpwl_button \
    UnpackedTarball/pdfium/fpdfsdk/pwl/cpwl_caret \
    UnpackedTarball/pdfium/fpdfsdk/pwl/cpwl_combo_box \
    UnpackedTarball/pdfium/fpdfsdk/pwl/cpwl_edit \
    UnpackedTarball/pdfium/fpdfsdk/pwl/cpwl_edit_impl \
    UnpackedTarball/pdfium/fpdfsdk/pwl/cpwl_sbbutton \
    UnpackedTarball/pdfium/fpdfsdk/pwl/cpwl_list_box \
    UnpackedTarball/pdfium/fpdfsdk/pwl/cpwl_cbbutton \
    UnpackedTarball/pdfium/fpdfsdk/pwl/cpwl_cblistbox \
    UnpackedTarball/pdfium/fpdfsdk/pwl/cpwl_list_ctrl \
    UnpackedTarball/pdfium/fpdfsdk/pwl/cpwl_scroll_bar \
    UnpackedTarball/pdfium/fpdfsdk/pwl/cpwl_special_button \
    UnpackedTarball/pdfium/core/fxcrt/cfx_timer \
    UnpackedTarball/pdfium/fpdfsdk/pwl/cpwl_wnd \
))

# third_party/fx_agg
$(eval $(call gb_Library_add_generated_exception_objects,pdfium,\
    UnpackedTarball/pdfium/third_party/agg23/agg_curves \
    UnpackedTarball/pdfium/third_party/agg23/agg_path_storage \
    UnpackedTarball/pdfium/third_party/agg23/agg_rasterizer_scanline_aa \
    UnpackedTarball/pdfium/third_party/agg23/agg_vcgen_dash \
    UnpackedTarball/pdfium/third_party/agg23/agg_vcgen_stroke \
))

ifneq ($(SYSTEM_ABSEIL),TRUE)
# third_party/abseil-cpp
$(eval $(call gb_Library_add_generated_exception_objects,pdfium,\
    UnpackedTarball/pdfium/third_party/abseil-cpp/absl/types/bad_optional_access \
    UnpackedTarball/pdfium/third_party/abseil-cpp/absl/types/bad_variant_access \
))
endif

ifneq ($(SYSTEM_OPENJPEG2),TRUE)
# third_party/fx_libopenjpeg
$(eval $(call gb_Library_add_generated_cobjects,pdfium,\
    UnpackedTarball/pdfium/third_party/libopenjpeg/bio \
    UnpackedTarball/pdfium/third_party/libopenjpeg/cio \
    UnpackedTarball/pdfium/third_party/libopenjpeg/dwt \
    UnpackedTarball/pdfium/third_party/libopenjpeg/event \
    UnpackedTarball/pdfium/third_party/libopenjpeg/function_list \
    UnpackedTarball/pdfium/third_party/libopenjpeg/image \
    UnpackedTarball/pdfium/third_party/libopenjpeg/invert \
    UnpackedTarball/pdfium/third_party/libopenjpeg/j2k \
    UnpackedTarball/pdfium/third_party/libopenjpeg/jp2 \
    UnpackedTarball/pdfium/third_party/libopenjpeg/mct \
    UnpackedTarball/pdfium/third_party/libopenjpeg/mqc \
    UnpackedTarball/pdfium/third_party/libopenjpeg/openjpeg \
    UnpackedTarball/pdfium/third_party/libopenjpeg/opj_clock \
    UnpackedTarball/pdfium/third_party/libopenjpeg/pi \
    UnpackedTarball/pdfium/third_party/libopenjpeg/thread \
    UnpackedTarball/pdfium/third_party/libopenjpeg/t1 \
    UnpackedTarball/pdfium/third_party/libopenjpeg/t2 \
    UnpackedTarball/pdfium/third_party/libopenjpeg/tcd \
    UnpackedTarball/pdfium/third_party/libopenjpeg/tgt \
    UnpackedTarball/pdfium/third_party/libopenjpeg/sparse_array \
    UnpackedTarball/pdfium/third_party/libopenjpeg/ht_dec \
))

$(eval $(call gb_Library_add_generated_exception_objects,pdfium,\
    UnpackedTarball/pdfium/third_party/libopenjpeg/opj_malloc \
))
endif

$(eval $(call gb_Library_use_externals,pdfium,\
    libjpeg \
    lcms2 \
    zlib \
    icu_headers \
    icuuc \
))

ifneq (,$(filter LINUX ANDROID,$(OS)))
$(eval $(call gb_Library_add_libs,pdfium,\
    -ldl \
    -lrt \
))

ifeq ($(SYSTEM_OPENJPEG2),TRUE)
$(eval $(call gb_Library_add_libs,pdfium,\
    $(OPENJPEG2_LIBS) \
))
endif

ifeq ($(SYSTEM_ABSEIL),TRUE)
$(eval $(call gb_Library_add_libs,pdfium,\
    $(ABSEIL_LIBS) \
))
endif

$(eval $(call gb_Library_use_external,pdfium,freetype))
$(eval $(call gb_Library_add_defs,pdfium,\
    -DDEFINE_PS_TABLES_DATA \
))

else
$(eval $(call gb_Library_set_include,pdfium,\
    -I$(gb_UnpackedTarball_workdir)/pdfium/third_party/freetype/include/ \
    -I$(gb_UnpackedTarball_workdir)/pdfium/third_party/freetype/src/include/ \
    $$(INCLUDE) \
))

$(eval $(call gb_Library_add_defs,pdfium,\
    -DFT2_BUILD_LIBRARY \
    -DFT_CONFIG_MODULES_H='<freetype-custom-config/ftmodule.h>' \
    -DFT_CONFIG_OPTIONS_H='<freetype-custom-config/ftoption.h>' \
))

# third_party/freetype
$(eval $(call gb_Library_add_generated_cobjects,pdfium,\
    UnpackedTarball/pdfium/third_party/freetype/src/src/base/ftbase \
    UnpackedTarball/pdfium/third_party/freetype/src/src/base/ftbitmap \
    UnpackedTarball/pdfium/third_party/freetype/src/src/base/ftglyph \
    UnpackedTarball/pdfium/third_party/freetype/src/src/base/ftinit \
    UnpackedTarball/pdfium/third_party/freetype/src/src/base/ftmm \
    UnpackedTarball/pdfium/third_party/freetype/src/src/base/ftsystem \
    UnpackedTarball/pdfium/third_party/freetype/src/src/cff/cff \
    UnpackedTarball/pdfium/third_party/freetype/src/src/cid/type1cid \
    UnpackedTarball/pdfium/third_party/freetype/src/src/psaux/psaux \
    UnpackedTarball/pdfium/third_party/freetype/src/src/pshinter/pshinter \
    UnpackedTarball/pdfium/third_party/freetype/src/src/psnames/psmodule \
    UnpackedTarball/pdfium/third_party/freetype/src/src/raster/raster \
    UnpackedTarball/pdfium/third_party/freetype/src/src/sfnt/sfnt \
    UnpackedTarball/pdfium/third_party/freetype/src/src/smooth/smooth \
    UnpackedTarball/pdfium/third_party/freetype/src/src/truetype/truetype \
    UnpackedTarball/pdfium/third_party/freetype/src/src/type1/type1 \
    UnpackedTarball/pdfium/third_party/freetype/src/src/base/ftdebug \
    UnpackedTarball/pdfium/third_party/freetype/src/src/base/ftfstype \
))
endif

ifneq ($(OS),WNT)
$(eval $(call gb_Library_add_generated_exception_objects,pdfium,\
    UnpackedTarball/pdfium/core/fxcrt/cfx_fileaccess_posix \
    UnpackedTarball/pdfium/core/fxcrt/fx_folder_posix \
))
endif

ifeq ($(OS),WNT)
# fxge
$(eval $(call gb_Library_add_generated_exception_objects,pdfium,\
    UnpackedTarball/pdfium/core/fxge/win32/cfx_psrenderer \
    UnpackedTarball/pdfium/core/fxge/win32/cpsoutput \
    UnpackedTarball/pdfium/core/fxge/win32/cgdi_device_driver \
    UnpackedTarball/pdfium/core/fxge/win32/cgdi_display_driver \
    UnpackedTarball/pdfium/core/fxge/win32/cgdi_plus_ext \
    UnpackedTarball/pdfium/core/fxge/win32/cgdi_printer_driver \
    UnpackedTarball/pdfium/core/fxge/win32/cps_printer_driver \
    UnpackedTarball/pdfium/core/fxge/win32/ctext_only_printer_driver \
    UnpackedTarball/pdfium/core/fxge/win32/cwin32_platform \
    UnpackedTarball/pdfium/core/fxge/win32/cfx_psfonttracker \
    UnpackedTarball/pdfium/core/fxge/cfx_windowsrenderdevice \
    UnpackedTarball/pdfium/core/fxcrt/cfx_fileaccess_windows \
    UnpackedTarball/pdfium/core/fxcrt/fx_folder_windows \
    UnpackedTarball/pdfium/core/fxcrt/win/win_util \
    UnpackedTarball/pdfium/core/fxcrt/code_point_view \
    UnpackedTarball/pdfium/core/fpdfapi/render/cpdf_windowsrenderdevice \
))

$(eval $(call gb_Library_use_system_win32_libs,pdfium,\
    gdi32 \
))

$(eval $(call gb_Library_add_defs,pdfium,\
    -DWIN32 \
))
endif

ifeq ($(OS),MACOSX)
# fxge
$(eval $(call gb_Library_add_generated_exception_objects,pdfium,\
    UnpackedTarball/pdfium/core/fxge/apple/fx_apple_platform \
    UnpackedTarball/pdfium/core/fxge/apple/fx_apple_impl \
    UnpackedTarball/pdfium/core/fxge/apple/fx_quartz_device \
))

$(eval $(call gb_Library_use_system_darwin_frameworks,pdfium,\
    AppKit \
    CoreFoundation \
))
endif

ifeq ($(OS),iOS)
# fxge
$(eval $(call gb_Library_add_generated_exception_objects,pdfium,\
    UnpackedTarball/pdfium/core/fxge/apple/fx_apple_platform \
    UnpackedTarball/pdfium/core/fxge/apple/fx_apple_impl \
    UnpackedTarball/pdfium/core/fxge/apple/fx_quartz_device \
))

$(eval $(call gb_Library_use_system_darwin_frameworks,pdfium,\
    CoreGraphics \
    CoreFoundation \
))
endif

ifeq ($(OS),ANDROID)
# fxge
$(eval $(call gb_Library_add_generated_exception_objects,pdfium,\
    UnpackedTarball/pdfium/core/fxge/android/cfpf_skiadevicemodule \
    UnpackedTarball/pdfium/core/fxge/android/cfpf_skiafont \
    UnpackedTarball/pdfium/core/fxge/android/cfpf_skiafontmgr \
    UnpackedTarball/pdfium/core/fxge/android/cfx_androidfontinfo \
    UnpackedTarball/pdfium/core/fxge/android/fx_android_impl \
    UnpackedTarball/pdfium/core/fxge/android/cfpf_skiapathfont \
))
endif

ifeq ($(OS),LINUX)
# fxge
$(eval $(call gb_Library_add_generated_exception_objects,pdfium,\
    UnpackedTarball/pdfium/core/fxge/linux/fx_linux_impl \
))
endif

# vim: set noet sw=4 ts=4:
