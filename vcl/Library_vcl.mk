# -*- Mode: makefile-gmake; tab-width: 4; indent-tabs-mode: t -*-
#
# This file is part of the LibreOffice project.
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This file incorporates work covered by the following license notice:
#
#   Licensed to the Apache Software Foundation (ASF) under one or more
#   contributor license agreements. See the NOTICE file distributed
#   with this work for additional information regarding copyright
#   ownership. The ASF licenses this file to you under the Apache
#   License, Version 2.0 (the "License"); you may not use this file
#   except in compliance with the License. You may obtain a copy of
#   the License at http://www.apache.org/licenses/LICENSE-2.0 .
#

$(eval $(call gb_Library_Library,vcl))

$(eval $(call gb_Library_set_componentfile,vcl,vcl/vcl.common,services))

ifeq ($(OS),MACOSX)
$(eval $(call gb_Library_add_componentimpl,vcl,macosx))
else ifeq ($(OS),WNT)
$(eval $(call gb_Library_add_componentimpl,vcl,windows))
else ifeq ($(OS),ANDROID)
$(eval $(call gb_Library_add_componentimpl,vcl,android))
else ifeq ($(OS),iOS)
$(eval $(call gb_Library_add_componentimpl,vcl,ios))
else ifeq ($(DISABLE_GUI),TRUE)
$(eval $(call gb_Library_add_componentimpl,vcl,headless))
else
$(eval $(call gb_Library_add_componentimpl,vcl,unx))
endif

$(eval $(call gb_Library_set_precompiled_header,vcl,vcl/inc/pch/precompiled_vcl))

$(eval $(call gb_Library_set_include,vcl,\
    $$(INCLUDE) \
    -I$(SRCDIR)/vcl/inc \
))

$(eval $(call gb_Library_add_defs,vcl,\
    -DVCL_DLLIMPLEMENTATION \
    -DDLLIMPLEMENTATION_UITEST \
    -DCUI_DLL_NAME=\"$(call gb_Library_get_runtime_filename,$(call gb_Library__get_name,cui))\" \
    -DTK_DLL_NAME=\"$(call gb_Library_get_runtime_filename,$(call gb_Library__get_name,tk))\" \
    $(if $(SYSTEM_LIBFIXMATH),-DSYSTEM_LIBFIXMATH) \
))

$(eval $(call gb_Library_use_sdk_api,vcl))

$(eval $(call gb_Library_use_custom_headers,vcl,\
    officecfg/registry \
))

$(eval $(call gb_Library_use_libraries,vcl,\
    basegfx \
    comphelper \
    cppu \
    cppuhelper \
    $(call gb_Helper_optional,BREAKPAD,crashreport) \
    drawinglayercore \
    i18nlangtag \
    i18nutil \
    $(if $(ENABLE_JAVA),jvmaccess) \
    $(if $(filter OPENCL,$(BUILD_TYPE)),opencl) \
    sal \
    salhelper \
    sot \
    svl \
    tl \
    ucbhelper \
    utl \
    xmlreader \
))

$(eval $(call gb_Library_use_externals,vcl,\
    boost_headers \
    expat \
    frozen \
    gio \
    graphite \
    harfbuzz \
    icu_headers \
    icuuc \
    lcms2 \
    libeot \
    libjpeg \
    libpng \
    libtiff \
    libwebp \
))

$(eval $(call gb_Library_add_exception_objects,vcl,\
    vcl/source/rendercontext/drawmode \
    vcl/skia/SkiaHelper \
    vcl/source/accessibility/AccessibleBrowseBox \
    vcl/source/accessibility/AccessibleBrowseBoxBase \
    vcl/source/accessibility/AccessibleBrowseBoxCheckBoxCell \
    vcl/source/accessibility/AccessibleBrowseBoxHeaderBar \
    vcl/source/accessibility/AccessibleBrowseBoxHeaderCell \
    vcl/source/accessibility/AccessibleBrowseBoxTable \
    vcl/source/accessibility/AccessibleBrowseBoxTableBase \
    vcl/source/accessibility/AccessibleBrowseBoxTableCell \
    vcl/source/accessibility/AccessibleTextAttributeHelper \
    vcl/source/accessibility/accessiblebrowseboxcell \
    vcl/source/accessibility/accessibleiconchoicectrl \
    vcl/source/accessibility/accessibleiconchoicectrlentry \
    vcl/source/accessibility/accessiblelistbox \
    vcl/source/accessibility/accessiblelistboxentry \
    vcl/source/accessibility/accessiblemenubasecomponent \
    vcl/source/accessibility/accessiblemenucomponent \
    vcl/source/accessibility/accessiblemenuitemcomponent \
    vcl/source/accessibility/accessibletablistbox \
    vcl/source/accessibility/accessibletablistboxtable \
    vcl/source/accessibility/characterattributeshelper \
    vcl/source/accessibility/floatingwindowaccessible \
    vcl/source/accessibility/IComboListBoxHelper \
    vcl/source/accessibility/svtaccessiblenumericfield \
    vcl/source/accessibility/vclxaccessiblebox \
    vcl/source/accessibility/vclxaccessiblebutton \
    vcl/source/accessibility/vclxaccessiblecheckbox \
    vcl/source/accessibility/vclxaccessiblecombobox \
    vcl/source/accessibility/vclxaccessiblecomponent \
    vcl/source/accessibility/vclxaccessibledropdowncombobox \
    vcl/source/accessibility/vclxaccessibledropdownlistbox \
    vcl/source/accessibility/vclxaccessibleedit \
    vcl/source/accessibility/vclxaccessiblefixedhyperlink \
    vcl/source/accessibility/vclxaccessiblefixedtext \
    vcl/source/accessibility/vclxaccessibleheaderbar \
    vcl/source/accessibility/vclxaccessibleheaderbaritem \
    vcl/source/accessibility/vclxaccessiblelist \
    vcl/source/accessibility/vclxaccessiblelistbox \
    vcl/source/accessibility/vclxaccessiblelistitem \
    vcl/source/accessibility/vclxaccessiblemenu \
    vcl/source/accessibility/vclxaccessiblemenubar \
    vcl/source/accessibility/vclxaccessiblemenuitem \
    vcl/source/accessibility/vclxaccessiblemenuseparator \
    vcl/source/accessibility/vclxaccessiblepopupmenu \
    vcl/source/accessibility/vclxaccessibleradiobutton \
    vcl/source/accessibility/vclxaccessiblescrollbar \
    vcl/source/accessibility/vclxaccessiblestatusbar \
    vcl/source/accessibility/vclxaccessiblestatusbaritem \
    vcl/source/accessibility/vclxaccessibletabcontrol \
    vcl/source/accessibility/vclxaccessibletabpage \
    vcl/source/accessibility/vclxaccessibletabpagewindow \
    vcl/source/accessibility/vclxaccessibletextcomponent \
    vcl/source/accessibility/vclxaccessibletextfield \
    vcl/source/accessibility/vclxaccessibletoolbox \
    vcl/source/accessibility/vclxaccessibletoolboxitem \
    vcl/source/animate/Animation \
    vcl/source/animate/AnimationFrame \
    vcl/source/animate/AnimationRenderer \
    vcl/source/cnttype/mcnttfactory \
    vcl/source/cnttype/mcnttype \
    vcl/source/printer/Options \
    vcl/source/printer/QueueInfo \
    vcl/source/window/bubblewindow \
    vcl/source/window/errinf \
    vcl/source/window/settings \
    vcl/source/window/paint \
    vcl/source/window/abstdlg \
    vcl/source/window/accel \
    vcl/source/window/accmgr \
    vcl/source/window/brdwin \
    vcl/source/window/bufferdevice \
    vcl/source/window/accessibility \
    vcl/source/window/legacyaccessibility \
    vcl/source/window/clipping \
    vcl/source/window/stacking \
    vcl/source/window/debug \
    vcl/source/window/globalization \
    vcl/source/window/builder \
    vcl/source/window/commandevent \
    vcl/source/window/cursor \
    vcl/source/window/debugevent \
    vcl/source/window/decoview \
    vcl/source/window/dialog \
    vcl/source/window/dlgctrl \
    vcl/source/window/dndeventdispatcher \
    vcl/source/window/dndlistenercontainer \
    vcl/source/window/dockingarea \
    vcl/source/window/dockmgr \
    vcl/source/window/dockwin \
    vcl/source/window/DocWindow \
    vcl/source/window/event \
    vcl/source/window/floatwin \
    $(if $(ENABLE_WASM_STRIP_SPLASH),,vcl/source/window/introwin) \
    vcl/source/window/keycod \
    vcl/source/window/keyevent \
    vcl/source/window/layout \
    vcl/source/window/menu \
    vcl/source/window/menubarwindow \
    vcl/source/window/menufloatingwindow \
    vcl/source/window/menuitemlist \
    vcl/source/window/menuwindow \
    vcl/source/window/mnemonic \
    vcl/source/window/mouse \
    vcl/source/window/NotebookBarAddonsMerger \
    vcl/source/window/OptionalBox \
    vcl/source/window/printdlg \
    vcl/source/window/scrwnd \
    vcl/source/window/seleng \
    vcl/source/window/split \
    vcl/source/window/splitwin \
    vcl/source/window/status \
    vcl/source/window/syschild \
    vcl/source/window/syswin \
    vcl/source/window/tabdlg \
    vcl/source/window/tabpage \
    vcl/source/window/taskpanelist \
    vcl/source/window/toolbox2 \
    vcl/source/window/toolbox \
    vcl/source/window/window2 \
    vcl/source/window/window3 \
    vcl/source/window/window \
    vcl/source/window/winproc \
    vcl/source/window/wrkwin \
    vcl/source/window/EnumContext \
    vcl/source/control/button \
    vcl/source/control/calendar \
    vcl/source/control/combobox \
    vcl/source/control/ctrl \
    vcl/source/control/ContextVBox \
    vcl/source/control/DropdownBox \
    vcl/source/control/edit \
    vcl/source/control/field2 \
    vcl/source/control/field \
    vcl/source/control/fixed \
    vcl/source/control/fixedhyper \
    vcl/source/control/hyperlabel \
    vcl/source/control/fmtfield \
    vcl/source/control/FormattedField \
    vcl/source/control/InterimItemWindow \
    vcl/source/control/imgctrl \
    vcl/source/control/imivctl1 \
    vcl/source/control/imivctl2 \
    vcl/source/control/ivctrl \
    vcl/source/control/longcurr \
    vcl/source/control/imp_listbox \
    vcl/source/control/listbox \
    vcl/source/control/managedmenubutton \
    vcl/source/control/MenuButton \
    vcl/source/control/NotebookbarPopup \
    vcl/source/control/PriorityHBox \
    vcl/source/control/PriorityMergedHBox \
    vcl/source/control/notebookbar \
    vcl/source/control/WeldedTabbedNotebookbar \
    vcl/source/control/quickselectionengine \
    vcl/source/control/prgsbar \
    vcl/source/control/roadmap \
    vcl/source/control/roadmapwizard \
    vcl/source/control/roadmapwizardmachine \
    vcl/source/control/scrbar \
    vcl/source/control/slider \
    vcl/source/control/spinbtn \
    vcl/source/control/spinfld \
    vcl/source/control/tabctrl \
    vcl/source/control/throbber \
    vcl/source/control/wizardmachine \
    vcl/source/edit/IdleFormatter \
    vcl/source/edit/vclmedit \
    vcl/source/edit/textdata \
    vcl/source/edit/textdoc \
    vcl/source/edit/texteng \
    vcl/source/edit/textundo \
    vcl/source/edit/textview \
    vcl/source/edit/txtattr \
    vcl/source/edit/xtextedt \
    vcl/source/toolkit/group \
    vcl/source/toolkit/morebtn \
    vcl/source/outdev/background \
    vcl/source/outdev/eps \
    vcl/source/outdev/outdev \
    vcl/source/outdev/EmphasisMarks \
    vcl/source/outdev/stack \
    vcl/source/outdev/clipping \
    vcl/source/outdev/fill \
    vcl/source/outdev/polygon \
    vcl/source/outdev/transparent \
    vcl/source/outdev/mask \
    vcl/source/outdev/bitmap \
    vcl/source/outdev/bitmapex \
    vcl/source/outdev/font \
    vcl/source/outdev/text \
    vcl/source/outdev/textline \
    vcl/source/outdev/pixel \
    vcl/source/outdev/rect \
    vcl/source/outdev/line \
    vcl/source/outdev/polyline \
    vcl/source/outdev/hatch \
    vcl/source/outdev/gradient \
    vcl/source/outdev/curvedshapes \
    vcl/source/outdev/wallpaper \
    vcl/source/outdev/vclreferencebase \
    vcl/source/outdev/nativecontrols \
    vcl/source/outdev/map \
    vcl/source/text/ImplLayoutArgs \
    vcl/source/text/ImplLayoutRuns \
    vcl/source/text/mnemonic \
    vcl/source/text/TextLayoutCache \
    vcl/source/text/textlayout \
    vcl/source/treelist/headbar \
    vcl/source/treelist/iconview \
    vcl/source/treelist/iconviewimpl \
    vcl/source/treelist/imap \
    vcl/source/treelist/imap2 \
    vcl/source/treelist/imap3 \
    vcl/source/treelist/inetimg \
    vcl/source/treelist/svtabbx \
    vcl/source/treelist/transfer \
    vcl/source/treelist/transfer2 \
    vcl/source/treelist/viewdataentry \
    vcl/source/treelist/treelist \
    vcl/source/treelist/treelistbox \
    vcl/source/treelist/treelistentry \
    vcl/source/treelist/svimpbox \
    vcl/source/treelist/svlbitm \
    vcl/source/treelist/uiobject \
    vcl/source/gdi/formpdfexport \
    vcl/source/gdi/cvtgrf \
    vcl/source/gdi/embeddedfontshelper \
    vcl/source/gdi/FileDefinitionWidgetDraw \
    vcl/source/gdi/WidgetDefinitionReader \
    vcl/source/gdi/WidgetDefinition \
    vcl/source/gdi/extoutdevdata \
    vcl/source/gdi/gdimtf \
    vcl/source/gdi/mtfxmldump \
    vcl/source/gdi/gdimetafiletools \
    vcl/source/gdi/gfxlink \
    vcl/source/gdi/gradient \
    vcl/source/gdi/graph \
    vcl/source/gdi/graphictools \
    vcl/source/gdi/hatch \
    vcl/source/gdi/impglyphitem \
    vcl/source/gdi/impgraph \
    vcl/source/gdi/jobset \
    vcl/source/gdi/lineinfo \
    vcl/source/gdi/mapmod \
    vcl/source/gdi/metaact \
    vcl/source/gdi/oldprintadaptor \
    vcl/source/gdi/pdfbuildin_fonts \
    vcl/source/gdi/pdfextoutdevdata \
    vcl/source/gdi/pdfwriter \
    vcl/source/gdi/pdfwriter_impl2 \
    vcl/source/gdi/pdfwriter_impl \
    vcl/source/gdi/pdfobjectcopier \
    vcl/source/gdi/print2 \
    vcl/source/gdi/print3 \
    vcl/source/gdi/print \
    vcl/source/gdi/regband \
    vcl/source/gdi/region \
    vcl/source/gdi/regionband \
    vcl/source/gdi/salgdilayout \
    vcl/source/gdi/salgdiimpl \
    vcl/source/gdi/sallayout \
    vcl/source/gdi/salmisc \
    vcl/source/gdi/vectorgraphicdata \
    vcl/source/gdi/virdev \
    vcl/source/gdi/wall \
    vcl/source/gdi/scrptrun \
    vcl/source/gdi/CommonSalLayout \
    vcl/source/gdi/TypeSerializer \
    vcl/source/graphic/BinaryDataContainer \
    vcl/source/graphic/BinaryDataContainerTools \
    vcl/source/graphic/GraphicID \
    vcl/source/graphic/GraphicLoader \
    vcl/source/graphic/GraphicObject \
    vcl/source/graphic/GraphicObject2 \
    vcl/source/graphic/Manager \
    vcl/source/graphic/UnoBinaryDataContainer \
    vcl/source/graphic/UnoGraphic \
    vcl/source/graphic/UnoGraphicMapper \
    vcl/source/graphic/UnoGraphicDescriptor \
    vcl/source/graphic/UnoGraphicObject \
    vcl/source/graphic/UnoGraphicProvider \
    vcl/source/graphic/VectorGraphicSearch \
    vcl/source/graphic/VectorGraphicLoader \
    vcl/source/bitmap/Vectorizer \
    vcl/source/bitmap/bitmap \
    vcl/source/bitmap/bitmappalette \
    vcl/source/bitmap/BitmapEx \
    vcl/source/bitmap/BitmapInfoAccess \
    vcl/source/bitmap/BitmapReadAccess \
    vcl/source/bitmap/BitmapWriteAccess \
    vcl/source/bitmap/alpha \
    vcl/source/bitmap/dibtools \
    vcl/source/bitmap/bmpfast \
    vcl/source/bitmap/bitmapfilter \
    vcl/source/bitmap/bitmappaint \
    vcl/source/bitmap/BitmapArithmeticBlendFilter \
    vcl/source/bitmap/BitmapShadowFilter \
    vcl/source/bitmap/BitmapAlphaClampFilter \
    vcl/source/bitmap/BitmapBasicMorphologyFilter \
    vcl/source/bitmap/BitmapDarkenBlendFilter \
    vcl/source/bitmap/BitmapLightenBlendFilter \
    vcl/source/bitmap/BitmapMaskToAlphaFilter \
    vcl/source/bitmap/BitmapMonochromeFilter \
    vcl/source/bitmap/BitmapMultiplyBlendFilter \
    vcl/source/bitmap/BitmapNormalBlendFilter \
    vcl/source/bitmap/BitmapScreenBlendFilter \
    vcl/source/bitmap/BitmapSmoothenFilter \
    vcl/source/bitmap/BitmapLightenFilter \
    vcl/source/bitmap/BitmapDisabledImageFilter \
    vcl/source/bitmap/BitmapColorizeFilter \
    vcl/source/bitmap/BitmapGaussianSeparableBlurFilter \
    vcl/source/bitmap/BitmapSobelGreyFilter \
    vcl/source/bitmap/BitmapSolarizeFilter \
    vcl/source/bitmap/BitmapSepiaFilter \
    vcl/source/bitmap/BitmapMosaicFilter \
    vcl/source/bitmap/BitmapEmbossGreyFilter \
    vcl/source/bitmap/BitmapPopArtFilter \
    vcl/source/bitmap/BitmapDuoToneFilter \
    vcl/source/bitmap/BitmapConvolutionMatrixFilter \
    vcl/source/bitmap/BitmapMedianFilter \
    vcl/source/bitmap/BitmapInterpolateScaleFilter \
    vcl/source/bitmap/BitmapSeparableUnsharpenFilter \
    vcl/source/bitmap/BitmapFastScaleFilter \
    vcl/source/bitmap/BitmapScaleSuperFilter \
    vcl/source/bitmap/BitmapScaleConvolutionFilter \
    vcl/source/bitmap/BitmapSymmetryCheck \
    vcl/source/bitmap/BitmapColorQuantizationFilter \
    vcl/source/bitmap/BitmapSimpleColorQuantizationFilter \
    vcl/source/bitmap/BitmapTools \
    vcl/source/bitmap/BlendFrameCache \
    vcl/source/bitmap/Octree \
    vcl/source/bitmap/salbmp \
    vcl/source/image/Image \
    vcl/source/image/ImageTree \
    vcl/source/image/ImageRepository \
    vcl/source/image/ImplImage \
    vcl/source/image/ImplImageTree \
    vcl/source/bitmap/BitmapFilterStackBlur \
    vcl/source/helper/canvasbitmap \
    vcl/source/helper/canvastools \
    vcl/source/helper/commandinfoprovider \
    vcl/source/helper/displayconnectiondispatch \
    vcl/source/helper/driverblocklist \
    vcl/source/helper/evntpost \
    vcl/source/helper/idletask \
    vcl/source/helper/strhelper \
    vcl/source/helper/threadex \
    vcl/source/app/brand \
    vcl/source/app/ColorDialog \
    vcl/source/app/customweld \
    vcl/source/app/dbggui \
    vcl/source/app/dndhelp \
    vcl/source/app/DropTarget \
    vcl/source/app/help \
    vcl/source/app/i18nhelp \
    vcl/source/app/idle \
    vcl/source/app/salplug \
    vcl/source/app/salusereventlist \
    vcl/source/app/salvtables \
    vcl/source/app/scheduler \
    vcl/source/app/session \
    vcl/source/app/settings \
    vcl/source/app/themecolors \
    vcl/source/app/IconThemeInfo \
    vcl/source/app/IconThemeScanner \
    vcl/source/app/IconThemeSelector \
    vcl/source/app/ITiledRenderable \
    vcl/source/app/sound \
    vcl/source/app/stdtext \
    vcl/source/app/svapp \
    vcl/source/app/svcache \
    vcl/source/app/svdata \
    vcl/source/app/svmain \
    vcl/source/app/timer \
    vcl/source/app/unohelp2 \
    vcl/source/app/htmltransferable \
    vcl/source/app/UserResourceScanner \
    vcl/source/app/unohelp \
    vcl/source/app/vclevent \
    vcl/source/app/watchdog \
    vcl/source/app/weldutils \
    vcl/source/app/winscheduler \
    vcl/source/components/dtranscomp \
    vcl/source/components/fontident \
    vcl/source/filter/bmp/BmpReader \
    vcl/source/filter/bmp/BmpWriter \
    vcl/source/filter/egif/egif \
    vcl/source/filter/egif/giflzwc \
    vcl/source/filter/eps/eps \
    vcl/source/filter/etiff/etiff \
    vcl/source/filter/FilterConfigCache \
    vcl/source/filter/FilterConfigItem \
    vcl/source/filter/graphicfilter \
    vcl/source/filter/graphicfilter2 \
    vcl/source/filter/GraphicNativeTransform \
    vcl/source/filter/GraphicNativeMetadata \
    vcl/source/filter/GraphicFormatDetector \
    vcl/source/filter/idxf/dxf2mtf \
    vcl/source/filter/idxf/dxfblkrd \
    vcl/source/filter/idxf/dxfentrd \
    vcl/source/filter/idxf/dxfgrprd \
    vcl/source/filter/idxf/dxfreprd \
    vcl/source/filter/idxf/dxftblrd \
    vcl/source/filter/idxf/dxfvec \
    vcl/source/filter/idxf/idxf \
    vcl/source/filter/ieps/ieps \
    vcl/source/filter/igif/decode \
    vcl/source/filter/igif/gifread \
    vcl/source/filter/imet/ios2met \
    vcl/source/filter/ipbm/ipbm \
    vcl/source/filter/ipcd/ipcd \
    vcl/source/filter/ipcx/ipcx \
    vcl/source/filter/ipict/ipict \
    vcl/source/filter/ipsd/ipsd \
    vcl/source/filter/ipict/shape \
    vcl/source/filter/ipdf/pdfcompat \
    vcl/source/filter/ipdf/pdfread \
    vcl/source/filter/ipdf/pdfdocument \
    vcl/source/filter/iras/iras \
    vcl/source/filter/itga/itga \
    vcl/source/filter/itiff/itiff \
    vcl/source/filter/ixbm/xbmread \
    vcl/source/filter/ixpm/xpmread \
    vcl/source/filter/jpeg/Exif \
    vcl/source/filter/jpeg/jpeg \
    vcl/source/filter/jpeg/jpegc \
    vcl/source/filter/jpeg/JpegReader \
    vcl/source/filter/jpeg/JpegWriter \
    vcl/source/filter/jpeg/JpegTransform \
    vcl/source/filter/svm/SvmConverter \
    vcl/source/filter/svm/SvmReader \
    vcl/source/filter/svm/SvmWriter \
    vcl/source/filter/wmf/emfwr \
    vcl/source/filter/wmf/wmf \
    vcl/source/filter/wmf/wmfexternal \
    vcl/source/filter/wmf/wmfwr \
    vcl/source/filter/png/PngImageReader \
    vcl/source/filter/png/PngImageWriter \
    vcl/source/filter/webp/reader \
    vcl/source/filter/webp/writer \
    vcl/source/font/DirectFontSubstitution \
    vcl/source/font/EmphasisMark \
    vcl/source/font/Feature \
    vcl/source/font/FeatureCollector \
    vcl/source/font/FeatureParser \
    vcl/source/font/FontSelectPattern \
    vcl/source/font/LogicalFontInstance \
    vcl/source/font/OpenTypeFeatureDefinitionList \
    vcl/source/font/PhysicalFontCollection \
    vcl/source/font/PhysicalFontFace \
    vcl/source/font/PhysicalFontFamily \
    vcl/source/font/fontattributes \
    vcl/source/font/fontcache \
    vcl/source/font/fontcharmap \
    vcl/source/font/fontmetric \
    vcl/source/font/font \
    vcl/source/font/EOTConverter \
    vcl/source/fontsubset/cff \
    vcl/source/fontsubset/fontsubset \
    vcl/source/fontsubset/sft \
    vcl/source/fontsubset/ttcr \
    vcl/source/pdf/COSWriter \
    vcl/source/pdf/EncryptionHashTransporter \
    vcl/source/pdf/ExternalPDFStreams \
    vcl/source/pdf/PDFiumTools \
    vcl/source/pdf/PDFEncryptionInitialization \
    vcl/source/pdf/PDFEncryptor \
    vcl/source/pdf/PDFEncryptorR6 \
    vcl/source/pdf/PdfConfig \
    vcl/source/pdf/pwdinteract \
    vcl/source/pdf/ResourceDict \
    vcl/source/pdf/Matrix3 \
    vcl/source/pdf/XmpMetadata \
    vcl/source/uitest/logger \
    vcl/source/uitest/toolboxitemuiobject \
    vcl/source/uitest/toolboxuiobject \
    vcl/source/uitest/uiobject \
    vcl/source/uitest/uitest \
    vcl/source/uitest/uno/uiobject_uno \
    vcl/source/uitest/uno/uitest_uno \
    vcl/backendtest/outputdevice/bitmap \
    vcl/backendtest/outputdevice/clip \
    vcl/backendtest/outputdevice/common \
    vcl/backendtest/outputdevice/gradient \
    vcl/backendtest/outputdevice/line \
    vcl/backendtest/outputdevice/outputdevice \
    vcl/backendtest/outputdevice/pixel \
    vcl/backendtest/outputdevice/polygon \
    vcl/backendtest/outputdevice/polypolygon \
    vcl/backendtest/outputdevice/polypolygon_b2d \
    vcl/backendtest/outputdevice/polyline \
    vcl/backendtest/outputdevice/polyline_b2d \
    vcl/backendtest/outputdevice/text \
    vcl/backendtest/outputdevice/rectangle \
    vcl/backendtest/GraphicsRenderTests \
    vcl/jsdialog/enabled \
    vcl/jsdialog/jsdialogbuilder \
    vcl/jsdialog/jsdialogregister \
    vcl/jsdialog/jsdialogsender \
    vcl/jsdialog/executor \
))

$(eval $(call gb_Library_add_cobjects,vcl,\
    vcl/source/filter/jpeg/transupp \
))

vcl_headless_code= \
    vcl/headless/svpframe \
    $(if $(filter-out iOS,$(OS)), \
        vcl/headless/svpbmp \
        vcl/headless/svpgdi \
        vcl/headless/SvpGraphicsBackend \
        vcl/headless/CairoCommon \
        vcl/headless/BitmapHelper \
    ) \
    vcl/headless/svpdummies \
    vcl/headless/svpinst \
    vcl/headless/svpvd \
    vcl/unx/generic/app/gendisp \
    vcl/unx/generic/app/geninst \
    vcl/unx/generic/app/gensys \

vcl_headless_freetype_code=\
    vcl/headless/svpprn \
    vcl/headless/svptext \
    vcl/unx/generic/app/gendata \
    vcl/unx/generic/gdi/cairotextrender \
    vcl/unx/generic/gdi/freetypetextrender \
    vcl/unx/generic/glyphs/freetype_glyphcache \
    vcl/unx/generic/glyphs/glyphcache \
    vcl/unx/generic/fontmanager/fontsubst \
    vcl/unx/generic/fontmanager/fontconfig \
    vcl/unx/generic/fontmanager/fontmanager \
    vcl/unx/generic/fontmanager/helper \
    vcl/unx/generic/print/genpspgraphics \
    vcl/unx/generic/print/genprnpsp \
    vcl/unx/generic/print/prtsetup \
    vcl/unx/generic/printer/jobdata \
    vcl/unx/generic/printer/ppdparser \

ifeq ($(SYSTEM_LIBFIXMATH),TRUE)
$(eval $(call gb_Library_add_libs,vcl,\
        $(LIBFIXMATH_LIBS) \
))
endif

ifeq ($(USING_X11),TRUE)
$(eval $(call gb_Library_add_exception_objects,vcl,\
    vcl/unx/generic/desktopdetect/desktopdetector \
    $(if $(ENABLE_CPDB), \
        vcl/unx/generic/printer/cpdmgr \
    ) \
))

$(eval $(call gb_Library_use_externals,vcl,\
    dbus \
    valgrind \
))

$(eval $(call gb_Library_add_libs,vcl,\
    -lX11 \
    -lXext \
))
endif # USING_X11

ifeq ($(OS), $(filter LINUX %BSD SOLARIS, $(OS)))
$(eval $(call gb_Library_add_libs,vcl,\
	-lm $(UNIX_DLAPI_LIBS) \
))
endif

ifeq ($(DISABLE_GUI),TRUE)
$(eval $(call gb_Library_add_exception_objects,vcl,\
    vcl/headless/headlessinst \
))

else # !DISABLE_GUI

$(eval $(call gb_Library_add_exception_objects,vcl,\
    vcl/source/opengl/OpenGLContext \
    vcl/source/opengl/OpenGLHelper \
    $(if $(filter SKIA,$(BUILD_TYPE)), \
        vcl/skia/salbmp \
        vcl/skia/zone \
        vcl/skia/gdiimpl \
    ) \
    $(if $(filter LINUX SOLARIS %BSD,$(OS)), \
	vcl/unx/generic/window/sessioninhibitor \
    ) \
))

$(eval $(call gb_Library_use_externals,vcl,\
    epoxy \
    $(if $(filter SKIA,$(BUILD_TYPE)),skia) \
))
endif # !DISABLE_GUI


#
# * plugin loader: used on all platforms except iOS and Android
# * select headless code and corresponding libraries
#
$(eval $(call gb_Library_add_exception_objects,vcl,\
    $(if $(USE_HEADLESS_CODE), \
        $(if $(ENABLE_CUPS), \
            vcl/unx/generic/printer/cupsmgr \
	) \
	$(if $(filter TRUE,$(ENABLE_CPDB) $(ENABLE_CUPS)),\
            vcl/unx/generic/printer/printerinfomanager \
        , \
            vcl/null/printerinfomanager \
        ) \
        $(vcl_headless_code) \
        $(vcl_headless_freetype_code) \
    ) \
    vcl/source/pdf/$(if $(filter PDFIUM,$(BUILD_TYPE)),,Dummy)PDFiumLibrary \
))

# fontconfig depends on expat for static builds
$(eval $(call gb_Library_use_externals,vcl,\
    $(if $(USE_HEADLESS_CODE), \
        cairo \
        $(if $(ENABLE_CPDB),cpdb) \
        $(if $(ENABLE_CUPS),cups) \
        fontconfig \
        freetype \
    ) \
    $(if $(filter PDFIUM,$(BUILD_TYPE)),pdfium) \
))

$(eval $(call gb_Library_add_libs,vcl,\
    $(if $(filter LINUX %BSD SOLARIS,$(OS)), \
        -lm \
        $(if $(DISABLE_DYNLOADING),,$(UNIX_DLAPI_LIBS)) \
    ) \
))


#
# OS specific stuff not handled yet
#

ifeq ($(OS),HAIKU)
$(eval $(call gb_Library_add_libs,vcl,\
    -lbe \
))
endif


ifeq ($(OS),ANDROID)
$(eval $(call gb_Library_add_libs,vcl,\
    -llog \
    -landroid \
    -llo-bootstrap \
))
$(eval $(call gb_Library_add_exception_objects,vcl,\
    vcl/android/androidinst \
))
endif


ifeq ($(OS),iOS)
$(eval $(call gb_Library_add_cxxflags,vcl,\
    $(gb_OBJCXXFLAGS) \
))
$(eval $(call gb_Library_add_objcxxobjects,vcl,\
    vcl/quartz/cgutils \
))
$(eval $(call gb_Library_add_exception_objects,vcl,\
    vcl/ios/iosinst \
    vcl/ios/dummies \
    vcl/ios/salios \
    vcl/ios/iOSTransferable \
    vcl/ios/DataFlavorMapping \
    vcl/ios/HtmlFmtFlt \
    vcl/quartz/CoreTextFont \
    vcl/quartz/CoreTextFontFace \
    vcl/quartz/SystemFontList \
    vcl/quartz/salbmp \
    vcl/quartz/salgdi \
    vcl/quartz/salgdicommon \
    vcl/quartz/salvd \
    vcl/quartz/utils \
    vcl/quartz/AquaGraphicsBackend \
    $(vcl_headless_code) \
    vcl/unx/generic/app/gendata \
))
$(eval $(call gb_Library_use_system_darwin_frameworks,vcl,\
    UIKit \
    CoreFoundation \
))
endif


ifeq ($(OS),MACOSX)
$(eval $(call gb_Library_add_objcxxobjects,vcl,\
    vcl/quartz/cgutils \
    $(if $(filter SKIA,$(BUILD_TYPE)), \
        vcl/skia/quartz/salbmp \
    ) \
))
$(eval $(call gb_Library_use_system_darwin_frameworks,vcl,\
    Cocoa \
    CoreFoundation \
    Metal \
))
endif


ifeq ($(OS),WNT)
$(eval $(call gb_Library_add_exception_objects,vcl,\
    vcl/source/opengl/win/WinDeviceInfo \
    vcl/win/app/fileregistration \
))

$(eval $(call gb_Library_use_system_win32_libs,vcl,\
    ole32 \
    setupapi \
    version \
))

$(eval $(call gb_Library_add_nativeres,vcl,vcl/salsrc))

# HACK: dependency on icon themes so running unit tests don't
# prevent delivering these by having open file handles on WNT
$(eval $(call gb_Library_use_packages,vcl, \
    vcl_opengl_denylist \
    $(if $(filter host,$(gb_Side)),postprocess_images) \
))
endif # WNT

# vim: set noet sw=4 ts=4:
