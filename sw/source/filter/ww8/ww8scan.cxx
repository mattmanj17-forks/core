/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * This file incorporates work covered by the following license notice:
 *
 *   Licensed to the Apache Software Foundation (ASF) under one or more
 *   contributor license agreements. See the NOTICE file distributed
 *   with this work for additional information regarding copyright
 *   ownership. The ASF licenses this file to you under the Apache
 *   License, Version 2.0 (the "License"); you may not use this file
 *   except in compliance with the License. You may obtain a copy of
 *   the License at http://www.apache.org/licenses/LICENSE-2.0 .
 */

#include <memory>
#include "ww8scan.hxx"
#include "ww8par.hxx"

#include <cassert>
#include <cstddef>
#include <cstring>
#include <algorithm>

#include <i18nlangtag/mslangid.hxx>
#include "sprmids.hxx"
#include <rtl/tencinfo.h>
#include <sal/macros.h>
#include <sal/log.hxx>
#include <osl/diagnose.h>

#include <swerror.h>

#include <comphelper/string.hxx>
#include <unotools/localedatawrapper.hxx>
#include <i18nlangtag/lang.h>
#include <o3tl/safeint.hxx>
#include <tools/stream.hxx>

#include <vcl/settings.hxx>
#include <vcl/svapp.hxx>

using namespace ::com::sun::star::lang;

namespace
{
    /**
        winword strings are typically Belt and Braces strings preceded with a
        pascal style count, and ending with a c style 0 terminator. 16bit chars
        and count for ww8+ and 8bit chars and count for ww7-. The count and 0
        can be checked for integrity to catch errors (e.g. lotus created
        documents) where in error 8bit strings are used instead of 16bits
        strings for style names.
    */
    bool TestBeltAndBraces(SvStream& rStrm)
    {
        bool bRet = false;
        sal_uInt64 nOldPos = rStrm.Tell();
        sal_uInt16 nBelt(0);
        rStrm.ReadUInt16( nBelt );
        nBelt *= sizeof(sal_Unicode);
        if (rStrm.good() && (rStrm.remainingSize() >= (nBelt + sizeof(sal_Unicode))))
        {
            rStrm.SeekRel(nBelt);
            if (rStrm.good())
            {
                sal_Unicode cBraces(0);
                rStrm.ReadUtf16( cBraces );
                if (rStrm.good() && cBraces == 0)
                    bRet = true;
            }
        }
        rStrm.Seek(nOldPos);
        return bRet;
    }
}

const wwSprmSearcher *wwSprmParser::GetWW2SprmSearcher()
{
    //double lock me
    // WW2 Sprms
    static const SprmInfoRow aSprms[] =
    {
        {  0, { 0, L_FIX} }, // "Default-sprm", will be skipped
        {  2, { 1, L_FIX} }, // "sprmPIstd",  pap.istd (style code)
        {  3, { 0, L_VAR} }, // "sprmPIstdPermute pap.istd permutation
        {  4, { 1, L_FIX} }, // "sprmPIncLv1" pap.istddifference
        {  5, { 1, L_FIX} }, // "sprmPJc" pap.jc (justification)
        {  6, { 1, L_FIX} }, // "sprmPFSideBySide" pap.fSideBySide
        {  7, { 1, L_FIX} }, // "sprmPFKeep" pap.fKeep
        {  8, { 1, L_FIX} }, // "sprmPFKeepFollow " pap.fKeepFollow
        {  9, { 1, L_FIX} }, // "sprmPPageBreakBefore" pap.fPageBreakBefore
        { 10, { 1, L_FIX} }, // "sprmPBrcl" pap.brcl
        { 11, { 1, L_FIX} }, // "sprmPBrcp" pap.brcp
        { 12, { 1, L_FIX} }, // "sprmPNfcSeqNumb" pap.nfcSeqNumb
        { 13, { 1, L_FIX} }, // "sprmPNoSeqNumb" pap.nnSeqNumb
        { 14, { 1, L_FIX} }, // "sprmPFNoLineNumb" pap.fNoLnn
        { 15, { 0, L_VAR} }, // "?sprmPChgTabsPapx" pap.itbdMac, ...
        { 16, { 2, L_FIX} }, // "sprmPDxaRight" pap.dxaRight
        { 17, { 2, L_FIX} }, // "sprmPDxaLeft" pap.dxaLeft
        { 18, { 2, L_FIX} }, // "sprmPNest" pap.dxaLeft
        { 19, { 2, L_FIX} }, // "sprmPDxaLeft1" pap.dxaLeft1
        { 20, { 2, L_FIX} }, // "sprmPDyaLine" pap.lspd an LSPD
        { 21, { 2, L_FIX} }, // "sprmPDyaBefore" pap.dyaBefore
        { 22, { 2, L_FIX} }, // "sprmPDyaAfter" pap.dyaAfter
        { 23, { 0, L_VAR} }, // "?sprmPChgTabs" pap.itbdMac, pap.rgdxaTab, ...
        { 24, { 1, L_FIX} }, // "sprmPFInTable" pap.fInTable
        { 25, { 1, L_FIX} }, // "sprmPTtp" pap.fTtp
        { 26, { 2, L_FIX} }, // "sprmPDxaAbs" pap.dxaAbs
        { 27, { 2, L_FIX} }, // "sprmPDyaAbs" pap.dyaAbs
        { 28, { 2, L_FIX} }, // "sprmPDxaWidth" pap.dxaWidth
        { 29, { 1, L_FIX} }, // "sprmPPc" pap.pcHorz, pap.pcVert
        { 30, { 2, L_FIX} }, // "sprmPBrcTop10" pap.brcTop BRC10
        { 31, { 2, L_FIX} }, // "sprmPBrcLeft10" pap.brcLeft BRC10
        { 32, { 2, L_FIX} }, // "sprmPBrcBottom10" pap.brcBottom BRC10
        { 33, { 2, L_FIX} }, // "sprmPBrcRight10" pap.brcRight BRC10
        { 34, { 2, L_FIX} }, // "sprmPBrcBetween10" pap.brcBetween BRC10
        { 35, { 2, L_FIX} }, // "sprmPBrcBar10" pap.brcBar BRC10
        { 36, { 2, L_FIX} }, // "sprmPFromText10" pap.dxaFromText dxa
        { 37, { 1, L_FIX} }, // "sprmPWr" pap.wr wr
        { 38, { 2, L_FIX} }, // "sprmPBrcTop" pap.brcTop BRC
        { 39, { 2, L_FIX} }, // "sprmPBrcLeft" pap.brcLeft BRC
        { 40, { 2, L_FIX} }, // "sprmPBrcBottom" pap.brcBottom BRC
        { 41, { 2, L_FIX} }, // "sprmPBrcRight" pap.brcRight BRC
        { 42, { 2, L_FIX} }, // "sprmPBrcBetween" pap.brcBetween BRC
        { 43, { 2, L_FIX} }, // "sprmPBrcBar" pap.brcBar BRC word
        { 44, { 1, L_FIX} }, // "sprmPFNoAutoHyph" pap.fNoAutoHyph
        { 45, { 2, L_FIX} }, // "sprmPWHeightAbs" pap.wHeightAbs w
        { 46, { 2, L_FIX} }, // "sprmPDcs" pap.dcs DCS
        { 47, { 2, L_FIX} }, // "sprmPShd" pap.shd SHD
        { 48, { 2, L_FIX} }, // "sprmPDyaFromText" pap.dyaFromText dya
        { 49, { 2, L_FIX} }, // "sprmPDxaFromText" pap.dxaFromText dxa
        { 50, { 1, L_FIX} }, // "sprmPFBiDi" pap.fBiDi 0 or 1 byte
        { 51, { 1, L_FIX} }, // "sprmPFWidowControl" pap.fWidowControl 0 or 1 byte
        { 52, { 0, L_FIX} }, // "?sprmPRuler 52"
        { 53, { 1, L_FIX} }, // "sprmCFStrikeRM" chp.fRMarkDel 1 or 0 bit
        { 54, { 1, L_FIX} }, // "sprmCFRMark" chp.fRMark 1 or 0 bit
        { 55, { 1, L_FIX} }, // "sprmCFFieldVanish" chp.fFieldVanish 1 or 0 bit
        { 57, { 0, L_VAR} }, // "sprmCDefault" whole CHP
        { 58, { 0, L_FIX} }, // "sprmCPlain" whole CHP
        { 60, { 1, L_FIX} }, // "sprmCFBold" chp.fBold 0,1, 128, or 129
        { 61, { 1, L_FIX} }, // "sprmCFItalic" chp.fItalic 0,1, 128, or 129
        { 62, { 1, L_FIX} }, // "sprmCFStrike" chp.fStrike 0,1, 128, or 129
        { 63, { 1, L_FIX} }, // "sprmCFOutline" chp.fOutline 0,1, 128, or 129
        { 64, { 1, L_FIX} }, // "sprmCFShadow" chp.fShadow 0,1, 128, or 129
        { 65, { 1, L_FIX} }, // "sprmCFSmallCaps" chp.fSmallCaps 0,1, 128, or 129
        { 66, { 1, L_FIX} }, // "sprmCFCaps" chp.fCaps 0,1, 128, or 129
        { 67, { 1, L_FIX} }, // "sprmCFVanish" chp.fVanish 0,1, 128, or 129
        { 68, { 2, L_FIX} }, // "sprmCFtc" chp.ftc ftc word
        { 69, { 1, L_FIX} }, // "sprmCKul" chp.kul kul byte
        { 70, { 3, L_FIX} }, // "sprmCSizePos" chp.hps, chp.hpsPos
        { 71, { 2, L_FIX} }, // "sprmCDxaSpace" chp.dxaSpace dxa
        { 72, { 2, L_FIX} }, // "sprmCLid" chp.lid LID
        { 73, { 1, L_FIX} }, // "sprmCIco" chp.ico ico byte
        { 74, { 1, L_FIX} }, // "sprmCHps" chp.hps hps !word!
        { 75, { 1, L_FIX} }, // "sprmCHpsInc" chp.hps
        { 76, { 1, L_FIX} }, // "sprmCHpsPos" chp.hpsPos hps !word!
        { 77, { 1, L_FIX} }, // "sprmCHpsPosAdj" chp.hpsPos hps
        { 78, { 0, L_VAR} }, // "?sprmCMajority" chp.fBold, chp.fItalic, ...
        { 80, { 1, L_FIX} }, // "sprmCFBoldBi" chp.fBoldBi
        { 81, { 1, L_FIX} }, // "sprmCFItalicBi" chp.fItalicBi
        { 82, { 2, L_FIX} }, // "sprmCFtcBi" chp.ftcBi
        { 83, { 2, L_FIX} }, // "sprmClidBi" chp.lidBi
        { 84, { 1, L_FIX} }, // "sprmCIcoBi" chp.icoBi
        { 85, { 1, L_FIX} }, // "sprmCHpsBi" chp.hpsBi
        { 86, { 1, L_FIX} }, // "sprmCFBiDi" chp.fBiDi
        { 87, { 1, L_FIX} }, // "sprmCFDiacColor" chp.fDiacUSico
        { 94, { 1, L_FIX} }, // "sprmPicBrcl" pic.brcl brcl (see PIC definition)
        { 95, {12, L_VAR} }, // "sprmPicScale" pic.mx, pic.my, pic.dxaCropleft,
        { 96, { 2, L_FIX} }, // "sprmPicBrcTop" pic.brcTop BRC word
        { 97, { 2, L_FIX} }, // "sprmPicBrcLeft" pic.brcLeft BRC word
        { 98, { 2, L_FIX} }, // "sprmPicBrcBottom" pic.brcBottom BRC word
        { 99, { 2, L_FIX} }, // "sprmPicBrcRight" pic.brcRight BRC word
        {112, { 1, L_FIX} }, // "sprmSFRTLGutter", set to one if gutter is on
        {114, { 1, L_FIX} }, // "sprmSFBiDi" ;;;
        {115, { 2, L_FIX} }, // "sprmSDmBinFirst" sep.dmBinFirst  word
        {116, { 2, L_FIX} }, // "sprmSDmBinOther" sep.dmBinOther  word
        {117, { 1, L_FIX} }, // "sprmSBkc" sep.bkc bkc byte
        {118, { 1, L_FIX} }, // "sprmSFTitlePage" sep.fTitlePage 0 or 1 byte
        {119, { 2, L_FIX} }, // "sprmSCcolumns" sep.ccolM1 # of cols - 1 word
        {120, { 2, L_FIX} }, // "sprmSDxaColumns" sep.dxaColumns dxa word
        {121, { 1, L_FIX} }, // "sprmSFAutoPgn" sep.fAutoPgn obsolete byte
        {122, { 1, L_FIX} }, // "sprmSNfcPgn" sep.nfcPgn nfc byte
        {123, { 2, L_FIX} }, // "sprmSDyaPgn" sep.dyaPgn dya short
        {124, { 2, L_FIX} }, // "sprmSDxaPgn" sep.dxaPgn dya short
        {125, { 1, L_FIX} }, // "sprmSFPgnRestart" sep.fPgnRestart 0 or 1 byte
        {126, { 1, L_FIX} }, // "sprmSFEndnote" sep.fEndnote 0 or 1 byte
        {127, { 1, L_FIX} }, // "sprmSLnc" sep.lnc lnc byte
        {128, { 1, L_FIX} }, // "sprmSGprfIhdt" sep.grpfIhdt grpfihdt
        {129, { 2, L_FIX} }, // "sprmSNLnnMod" sep.nLnnMod non-neg int. word
        {130, { 2, L_FIX} }, // "sprmSDxaLnn" sep.dxaLnn dxa word
        {131, { 2, L_FIX} }, // "sprmSDyaHdrTop" sep.dyaHdrTop dya word
        {132, { 2, L_FIX} }, // "sprmSDyaHdrBottom" sep.dyaHdrBottom dya word
        {133, { 1, L_FIX} }, // "sprmSLBetween" sep.fLBetween 0 or 1 byte
        {134, { 1, L_FIX} }, // "sprmSVjc" sep.vjc vjc byte
        {135, { 2, L_FIX} }, // "sprmSLnnMin" sep.lnnMin lnn word
        {136, { 2, L_FIX} }, // "sprmSPgnStart" sep.pgnStart pgn word
        {137, { 1, L_FIX} }, // "sprmSBOrientation" sep.dmOrientPage dm byte
        {138, { 1, L_FIX} }, // "sprmSFFacingCol" ;;;
        {139, { 2, L_FIX} }, // "sprmSXaPage" sep.xaPage xa word
        {140, { 2, L_FIX} }, // "sprmSYaPage" sep.yaPage ya word
        {141, { 2, L_FIX} }, // "sprmSDxaLeft" sep.dxaLeft dxa word
        {142, { 2, L_FIX} }, // "sprmSDxaRight" sep.dxaRight dxa word
        {143, { 2, L_FIX} }, // "sprmSDyaTop" sep.dyaTop dya word
        {144, { 2, L_FIX} }, // "sprmSDyaBottom" sep.dyaBottom dya word
        {145, { 2, L_FIX} }, // "sprmSDzaGutter" sep.dzaGutter dza word
        {146, { 2, L_FIX} }, // "sprmTJc" tap.jc jc (low order byte is significant)
        {147, { 2, L_FIX} }, // "sprmTDxaLeft" tap.rgdxaCenter dxa word
        {148, { 2, L_FIX} }, // "sprmTDxaGapHalf" tap.dxaGapHalf, tap.rgdxaCenter
        {149, { 1, L_FIX} }, // "sprmTFBiDi" ;;;
        {152, { 0, L_VAR2} },// "sprmTDefTable10" tap.rgdxaCenter, tap.rgtc complex
        {153, { 2, L_FIX} }, // "sprmTDyaRowHeight" tap.dyaRowHeight dya word
        {154, { 0, L_VAR2} },// "sprmTDefTable" tap.rgtc complex
        {155, { 1, L_VAR} }, // "sprmTDefTableShd" tap.rgshd complex
        {157, { 5, L_FIX} }, // "sprmTSetBrc" tap.rgtc[].rgbrc complex 5 bytes
        {158, { 4, L_FIX} }, // "sprmTInsert" tap.rgdxaCenter,tap.rgtc complex
        {159, { 2, L_FIX} }, // "sprmTDelete" tap.rgdxaCenter, tap.rgtc complex
        {160, { 4, L_FIX} }, // "sprmTDxaCol" tap.rgdxaCenter complex
        {161, { 2, L_FIX} }, // "sprmTMerge" tap.fFirstMerged, tap.fMerged complex
        {162, { 2, L_FIX} }, // "sprmTSplit" tap.fFirstMerged, tap.fMerged complex
        {163, { 5, L_FIX} }, // "sprmTSetBrc10" tap.rgtc[].rgbrc complex 5 bytes
        {164, { 4, L_FIX} }, // "sprmTSetShd", tap.rgshd complex 4 bytes
    };

    static wwSprmSearcher aSprmSrch(aSprms, SAL_N_ELEMENTS(aSprms));
    return &aSprmSrch;
};

const wwSprmSearcher *wwSprmParser::GetWW6SprmSearcher(const WW8Fib& rFib)
{
    //double lock me
    // WW7- Sprms
    static const SprmInfoRow aSprms[] =
    {
        {  0, { 0, L_FIX} }, // "Default-sprm", is skipped
        {NS_sprm::v6::sprmPIstd, { 2, L_FIX} }, // pap.istd (style code)
        {NS_sprm::v6::sprmPIstdPermute, { 3, L_VAR} }, // pap.istd permutation
        {NS_sprm::v6::sprmPIncLv1, { 1, L_FIX} }, // pap.istddifference
        {NS_sprm::v6::sprmPJc, { 1, L_FIX} }, // pap.jc (justification)
        {NS_sprm::v6::sprmPFSideBySide, { 1, L_FIX} }, // pap.fSideBySide
        {NS_sprm::v6::sprmPFKeep, { 1, L_FIX} }, // pap.fKeep
        {NS_sprm::v6::sprmPFKeepFollow, { 1, L_FIX} }, // pap.fKeepFollow
        {NS_sprm::v6::sprmPPageBreakBefore, { 1, L_FIX} }, // pap.fPageBreakBefore
        {NS_sprm::v6::sprmPBrcl, { 1, L_FIX} }, // pap.brcl
        {NS_sprm::v6::sprmPBrcp, { 1, L_FIX} }, // pap.brcp
        {NS_sprm::v6::sprmPAnld, { 0, L_VAR} }, // pap.anld (ANLD structure)
        {NS_sprm::v6::sprmPNLvlAnm, { 1, L_FIX} }, // pap.nLvlAnm nn
        {NS_sprm::v6::sprmPFNoLineNumb, { 1, L_FIX} }, // pap.fNoLnn
        {NS_sprm::v6::sprmPChgTabsPapx, { 0, L_VAR} }, // pap.itbdMac, ...
        {NS_sprm::v6::sprmPDxaRight, { 2, L_FIX} }, // pap.dxaRight
        {NS_sprm::v6::sprmPDxaLeft, { 2, L_FIX} }, // pap.dxaLeft
        {NS_sprm::v6::sprmPNest, { 2, L_FIX} }, // pap.dxaLeft
        {NS_sprm::v6::sprmPDxaLeft1, { 2, L_FIX} }, // pap.dxaLeft1
        {NS_sprm::v6::sprmPDyaLine, { 4, L_FIX} }, // pap.lspd an LSPD
        {NS_sprm::v6::sprmPDyaBefore, { 2, L_FIX} }, // pap.dyaBefore
        {NS_sprm::v6::sprmPDyaAfter, { 2, L_FIX} }, // pap.dyaAfter
        {NS_sprm::v6::sprmPChgTabs, { 0, L_VAR} }, // pap.itbdMac, pap.rgdxaTab, ...
        {NS_sprm::v6::sprmPFInTable, { 1, L_FIX} }, // pap.fInTable
        {NS_sprm::v6::sprmPTtp, { 1, L_FIX} }, // pap.fTtp
        {NS_sprm::v6::sprmPDxaAbs, { 2, L_FIX} }, // pap.dxaAbs
        {NS_sprm::v6::sprmPDyaAbs, { 2, L_FIX} }, // pap.dyaAbs
        {NS_sprm::v6::sprmPDxaWidth, { 2, L_FIX} }, // pap.dxaWidth
        {NS_sprm::v6::sprmPPc, { 1, L_FIX} }, // pap.pcHorz, pap.pcVert
        {NS_sprm::v6::sprmPBrcTop10, { 2, L_FIX} }, // pap.brcTop BRC10
        {NS_sprm::v6::sprmPBrcLeft10, { 2, L_FIX} }, // pap.brcLeft BRC10
        {NS_sprm::v6::sprmPBrcBottom10, { 2, L_FIX} }, // pap.brcBottom BRC10
        {NS_sprm::v6::sprmPBrcRight10, { 2, L_FIX} }, // pap.brcRight BRC10
        {NS_sprm::v6::sprmPBrcBetween10, { 2, L_FIX} }, // pap.brcBetween BRC10
        {NS_sprm::v6::sprmPBrcBar10, { 2, L_FIX} }, // pap.brcBar BRC10
        {NS_sprm::v6::sprmPFromText10, { 2, L_FIX} }, // pap.dxaFromText dxa
        {NS_sprm::v6::sprmPWr, { 1, L_FIX} }, // pap.wr wr
        {NS_sprm::v6::sprmPBrcTop, { 2, L_FIX} }, // pap.brcTop BRC
        {NS_sprm::v6::sprmPBrcLeft, { 2, L_FIX} }, // pap.brcLeft BRC
        {NS_sprm::v6::sprmPBrcBottom, { 2, L_FIX} }, // pap.brcBottom BRC
        {NS_sprm::v6::sprmPBrcRight, { 2, L_FIX} }, // pap.brcRight BRC
        {NS_sprm::v6::sprmPBrcBetween, { 2, L_FIX} }, // pap.brcBetween BRC
        {NS_sprm::v6::sprmPBrcBar, { 2, L_FIX} }, // pap.brcBar BRC word
        {NS_sprm::v6::sprmPFNoAutoHyph, { 1, L_FIX} }, // pap.fNoAutoHyph
        {NS_sprm::v6::sprmPWHeightAbs, { 2, L_FIX} }, // pap.wHeightAbs w
        {NS_sprm::v6::sprmPDcs, { 2, L_FIX} }, // pap.dcs DCS
        {NS_sprm::v6::sprmPShd, { 2, L_FIX} }, // pap.shd SHD
        {NS_sprm::v6::sprmPDyaFromText, { 2, L_FIX} }, // pap.dyaFromText dya
        {NS_sprm::v6::sprmPDxaFromText, { 2, L_FIX} }, // pap.dxaFromText dxa
        {NS_sprm::v6::sprmPFLocked, { 1, L_FIX} }, // pap.fLocked 0 or 1 byte
        {NS_sprm::v6::sprmPFWidowControl, { 1, L_FIX} }, // pap.fWidowControl 0 or 1 byte
        {NS_sprm::v6::sprmPRuler, { 0, L_FIX} },
        { 64, { 0, L_VAR} }, // rtl property ?
        {NS_sprm::v6::sprmCFStrikeRM, { 1, L_FIX} }, // chp.fRMarkDel 1 or 0 bit
        {NS_sprm::v6::sprmCFRMark, { 1, L_FIX} }, // chp.fRMark 1 or 0 bit
        {NS_sprm::v6::sprmCFFldVanish, { 1, L_FIX} }, // chp.fFieldVanish 1 or 0 bit
        {NS_sprm::v6::sprmCPicLocation, { 0, L_VAR} }, // chp.fcPic and chp.fSpec
        {NS_sprm::v6::sprmCIbstRMark, { 2, L_FIX} }, // chp.ibstRMark index into sttbRMark
        {NS_sprm::v6::sprmCDttmRMark, { 4, L_FIX} }, // chp.dttm DTTM long
        {NS_sprm::v6::sprmCFData, { 1, L_FIX} }, // chp.fData 1 or 0 bit
        {NS_sprm::v6::sprmCRMReason, { 2, L_FIX} }, // chp.idslRMReason an index to a table
        {NS_sprm::v6::sprmCChse, { 3, L_FIX} }, // chp.fChsDiff and chp.chse
        {NS_sprm::v6::sprmCSymbol, { 0, L_VAR} }, // chp.fSpec, chp.chSym and chp.ftcSym
        {NS_sprm::v6::sprmCFOle2, { 1, L_FIX} }, // chp.fOle2 1 or 0   bit
        { 77, { 0, L_VAR} }, // unknown
        { 79, { 0, L_VAR} }, // unknown
        {NS_sprm::v6::sprmCIstd, { 2, L_FIX} }, // chp.istd istd, see stylesheet definition
        {NS_sprm::v6::sprmCIstdPermute, { 0, L_VAR} }, // chp.istd permutation vector
        {NS_sprm::v6::sprmCDefault, { 0, L_VAR} }, // whole CHP
        {NS_sprm::v6::sprmCPlain, { 0, L_FIX} }, // whole CHP
        {NS_sprm::v6::sprmCFBold, { 1, L_FIX} }, // chp.fBold 0,1, 128, or 129
        {NS_sprm::v6::sprmCFItalic, { 1, L_FIX} }, // chp.fItalic 0,1, 128, or 129
        {NS_sprm::v6::sprmCFStrike, { 1, L_FIX} }, // chp.fStrike 0,1, 128, or 129
        {NS_sprm::v6::sprmCFOutline, { 1, L_FIX} }, // chp.fOutline 0,1, 128, or 129
        {NS_sprm::v6::sprmCFShadow, { 1, L_FIX} }, // chp.fShadow 0,1, 128, or 129
        {NS_sprm::v6::sprmCFSmallCaps, { 1, L_FIX} }, // chp.fSmallCaps 0,1, 128, or 129
        {NS_sprm::v6::sprmCFCaps, { 1, L_FIX} }, // chp.fCaps 0,1, 128, or 129
        {NS_sprm::v6::sprmCFVanish, { 1, L_FIX} }, // chp.fVanish 0,1, 128, or 129
        {NS_sprm::v6::sprmCFtc, { 2, L_FIX} }, // chp.ftc ftc word
        {NS_sprm::v6::sprmCKul, { 1, L_FIX} }, // chp.kul kul byte
        {NS_sprm::v6::sprmCSizePos, { 3, L_FIX} }, // chp.hps, chp.hpsPos
        {NS_sprm::v6::sprmCDxaSpace, { 2, L_FIX} }, // chp.dxaSpace dxa
        {NS_sprm::v6::sprmCLid, { 2, L_FIX} }, // chp.lid LID
        {NS_sprm::v6::sprmCIco, { 1, L_FIX} }, // chp.ico ico byte
        {NS_sprm::v6::sprmCHps, { 2, L_FIX} }, // chp.hps hps !word!
        {NS_sprm::v6::sprmCHpsInc, { 1, L_FIX} }, // chp.hps
        {NS_sprm::v6::sprmCHpsPos, { 2, L_FIX} }, // chp.hpsPos hps !word!
        {NS_sprm::v6::sprmCHpsPosAdj, { 1, L_FIX} }, // chp.hpsPos hps
        {NS_sprm::v6::sprmCMajority, { 0, L_VAR} }, // chp.fBold, chp.fItalic, ...
        {NS_sprm::v6::sprmCIss, { 1, L_FIX} }, // chp.iss iss
        {NS_sprm::v6::sprmCHpsNew50, { 0, L_VAR} }, // chp.hps hps variable width
        {NS_sprm::v6::sprmCHpsInc1, { 0, L_VAR} }, // chp.hps complex
        {NS_sprm::v6::sprmCHpsKern, { 2, L_FIX} }, // chp.hpsKern hps
        {NS_sprm::v6::sprmCMajority50, { 0, L_VAR} }, // chp.fBold, chp.fItalic, ...
        {NS_sprm::v6::sprmCHpsMul, { 2, L_FIX} }, // chp.hps percentage to grow hps
        {NS_sprm::v6::sprmCCondHyhen, { 2, L_FIX} }, // chp.ysri ysri
        {111, { 0, L_VAR} }, // sprmCFBoldBi or font code
        {112, { 0, L_VAR} }, // sprmCFItalicBi or font code
        {113, { 0, L_VAR} }, // ww7 rtl font
        {114, { 0, L_VAR} }, // ww7 lid
        {115, { 0, L_VAR} }, // ww7 CJK font
        {116, { 0, L_VAR} }, // ww7 fontsize
        {NS_sprm::v6::sprmCFSpec, { 1, L_FIX} }, // chp.fSpec  1 or 0 bit
        {NS_sprm::v6::sprmCFObj, { 1, L_FIX} }, // chp.fObj 1 or 0 bit
        {NS_sprm::v6::sprmPicBrcl, { 1, L_FIX} }, // pic.brcl brcl (see PIC definition)
        {NS_sprm::v6::sprmPicScale, {12, L_VAR} }, // pic.mx, pic.my, pic.dxaCropleft,
        {NS_sprm::v6::sprmPicBrcTop, { 2, L_FIX} }, // pic.brcTop BRC word
        {NS_sprm::v6::sprmPicBrcLeft, { 2, L_FIX} }, // pic.brcLeft BRC word
        {NS_sprm::v6::sprmPicBrcBottom, { 2, L_FIX} }, // pic.brcBottom BRC word
        {NS_sprm::v6::sprmPicBrcRight, { 2, L_FIX} }, // pic.brcRight BRC word
        {NS_sprm::v6::sprmSScnsPgn, { 1, L_FIX} }, // sep.cnsPgn cns byte
        {NS_sprm::v6::sprmSiHeadingPgn, { 1, L_FIX} }, // sep.iHeadingPgn
        {NS_sprm::v6::sprmSOlstAnm, { 0, L_VAR} }, // sep.olstAnm OLST variable length
        {NS_sprm::v6::sprmSDxaColWidth, { 3, L_FIX} }, // sep.rgdxaColWidthSpacing complex
        {NS_sprm::v6::sprmSDxaColSpacing, { 3, L_FIX} }, // sep.rgdxaColWidthSpacing
        {NS_sprm::v6::sprmSFEvenlySpaced, { 1, L_FIX} }, // sep.fEvenlySpaced 1 or 0
        {NS_sprm::v6::sprmSFProtected, { 1, L_FIX} }, // sep.fUnlocked 1 or 0 byte
        {NS_sprm::v6::sprmSDmBinFirst, { 2, L_FIX} }, // sep.dmBinFirst  word
        {NS_sprm::v6::sprmSDmBinOther, { 2, L_FIX} }, // sep.dmBinOther  word
        {NS_sprm::v6::sprmSBkc, { 1, L_FIX} }, // sep.bkc bkc byte
        {NS_sprm::v6::sprmSFTitlePage, { 1, L_FIX} }, // sep.fTitlePage 0 or 1 byte
        {NS_sprm::v6::sprmSCcolumns, { 2, L_FIX} }, // sep.ccolM1 # of cols - 1 word
        {NS_sprm::v6::sprmSDxaColumns, { 2, L_FIX} }, // sep.dxaColumns dxa word
        {NS_sprm::v6::sprmSFAutoPgn, { 1, L_FIX} }, // sep.fAutoPgn obsolete byte
        {NS_sprm::v6::sprmSNfcPgn, { 1, L_FIX} }, // sep.nfcPgn nfc byte
        {NS_sprm::v6::sprmSDyaPgn, { 2, L_FIX} }, // sep.dyaPgn dya short
        {NS_sprm::v6::sprmSDxaPgn, { 2, L_FIX} }, // sep.dxaPgn dya short
        {NS_sprm::v6::sprmSFPgnRestart, { 1, L_FIX} }, // sep.fPgnRestart 0 or 1 byte
        {NS_sprm::v6::sprmSFEndnote, { 1, L_FIX} }, // sep.fEndnote 0 or 1 byte
        {NS_sprm::v6::sprmSLnc, { 1, L_FIX} }, // sep.lnc lnc byte
        {NS_sprm::v6::sprmSGprfIhdt, { 1, L_FIX} }, // sep.grpfIhdt grpfihdt
        {NS_sprm::v6::sprmSNLnnMod, { 2, L_FIX} }, // sep.nLnnMod non-neg int. word
        {NS_sprm::v6::sprmSDxaLnn, { 2, L_FIX} }, // sep.dxaLnn dxa word
        {NS_sprm::v6::sprmSDyaHdrTop, { 2, L_FIX} }, // sep.dyaHdrTop dya word
        {NS_sprm::v6::sprmSDyaHdrBottom, { 2, L_FIX} }, // sep.dyaHdrBottom dya word
        {NS_sprm::v6::sprmSLBetween, { 1, L_FIX} }, // sep.fLBetween 0 or 1 byte
        {NS_sprm::v6::sprmSVjc, { 1, L_FIX} }, // sep.vjc vjc byte
        {NS_sprm::v6::sprmSLnnMin, { 2, L_FIX} }, // sep.lnnMin lnn word
        {NS_sprm::v6::sprmSPgnStart, { 2, L_FIX} }, // sep.pgnStart pgn word
        {NS_sprm::v6::sprmSBOrientation, { 1, L_FIX} }, // sep.dmOrientPage dm byte
        {NS_sprm::v6::sprmSBCustomize, { 0, L_FIX} },
        {NS_sprm::v6::sprmSXaPage, { 2, L_FIX} }, // sep.xaPage xa word
        {NS_sprm::v6::sprmSYaPage, { 2, L_FIX} }, // sep.yaPage ya word
        {NS_sprm::v6::sprmSDxaLeft, { 2, L_FIX} }, // sep.dxaLeft dxa word
        {NS_sprm::v6::sprmSDxaRight, { 2, L_FIX} }, // sep.dxaRight dxa word
        {NS_sprm::v6::sprmSDyaTop, { 2, L_FIX} }, // sep.dyaTop dya word
        {NS_sprm::v6::sprmSDyaBottom, { 2, L_FIX} }, // sep.dyaBottom dya word
        {NS_sprm::v6::sprmSDzaGutter, { 2, L_FIX} }, // sep.dzaGutter dza word
        {NS_sprm::v6::sprmSDMPaperReq, { 2, L_FIX} }, // sep.dmPaperReq dm word
        {179, { 0, L_VAR} }, // rtl property ?
        {181, { 0, L_VAR} }, // rtl property ?
        {NS_sprm::v6::sprmTJc, { 2, L_FIX} }, // tap.jc jc (low order byte is significant)
        {NS_sprm::v6::sprmTDxaLeft, { 2, L_FIX} }, // tap.rgdxaCenter dxa word
        {NS_sprm::v6::sprmTDxaGapHalf, { 2, L_FIX} }, // tap.dxaGapHalf, tap.rgdxaCenter
        {NS_sprm::v6::sprmTFCantSplit, { 1, L_FIX} }, // tap.fCantSplit 1 or 0 byte
        {NS_sprm::v6::sprmTTableHeader, { 1, L_FIX} }, // tap.fTableHeader 1 or 0 byte
        {NS_sprm::v6::sprmTTableBorders, {12, L_FIX} }, // tap.rgbrcTable complex 12 bytes
        {NS_sprm::v6::sprmTDefTable10, { 0, L_VAR2} }, // tap.rgdxaCenter, tap.rgtc complex
        {NS_sprm::v6::sprmTDyaRowHeight, { 2, L_FIX} }, // tap.dyaRowHeight dya word
        {NS_sprm::v6::sprmTDefTable, { 0, L_VAR2} }, // tap.rgtc complex
        {NS_sprm::v6::sprmTDefTableShd, { 1, L_VAR} }, // tap.rgshd complex
        {NS_sprm::v6::sprmTTlp, { 4, L_FIX} }, // tap.tlp TLP 4 bytes
        {NS_sprm::v6::sprmTSetBrc, { 5, L_FIX} }, // tap.rgtc[].rgbrc complex 5 bytes
        {NS_sprm::v6::sprmTInsert, { 4, L_FIX} }, // tap.rgdxaCenter,tap.rgtc complex
        {NS_sprm::v6::sprmTDelete, { 2, L_FIX} }, // tap.rgdxaCenter, tap.rgtc complex
        {NS_sprm::v6::sprmTDxaCol, { 4, L_FIX} }, // tap.rgdxaCenter complex
        {NS_sprm::v6::sprmTMerge, { 2, L_FIX} }, // tap.fFirstMerged, tap.fMerged complex
        {NS_sprm::v6::sprmTSplit, { 2, L_FIX} }, // tap.fFirstMerged, tap.fMerged complex
        {NS_sprm::v6::sprmTSetBrc10, { 5, L_FIX} }, // tap.rgtc[].rgbrc complex 5 bytes
        {NS_sprm::v6::sprmTSetShd, { 4, L_FIX} }, // tap.rgshd complex 4 bytes
        {207, { 0, L_VAR} }  // rtl property ?
    };

    if (rFib.m_wIdent >= 0xa697 && rFib.m_wIdent <= 0xa699)
    {
        //see Read_AmbiguousSPRM for this oddity
        static wwSprmSearcher aSprmSrch(aSprms, SAL_N_ELEMENTS(aSprms), true);
        return &aSprmSrch;
    }

    static wwSprmSearcher aSprmSrch(aSprms, SAL_N_ELEMENTS(aSprms));
    return &aSprmSrch;
};

void wwSprmSearcher::patchCJKVariant()
{
    for (sal_uInt16 nId = 111; nId <= 113; ++nId)
    {
        SprmInfo& amb1 = map_[nId];
        amb1.nLen = 2;
        amb1.nVari = wwSprmParser::L_FIX;
    }
}

template <class Sprm> static constexpr SprmInfoRow InfoRow()
{
    return { Sprm::val, { Sprm::len, Sprm::varlen ? wwSprmParser::L_VAR : wwSprmParser::L_FIX } };
}

const wwSprmSearcher *wwSprmParser::GetWW8SprmSearcher()
{
    //double lock me
    //WW8+ Sprms
    static const SprmInfoRow aSprms[] =
    {
        {     0, { 0, L_FIX} }, // "Default-sprm"/ is skipped
        InfoRow<NS_sprm::PIstd>(), // pap.istd;istd (style code);short;
        InfoRow<NS_sprm::PIstdPermute>(), // pap.istd;permutation vector
        InfoRow<NS_sprm::PIncLvl>(), // pap.istd, pap.lvl;difference
                            // between istd of base PAP and istd of PAP to be
                            // produced
        InfoRow<NS_sprm::PJc80>(), // pap.jc;jc (justification);byte;
        {NS_sprm::LN_PFSideBySide, { 1, L_FIX} }, // "sprmPFSideBySide" pap.fSideBySide;0 or 1;byte;
        InfoRow<NS_sprm::PFKeep>(), // pap.fKeep;0 or 1;byte;
        InfoRow<NS_sprm::PFKeepFollow>(), // pap.fKeepFollow;0 or 1;byte;
        InfoRow<NS_sprm::PFPageBreakBefore>(), // pap.fPageBreakBefore;
                            // 0 or 1
        {NS_sprm::LN_PBrcl, { 1, L_FIX} }, // "sprmPBrcl" pap.brcl;brcl;byte;
        {NS_sprm::LN_PBrcp, { 1, L_FIX} }, // "sprmPBrcp" pap.brcp;brcp;byte;
        InfoRow<NS_sprm::PIlvl>(), // pap.ilvl;ilvl;byte;
        InfoRow<NS_sprm::PIlfo>(), // pap.ilfo;ilfo (list index) ;short;
        InfoRow<NS_sprm::PFNoLineNumb>(), // pap.fNoLnn;0 or 1;byte;
        InfoRow<NS_sprm::PChgTabsPapx>(), // pap.itbdMac, pap.rgdxaTab,
                            // pap.rgtbd;complex
        InfoRow<NS_sprm::PDxaRight80>(), // pap.dxaRight;dxa;word;
        InfoRow<NS_sprm::PDxaLeft80>(), // pap.dxaLeft;dxa;word;
        InfoRow<NS_sprm::PNest80>(), // pap.dxaLeft;dxa
        InfoRow<NS_sprm::PDxaLeft180>(), // pap.dxaLeft1;dxa;word;
        InfoRow<NS_sprm::PDyaLine>(), // pap.lspd;an LSPD, a long word
                            // structure consisting of a short of dyaLine
                            // followed by a short of fMultLinespace
        InfoRow<NS_sprm::PDyaBefore>(), // pap.dyaBefore;dya;word;
        InfoRow<NS_sprm::PDyaAfter>(), // pap.dyaAfter;dya;word;
        InfoRow<NS_sprm::PChgTabs>(), // pap.itbdMac, pap.rgdxaTab,
                            // pap.rgtbd;complex
        InfoRow<NS_sprm::PFInTable>(), // pap.fInTable;0 or 1;byte;
        InfoRow<NS_sprm::PFTtp>(), // pap.fTtp;0 or 1;byte;
        InfoRow<NS_sprm::PDxaAbs>(), // pap.dxaAbs;dxa;word;
        InfoRow<NS_sprm::PDyaAbs>(), // pap.dyaAbs;dya;word;
        InfoRow<NS_sprm::PDxaWidth>(), // pap.dxaWidth;dxa;word;
        InfoRow<NS_sprm::PPc>(), // pap.pcHorz, pap.pcVert;complex
        {NS_sprm::LN_PBrcTop10, { 2, L_FIX} }, // "sprmPBrcTop10" pap.brcTop;BRC10;word;
        {NS_sprm::LN_PBrcLeft10, { 2, L_FIX} }, // "sprmPBrcLeft10" pap.brcLeft;BRC10;word;
        {NS_sprm::LN_PBrcBottom10, { 2, L_FIX} }, // "sprmPBrcBottom10" pap.brcBottom;BRC10;word;
        {NS_sprm::LN_PBrcRight10, { 2, L_FIX} }, // "sprmPBrcRight10" pap.brcRight;BRC10;word;
        {NS_sprm::LN_PBrcBetween10, { 2, L_FIX} }, // "sprmPBrcBetween10" pap.brcBetween;BRC10;word;
        {NS_sprm::LN_PBrcBar10, { 2, L_FIX} }, // "sprmPBrcBar10" pap.brcBar;BRC10;word;
        {NS_sprm::LN_PDxaFromText10, { 2, L_FIX} }, // "sprmPDxaFromText10" pap.dxaFromText;dxa;word;
        InfoRow<NS_sprm::PWr>(), // pap.wr;wr
        InfoRow<NS_sprm::PBrcTop80>(), // pap.brcTop;BRC;long;
        InfoRow<NS_sprm::PBrcLeft80>(), // pap.brcLeft;BRC;long;
        InfoRow<NS_sprm::PBrcBottom80>(), // pap.brcBottom;BRC;long;
        InfoRow<NS_sprm::PBrcRight80>(), // pap.brcRight;BRC;long;
        InfoRow<NS_sprm::PBrcBetween80>(), // pap.brcBetween;BRC;long;
        InfoRow<NS_sprm::PBrcBar80>(), // pap.brcBar;BRC;long;
        InfoRow<NS_sprm::PFNoAutoHyph>(), // pap.fNoAutoHyph;0 or 1;byte;
        InfoRow<NS_sprm::PWHeightAbs>(), // pap.wHeightAbs;w;word;
        InfoRow<NS_sprm::PDcs>(), // pap.dcs;DCS;short;
        InfoRow<NS_sprm::PShd80>(), // pap.shd;SHD;word;
        InfoRow<NS_sprm::PDyaFromText>(), // pap.dyaFromText;dya;word;
        InfoRow<NS_sprm::PDxaFromText>(), // pap.dxaFromText;dxa;word;
        InfoRow<NS_sprm::PFLocked>(), // pap.fLocked;0 or 1;byte;
        InfoRow<NS_sprm::PFWidowControl>(), // pap.fWidowControl;0 or 1
        {NS_sprm::LN_PRuler, { 0, L_VAR} }, // "sprmPRuler" ;;variable length;
        InfoRow<NS_sprm::PFKinsoku>(), // pap.fKinsoku;0 or 1;byte;
        InfoRow<NS_sprm::PFWordWrap>(), // pap.fWordWrap;0 or 1;byte;
        InfoRow<NS_sprm::PFOverflowPunct>(), // pap.fOverflowPunct;0 or 1
        InfoRow<NS_sprm::PFTopLinePunct>(), // pap.fTopLinePunct;0 or 1
        InfoRow<NS_sprm::PFAutoSpaceDE>(), // pap.fAutoSpaceDE;0 or 1
        InfoRow<NS_sprm::PFAutoSpaceDN>(), // pap.fAutoSpaceDN;0 or 1
        InfoRow<NS_sprm::PWAlignFont>(), // pap.wAlignFont;iFa
        InfoRow<NS_sprm::PFrameTextFlow>(), // pap.fVertical pap.fBackward
                            // pap.fRotateFont;complex
        {NS_sprm::LN_PISnapBaseLine, { 1, L_FIX} }, // "sprmPISnapBaseLine" obsolete: not applicable in
                            // Word97 and later versions;
        {NS_sprm::LN_PAnld, { 0, L_VAR} }, // "sprmPAnld" pap.anld;;variable length;
        {NS_sprm::LN_PPropRMark, { 0, L_VAR} }, // "sprmPPropRMark" pap.fPropRMark;complex
        InfoRow<NS_sprm::POutLvl>(), // pap.lvl;has no effect if pap.istd
                            // is < 1 or is > 9
        InfoRow<NS_sprm::PFBiDi>(), // ;;byte;
        InfoRow<NS_sprm::PFNumRMIns>(), // pap.fNumRMIns;1 or 0;bit;
        {NS_sprm::LN_PCrLf, { 1, L_FIX} }, // "sprmPCrLf" ;;byte;
        InfoRow<NS_sprm::PNumRM>(), // pap.numrm;;variable length;
        {NS_sprm::LN_PHugePapx, { 4, L_FIX} }, // "sprmPHugePapx" fc in the data stream to locate
                            // the huge grpprl
        InfoRow<NS_sprm::PHugePapx>(), // fc in the data stream to locate
                            // the huge grpprl
        InfoRow<NS_sprm::PFUsePgsuSettings>(), // pap.fUsePgsuSettings;
                            // 1 or 0
        InfoRow<NS_sprm::PFAdjustRight>(), // pap.fAdjustRight;1 or 0;byte;
        InfoRow<NS_sprm::CFRMarkDel>(), // chp.fRMarkDel;1 or 0;bit;
        InfoRow<NS_sprm::CFRMarkIns>(), // chp.fRMark;1 or 0;bit;
        InfoRow<NS_sprm::CFFldVanish>(), // chp.fFieldVanish;1 or 0;bit;
        InfoRow<NS_sprm::CPicLocation>(), // chp.fcPic and chp.fSpec;
        InfoRow<NS_sprm::CIbstRMark>(), // chp.ibstRMark;index into
                            // sttbRMark
        InfoRow<NS_sprm::CDttmRMark>(), // chp.dttmRMark;DTTM;long;
        InfoRow<NS_sprm::CFData>(), // chp.fData;1 or 0;bit;
        InfoRow<NS_sprm::CIdslRMark>(), // chp.idslRMReason;an index to a
                            // table of strings defined in Word 6.0
                            // executables;short;
        {NS_sprm::LN_CChs, { 1, L_FIX} }, // "sprmCChs" chp.fChsDiff and chp.chse;
        InfoRow<NS_sprm::CSymbol>(), // chp.fSpec, chp.xchSym and
                            // chp.ftcSym
        InfoRow<NS_sprm::CFOle2>(), // chp.fOle2;1 or 0;bit;
        {NS_sprm::LN_CIdCharType, { 0, L_FIX} }, // "sprmCIdCharType" obsolete: not applicable in
                            // Word97 and later versions;
        InfoRow<NS_sprm::CHighlight>(), // chp.fHighlight,
                            // chp.icoHighlight;ico (fHighlight is set to 1 iff
                            // ico is not 0)
        {NS_sprm::LN_CObjLocation, { 4, L_FIX} }, // "sprmCObjLocation" chp.fcObj;FC;long;
        {NS_sprm::LN_CFFtcAsciSymb, { 0, L_FIX} }, // "sprmCFFtcAsciSymb" ;;;
        InfoRow<NS_sprm::CIstd>(), // chp.istd;istd, see stylesheet def
        InfoRow<NS_sprm::CIstdPermute>(), // chp.istd;permutation vector
        {NS_sprm::LN_CDefault, { 0, L_VAR} }, // "sprmCDefault" whole CHP;none;variable length;
        InfoRow<NS_sprm::CPlain>(), // whole CHP;none;0;
        InfoRow<NS_sprm::CKcd>(), // ;;;
        InfoRow<NS_sprm::CFBold>(), // chp.fBold;0,1, 128, or 129
        InfoRow<NS_sprm::CFItalic>(), // chp.fItalic;0,1, 128, or 129
        InfoRow<NS_sprm::CFStrike>(), // chp.fStrike;0,1, 128, or 129
        InfoRow<NS_sprm::CFOutline>(), // chp.fOutline;0,1, 128, or 129
        InfoRow<NS_sprm::CFShadow>(), // chp.fShadow;0,1, 128, or 129
        InfoRow<NS_sprm::CFSmallCaps>(), // chp.fSmallCaps;0,1, 128, or 129
        InfoRow<NS_sprm::CFCaps>(), // chp.fCaps;0,1, 128, or 129
        InfoRow<NS_sprm::CFVanish>(), // chp.fVanish;0,1, 128, or 129
        {NS_sprm::LN_CFtcDefault, { 2, L_FIX} }, // "sprmCFtcDefault" ;ftc, only used internally
        InfoRow<NS_sprm::CKul>(), // chp.kul;kul;byte;
        {NS_sprm::LN_CSizePos, { 3, L_FIX} }, // "sprmCSizePos" chp.hps, chp.hpsPos;3 bytes;
        InfoRow<NS_sprm::CDxaSpace>(), // chp.dxaSpace;dxa;word;
        {NS_sprm::LN_CLid, { 2, L_FIX} }, // "sprmCLid" ;only used internally never stored
        InfoRow<NS_sprm::CIco>(), // chp.ico;ico;byte;
        InfoRow<NS_sprm::CHps>(), // chp.hps;hps
        {NS_sprm::LN_CHpsInc, { 1, L_FIX} }, // "sprmCHpsInc" chp.hps;
        InfoRow<NS_sprm::CHpsPos>(), // chp.hpsPos;hps;short; (doc wrong)
        {NS_sprm::LN_CHpsPosAdj, { 1, L_FIX} }, // "sprmCHpsPosAdj" chp.hpsPos;hps
        InfoRow<NS_sprm::CMajority>(), // chp.fBold, chp.fItalic,
                            // chp.fSmallCaps, chp.fVanish, chp.fStrike,
                            // chp.fCaps, chp.rgftc, chp.hps, chp.hpsPos,
                            // chp.kul, chp.dxaSpace, chp.ico,
                            // chp.rglid;complex;variable length, length byte
                            // plus size of following grpprl;
        InfoRow<NS_sprm::CIss>(), // chp.iss;iss;byte;
        {NS_sprm::LN_CHpsNew50, { 0, L_VAR} }, // "sprmCHpsNew50" chp.hps;hps;variable width
        {NS_sprm::LN_CHpsInc1, { 0, L_VAR} }, // "sprmCHpsInc1" chp.hps;complex
        InfoRow<NS_sprm::CHpsKern>(), // chp.hpsKern;hps;short;
        {NS_sprm::LN_CMajority50, { 2, L_FIX} }, // "sprmCMajority50" chp.fBold, chp.fItalic,
                            // chp.fSmallCaps, chp.fVanish, chp.fStrike,
                            // chp.fCaps, chp.ftc, chp.hps, chp.hpsPos, chp.kul,
                            // chp.dxaSpace, chp.ico,;complex
        {NS_sprm::LN_CHpsMul, { 2, L_FIX} }, // "sprmCHpsMul" chp.hps;percentage to grow hps
        InfoRow<NS_sprm::CHresi>(), // chp.ysri;ysri;short;
        InfoRow<NS_sprm::CRgFtc0>(), // chp.rgftc[0];ftc for ASCII text
        InfoRow<NS_sprm::CRgFtc1>(), // chp.rgftc[1];ftc for Far East text
        InfoRow<NS_sprm::CRgFtc2>(), // chp.rgftc[2];ftc for non-FE text
        InfoRow<NS_sprm::CCharScale>(),
        InfoRow<NS_sprm::CFDStrike>(), // chp.fDStrike;;byte;
        InfoRow<NS_sprm::CFImprint>(), // chp.fImprint;1 or 0;bit;
        InfoRow<NS_sprm::CFSpec>(), // chp.fSpec ;1 or 0;bit;
        InfoRow<NS_sprm::CFObj>(), // chp.fObj;1 or 0;bit;
        InfoRow<NS_sprm::CPropRMark90>(), // chp.fPropRMark,
                            // chp.ibstPropRMark, chp.dttmPropRMark;Complex
        InfoRow<NS_sprm::CFEmboss>(), // chp.fEmboss;1 or 0;bit;
        InfoRow<NS_sprm::CSfxText>(), // chp.sfxtText;text animation;byte;
        InfoRow<NS_sprm::CFBiDi>(), // ;;;
        {NS_sprm::LN_CFDiacColor, { 1, L_FIX} }, // "sprmCFDiacColor" ;;;
        InfoRow<NS_sprm::CFBoldBi>(), // ;;;
        InfoRow<NS_sprm::CFItalicBi>(), // ;;;
        InfoRow<NS_sprm::CFtcBi>(),
        InfoRow<NS_sprm::CLidBi>(), // ;;;
        InfoRow<NS_sprm::CIcoBi>(), // ;;;
        InfoRow<NS_sprm::CHpsBi>(), // ;;;
        InfoRow<NS_sprm::CDispFldRMark>(), // chp.fDispFieldRMark,
                            // chp.ibstDispFieldRMark, chp.dttmDispFieldRMark ;
        InfoRow<NS_sprm::CIbstRMarkDel>(), // chp.ibstRMarkDel;index into
                            // sttbRMark;short;
        InfoRow<NS_sprm::CDttmRMarkDel>(), // chp.dttmRMarkDel;DTTM;long;
        InfoRow<NS_sprm::CBrc80>(), // chp.brc;BRC;long;
        InfoRow<NS_sprm::CShd80>(), // chp.shd;SHD;short;
        InfoRow<NS_sprm::CIdslRMarkDel>(), // chp.idslRMReasonDel;an index
                            // to a table of strings defined in Word 6.0
                            // executables;short;
        InfoRow<NS_sprm::CFUsePgsuSettings>(),
                            // chp.fUsePgsuSettings;1 or 0
        {NS_sprm::LN_CCpg, { 2, L_FIX} }, // "sprmCCpg" ;;word;
        InfoRow<NS_sprm::CRgLid0_80>(), // chp.rglid[0];LID: for non-FE text
        InfoRow<NS_sprm::CRgLid1_80>(), // chp.rglid[1];LID: for Far East text
        InfoRow<NS_sprm::CIdctHint>(), // chp.idctHint;IDCT:
        {NS_sprm::LN_PicBrcl, { 1, L_FIX} }, // "sprmPicBrcl" pic.brcl;brcl (see PIC definition)
        {NS_sprm::LN_PicScale, { 0, L_VAR} }, // "sprmPicScale" pic.mx, pic.my, pic.dxaCropleft,
                            // pic.dyaCropTop pic.dxaCropRight,
                            // pic.dyaCropBottom;Complex
        InfoRow<NS_sprm::PicBrcTop80>(), // pic.brcTop;BRC;long;
        InfoRow<NS_sprm::PicBrcLeft80>(), // pic.brcLeft;BRC;long;
        InfoRow<NS_sprm::PicBrcBottom80>(), // pic.brcBottom;BRC;long;
        InfoRow<NS_sprm::PicBrcRight80>(), // pic.brcRight;BRC;long;
        InfoRow<NS_sprm::ScnsPgn>(), // sep.cnsPgn;cns;byte;
        InfoRow<NS_sprm::SiHeadingPgn>(), // sep.iHeadingPgn;heading number
                            // level;byte;
        {NS_sprm::LN_SOlstAnm, { 0, L_VAR} }, // "sprmSOlstAnm" sep.olstAnm;OLST;variable length;
        InfoRow<NS_sprm::SDxaColWidth>(), // sep.rgdxaColWidthSpacing;
        InfoRow<NS_sprm::SDxaColSpacing>(), // sep.rgdxaColWidthSpacing;
                            // complex
        InfoRow<NS_sprm::SFEvenlySpaced>(), // sep.fEvenlySpaced;1 or 0
        InfoRow<NS_sprm::SFProtected>(), // sep.fUnlocked;1 or 0;byte;
        InfoRow<NS_sprm::SDmBinFirst>(), // sep.dmBinFirst;;word;
        InfoRow<NS_sprm::SDmBinOther>(), // sep.dmBinOther;;word;
        InfoRow<NS_sprm::SBkc>(), // sep.bkc;bkc;byte;
        InfoRow<NS_sprm::SFTitlePage>(), // sep.fTitlePage;0 or 1;byte;
        InfoRow<NS_sprm::SCcolumns>(), // sep.ccolM1;# of cols - 1;word;
        InfoRow<NS_sprm::SDxaColumns>(), // sep.dxaColumns;dxa;word;
        {NS_sprm::LN_SFAutoPgn, { 1, L_FIX} }, // "sprmSFAutoPgn" sep.fAutoPgn;obsolete;byte;
        InfoRow<NS_sprm::SNfcPgn>(), // sep.nfcPgn;nfc;byte;
        {NS_sprm::LN_SDyaPgn, { 2, L_FIX} }, // "sprmSDyaPgn" sep.dyaPgn;dya;short;
        {NS_sprm::LN_SDxaPgn, { 2, L_FIX} }, // "sprmSDxaPgn" sep.dxaPgn;dya;short;
        InfoRow<NS_sprm::SFPgnRestart>(), // sep.fPgnRestart;0 or 1;byte;
        InfoRow<NS_sprm::SFEndnote>(), // sep.fEndnote;0 or 1;byte;
        InfoRow<NS_sprm::SLnc>(), // sep.lnc;lnc;byte;
        {NS_sprm::LN_SGprfIhdt, { 1, L_FIX} }, // "sprmSGprfIhdt" sep.grpfIhdt;grpfihdt
        InfoRow<NS_sprm::SNLnnMod>(), // sep.nLnnMod;non-neg int.;word;
        InfoRow<NS_sprm::SDxaLnn>(), // sep.dxaLnn;dxa;word;
        InfoRow<NS_sprm::SDyaHdrTop>(), // sep.dyaHdrTop;dya;word;
        InfoRow<NS_sprm::SDyaHdrBottom>(), // sep.dyaHdrBottom;dya;word;
        InfoRow<NS_sprm::SLBetween>(), // sep.fLBetween;0 or 1;byte;
        InfoRow<NS_sprm::SVjc>(), // sep.vjc;vjc;byte;
        InfoRow<NS_sprm::SLnnMin>(), // sep.lnnMin;lnn;word;
        InfoRow<NS_sprm::SPgnStart97>(), // sep.pgnStart;pgn;word;
        InfoRow<NS_sprm::SBOrientation>(), // sep.dmOrientPage;dm;byte;
        {NS_sprm::LN_SBCustomize, { 1, L_FIX} }, // "sprmSBCustomize" ;;;
        InfoRow<NS_sprm::SXaPage>(), // sep.xaPage;xa;word;
        InfoRow<NS_sprm::SYaPage>(), // sep.yaPage;ya;word;
        InfoRow<NS_sprm::SDxaLeft>(), // sep.dxaLeft;dxa;word;
        InfoRow<NS_sprm::SDxaRight>(), // sep.dxaRight;dxa;word;
        InfoRow<NS_sprm::SDyaTop>(), // sep.dyaTop;dya;word;
        InfoRow<NS_sprm::SDyaBottom>(), // sep.dyaBottom;dya;word;
        InfoRow<NS_sprm::SDzaGutter>(), // sep.dzaGutter;dza;word;
        InfoRow<NS_sprm::SDmPaperReq>(), // sep.dmPaperReq;dm;word;
        {NS_sprm::LN_SPropRMark, { 0, L_VAR} }, // "sprmSPropRMark" sep.fPropRMark,
                            // sep.ibstPropRMark, sep.dttmPropRMark ;complex
        InfoRow<NS_sprm::SFBiDi>(), // ;;;
        {NS_sprm::LN_SFFacingCol, { 1, L_FIX} }, // "sprmSFFacingCol" ;;;
        InfoRow<NS_sprm::SFRTLGutter>(), //, set to one if gutter is on
                            // right
        InfoRow<NS_sprm::SBrcTop80>(), // sep.brcTop;BRC;long;
        InfoRow<NS_sprm::SBrcLeft80>(), // sep.brcLeft;BRC;long;
        InfoRow<NS_sprm::SBrcBottom80>(), // sep.brcBottom;BRC;long;
        InfoRow<NS_sprm::SBrcRight80>(), // sep.brcRight;BRC;long;
        InfoRow<NS_sprm::SPgbProp>(), // sep.pgbProp;;word;
        InfoRow<NS_sprm::SDxtCharSpace>(), // sep.dxtCharSpace;dxt;long;
        InfoRow<NS_sprm::SDyaLinePitch>(),
                            // sep.dyaLinePitch;dya; WRONG:long; RIGHT:short; !
        InfoRow<NS_sprm::SClm>(), // ;;;
        InfoRow<NS_sprm::STextFlow>(), // sep.wTextFlow;complex
        InfoRow<NS_sprm::TJc90>(), // tap.jc;jc;word (low order byte is
                            // significant);
        InfoRow<NS_sprm::TDxaLeft>(), // tap.rgdxaCenter
        InfoRow<NS_sprm::TDxaGapHalf>(), // tap.dxaGapHalf,
                            // tap.rgdxaCenter
        InfoRow<NS_sprm::TFCantSplit90>(), // tap.fCantSplit90;1 or 0;byte;
        InfoRow<NS_sprm::TTableHeader>(), // tap.fTableHeader;1 or 0;byte;
        InfoRow<NS_sprm::TFCantSplit>(), // tap.fCantSplit;1 or 0;byte;
        InfoRow<NS_sprm::TTableBorders80>(), // tap.rgbrcTable;complex
        {NS_sprm::LN_TDefTable10, { 0, L_VAR2} }, // "sprmTDefTable10" tap.rgdxaCenter,
                            // tap.rgtc;complex
        InfoRow<NS_sprm::TDyaRowHeight>(), // tap.dyaRowHeight;dya;word;
        {NS_sprm::LN_TDefTable, { 0, L_VAR2} }, // "sprmTDefTable" tap.rgtc;complex
        InfoRow<NS_sprm::TDefTableShd80>(), // tap.rgshd;complex
        InfoRow<NS_sprm::TTlp>(), // tap.tlp;TLP;4 bytes;
        InfoRow<NS_sprm::TFBiDi>(), // ;;;
        {NS_sprm::LN_THTMLProps, { 1, L_FIX} }, // "sprmTHTMLProps" ;;;
        InfoRow<NS_sprm::TSetBrc80>(), // tap.rgtc[].rgbrc;complex
        InfoRow<NS_sprm::TInsert>(), // tap.rgdxaCenter, tap.rgtc;complex
        InfoRow<NS_sprm::TDelete>(), // tap.rgdxaCenter, tap.rgtc;complex
        InfoRow<NS_sprm::TDxaCol>(), // tap.rgdxaCenter;complex
        InfoRow<NS_sprm::TMerge>(), // tap.fFirstMerged, tap.fMerged;
        InfoRow<NS_sprm::TSplit>(), // tap.fFirstMerged, tap.fMerged;
        {NS_sprm::LN_TSetBrc10, { 0, L_VAR} }, // "sprmTSetBrc10" tap.rgtc[].rgbrc;complex
        {NS_sprm::LN_TSetShd80, { 0, L_VAR} }, // "sprmTSetShd80" tap.rgshd;complex
        {NS_sprm::LN_TSetShdOdd80, { 0, L_VAR} }, // "sprmTSetShdOdd80" tap.rgshd;complex
        InfoRow<NS_sprm::TTextFlow>(), // tap.rgtc[].fVerticaltap,
                            // rgtc[].fBackwardtap, rgtc[].fRotateFont;0 or 10
                            // or 10 or 1;word;
        {NS_sprm::LN_TDiagLine, { 1, L_FIX} }, // "sprmTDiagLine" ;;;
        InfoRow<NS_sprm::TVertMerge>(), // tap.rgtc[].vertMerge
        InfoRow<NS_sprm::TVertAlign>(), // tap.rgtc[].vertAlign
        InfoRow<NS_sprm::CFELayout>(),
        InfoRow<NS_sprm::PItap>(), // undocumented
        InfoRow<NS_sprm::TTableWidth>(), // undocumented
        InfoRow<NS_sprm::TDefTableShd>(),
        InfoRow<NS_sprm::TTableBorders>(),
        InfoRow<NS_sprm::TBrcTopCv>(), // undocumented
        InfoRow<NS_sprm::TBrcLeftCv>(), // undocumented
        InfoRow<NS_sprm::TBrcBottomCv>(), // undocumented
        InfoRow<NS_sprm::TBrcRightCv>(), // undocumented
        InfoRow<NS_sprm::TCellPadding>(), // undocumented
        InfoRow<NS_sprm::TCellPaddingDefault>(), // undocumented
        {0xD238, { 0, L_VAR} }, // undocumented sep
        InfoRow<NS_sprm::PBrcTop>(),
        InfoRow<NS_sprm::PBrcLeft>(),
        InfoRow<NS_sprm::PBrcBottom>(),
        InfoRow<NS_sprm::PBrcRight>(),
        InfoRow<NS_sprm::PBrcBetween>(),
        InfoRow<NS_sprm::TWidthIndent>(), // undocumented
        InfoRow<NS_sprm::CRgLid0>(), // chp.rglid[0];LID: for non-FE text
        InfoRow<NS_sprm::CRgLid1>(), // chp.rglid[1];LID: for Far East text
        {0x6463, { 4, L_FIX} }, // undocumented
        InfoRow<NS_sprm::PJc>(), // undoc, must be asian version of "sprmPJc"
        InfoRow<NS_sprm::PDxaRight>(), // undoc, must be asian version of "sprmPDxaRight"
        InfoRow<NS_sprm::PDxaLeft>(), // undoc, must be asian version of "sprmPDxaLeft"
        InfoRow<NS_sprm::PDxaLeft1>(), // undoc, must be asian version of "sprmPDxaLeft1"
        InfoRow<NS_sprm::TFAutofit>(), // undocumented
        InfoRow<NS_sprm::TPc>(), // undocumented
        InfoRow<NS_sprm::SRsid>(), // undocumented, sep, perhaps related to textgrids ?
        InfoRow<NS_sprm::SFpc>(), // undocumented, sep
        InfoRow<NS_sprm::PFInnerTableCell>(), // undocumented, subtable "sprmPFInTable" equiv ?
        InfoRow<NS_sprm::PFInnerTtp>(), // undocumented, subtable "sprmPFTtp" equiv ?
        InfoRow<NS_sprm::TDxaAbs>(), // undocumented
        InfoRow<NS_sprm::TDyaAbs>(), // undocumented
        InfoRow<NS_sprm::TDxaFromText>(), // undocumented
        InfoRow<NS_sprm::CRsidProp>(), // undocumented
        InfoRow<NS_sprm::CRsidText>(), // undocumented
        InfoRow<NS_sprm::CCv>(), // text colour
        InfoRow<NS_sprm::PShd>(), // undocumented, para back colour
        InfoRow<NS_sprm::PRsid>(), // undocumented
        InfoRow<NS_sprm::PTableProps>(), // undocumented
        InfoRow<NS_sprm::TWidthBefore>(), // undocumented
        InfoRow<NS_sprm::TSetShdTable>(), // undocumented, something to do with colour.
        InfoRow<NS_sprm::TDefTableShdRaw>(), // undocumented, something to do with colour.
        InfoRow<NS_sprm::CShd>(), // text backcolour
        InfoRow<NS_sprm::SRncFtn>(), // undocumented, sep
        InfoRow<NS_sprm::PFDyaBeforeAuto>(), // undocumented, para autobefore
        InfoRow<NS_sprm::PFDyaAfterAuto>(), // undocumented, para autoafter
        // "sprmPFContextualSpacing", don't add space between para of the same style
        InfoRow<NS_sprm::PFContextualSpacing>(),
    };

    static wwSprmSearcher aSprmSrch(aSprms, SAL_N_ELEMENTS(aSprms));
    return &aSprmSrch;
};

wwSprmParser::wwSprmParser(const WW8Fib& rFib) : meVersion(rFib.GetFIBVersion())
{
    OSL_ENSURE((meVersion >= ww::eWW1 && meVersion <= ww::eWW8),
        "Impossible value for version");

    mnDelta = (ww::IsSevenMinus(meVersion)) ? 0 : 1;

    if (meVersion <= ww::eWW2)
        mpKnownSprms = GetWW2SprmSearcher();
    else if (meVersion < ww::eWW8)
        mpKnownSprms = GetWW6SprmSearcher(rFib);
    else
        mpKnownSprms = GetWW8SprmSearcher();
}

SprmInfo wwSprmParser::GetSprmInfo(sal_uInt16 nId) const
{
    const SprmInfo* pFound = mpKnownSprms->search(nId);
    if (pFound != nullptr)
    {
        return *pFound;
    }

    OSL_ENSURE(ww::IsEightPlus(meVersion),
               "Unknown ww7- sprm, dangerous, report to development");

    //All the unknown ww7 sprms appear to be variable (which makes sense)
    SprmInfo aSrch = { 0, L_VAR };
    if (ww::IsEightPlus(meVersion)) //We can recover perfectly in this case
    {
        aSrch.nVari = L_FIX;
        switch (nId >> 13)
        {
        case 0:
        case 1:
            aSrch.nLen = 1;
            break;
        case 2:
            aSrch.nLen = 2;
            break;
        case 3:
            aSrch.nLen = 4;
            break;
        case 4:
        case 5:
            aSrch.nLen = 2;
            break;
        case 6:
            aSrch.nLen = 0;
            aSrch.nVari =  L_VAR;
            break;
        case 7:
        default:
            aSrch.nLen = 3;
            break;
        }
    }
    return aSrch;
}

//-end

static sal_uInt8 Get_Byte( sal_uInt8 *& p )
{
    sal_uInt8 n = *p;
    p += 1;
    return n;
}

static sal_uInt16 Get_UShort( sal_uInt8 *& p )
{
    const sal_uInt16 n = SVBT16ToUInt16( *reinterpret_cast<SVBT16*>(p) );
    p += 2;
    return n;
}

static sal_Int16 Get_Short( sal_uInt8 *& p )
{
    return Get_UShort(p);
}

static sal_uInt32 Get_ULong( sal_uInt8 *& p )
{
    sal_uInt32 n = SVBT32ToUInt32( *reinterpret_cast<SVBT32*>(p) );
    p += 4;
    return n;
}

static sal_Int32 Get_Long( sal_uInt8 *& p )
{
    return Get_ULong(p);
}

WW8SprmIter::WW8SprmIter(const sal_uInt8* pSprms_, sal_Int32 nLen_,
    const wwSprmParser &rParser)
    :  mrSprmParser(rParser), m_pSprms( pSprms_), m_nRemLen( nLen_)
{
    UpdateMyMembers();
}

void WW8SprmIter::SetSprms(const sal_uInt8* pSprms_, sal_Int32 nLen_)
{
    m_pSprms = pSprms_;
    m_nRemLen = nLen_;
    UpdateMyMembers();
}

void WW8SprmIter::advance()
{
    if (m_nRemLen > 0 )
    {
        sal_uInt16 nSize = m_nCurrentSize;
        if (nSize > m_nRemLen)
            nSize = m_nRemLen;
        m_pSprms += nSize;
        m_nRemLen -= nSize;
        UpdateMyMembers();
    }
}

void WW8SprmIter::UpdateMyMembers()
{
    bool bValid = (m_pSprms && m_nRemLen >= mrSprmParser.MinSprmLen());

    if (bValid)
    {
        m_nCurrentId = mrSprmParser.GetSprmId(m_pSprms);
        m_nCurrentSize = mrSprmParser.GetSprmSize(m_nCurrentId, m_pSprms, m_nRemLen);
        m_pCurrentParams = m_pSprms + mrSprmParser.DistanceToData(m_nCurrentId);
        bValid = m_nCurrentSize <= m_nRemLen;
        SAL_WARN_IF(!bValid, "sw.ww8", "sprm longer than remaining bytes, doc or parser is wrong");
    }

    if (!bValid)
    {
        m_nCurrentId = 0;
        m_pCurrentParams = nullptr;
        m_nCurrentSize = 0;
        m_nRemLen = 0;
    }
}

SprmResult WW8SprmIter::FindSprm(sal_uInt16 nId, bool bFindFirst, const sal_uInt8* pNextByteMatch)
{
    SprmResult aRet;

    while (GetSprms())
    {
        if (GetCurrentId() == nId)
        {
            sal_Int32 nFixedLen =  mrSprmParser.DistanceToData(nId);
            sal_Int32 nL = mrSprmParser.GetSprmSize(nId, GetSprms(), GetRemLen());
            SprmResult aSprmResult(GetCurrentParams(), nL - nFixedLen);
            // typically pNextByteMatch is nullptr and we just return the first match
            // very occasionally we want one with a specific following byte
            if ( !pNextByteMatch || (aSprmResult.nRemainingData >= 1 && *aSprmResult.pSprm == *pNextByteMatch) )
            {
                if ( bFindFirst )
                    return aSprmResult;
                aRet = aSprmResult;
            }
        }
        advance();
    }

    return aRet;
}

// temporary test
// WW8PLCFx_PCDAttrs cling to WW8PLCF_Pcd and therefore do not have their own iterators.
// All methods relating to iterators are therefore dummies.
WW8PLCFx_PCDAttrs::WW8PLCFx_PCDAttrs(const WW8Fib& rFib,
    WW8PLCFx_PCD* pPLCFx_PCD, const WW8ScannerBase* pBase)
    : WW8PLCFx(rFib, true), m_pPcdI(pPLCFx_PCD->GetPLCFIter()),
    m_pPcd(pPLCFx_PCD), mrGrpprls(pBase->m_aPieceGrpprls)
{
}

sal_uInt32 WW8PLCFx_PCDAttrs::GetIdx() const
{
    return 0;
}

void WW8PLCFx_PCDAttrs::SetIdx(sal_uInt32)
{
}

bool WW8PLCFx_PCDAttrs::SeekPos(WW8_CP )
{
    return true;
}

void WW8PLCFx_PCDAttrs::advance()
{
}

WW8_CP WW8PLCFx_PCDAttrs::Where()
{
    return m_pPcd ? m_pPcd->Where() : WW8_CP_MAX;
}

void WW8PLCFx_PCDAttrs::GetSprms(WW8PLCFxDesc* p)
{
    void* pData;

    p->bRealLineEnd = false;
    if ( !m_pPcdI || !m_pPcdI->Get(p->nStartPos, p->nEndPos, pData) )
    {
        // PLCF fully processed
        p->nStartPos = p->nEndPos = WW8_CP_MAX;
        p->pMemPos = nullptr;
        p->nSprmsLen = 0;
        return;
    }

    const sal_uInt16 nPrm = SVBT16ToUInt16( static_cast<WW8_PCD*>(pData)->prm );
    if ( nPrm & 1 )
    {
        // PRM Variant 2
        const sal_uInt16 nSprmIdx = nPrm >> 1;

        if( nSprmIdx >= mrGrpprls.size() )
        {
            // Invalid Index
            p->nStartPos = p->nEndPos = WW8_CP_MAX;
            p->pMemPos = nullptr;
            p->nSprmsLen = 0;
            return;
        }
        const sal_uInt8* pSprms = mrGrpprls[ nSprmIdx ].get();

        p->nSprmsLen = SVBT16ToUInt16( pSprms ); // Length
        pSprms += 2;
        p->pMemPos = pSprms;                    // Position
    }
    else
    {
        // SPRM is stored directly into members var
        /*
            These are the attr that are in the piece-table instead of in the text!
        */

        if (IsSevenMinus(GetFIBVersion()))
        {
            m_aShortSprm[0] = static_cast<sal_uInt8>( ( nPrm & 0xfe) >> 1 );
            m_aShortSprm[1] = static_cast<sal_uInt8>(   nPrm         >> 8 );
            p->nSprmsLen = nPrm ? 2 : 0;        // length

            // store Position of internal mini storage in Data Pointer
            p->pMemPos = m_aShortSprm;
        }
        else
        {
            p->pMemPos = nullptr;
            p->nSprmsLen = 0;
            sal_uInt8 nSprmListIdx = static_cast<sal_uInt8>((nPrm & 0xfe) >> 1);
            if( nSprmListIdx )
            {
                // process Sprm Id Matching as explained in MS Documentation

                // ''Property Modifier(variant 1) (PRM)''
                // see file: s62f39.htm

                // Since Sprm is 7 bits, rgsprmPrm can hold 0x80 entries.
                static const sal_uInt16 aSprmId[0x80] =
                {
                    // sprmNoop, sprmNoop, sprmNoop, sprmNoop
                    0x0000,0x0000,0x0000,0x0000,
                    // sprmPIncLvl, sprmPJc, sprmPFSideBySide, sprmPFKeep
                    0x2402,0x2403,NS_sprm::LN_PFSideBySide,0x2405,
                    // sprmPFKeepFollow, sprmPFPageBreakBefore, sprmPBrcl,
                    // sprmPBrcp
                    0x2406,0x2407,NS_sprm::LN_PBrcl,NS_sprm::LN_PBrcp,
                    // sprmPIlvl, sprmNoop, sprmPFNoLineNumb, sprmNoop
                    0x260A,0x0000,0x240C,0x0000,
                    // sprmNoop, sprmNoop, sprmNoop, sprmNoop
                    0x0000,0x0000,0x0000,0x0000,
                    // sprmNoop, sprmNoop, sprmNoop, sprmNoop
                    0x0000,0x0000,0x0000,0x0000,
                    // sprmPFInTable, sprmPFTtp, sprmNoop, sprmNoop
                    0x2416,0x2417,0x0000,0x0000,
                    // sprmNoop, sprmPPc,  sprmNoop, sprmNoop
                    0x0000,0x261B,0x0000,0x0000,
                    // sprmNoop, sprmNoop, sprmNoop, sprmNoop
                    0x0000,0x0000,0x0000,0x0000,
                    // sprmNoop, sprmPWr,  sprmNoop, sprmNoop
                    0x0000,0x2423,0x0000,0x0000,
                    // sprmNoop, sprmNoop, sprmNoop, sprmNoop
                    0x0000,0x0000,0x0000,0x0000,
                    // sprmPFNoAutoHyph, sprmNoop, sprmNoop, sprmNoop
                    0x242A,0x0000,0x0000,0x0000,
                    // sprmNoop, sprmNoop, sprmPFLocked, sprmPFWidowControl
                    0x0000,0x0000,0x2430,0x2431,
                    // sprmNoop, sprmPFKinsoku, sprmPFWordWrap,
                    // sprmPFOverflowPunct
                    0x0000,0x2433,0x2434,0x2435,
                    // sprmPFTopLinePunct, sprmPFAutoSpaceDE,
                    // sprmPFAutoSpaceDN, sprmNoop
                    0x2436,0x2437,0x2438,0x0000,
                    // sprmNoop, sprmPISnapBaseLine, sprmNoop, sprmNoop
                    0x0000,NS_sprm::LN_PISnapBaseLine,0x000,0x0000,
                    // sprmNoop, sprmCFStrikeRM, sprmCFRMark, sprmCFFieldVanish
                    0x0000,0x0800,0x0801,0x0802,
                    // sprmNoop, sprmNoop, sprmNoop, sprmCFData
                    0x0000,0x0000,0x0000,0x0806,
                    // sprmNoop, sprmNoop, sprmNoop, sprmCFOle2
                    0x0000,0x0000,0x0000,0x080A,
                    // sprmNoop, sprmCHighlight, sprmCFEmboss, sprmCSfxText
                    0x0000,0x2A0C,0x0858,0x2859,
                    // sprmNoop, sprmNoop, sprmNoop, sprmCPlain
                    0x0000,0x0000,0x0000,0x2A33,
                    // sprmNoop, sprmCFBold, sprmCFItalic, sprmCFStrike
                    0x0000,0x0835,0x0836,0x0837,
                    // sprmCFOutline, sprmCFShadow, sprmCFSmallCaps, sprmCFCaps,
                    0x0838,0x0839,0x083a,0x083b,
                    // sprmCFVanish, sprmNoop, sprmCKul, sprmNoop,
                    0x083C,0x0000,0x2A3E,0x0000,
                    // sprmNoop, sprmNoop, sprmCIco, sprmNoop,
                    0x0000,0x0000,0x2A42,0x0000,
                    // sprmCHpsInc, sprmNoop, sprmCHpsPosAdj, sprmNoop,
                    NS_sprm::LN_CHpsInc,0x0000,NS_sprm::LN_CHpsPosAdj,0x0000,
                    // sprmCIss, sprmNoop, sprmNoop, sprmNoop,
                    0x2A48,0x0000,0x0000,0x0000,
                    // sprmNoop, sprmNoop, sprmNoop, sprmNoop,
                    0x0000,0x0000,0x0000,0x0000,
                    // sprmNoop, sprmNoop, sprmNoop, sprmCFDStrike,
                    0x0000,0x0000,0x0000,0x2A53,
                    // sprmCFImprint, sprmCFSpec, sprmCFObj, sprmPicBrcl,
                    0x0854,0x0855,0x0856,NS_sprm::LN_PicBrcl,
                    // sprmPOutLvl, sprmPFBiDi, sprmNoop, sprmNoop,
                    0x2640,0x2441,0x0000,0x0000,
                    // sprmNoop, sprmNoop, sprmPPnbrRMarkNot
                    0x0000,0x0000,0x0000,0x0000
                };

                // find real Sprm Id:
                const sal_uInt16 nSprmId = aSprmId[ nSprmListIdx ];

                if( nSprmId )
                {
                    // move Sprm Id and Sprm Param to internal mini storage:
                    m_aShortSprm[0] = static_cast<sal_uInt8>( nSprmId & 0x00ff)       ;
                    m_aShortSprm[1] = static_cast<sal_uInt8>( ( nSprmId & 0xff00) >> 8 );
                    m_aShortSprm[2] = static_cast<sal_uInt8>( nPrm >> 8 );

                    // store Sprm Length in member:
                    p->nSprmsLen = nPrm ? 3 : 0;

                    // store Position of internal mini storage in Data Pointer
                    p->pMemPos = m_aShortSprm;
                }
            }
        }
    }
}

WW8PLCFx_PCD::WW8PLCFx_PCD(const WW8Fib& rFib, WW8PLCFpcd* pPLCFpcd,
    WW8_CP nStartCp, bool bVer67P)
    : WW8PLCFx(rFib, false), m_nClipStart(-1)
{
    // construct own iterator
    m_pPcdI.reset( new WW8PLCFpcd_Iter(*pPLCFpcd, nStartCp) );
    m_bVer67= bVer67P;
}

WW8PLCFx_PCD::~WW8PLCFx_PCD()
{
}

sal_uInt32 WW8PLCFx_PCD::GetIMax() const
{
    return m_pPcdI ? m_pPcdI->GetIMax() : 0;
}

sal_uInt32 WW8PLCFx_PCD::GetIdx() const
{
    return m_pPcdI ? m_pPcdI->GetIdx() : 0;
}

void WW8PLCFx_PCD::SetIdx(sal_uInt32 nIdx)
{
    if (m_pPcdI)
        m_pPcdI->SetIdx( nIdx );
}

bool WW8PLCFx_PCD::SeekPos(WW8_CP nCpPos)
{
    return m_pPcdI && m_pPcdI->SeekPos( nCpPos );
}

WW8_CP WW8PLCFx_PCD::Where()
{
    return m_pPcdI ? m_pPcdI->Where() : WW8_CP_MAX;
}

tools::Long WW8PLCFx_PCD::GetNoSprms( WW8_CP& rStart, WW8_CP& rEnd, sal_Int32& rLen )
{
    void* pData;
    rLen = 0;

    if ( !m_pPcdI || !m_pPcdI->Get(rStart, rEnd, pData) )
    {
        rStart = rEnd = WW8_CP_MAX;
        return -1;
    }
    return m_pPcdI->GetIdx();
}

void WW8PLCFx_PCD::advance()
{
    OSL_ENSURE(m_pPcdI , "missing pPcdI");
    if (m_pPcdI)
        m_pPcdI->advance();
}

WW8_FC WW8PLCFx_PCD::CurrentPieceStartCp2Fc( WW8_CP nCp )
{
    WW8_CP nCpStart, nCpEnd;
    void* pData;

    if ( !m_pPcdI->Get(nCpStart, nCpEnd, pData) )
    {
        OSL_ENSURE( false, "CurrentPieceStartCp2Fc() with false Cp found (1)" );
        return WW8_FC_MAX;
    }

    OSL_ENSURE( nCp >= nCpStart && nCp < nCpEnd,
        "AktPieceCp2Fc() with false Cp found (2)" );

    if( nCp < nCpStart )
        nCp = nCpStart;
    if( nCp >= nCpEnd )
        nCp = nCpEnd - 1;

    bool bIsUnicode = false;
    WW8_FC nFC = SVBT32ToUInt32( static_cast<WW8_PCD*>(pData)->fc );
    if( !m_bVer67 )
        nFC = WW8PLCFx_PCD::TransformPieceAddress( nFC, bIsUnicode );

    WW8_CP nDistance;
    bool bFail = o3tl::checked_sub(nCp, nCpStart, nDistance);
    if (bFail)
    {
        SAL_WARN("sw.ww8", "broken offset, ignoring");
        return WW8_FC_MAX;
    }

    if (bIsUnicode)
    {
        bFail = o3tl::checked_multiply<WW8_CP>(nDistance, 2, nDistance);
        if (bFail)
        {
            SAL_WARN("sw.ww8", "broken offset, ignoring");
            return WW8_FC_MAX;
        }
    }

    WW8_FC nRet;
    bFail = o3tl::checked_add(nFC, nDistance, nRet);
    if (bFail)
    {
        SAL_WARN("sw.ww8", "broken offset, ignoring");
        return WW8_FC_MAX;
    }

    return nRet;
}

void WW8PLCFx_PCD::CurrentPieceFc2Cp( WW8_CP& rStartPos, WW8_CP& rEndPos,
    const WW8ScannerBase *pSBase )
{
    //No point going anywhere with this
    if ((rStartPos == WW8_CP_MAX) && (rEndPos == WW8_CP_MAX))
        return;

    rStartPos = pSBase->WW8Fc2Cp( rStartPos );
    rEndPos = pSBase->WW8Fc2Cp( rEndPos );
}

WW8_CP WW8PLCFx_PCD::CurrentPieceStartFc2Cp( WW8_FC nStartPos )
{
    WW8_CP nCpStart, nCpEnd;
    void* pData;
    if ( !m_pPcdI->Get( nCpStart, nCpEnd, pData ) )
    {
        OSL_ENSURE( false, "CurrentPieceStartFc2Cp() - error" );
        return WW8_CP_MAX;
    }
    bool bIsUnicode = false;
    sal_Int32 nFcStart  = SVBT32ToUInt32( static_cast<WW8_PCD*>(pData)->fc );
    if( !m_bVer67 )
        nFcStart = WW8PLCFx_PCD::TransformPieceAddress( nFcStart, bIsUnicode );

    sal_Int32 nUnicodeFactor = bIsUnicode ? 2 : 1;

    if( nStartPos < nFcStart )
        nStartPos = nFcStart;

    WW8_CP nCpLen;
    bool bFail = o3tl::checked_sub(nCpEnd, nCpStart, nCpLen);
    if (bFail)
    {
        SAL_WARN("sw.ww8", "broken offset, ignoring");
        return WW8_CP_MAX;
    }

    WW8_CP nCpLenBytes;
    bFail = o3tl::checked_multiply(nCpLen, nUnicodeFactor, nCpLenBytes);
    if (bFail)
    {
        SAL_WARN("sw.ww8", "broken offset, ignoring");
        return WW8_CP_MAX;
    }

    WW8_FC nFcLen;
    bFail = o3tl::checked_add(nFcStart, nCpLenBytes, nFcLen);
    if (bFail)
    {
        SAL_WARN("sw.ww8", "broken offset, ignoring");
        return WW8_CP_MAX;
    }

    WW8_FC nFcEnd;
    bFail = o3tl::checked_add(nFcStart, nFcLen, nFcEnd);
    if (bFail)
    {
        SAL_WARN("sw.ww8", "broken offset, ignoring");
        return WW8_CP_MAX;
    }


    if (nStartPos >= nFcEnd)
        nStartPos = nFcEnd - (1 * nUnicodeFactor);

    WW8_FC nFcDiff = (nStartPos - nFcStart) / nUnicodeFactor;

    WW8_FC nCpRet;
    bFail = o3tl::checked_add(nCpStart, nFcDiff, nCpRet);
    if (bFail)
    {
        SAL_WARN("sw.ww8", "broken offset, ignoring");
        return WW8_CP_MAX;
    }

    return nCpRet;
}

//      Helper routines for all

// Convert BRC from WW6 to WW8 format
WW8_BRC::WW8_BRC(const WW8_BRCVer6& brcVer6)
{
    sal_uInt8 _dptLineWidth = brcVer6.dxpLineWidth(),
              _brcType = brcVer6.brcType();

    if (_dptLineWidth > 5) // this signifies dashed(6) or dotted(7) line
    {
        _brcType = _dptLineWidth;
        _dptLineWidth = 1;
    }
    _dptLineWidth *= 6; // convert units from 0.75pt to 1/8pt

    *this = WW8_BRC(_dptLineWidth, _brcType, brcVer6.ico(), brcVer6.dxpSpace(),
        brcVer6.fShadow(), false);
}

// Convert BRC from WW8 to WW9 format
WW8_BRCVer9::WW8_BRCVer9(const WW8_BRC& brcVer8)
{
    if (brcVer8.isNil()) {
        UInt32ToSVBT32(0, aBits1);
        UInt32ToSVBT32(0xffffffff, aBits2);
    }
    else
    {
        sal_uInt32 _cv = brcVer8.ico() == 0 ? 0xff000000 // "auto" colour
            : wwUtility::RGBToBGR(SwWW8ImplReader::GetCol(brcVer8.ico()));
        *this = WW8_BRCVer9(_cv, brcVer8.dptLineWidth(), brcVer8.brcType(),
            brcVer8.dptSpace(), brcVer8.fShadow(), brcVer8.fFrame());
    }
}

short WW8_BRC::DetermineBorderProperties(short *pSpace) const
{
    WW8_BRCVer9 brcVer9(*this);
    return brcVer9.DetermineBorderProperties(pSpace);
}

short WW8_BRCVer9::DetermineBorderProperties(short *pSpace) const
{
    /*
        Word does not factor the width of the border into the width/height
        stored in the information for graphic/table/object widths, so we need
        to figure out this extra width here and utilize the returned size in
        our calculations
    */
    short nMSTotalWidth;

    //Specification in 8ths of a point, 1 Point = 20 Twips, so by 2.5
    nMSTotalWidth  = static_cast<short>(dptLineWidth()) * 20 / 8;

    //Figure out the real size of the border according to word
    switch (brcType())
    {
        //Note that codes over 25 are undocumented, and I can't create
        //these 4 here in the wild.
        case 2:
        case 4:
        case 5:
        case 22:
            OSL_FAIL("Can't create these from the menus, please report");
            break;
        default:
        case 23:    //Only 3pt in the menus, but honours the size setting.
            break;
        case 10:
            /*
            triple line is five times the width of an ordinary line,
            except that the smallest 1/4 point size appears to have
            exactly the same total border width as a 3/4 point size
            ordinary line, i.e. three times the nominal line width.  The
            second smallest 1/2 point size appears to have exactly the
            total border width as a 2 1/4 border, i.e 4.5 times the size.
            */
            if (nMSTotalWidth == 5)
                nMSTotalWidth*=3;
            else if (nMSTotalWidth == 10)
                nMSTotalWidth = nMSTotalWidth*9/2;
            else
                nMSTotalWidth*=5;
            break;
        case 20:
            /*
            wave, the dimensions appear to be created by the drawing of
            the wave, so we have only two possibilities in the menus, 3/4
            point is equal to solid 3 point. This calculation seems to
            match well to results.
            */
            nMSTotalWidth +=45;
            break;
        case 21:
            /*
            double wave, the dimensions appear to be created by the
            drawing of the wave, so we have only one possibilities in the
            menus, that of 3/4 point is equal to solid 3 point. This
            calculation seems to match well to results.
            */
            nMSTotalWidth += 45*2;
            break;
    }

    if (pSpace)
        *pSpace = static_cast<short>(dptSpace()) * 20; // convert from points to twips
    return nMSTotalWidth;
}

/*
 * WW8Cp2Fc is a good method, a CP always maps to a FC
 * WW8Fc2Cp on the other hand is more dubious, a random FC
 * may not map to a valid CP. Try and avoid WW8Fc2Cp where
 * possible
 */
WW8_CP WW8ScannerBase::WW8Fc2Cp( WW8_FC nFcPos ) const
{
    WW8_CP nFallBackCpEnd = WW8_CP_MAX;
    if( nFcPos == WW8_FC_MAX )
        return nFallBackCpEnd;

    bool bIsUnicode;
    if (m_pWw8Fib->m_nVersion >= 8)
        bIsUnicode = false;
    else
        bIsUnicode = m_pWw8Fib->m_fExtChar;

    if( m_pPieceIter )    // Complex File ?
    {
        sal_uInt32 nOldPos = m_pPieceIter->GetIdx();

        for (m_pPieceIter->SetIdx(0);
            m_pPieceIter->GetIdx() < m_pPieceIter->GetIMax(); m_pPieceIter->advance())
        {
            WW8_CP nCpStart, nCpEnd;
            void* pData;
            if( !m_pPieceIter->Get( nCpStart, nCpEnd, pData ) )
            {   // outside PLCFfpcd ?
                OSL_ENSURE( false, "PLCFpcd-WW8Fc2Cp() went wrong" );
                break;
            }
            sal_Int32 nFcStart  = SVBT32ToUInt32( static_cast<WW8_PCD*>(pData)->fc );
            if (m_pWw8Fib->m_nVersion >= 8)
            {
                nFcStart = WW8PLCFx_PCD::TransformPieceAddress( nFcStart,
                                                                bIsUnicode );
            }
            else
            {
                bIsUnicode = m_pWw8Fib->m_fExtChar;
            }

            sal_Int32 nLen;
            if (o3tl::checked_sub(nCpEnd, nCpStart, nLen))
            {
                SAL_WARN("sw.ww8", "broken offset, ignoring");
                return WW8_CP_MAX;
            }
            if (bIsUnicode)
            {
                if (o3tl::checked_multiply<WW8_CP>(nLen, 2, nLen))
                {
                    SAL_WARN("sw.ww8", "broken offset, ignoring");
                    return WW8_CP_MAX;
                }
            }

            /*
            If this cp is inside this piece, or it's the last piece and we are
            on the very last cp of that piece
            */
            if (nFcPos >= nFcStart)
            {
                // found
                WW8_FC nFcDiff;
                if (o3tl::checked_sub(nFcPos, nFcStart, nFcDiff))
                {
                    SAL_WARN("sw.ww8", "broken offset, ignoring");
                    return WW8_CP_MAX;
                }
                if (bIsUnicode)
                    nFcDiff /= 2;
                WW8_CP nTempCp;
                if (o3tl::checked_add(nCpStart, nFcDiff, nTempCp))
                {
                    SAL_WARN("sw.ww8", "broken offset, ignoring");
                    return WW8_CP_MAX;
                }
                WW8_FC nFcEnd;
                if (o3tl::checked_add(nFcStart, nLen, nFcEnd))
                {
                    SAL_WARN("sw.ww8", "broken offset, ignoring");
                    return WW8_CP_MAX;
                }
                if (nFcPos < nFcEnd)
                {
                    m_pPieceIter->SetIdx( nOldPos );
                    return nTempCp;
                }
                else if (nFcPos == nFcEnd)
                {
                    //Keep this cp as its on a piece boundary because we might
                    //need it if tests fail
                    nFallBackCpEnd = nTempCp;
                }
            }
        }
        // not found
        m_pPieceIter->SetIdx( nOldPos );      // not found
        /*
        If it was not found, then this is because it has fallen between two
        stools, i.e. either it is the last cp/fc of the last piece, or it is
        the last cp/fc of a disjoint piece.
        */
        return nFallBackCpEnd;
    }

    WW8_FC nFcDiff;
    if (o3tl::checked_sub(nFcPos, m_pWw8Fib->m_fcMin, nFcDiff))
    {
        SAL_WARN("sw.ww8", "broken offset, ignoring");
        return WW8_CP_MAX;
    }

    // No complex file
    if (!bIsUnicode)
        nFallBackCpEnd = nFcDiff;
    else
        nFallBackCpEnd = (nFcDiff + 1) / 2;

    return nFallBackCpEnd;
}

// the fib of WinWord2 has a last entry of cpnBtePap of 2 byte sized type PN at
// offset 324
const int nSmallestPossibleFib = 326;

WW8_FC WW8ScannerBase::WW8Cp2Fc(WW8_CP nCpPos, bool* pIsUnicode,
    WW8_CP* pNextPieceCp, bool* pTestFlag) const
{
    if( pTestFlag )
        *pTestFlag = true;
    if( WW8_CP_MAX == nCpPos )
        return WW8_CP_MAX;

    bool bIsUnicode;
    if( !pIsUnicode )
        pIsUnicode = &bIsUnicode;

    if (m_pWw8Fib->m_nVersion >= 8)
        *pIsUnicode = false;
    else
        *pIsUnicode = m_pWw8Fib->m_fExtChar;

    WW8_FC nRet;

    if( m_pPieceIter )
    {
        // Complex File
        if( pNextPieceCp )
            *pNextPieceCp = WW8_CP_MAX;

        if( !m_pPieceIter->SeekPos( nCpPos ) )
        {
            if( pTestFlag )
                *pTestFlag = false;
            else {
                OSL_ENSURE( false, "Handed over wrong CP to WW8Cp2Fc()" );
            }
            return WW8_FC_MAX;
        }
        WW8_CP nCpStart, nCpEnd;
        void* pData;
        if( !m_pPieceIter->Get( nCpStart, nCpEnd, pData ) )
        {
            if( pTestFlag )
                *pTestFlag = false;
            else {
                OSL_ENSURE( false, "PLCFfpcd-Get went wrong" );
            }
            return WW8_FC_MAX;
        }
        if( pNextPieceCp )
            *pNextPieceCp = nCpEnd;

        nRet = SVBT32ToUInt32( static_cast<WW8_PCD*>(pData)->fc );
        if (m_pWw8Fib->m_nVersion >= 8)
            nRet = WW8PLCFx_PCD::TransformPieceAddress( nRet, *pIsUnicode );
        else
            *pIsUnicode = m_pWw8Fib->m_fExtChar;

        WW8_CP nCpLen;
        bool bFail = o3tl::checked_sub(nCpPos, nCpStart, nCpLen);
        if (bFail)
        {
            SAL_WARN("sw.ww8", "broken offset, ignoring");
            return WW8_CP_MAX;
        }

        if (*pIsUnicode)
        {
            bFail = o3tl::checked_multiply<WW8_CP>(nCpLen, 2, nCpLen);
            if (bFail)
            {
                SAL_WARN("sw.ww8", "broken offset, ignoring");
                return WW8_CP_MAX;
            }
        }

        bFail = o3tl::checked_add(nRet, nCpLen, nRet);
        if (bFail)
        {
            SAL_WARN("sw.ww8", "broken offset, ignoring");
            return WW8_CP_MAX;
        }

        return nRet;
    }

    if (*pIsUnicode)
    {
        const bool bFail = o3tl::checked_multiply<WW8_CP>(nCpPos, 2, nCpPos);
        if (bFail)
        {
            SAL_WARN("sw.ww8", "broken offset, ignoring");
            return WW8_CP_MAX;
        }
    }

    // No complex file
    const bool bFail = o3tl::checked_add(m_pWw8Fib->m_fcMin, nCpPos, nRet);
    if (bFail)
    {
        SAL_WARN("sw.ww8", "broken offset, ignoring");
        return WW8_CP_MAX;
    }

    // the text and the fib share the same stream, if the text is inside the fib
    // then it's definitely a bad offset. The smallest FIB supported is that of
    // WW2 which is 326 bytes in size
    if (nRet < nSmallestPossibleFib)
    {
        SAL_WARN("sw.ww8", "broken offset, ignoring");
        return WW8_CP_MAX;
    }

    return nRet;
}

std::unique_ptr<WW8PLCFpcd> WW8ScannerBase::OpenPieceTable( SvStream* pStr, const WW8Fib* pWwF )
{
    if ( ((8 > m_pWw8Fib->m_nVersion) && !pWwF->m_fComplex) || !pWwF->m_lcbClx )
        return nullptr;

    if (pWwF->m_lcbClx < 0)
        return nullptr;

    WW8_FC nClxPos = pWwF->m_fcClx;

    if (!checkSeek(*pStr, nClxPos))
        return nullptr;

    sal_Int32 nClxLen = pWwF->m_lcbClx;
    sal_Int32 nLeft = nClxLen;

    while (true)
    {
        sal_uInt8 clxt(2);
        pStr->ReadUChar( clxt );
        nLeft--;
        if( 2 == clxt)                          // PLCFfpcd ?
            break;                              // PLCFfpcd found
        sal_uInt16 nLen(0);
        pStr->ReadUInt16( nLen );
        nLeft -= 2 + nLen;
        if( nLeft < 0 )
            return nullptr;                        // gone wrong
        if( 1 == clxt )                         // clxtGrpprl ?
        {
            if (m_aPieceGrpprls.size() == SHRT_MAX)
                return nullptr;
            if (nLen > pStr->remainingSize())
                return nullptr;
            std::unique_ptr<sal_uInt8[]> p(new sal_uInt8[nLen+2]);         // allocate
            ShortToSVBT16(nLen, p.get());             // add length
            if (!checkRead(*pStr, p.get()+2, nLen))   // read grpprl
            {
                return nullptr;
            }
            m_aPieceGrpprls.push_back(std::move(p));    // add to array
        }
        else
        {
            nLen = std::min<sal_uInt64>(nLen, pStr->remainingSize());
            pStr->Seek(pStr->Tell() + nLen);         // non-Grpprl left
        }
    }

    // read Piece Table PLCF
    sal_Int32 nPLCFfLen(0);
    if (pWwF->GetFIBVersion() <= ww::eWW2)
    {
        sal_Int16 nWordTwoLen(0);
        pStr->ReadInt16( nWordTwoLen );
        nPLCFfLen = nWordTwoLen;
    }
    else
        pStr->ReadInt32( nPLCFfLen );
    OSL_ENSURE( 65536 > nPLCFfLen, "PLCFfpcd above 64 k" );
    return std::make_unique<WW8PLCFpcd>( pStr, pStr->Tell(), nPLCFfLen, 8 );
}

WW8ScannerBase::WW8ScannerBase( SvStream* pSt, SvStream* pTableSt,
    SvStream* pDataSt, WW8Fib* pWwFib )
    : m_pWw8Fib(pWwFib)
{
    m_pPiecePLCF = OpenPieceTable( pTableSt, m_pWw8Fib );             // Complex
    if( m_pPiecePLCF )
    {
        m_pPieceIter.reset(new WW8PLCFpcd_Iter( *m_pPiecePLCF ));
        m_pPLCFx_PCD.reset( new WW8PLCFx_PCD(*pWwFib, m_pPiecePLCF.get(), 0,
            IsSevenMinus(m_pWw8Fib->GetFIBVersion())));
        m_pPLCFx_PCDAttrs.reset(new WW8PLCFx_PCDAttrs(*pWwFib,
            m_pPLCFx_PCD.get(), this));
    }
    else
    {
        m_pPieceIter = nullptr;
        m_pPLCFx_PCD = nullptr;
        m_pPLCFx_PCDAttrs = nullptr;
    }

    // pChpPLCF and pPapPLCF may NOT be created before pPLCFx_PCD !!
    m_pChpPLCF.reset(new WW8PLCFx_Cp_FKP( pSt, pTableSt, pDataSt, *this, CHP )); // CHPX
    m_pPapPLCF.reset(new WW8PLCFx_Cp_FKP( pSt, pTableSt, pDataSt, *this, PAP )); // PAPX

    m_pSepPLCF.reset(new WW8PLCFx_SEPX(   pSt, pTableSt, *pWwFib, 0 ));          // SEPX

    // Footnotes
    m_pFootnotePLCF.reset(new WW8PLCFx_SubDoc( pTableSt, *pWwFib, 0,
        pWwFib->m_fcPlcffndRef, pWwFib->m_lcbPlcffndRef, pWwFib->m_fcPlcffndText,
        pWwFib->m_lcbPlcffndText, 2 ));
    // Endnotes
    m_pEdnPLCF.reset(new WW8PLCFx_SubDoc( pTableSt, *pWwFib, 0,
        pWwFib->m_fcPlcfendRef, pWwFib->m_lcbPlcfendRef, pWwFib->m_fcPlcfendText,
        pWwFib->m_lcbPlcfendText, 2 ));
    // Comments
    m_pAndPLCF.reset(new WW8PLCFx_SubDoc( pTableSt, *pWwFib, 0,
        pWwFib->m_fcPlcfandRef, pWwFib->m_lcbPlcfandRef, pWwFib->m_fcPlcfandText,
        pWwFib->m_lcbPlcfandText, IsSevenMinus(pWwFib->GetFIBVersion()) ? 20 : 30));

    // Fields Main Text
    m_pFieldPLCF.reset(new WW8PLCFx_FLD(pTableSt, *pWwFib, MAN_MAINTEXT));
    // Fields Header / Footer
    m_pFieldHdFtPLCF.reset(new WW8PLCFx_FLD(pTableSt, *pWwFib, MAN_HDFT));
    // Fields Footnote
    m_pFieldFootnotePLCF.reset(new WW8PLCFx_FLD(pTableSt, *pWwFib, MAN_FTN));
    // Fields Endnote
    m_pFieldEdnPLCF.reset(new WW8PLCFx_FLD(pTableSt, *pWwFib, MAN_EDN));
    // Fields Comments
    m_pFieldAndPLCF.reset(new WW8PLCFx_FLD(pTableSt, *pWwFib, MAN_AND));
    // Fields in Textboxes in Main Text
    m_pFieldTxbxPLCF.reset(new WW8PLCFx_FLD(pTableSt, *pWwFib, MAN_TXBX));
    // Fields in Textboxes in Header / Footer
    m_pFieldTxbxHdFtPLCF.reset(new WW8PLCFx_FLD(pTableSt,*pWwFib,MAN_TXBX_HDFT));

    // Note: 6 stands for "6 OR 7",  7 stands for "ONLY 7"
    switch( m_pWw8Fib->m_nVersion )
    {
        case 6:
        case 7:
            if( pWwFib->m_fcPlcfdoaMom && pWwFib->m_lcbPlcfdoaMom )
            {
                m_pMainFdoa.reset(new WW8PLCFspecial( pTableSt, pWwFib->m_fcPlcfdoaMom,
                    pWwFib->m_lcbPlcfdoaMom, 6 ));
            }
            if( pWwFib->m_fcPlcfdoaHdr && pWwFib->m_lcbPlcfdoaHdr )
            {
                m_pHdFtFdoa.reset(new WW8PLCFspecial( pTableSt, pWwFib->m_fcPlcfdoaHdr,
                pWwFib->m_lcbPlcfdoaHdr, 6 ));
            }
            break;
        case 8:
            if( pWwFib->m_fcPlcfspaMom && pWwFib->m_lcbPlcfspaMom )
            {
                m_pMainFdoa.reset(new WW8PLCFspecial( pTableSt, pWwFib->m_fcPlcfspaMom,
                    pWwFib->m_lcbPlcfspaMom, 26 ));
            }
            if( pWwFib->m_fcPlcfspaHdr && pWwFib->m_lcbPlcfspaHdr )
            {
                m_pHdFtFdoa.reset(new WW8PLCFspecial( pTableSt, pWwFib->m_fcPlcfspaHdr,
                    pWwFib->m_lcbPlcfspaHdr, 26 ));
            }
            // PLCF for TextBox break-descriptors in the main text
            if( pWwFib->m_fcPlcftxbxBkd && pWwFib->m_lcbPlcftxbxBkd )
            {
                m_pMainTxbxBkd.reset(new WW8PLCFspecial( pTableSt,
                    pWwFib->m_fcPlcftxbxBkd, pWwFib->m_lcbPlcftxbxBkd, 0));
            }
            // PLCF for TextBox break-descriptors in Header/Footer range
            if( pWwFib->m_fcPlcfHdrtxbxBkd && pWwFib->m_lcbPlcfHdrtxbxBkd )
            {
                m_pHdFtTxbxBkd.reset(new WW8PLCFspecial( pTableSt,
                    pWwFib->m_fcPlcfHdrtxbxBkd, pWwFib->m_lcbPlcfHdrtxbxBkd, 0));
            }
            // Sub table cp positions
            if (pWwFib->m_fcPlcfTch && pWwFib->m_lcbPlcfTch)
            {
                m_pMagicTables.reset(new WW8PLCFspecial( pTableSt,
                    pWwFib->m_fcPlcfTch, pWwFib->m_lcbPlcfTch, 4));
            }
            // Sub document cp positions
            if (pWwFib->m_fcPlcfwkb && pWwFib->m_lcbPlcfwkb)
            {
                m_pSubdocs.reset(new WW8PLCFspecial( pTableSt,
                    pWwFib->m_fcPlcfwkb, pWwFib->m_lcbPlcfwkb, 12));
            }
            // Extended ATRD
            if (pWwFib->m_fcAtrdExtra && pWwFib->m_lcbAtrdExtra)
            {
                sal_uInt64 const nOldPos = pTableSt->Tell();
                if (checkSeek(*pTableSt, pWwFib->m_fcAtrdExtra) && (pTableSt->remainingSize() >= pWwFib->m_lcbAtrdExtra))
                {
                    m_pExtendedAtrds.reset( new sal_uInt8[pWwFib->m_lcbAtrdExtra] );
                    pWwFib->m_lcbAtrdExtra = pTableSt->ReadBytes(m_pExtendedAtrds.get(), pWwFib->m_lcbAtrdExtra);
                }
                else
                    pWwFib->m_lcbAtrdExtra = 0;
                pTableSt->Seek(nOldPos);
            }

            break;
        default:
            OSL_ENSURE( false, "nVersion not implemented!" );
            break;
    }

    // PLCF for TextBox stories in main text
    sal_uInt32 nLenTxBxS = (8 > m_pWw8Fib->m_nVersion) ? 0 : 22;
    if( pWwFib->m_fcPlcftxbxText && pWwFib->m_lcbPlcftxbxText )
    {
        m_pMainTxbx.reset(new WW8PLCFspecial( pTableSt, pWwFib->m_fcPlcftxbxText,
            pWwFib->m_lcbPlcftxbxText, nLenTxBxS ));
    }

    // PLCF for TextBox stories in Header/Footer range
    if( pWwFib->m_fcPlcfHdrtxbxText && pWwFib->m_lcbPlcfHdrtxbxText )
    {
        m_pHdFtTxbx.reset(new WW8PLCFspecial( pTableSt, pWwFib->m_fcPlcfHdrtxbxText,
            pWwFib->m_lcbPlcfHdrtxbxText, nLenTxBxS ));
    }

    m_pBook.reset(new WW8PLCFx_Book(pTableSt, *pWwFib));
    m_pAtnBook.reset(new WW8PLCFx_AtnBook(pTableSt, *pWwFib));
    m_pFactoidBook.reset(new WW8PLCFx_FactoidBook(pTableSt, *pWwFib));
}

WW8ScannerBase::~WW8ScannerBase()
{
    m_aPieceGrpprls.clear();
    m_pPLCFx_PCDAttrs.reset();
    m_pPLCFx_PCD.reset();
    m_pPieceIter.reset();
    m_pPiecePLCF.reset();
    m_pFactoidBook.reset();
    m_pAtnBook.reset();
    m_pBook.reset();
    m_pFieldEdnPLCF.reset();
    m_pFieldFootnotePLCF.reset();
    m_pFieldAndPLCF.reset();
    m_pFieldHdFtPLCF.reset();
    m_pFieldPLCF.reset();
    m_pFieldTxbxPLCF.reset();
    m_pFieldTxbxHdFtPLCF.reset();
    m_pEdnPLCF.reset();
    m_pFootnotePLCF.reset();
    m_pAndPLCF.reset();
    m_pSepPLCF.reset();
    m_pPapPLCF.reset();
    m_pChpPLCF.reset();
    m_pMainFdoa.reset();
    m_pHdFtFdoa.reset();
    m_pMainTxbx.reset();
    m_pMainTxbxBkd.reset();
    m_pHdFtTxbx.reset();
    m_pHdFtTxbxBkd.reset();
    m_pMagicTables.reset();
    m_pSubdocs.reset();
}

// Fields

static bool WW8SkipField(WW8PLCFspecial& rPLCF)
{
    void* pData;
    WW8_CP nP;

    if (!rPLCF.Get(nP, pData))              // End of PLCFspecial?
        return false;

    rPLCF.advance();

    if((static_cast<sal_uInt8*>(pData)[0] & 0x1f ) != 0x13 )    // No beginning?
        return true;                            // Do not terminate on error

    if( !rPLCF.Get( nP, pData ) )
        return false;

    while((static_cast<sal_uInt8*>(pData)[0] & 0x1f ) == 0x13 )
    {
        // still new (nested) beginnings ?
        WW8SkipField( rPLCF );              // nested Field in description
        if( !rPLCF.Get( nP, pData ) )
            return false;
    }

    if((static_cast<sal_uInt8*>(pData)[0] & 0x1f ) == 0x14 )
    {

        // Field Separator ?
        rPLCF.advance();

        if( !rPLCF.Get( nP, pData ) )
            return false;

        while ((static_cast<sal_uInt8*>(pData)[0] & 0x1f ) == 0x13)
        {
            // still new (nested) beginnings?
            WW8SkipField( rPLCF );          // nested Field in Results
            if( !rPLCF.Get( nP, pData ) )
                return false;
        }
    }
    rPLCF.advance();

    return true;
}

static bool WW8GetFieldPara(WW8PLCFspecial& rPLCF, WW8FieldDesc& rF)
{
    void* pData;
    sal_uInt32 nOldIdx = rPLCF.GetIdx();

    rF.nLen = rF.nId = rF.nOpt = 0;
    rF.bCodeNest = rF.bResNest = false;

    if (!rPLCF.Get(rF.nSCode, pData) || rF.nSCode < 0)             // end of PLCFspecial?
        goto Err;

    rPLCF.advance();

    if (!pData || (static_cast<sal_uInt8*>(pData)[0] & 0x1f) != 0x13)        // No beginning?
        goto Err;

    rF.nId = static_cast<sal_uInt8*>(pData)[1];

    if( !rPLCF.Get( rF.nLCode, pData ) )
        goto Err;

    if (rF.nLCode < rF.nSCode)
        goto Err;

    rF.nSRes = rF.nLCode;                           // Default
    rF.nSCode++;                                    // without markers
    rF.nLCode -= rF.nSCode;                         // Pos -> length

    while((static_cast<sal_uInt8*>(pData)[0] & 0x1f ) == 0x13 )
    {
        // still new (nested) beginnings ?
        WW8SkipField( rPLCF );              // nested Field in description
        rF.bCodeNest = true;
        if (!rPLCF.Get(rF.nSRes, pData) || rF.nSRes < 0)
            goto Err;
    }

    if ((static_cast<sal_uInt8*>(pData)[0] & 0x1f ) == 0x14 )       // Field Separator?
    {
        rPLCF.advance();

        if (!rPLCF.Get(rF.nLRes, pData) || rF.nLRes < 0)
            goto Err;

        while((static_cast<sal_uInt8*>(pData)[0] & 0x1f ) == 0x13 )
        {
            // still new (nested) beginnings ?
            WW8SkipField( rPLCF );              // nested Field in results
            rF.bResNest = true;
            if (!rPLCF.Get(rF.nLRes, pData) || rF.nLRes < 0)
                goto Err;
        }
        WW8_CP nTmp;
        if (o3tl::checked_sub<WW8_CP>(rF.nLRes, rF.nSCode, nTmp))
        {
            rF.nLen = 0;
            goto Err;
        }
        if (o3tl::checked_add<WW8_CP>(nTmp, 2, rF.nLen)) // nLRes is still the final position
        {
            rF.nLen = 0;
            goto Err;
        }
        rF.nLRes -= rF.nSRes;                       // now: nLRes = length
        if (o3tl::checked_add<WW8_CP>(rF.nSRes, 1, rF.nSRes)) // Endpos including Markers
        {
            rF.nLen = 0;
            goto Err;
        }
        rF.nLRes--;
    }else{
        rF.nLRes = 0;                               // no result found
        WW8_CP nTmp;
        if (o3tl::checked_sub<WW8_CP>(rF.nSRes, rF.nSCode, nTmp))
        {
            rF.nLen = 0;
            goto Err;
        }
        if (o3tl::checked_add<WW8_CP>(nTmp, 2, rF.nLen)) // total length
        {
            rF.nLen = 0;
            goto Err;
        }
    }

    if (rF.nLen < 0)
    {
        rF.nLen = 0;
        goto Err;
    }

    rPLCF.advance();
    if((static_cast<sal_uInt8*>(pData)[0] & 0x1f ) == 0x15 )
    {
        // Field end ?
        // INDEX-Field has set Bit7?
        rF.nOpt = static_cast<sal_uInt8*>(pData)[1];                // yes -> copy flags
    }else{
        rF.nId = 0;                                      // no -> Field invalid
    }

    rPLCF.SetIdx( nOldIdx );
    return true;
Err:
    rPLCF.SetIdx( nOldIdx );
    return false;
}

OUString read_uInt8_BeltAndBracesString(SvStream& rStrm, rtl_TextEncoding eEnc)
{
    const OUString aRet = read_uInt8_lenPrefixed_uInt8s_ToOUString(rStrm, eEnc);
    rStrm.SeekRel(sizeof(sal_uInt8)); // skip null-byte at end
    return aRet;
}

OUString read_uInt16_BeltAndBracesString(SvStream& rStrm)
{
    const OUString aRet = read_uInt16_PascalString(rStrm);
    rStrm.SeekRel(sizeof(sal_Unicode)); // skip null-byte at end
    return aRet;
}

sal_Int32 WW8ScannerBase::WW8ReadString( SvStream& rStrm, OUString& rStr,
    WW8_CP nCurrentStartCp, tools::Long nTotalLen, rtl_TextEncoding eEnc ) const
{
    // Read in plain text, which can extend over several pieces
    rStr.clear();

    if (nCurrentStartCp < 0 || nTotalLen < 0)
        return 0;

    WW8_CP nBehindTextCp = nCurrentStartCp + nTotalLen;
    WW8_CP nNextPieceCp  = nBehindTextCp; // Initialization, important for Ver6
    tools::Long nTotalRead = 0;
    do
    {
        bool bIsUnicode(false), bPosOk(false);
        WW8_FC fcAct = WW8Cp2Fc(nCurrentStartCp,&bIsUnicode,&nNextPieceCp,&bPosOk);

        // Probably aimed beyond file end, doesn't matter!
        if( !bPosOk )
            break;

        bool bValid = checkSeek(rStrm, fcAct);
        if (!bValid)
            break;

        WW8_CP nEnd = (nNextPieceCp < nBehindTextCp) ? nNextPieceCp
            : nBehindTextCp;
        WW8_CP nLen;
        const bool bFail = o3tl::checked_sub(nEnd, nCurrentStartCp, nLen);
        if (bFail)
            break;

        if( 0 >= nLen )
            break;

        rStr += bIsUnicode
             ? read_uInt16s_ToOUString(rStrm, nLen)
             : read_uInt8s_ToOUString(rStrm, nLen, eEnc);

        nTotalRead  += nLen;
        nCurrentStartCp += nLen;
        if ( nTotalRead != rStr.getLength() )
            break;
    }
    while( nTotalRead < nTotalLen );

    return rStr.getLength();
}

WW8PLCFspecial::WW8PLCFspecial(SvStream* pSt, sal_uInt32 nFilePos,
    sal_uInt32 nPLCF, sal_uInt32 nStruct)
    : m_nIdx(0), m_nStru(nStruct)
{
    const sal_uInt32 nValidMin=4;

    sal_uInt64 const nOldPos = pSt->Tell();

    bool bValid = checkSeek(*pSt, nFilePos);
    std::size_t nRemainingSize = pSt->remainingSize();
    if( nRemainingSize < nValidMin || nPLCF < nValidMin )
        bValid = false;
    nPLCF = bValid ? std::min(nRemainingSize, static_cast<std::size_t>(nPLCF)) : nValidMin;

    // Pointer to Pos- and Struct-array
    m_pPLCF_PosArray.reset( new sal_Int32[ ( nPLCF + 3 ) / 4 ] );
    m_pPLCF_PosArray[0] = 0;

    nPLCF = bValid ? pSt->ReadBytes(m_pPLCF_PosArray.get(), nPLCF) : nValidMin;

    nPLCF = std::max(nPLCF, nValidMin);

    m_nIMax = ( nPLCF - 4 ) / ( 4 + nStruct );
#ifdef OSL_BIGENDIAN
    for( m_nIdx = 0; m_nIdx <= m_nIMax; m_nIdx++ )
        m_pPLCF_PosArray[m_nIdx] = OSL_SWAPDWORD( m_pPLCF_PosArray[m_nIdx] );
    m_nIdx = 0;
#endif // OSL_BIGENDIAN
    if( nStruct ) // Pointer to content array
        m_pPLCF_Contents = reinterpret_cast<sal_uInt8*>(&m_pPLCF_PosArray[m_nIMax + 1]);
    else
        m_pPLCF_Contents = nullptr;                         // no content

    pSt->Seek(nOldPos);
}

// WW8PLCFspecial::SeekPos() sets WW8PLCFspecial to position nPos, while also the entry is used
// that begins before nPos and ends after nPos.
// Suitable for normal attributes. However, the beginning of the attribute is not corrected onto
// the position nPos.
bool WW8PLCFspecial::SeekPos(tools::Long nP)
{
    if( nP < m_pPLCF_PosArray[0] )
    {
        m_nIdx = 0;
        return false;   // Not found: nP less than smallest entry
    }

    // Search from beginning?
    if ((m_nIdx < 1) || (nP < m_pPLCF_PosArray[m_nIdx - 1]))
        m_nIdx = 1;

    tools::Long nI   = m_nIdx;
    tools::Long nEnd = m_nIMax;

    for(int n = (1==m_nIdx ? 1 : 2); n; --n )
    {
        for( ; nI <=nEnd; ++nI)
        {                                   // search with an index that is incremented by 1
            if( nP < m_pPLCF_PosArray[nI] )
            {                               // found position
                m_nIdx = nI - 1;              // nI - 1 is the correct index
                return true;                // done
            }
        }
        nI   = 1;
        nEnd = m_nIdx-1;
    }
    m_nIdx = m_nIMax;               // not found, greater than all entries
    return false;
}

// WW8PLCFspecial::SeekPosExact() like SeekPos(), but it is ensured that no attribute is cut,
// i.e. the next given attribute begins at or after nPos.
// Is used for fields and bookmarks.
bool WW8PLCFspecial::SeekPosExact(tools::Long nP)
{
    if( nP < m_pPLCF_PosArray[0] )
    {
        m_nIdx = 0;
        return false;       // Not found: nP less than smallest entry
    }
    // Search from beginning?
    if( nP <=m_pPLCF_PosArray[m_nIdx] )
        m_nIdx = 0;

    tools::Long nI   = m_nIdx ? m_nIdx-1 : 0;
    tools::Long nEnd = m_nIMax;

    for(int n = (0==m_nIdx ? 1 : 2); n; --n )
    {
        for( ; nI < nEnd; ++nI)
        {
            if( nP <=m_pPLCF_PosArray[nI] )
            {                           // found position
                m_nIdx = nI;              // nI is the correct index
                return true;            // done
            }
        }
        nI   = 0;
        nEnd = m_nIdx;
    }
    m_nIdx = m_nIMax;               // Not found, greater than all entries
    return false;
}

bool WW8PLCFspecial::Get(WW8_CP& rPos, void*& rpValue) const
{
    return GetData( m_nIdx, rPos, rpValue );
}

bool WW8PLCFspecial::GetData(tools::Long nInIdx, WW8_CP& rPos, void*& rpValue) const
{
    if ( nInIdx >= m_nIMax )
    {
        rPos = WW8_CP_MAX;
        return false;
    }
    rPos = m_pPLCF_PosArray[nInIdx];
    rpValue = m_pPLCF_Contents ? static_cast<void*>(&m_pPLCF_Contents[nInIdx * m_nStru]) : nullptr;
    return true;
}

// WW8PLCF e.g. for SEPX
// Ctor for *others* than Fkps
// With nStartPos < 0, the first element of PLCFs will be taken
WW8PLCF::WW8PLCF(SvStream& rSt, WW8_FC nFilePos, sal_Int32 nPLCF, int nStruct,
    WW8_CP nStartPos) : m_nIdx(0), m_nStru(nStruct)
{
    if (nPLCF < 0)
    {
        SAL_WARN("sw.ww8", "broken WW8PLCF, ignoring");
        nPLCF = 0;
    }
    else
        m_nIMax = (nPLCF - 4) / (4 + nStruct);

    ReadPLCF(rSt, nFilePos, nPLCF);

    if( nStartPos >= 0 )
        SeekPos( nStartPos );
}

// Ctor *only* for Fkps
// The last 2 parameters are needed for PLCF.Chpx and PLCF.Papx.
// If ncpN != 0, then an incomplete PLCF will be completed. This is always required for WW6 with
// lack of resources and for WordPad (W95).
// With nStartPos < 0, the first element of the PLCFs is taken.
WW8PLCF::WW8PLCF(SvStream& rSt, WW8_FC nFilePos, sal_Int32 nPLCF, int nStruct,
    WW8_CP nStartPos, sal_Int32 nPN, sal_Int32 ncpN): m_nIdx(0),
    m_nStru(nStruct)
{
    if (nPLCF < 0)
    {
        SAL_WARN("sw.ww8", "broken WW8PLCF, ignoring");
        m_nIMax = SAL_MAX_INT32;
    }
    else
        m_nIMax = (nPLCF - 4) / (4 + nStruct);

    if( m_nIMax >= ncpN )
        ReadPLCF(rSt, nFilePos, nPLCF);
    else
        GeneratePLCF(rSt, nPN, ncpN);

    if( nStartPos >= 0 )
        SeekPos( nStartPos );
}

void WW8PLCF::ReadPLCF(SvStream& rSt, WW8_FC nFilePos, sal_uInt32 nPLCF)
{
    sal_uInt64 const nOldPos = rSt.Tell();
    bool bValid = nPLCF != 0 && checkSeek(rSt, nFilePos)
        && (rSt.remainingSize() >= nPLCF);

    if (bValid)
    {
        // Pointer to Pos-array
        const size_t nEntries = (nPLCF + 3) / 4;
        m_pPLCF_PosArray.reset(new WW8_CP[nEntries]);
        bValid = checkRead(rSt, m_pPLCF_PosArray.get(), nPLCF);
        size_t nBytesAllocated = nEntries * sizeof(WW8_CP);
        if (bValid && nPLCF != nBytesAllocated)
        {
            sal_uInt8* pStartBlock = reinterpret_cast<sal_uInt8*>(m_pPLCF_PosArray.get());
            memset(pStartBlock + nPLCF, 0, nBytesAllocated - nPLCF);
        }
    }

    if (bValid)
    {
#ifdef OSL_BIGENDIAN
        for( m_nIdx = 0; m_nIdx <= m_nIMax; m_nIdx++ )
            m_pPLCF_PosArray[m_nIdx] = OSL_SWAPDWORD( m_pPLCF_PosArray[m_nIdx] );
        m_nIdx = 0;
#endif // OSL_BIGENDIAN
        // Pointer to content array
        m_pPLCF_Contents = reinterpret_cast<sal_uInt8*>(&m_pPLCF_PosArray[m_nIMax + 1]);

        TruncToSortedRange();
    }

    OSL_ENSURE(bValid, "Document has corrupt PLCF, ignoring it");

    if (!bValid)
        MakeFailedPLCF();

    rSt.Seek(nOldPos);
}

void WW8PLCF::MakeFailedPLCF()
{
    m_nIMax = 0;
    m_pPLCF_PosArray.reset( new WW8_CP[2] );
    m_pPLCF_PosArray[0] = m_pPLCF_PosArray[1] = WW8_CP_MAX;
    m_pPLCF_Contents = reinterpret_cast<sal_uInt8*>(&m_pPLCF_PosArray[m_nIMax + 1]);
}

namespace
{
    sal_Int32 TruncToSortedRange(const sal_Int32* pPLCF_PosArray, sal_Int32 nIMax)
    {
        //Docs state that: ... all Plcs ... are sorted in ascending order.
        //So ensure that here for broken documents.
        for (auto nI = 0; nI < nIMax; ++nI)
        {
            if (pPLCF_PosArray[nI] > pPLCF_PosArray[nI+1])
            {
                SAL_WARN("sw.ww8", "Document has unsorted PLCF, truncated to sorted portion");
                nIMax = nI;
                break;
            }
        }
        return nIMax;
    }
}

void WW8PLCFpcd::TruncToSortedRange()
{
    m_nIMax = ::TruncToSortedRange(m_pPLCF_PosArray.get(), m_nIMax);
}

void WW8PLCF::TruncToSortedRange()
{
    m_nIMax = ::TruncToSortedRange(m_pPLCF_PosArray.get(), m_nIMax);
}

void WW8PLCF::GeneratePLCF(SvStream& rSt, sal_Int32 nPN, sal_Int32 ncpN)
{
    OSL_ENSURE( m_nIMax < ncpN, "Pcl.Fkp: Why is PLCF too big?" );

    bool failure = false;
    m_nIMax = ncpN;

    if ((m_nIMax < 1) || (m_nIMax > (WW8_CP_MAX - 4) / (4 + m_nStru)) || nPN < 0)
        failure = true;

    if (!failure)
    {
        // Check arguments to ShortToSVBT16 in loop below will all be valid:
        sal_Int32 nResult;
        failure = o3tl::checked_add(nPN, ncpN, nResult) || nResult > SAL_MAX_UINT16;
    }

    if (!failure)
    {
        size_t nSiz = (4 + m_nStru) * m_nIMax + 4;
        size_t nElems = ( nSiz + 3 ) / 4;
        m_pPLCF_PosArray.reset( new WW8_CP[ nElems ] ); // Pointer to Pos-array

        for (sal_Int32 i = 0; i < ncpN && !failure; ++i)
        {
            failure = true;
            // construct FC entries
            // first FC entry of each Fkp
            if (!checkSeek(rSt, (nPN + i) << 9))
                break;

            WW8_CP nFc(0);
            rSt.ReadInt32( nFc );
            m_pPLCF_PosArray[i] = nFc;

            failure = bool(rSt.GetError());
        }
    }

    if (!failure)
    {
        do
        {
            failure = true;

            std::size_t nLastFkpPos = nPN + m_nIMax - 1;
            nLastFkpPos = nLastFkpPos << 9;
            // number of FC entries of last Fkp
            if (!checkSeek(rSt, nLastFkpPos + 511))
                break;

            sal_uInt8 nb(0);
            rSt.ReadUChar( nb );
            // last FC entry of last Fkp
            if (!checkSeek(rSt, nLastFkpPos + nb * 4))
                break;

            WW8_CP nFc(0);
            rSt.ReadInt32( nFc );
            m_pPLCF_PosArray[m_nIMax] = nFc;        // end of the last Fkp

            failure = bool(rSt.GetError());
        } while(false);
    }

    if (!failure)
    {
        // Pointer to content array
        m_pPLCF_Contents = reinterpret_cast<sal_uInt8*>(&m_pPLCF_PosArray[m_nIMax + 1]);
        sal_uInt8* p = m_pPLCF_Contents;

        for (sal_Int32 i = 0; i < ncpN; ++i)         // construct PNs
        {
            ShortToSVBT16(o3tl::narrowing<sal_uInt16>(nPN + i), p);
            p += m_nStru;
        }
    }

    SAL_WARN_IF(failure, "sw.ww8", "Document has corrupt PLCF, ignoring it");

    if (failure)
        MakeFailedPLCF();
}

bool WW8PLCF::SeekPos(WW8_CP nPos)
{
    WW8_CP nP = nPos;

    if( nP < m_pPLCF_PosArray[0] )
    {
        m_nIdx = 0;
        // not found: nPos less than smallest entry
        return false;
    }

    // Search from beginning?
    if ((m_nIdx < 1) || (nP < m_pPLCF_PosArray[m_nIdx - 1]))
        m_nIdx = 1;

    sal_Int32 nI   = m_nIdx;
    sal_Int32 nEnd = m_nIMax;

    for(int n = (1==m_nIdx ? 1 : 2); n; --n )
    {
        for( ; nI <=nEnd; ++nI)             // search with an index that is incremented by 1
        {
            if( nP < m_pPLCF_PosArray[nI] )   // found position
            {
                m_nIdx = nI - 1;              // nI - 1 is the correct index
                return true;                // done
            }
        }
        nI   = 1;
        nEnd = m_nIdx-1;
    }

    m_nIdx = m_nIMax;               // not found, greater than all entries
    return false;
}

bool WW8PLCF::Get(WW8_CP& rStart, WW8_CP& rEnd, void*& rpValue) const
{
    if ( m_nIdx >= m_nIMax )
    {
        rStart = rEnd = WW8_CP_MAX;
        return false;
    }
    rStart = m_pPLCF_PosArray[ m_nIdx ];
    rEnd   = m_pPLCF_PosArray[ m_nIdx + 1 ];
    rpValue = static_cast<void*>(&m_pPLCF_Contents[m_nIdx * m_nStru]);
    return true;
}

WW8_CP WW8PLCF::Where() const
{
    if ( m_nIdx >= m_nIMax )
        return WW8_CP_MAX;

    return m_pPLCF_PosArray[m_nIdx];
}

WW8PLCFpcd::WW8PLCFpcd(SvStream* pSt, sal_uInt32 nFilePos,
    sal_uInt32 nPLCF, sal_uInt32 nStruct)
    : m_nStru( nStruct )
{
    const sal_uInt32 nValidMin=4;

    sal_uInt64 const nOldPos = pSt->Tell();

    bool bValid = checkSeek(*pSt, nFilePos);
    std::size_t nRemainingSize = pSt->remainingSize();
    if( nRemainingSize < nValidMin || nPLCF < nValidMin )
        bValid = false;
    nPLCF = bValid ? std::min(nRemainingSize, static_cast<std::size_t>(nPLCF)) : nValidMin;

    m_pPLCF_PosArray.reset( new WW8_CP[ ( nPLCF + 3 ) / 4 ] );    // Pointer to Pos-array
    m_pPLCF_PosArray[0] = 0;

    nPLCF = bValid ? pSt->ReadBytes(m_pPLCF_PosArray.get(), nPLCF) : nValidMin;
    nPLCF = std::max(nPLCF, nValidMin);

    m_nIMax = ( nPLCF - 4 ) / ( 4 + nStruct );
#ifdef OSL_BIGENDIAN
    for( tools::Long nI = 0; nI <= m_nIMax; nI++ )
      m_pPLCF_PosArray[nI] = OSL_SWAPDWORD( m_pPLCF_PosArray[nI] );
#endif // OSL_BIGENDIAN

    // Pointer to content array
    m_pPLCF_Contents = reinterpret_cast<sal_uInt8*>(&m_pPLCF_PosArray[m_nIMax + 1]);
    TruncToSortedRange();

    pSt->Seek( nOldPos );
}

// If nStartPos < 0, the first element of PLCFs will be taken
WW8PLCFpcd_Iter::WW8PLCFpcd_Iter( WW8PLCFpcd& rPLCFpcd, tools::Long nStartPos )
    :m_rPLCF( rPLCFpcd ), m_nIdx( 0 )
{
    if( nStartPos >= 0 )
        SeekPos( nStartPos );
}

bool WW8PLCFpcd_Iter::SeekPos(tools::Long nPos)
{
    tools::Long nP = nPos;

    if( nP < m_rPLCF.m_pPLCF_PosArray[0] )
    {
        m_nIdx = 0;
        return false;       // not found: nPos less than smallest entry
    }
    // Search from beginning?
    if ((m_nIdx < 1) || (nP < m_rPLCF.m_pPLCF_PosArray[m_nIdx - 1]))
        m_nIdx = 1;

    tools::Long nI   = m_nIdx;
    tools::Long nEnd = m_rPLCF.m_nIMax;

    for(int n = (1==m_nIdx ? 1 : 2); n; --n )
    {
        for( ; nI <=nEnd; ++nI)
        {                               // search with an index that is incremented by 1
            if( nP < m_rPLCF.m_pPLCF_PosArray[nI] )
            {                           // found position
                m_nIdx = nI - 1;          // nI - 1 is the correct index
                return true;            // done
            }
        }
        nI   = 1;
        nEnd = m_nIdx-1;
    }
    m_nIdx = m_rPLCF.m_nIMax;         // not found, greater than all entries
    return false;
}

bool WW8PLCFpcd_Iter::Get(WW8_CP& rStart, WW8_CP& rEnd, void*& rpValue) const
{
    if( m_nIdx >= m_rPLCF.m_nIMax )
    {
        rStart = rEnd = WW8_CP_MAX;
        return false;
    }
    rStart = m_rPLCF.m_pPLCF_PosArray[m_nIdx];
    rEnd = m_rPLCF.m_pPLCF_PosArray[m_nIdx + 1];
    rpValue = static_cast<void*>(&m_rPLCF.m_pPLCF_Contents[m_nIdx * m_rPLCF.m_nStru]);
    return true;
}

sal_Int32 WW8PLCFpcd_Iter::Where() const
{
    if ( m_nIdx >= m_rPLCF.m_nIMax )
        return SAL_MAX_INT32;

    return m_rPLCF.m_pPLCF_PosArray[m_nIdx];
}

bool WW8PLCFx_Fc_FKP::WW8Fkp::Entry::operator<
    (const WW8PLCFx_Fc_FKP::WW8Fkp::Entry& rSecond) const
{
    return (mnFC < rSecond.mnFC);
}

static bool IsReplaceAllSprm(sal_uInt16 nSpId)
{
    return (NS_sprm::LN_PHugePapx == nSpId || 0x6646 == nSpId);
}

static bool IsExpandableSprm(sal_uInt16 nSpId)
{
    return 0x646B == nSpId;
}

void WW8PLCFx_Fc_FKP::WW8Fkp::FillEntry(WW8PLCFx_Fc_FKP::WW8Fkp::Entry &rEntry,
    std::size_t nDataOffset, sal_uInt16 nLen)
{
    bool bValidPos = (nDataOffset < sizeof(maRawData));

    OSL_ENSURE(bValidPos, "sprm sequence offset is out of range, ignoring");

    if (!bValidPos)
    {
        rEntry.mnLen = 0;
        return;
    }

    const sal_uInt16 nAvailableData = sizeof(maRawData)-nDataOffset;
    OSL_ENSURE(nLen <= nAvailableData, "sprm sequence len is out of range, clipping");
    rEntry.mnLen = std::min(nLen, nAvailableData);
    rEntry.mpData = maRawData + nDataOffset;
}

WW8PLCFx_Fc_FKP::WW8Fkp::WW8Fkp(const WW8Fib& rFib, SvStream* pSt,
    SvStream* pDataSt, tools::Long _nFilePos, tools::Long nItemSiz, ePLCFT ePl,
    WW8_FC nStartFc)
    : m_nItemSize(nItemSiz), m_nFilePos(_nFilePos), mnIdx(0), m_ePLCF(ePl)
    , mnMustRemainCached(0), maSprmParser(rFib)
{
    memset(maRawData, 0, 512);

    const ww::WordVersion eVersion = rFib.GetFIBVersion();

    sal_uInt64 const nOldPos = pSt->Tell();

    bool bCouldSeek = checkSeek(*pSt, m_nFilePos);
    bool bCouldRead = bCouldSeek && checkRead(*pSt, maRawData, 512);

    mnIMax = bCouldRead ? maRawData[511] : 0;

    sal_uInt8 *pStart = maRawData;
    // Offset-Location in maRawData
    const size_t nRawDataStart = (mnIMax + 1) * 4;

    for (mnIdx = 0; mnIdx < mnIMax; ++mnIdx)
    {
        const size_t nRawDataOffset = nRawDataStart + mnIdx * m_nItemSize;

        //clip to available data, corrupt fkp
        if (nRawDataOffset >= 511)
        {
            mnIMax = mnIdx;
            break;
        }

        unsigned int nOfs = maRawData[nRawDataOffset] * 2;
        // nOfs in [0..0xff*2=510]

        Entry aEntry(Get_Long(pStart));

        if (nOfs)
        {
            switch (m_ePLCF)
            {
                case CHP:
                {
                    aEntry.mnLen = maRawData[nOfs];

                    //len byte
                    std::size_t nDataOffset = nOfs + 1;

                    FillEntry(aEntry, nDataOffset, aEntry.mnLen);

                    if (aEntry.mnLen && eVersion <= ww::eWW2)
                    {
                        Word2CHPX aChpx = ReadWord2Chpx(*pSt, m_nFilePos + nOfs + 1, static_cast< sal_uInt8 >(aEntry.mnLen));
                        std::vector<sal_uInt8> aSprms = ChpxToSprms(aChpx);
                        aEntry.mnLen = static_cast< sal_uInt16 >(aSprms.size());
                        if (aEntry.mnLen)
                        {
                            aEntry.mpData = new sal_uInt8[aEntry.mnLen];
                            memcpy(aEntry.mpData, aSprms.data(), aEntry.mnLen);
                            aEntry.mbMustDelete = true;
                        }
                    }
                    break;
                }
                case PAP:
                    {
                        sal_uInt8 nDelta = 0;

                        aEntry.mnLen = maRawData[nOfs];
                        if (IsEightPlus(eVersion) && !aEntry.mnLen)
                        {
                            aEntry.mnLen = maRawData[nOfs+1];
                            nDelta++;
                        }
                        aEntry.mnLen *= 2;

                        //stylecode, std/istd
                        if (eVersion <= ww::eWW2)
                        {
                            if (aEntry.mnLen >= 1)
                            {
                                aEntry.mnIStd = *(maRawData+nOfs+1+nDelta);
                                aEntry.mnLen--;  //style code
                                if (aEntry.mnLen >= 6)
                                {
                                    aEntry.mnLen-=6; //PHE
                                    //skip stc, len byte + 6 byte PHE
                                    unsigned int nOffset = nOfs + 8;
                                    if (nOffset >= 511) //Bad offset
                                        aEntry.mnLen=0;
                                    if (aEntry.mnLen)   //start is ok
                                    {
                                        if (nOffset + aEntry.mnLen > 512)   //Bad end, clip
                                            aEntry.mnLen = 512 - nOffset;
                                        aEntry.mpData = maRawData + nOffset;
                                    }
                                }
                                else
                                    aEntry.mnLen=0; //Too short
                            }
                        }
                        else
                        {
                            if (aEntry.mnLen >= 2)
                            {
                                //len byte + optional extra len byte
                                std::size_t nDataOffset = nOfs + 1 + nDelta;
                                aEntry.mnIStd = nDataOffset <= sizeof(maRawData)-sizeof(aEntry.mnIStd) ?
                                    SVBT16ToUInt16(maRawData+nDataOffset) : 0;
                                aEntry.mnLen-=2; //istd
                                if (aEntry.mnLen)
                                {
                                    //additional istd
                                    nDataOffset += sizeof(aEntry.mnIStd);

                                    FillEntry(aEntry, nDataOffset, aEntry.mnLen);
                                }
                            }
                            else
                                aEntry.mnLen=0; //Too short, ignore
                        }

                        const sal_uInt16 nSpId = aEntry.mnLen
                            ? maSprmParser.GetSprmId(aEntry.mpData) : 0;

                        /*
                         If we replace then we throw away the old data, if we
                         are expanding, then we tack the old data onto the end
                         of the new data
                        */
                        const bool bExpand = IsExpandableSprm(nSpId);
                        const sal_uInt8* pStartData
                            = aEntry.mpData == nullptr ? nullptr : aEntry.mpData + 2;
                        const sal_uInt8* pLastValidDataPos = maRawData + 512 - sizeof(sal_uInt32);
                        if (pStartData != nullptr && pStartData > pLastValidDataPos)
                            pStartData = nullptr;
                        if ((IsReplaceAllSprm(nSpId) || bExpand) && pStartData)
                        {
                            sal_uInt64 nCurr = pDataSt->Tell();
                            sal_uInt32 nPos = SVBT32ToUInt32(pStartData);
                            sal_uInt16 nLen(0);

                            bool bOk = checkSeek(*pDataSt, nPos);
                            if (bOk)
                            {
                                pDataSt->ReadUInt16( nLen );
                                bOk = nLen <= pDataSt->remainingSize();
                            }

                            if (bOk)
                            {
                                const sal_uInt16 nOrigLen = bExpand ? aEntry.mnLen : 0;
                                sal_uInt8 *pOrigData = bExpand ? aEntry.mpData : nullptr;

                                aEntry.mnLen = nLen;
                                aEntry.mpData =
                                    new sal_uInt8[aEntry.mnLen + nOrigLen];
                                aEntry.mbMustDelete = true;
                                aEntry.mnLen =
                                    pDataSt->ReadBytes(aEntry.mpData, aEntry.mnLen);

                                pDataSt->Seek( nCurr );

                                if (pOrigData)
                                {
                                    memcpy(aEntry.mpData + aEntry.mnLen,
                                        pOrigData, nOrigLen);
                                    aEntry.mnLen = aEntry.mnLen + nOrigLen;
                                }
                            }
                        }
                    }
                    break;
                default:
                    OSL_FAIL("sweet god, what have you done!");
                    break;
            }
        }

        maEntries.push_back(aEntry);
    }

    //one more FC than grrpl entries
    maEntries.emplace_back(Get_Long(pStart));

    //we expect them sorted, but it appears possible for them to arrive unsorted
    std::stable_sort(maEntries.begin(), maEntries.end());

    mnIdx = 0;

    if (nStartFc >= 0)
        SeekPos(nStartFc);

    pSt->Seek(nOldPos);
}

WW8PLCFx_Fc_FKP::WW8Fkp::Entry::Entry(const Entry &rEntry)
    : mnFC(rEntry.mnFC), mnLen(rEntry.mnLen), mnIStd(rEntry.mnIStd),
    mbMustDelete(rEntry.mbMustDelete)
{
    if (mbMustDelete)
    {
        mpData = new sal_uInt8[mnLen];
        memcpy(mpData, rEntry.mpData, mnLen);
    }
    else
        mpData = rEntry.mpData;
}

WW8PLCFx_Fc_FKP::WW8Fkp::Entry&
    WW8PLCFx_Fc_FKP::WW8Fkp::Entry::operator=(const Entry &rEntry)
{
    if (this == &rEntry)
        return *this;

    if (mbMustDelete)
        delete[] mpData;

    mnFC = rEntry.mnFC;
    mnLen = rEntry.mnLen;
    mnIStd = rEntry.mnIStd;
    mbMustDelete = rEntry.mbMustDelete;

    if (rEntry.mbMustDelete)
    {
        mpData = new sal_uInt8[mnLen];
        memcpy(mpData, rEntry.mpData, mnLen);
    }
    else
        mpData = rEntry.mpData;

    return *this;
}

WW8PLCFx_Fc_FKP::WW8Fkp::Entry::~Entry()
{
    if (mbMustDelete)
        delete[] mpData;
}

void WW8PLCFx_Fc_FKP::WW8Fkp::Reset(WW8_FC nFc)
{
    SetIdx(0);
    if (nFc >= 0)
        SeekPos(nFc);
}

bool WW8PLCFx_Fc_FKP::WW8Fkp::SeekPos(WW8_FC nFc)
{
    if (nFc < maEntries[0].mnFC)
    {
        mnIdx = 0;
        return false;       // not found: nPos less than smallest entry
    }

    // Search from beginning?
    if ((mnIdx < 1) || (nFc < maEntries[mnIdx - 1].mnFC))
        mnIdx = 1;

    sal_uInt8 nI   = mnIdx;
    sal_uInt8 nEnd = mnIMax;

    for(sal_uInt8 n = (1==mnIdx ? 1 : 2); n; --n )
    {
        for( ; nI <=nEnd; ++nI)
        {                               // search with an index that is incremented by 1
            if (nFc < maEntries[nI].mnFC)
            {                           // found position
                mnIdx = nI - 1;          // nI - 1 is the correct index
                return true;            // done
            }
        }
        nI = 1;
        nEnd = mnIdx-1;
    }
    mnIdx = mnIMax;               // not found, greater than all entries
    return false;
}

sal_uInt8* WW8PLCFx_Fc_FKP::WW8Fkp::Get(WW8_FC& rStart, WW8_FC& rEnd, sal_Int32& rLen)
    const
{
    rLen = 0;

    if (mnIdx >= mnIMax)
    {
        rStart = WW8_FC_MAX;
        return nullptr;
    }

    rStart = maEntries[mnIdx].mnFC;
    rEnd   = maEntries[mnIdx + 1].mnFC;

    sal_uInt8* pSprms = GetLenAndIStdAndSprms( rLen );
    return pSprms;
}

void WW8PLCFx_Fc_FKP::WW8Fkp::SetIdx(sal_uInt8 nI)
{
    if (nI < mnIMax)
    {
        mnIdx = nI;
    }
}

sal_uInt8* WW8PLCFx_Fc_FKP::WW8Fkp::GetLenAndIStdAndSprms(sal_Int32& rLen) const
{
    rLen = maEntries[mnIdx].mnLen;
    return maEntries[mnIdx].mpData;
}

SprmResult WW8PLCFx_Fc_FKP::WW8Fkp::HasSprm( sal_uInt16 nId, bool bFindFirst )
{
    if (mnIdx >= mnIMax)
        return SprmResult();

    sal_Int32 nLen;
    sal_uInt8* pSprms = GetLenAndIStdAndSprms( nLen );

    WW8SprmIter aIter(pSprms, nLen, maSprmParser);
    return aIter.FindSprm(nId, bFindFirst);
}

void WW8PLCFx_Fc_FKP::WW8Fkp::HasSprm(sal_uInt16 nId,
    std::vector<SprmResult> &rResult)
{
    if (mnIdx >= mnIMax)
       return;

    sal_Int32 nLen;
    sal_uInt8* pSprms = GetLenAndIStdAndSprms( nLen );

    WW8SprmIter aIter(pSprms, nLen, maSprmParser);

    while(aIter.GetSprms())
    {
        if (aIter.GetCurrentId() == nId)
        {
            sal_Int32 nFixedLen = maSprmParser.DistanceToData(nId);
            sal_Int32 nL = maSprmParser.GetSprmSize(nId, aIter.GetSprms(), aIter.GetRemLen());
            rResult.emplace_back(aIter.GetCurrentParams(), nL - nFixedLen);
        }
        aIter.advance();
    };
}

ww::WordVersion WW8PLCFx::GetFIBVersion() const
{
    return mrFib.GetFIBVersion();
}

void WW8PLCFx::GetSprms( WW8PLCFxDesc* p )
{
    OSL_ENSURE( false, "Called wrong GetSprms" );
    p->nStartPos = p->nEndPos = WW8_CP_MAX;
    p->pMemPos = nullptr;
    p->nSprmsLen = 0;
    p->bRealLineEnd = false;
}

tools::Long WW8PLCFx::GetNoSprms( WW8_CP& rStart, WW8_CP& rEnd, sal_Int32& rLen )
{
    OSL_ENSURE( false, "Called wrong GetNoSprms" );
    rStart = rEnd = WW8_CP_MAX;
    rLen = 0;
    return 0;
}

// ...Idx2: Default: ignore
sal_uInt32 WW8PLCFx::GetIdx2() const
{
    return 0;
}

void WW8PLCFx::SetIdx2(sal_uInt32)
{
}

namespace {

class SamePos
{
private:
    tools::Long mnPo;
public:
    explicit SamePos(tools::Long nPo) : mnPo(nPo) {}
    bool operator()(const std::unique_ptr<WW8PLCFx_Fc_FKP::WW8Fkp>& pFkp)
        {return mnPo == pFkp->GetFilePos();}
};

}

bool WW8PLCFx_Fc_FKP::NewFkp()
{
    WW8_CP nPLCFStart, nPLCFEnd;
    void* pPage;

    static const int WW8FkpSizeTabVer2[ PLCF_END ] =
    {
        1,  1, 0 /*, 0, 0, 0*/
    };
    static const int WW8FkpSizeTabVer6[ PLCF_END ] =
    {
        1,  7, 0 /*, 0, 0, 0*/
    };
    static const int WW8FkpSizeTabVer8[ PLCF_END ] =
    {
        1, 13, 0 /*, 0, 0, 0*/
    };
    const int* pFkpSizeTab;

    switch (GetFIBVersion())
    {
        case ww::eWW1:
        case ww::eWW2:
            pFkpSizeTab = WW8FkpSizeTabVer2;
            break;
        case ww::eWW6:
        case ww::eWW7:
            pFkpSizeTab = WW8FkpSizeTabVer6;
            break;
        case ww::eWW8:
            pFkpSizeTab = WW8FkpSizeTabVer8;
            break;
        default:
            // program error!
            OSL_ENSURE( false, "nVersion not implemented!" );
            return false;
    }

    if (!m_pPLCF->Get( nPLCFStart, nPLCFEnd, pPage ))
    {
        m_pFkp = nullptr;
        return false;                           // PLCF completely processed
    }
    m_pPLCF->advance();
    tools::Long nPo = SVBT16ToUInt16( static_cast<sal_uInt8 *>(pPage) );
    nPo <<= 9;                                  // shift as LONG

    tools::Long nCurrentFkpFilePos = m_pFkp ? m_pFkp->GetFilePos() : -1;
    if (nCurrentFkpFilePos == nPo)
        m_pFkp->Reset(GetStartFc());
    else
    {
        auto aIter =
            std::find_if(maFkpCache.begin(), maFkpCache.end(), SamePos(nPo));
        if (aIter != maFkpCache.end())
        {
            m_pFkp = aIter->get();
            m_pFkp->Reset(GetStartFc());
        }
        else
        {
            m_pFkp = new WW8Fkp(GetFIB(), m_pFKPStrm, m_pDataStrm, nPo,
                pFkpSizeTab[ m_ePLCF ], m_ePLCF, GetStartFc());
            maFkpCache.push_back(std::unique_ptr<WW8Fkp>(m_pFkp));

            if (maFkpCache.size() > eMaxCache)
            {
                WW8Fkp* pCachedFkp = maFkpCache.front().get();
                if (!pCachedFkp->IsMustRemainCache())
                {
                    maFkpCache.pop_front();
                }
            }
        }
    }

    SetStartFc( -1 );                                   // only the first time
    return true;
}

WW8PLCFx_Fc_FKP::WW8PLCFx_Fc_FKP(SvStream* pSt, SvStream* pTableSt,
    SvStream* pDataSt, const WW8Fib& rFib, ePLCFT ePl, WW8_FC nStartFcL)
    : WW8PLCFx(rFib, true), m_pFKPStrm(pSt), m_pDataStrm(pDataSt)
    , m_pFkp(nullptr), m_ePLCF(ePl)
{
    SetStartFc(nStartFcL);
    tools::Long nLenStruct = (8 > rFib.m_nVersion) ? 2 : 4;
    if (ePl == CHP)
    {
        m_pPLCF.reset(new WW8PLCF(*pTableSt, rFib.m_fcPlcfbteChpx, rFib.m_lcbPlcfbteChpx,
            nLenStruct, GetStartFc(), rFib.m_pnChpFirst, rFib.m_cpnBteChp));
    }
    else
    {
        m_pPLCF.reset(new WW8PLCF(*pTableSt, rFib.m_fcPlcfbtePapx, rFib.m_lcbPlcfbtePapx,
            nLenStruct, GetStartFc(), rFib.m_pnPapFirst, rFib.m_cpnBtePap));
    }
}

WW8PLCFx_Fc_FKP::~WW8PLCFx_Fc_FKP()
{
    maFkpCache.clear();
    m_pPLCF.reset();
    m_pPCDAttrs.reset();
}

sal_uInt32 WW8PLCFx_Fc_FKP::GetIdx() const
{
    sal_uInt32 u = m_pPLCF->GetIdx() << 8;
    if (m_pFkp)
        u |= m_pFkp->GetIdx();
    return u;
}

void WW8PLCFx_Fc_FKP::SetIdx(sal_uInt32 nIdx)
{
    if( !( nIdx & 0xffffff00L ) )
    {
        m_pPLCF->SetIdx( nIdx >> 8 );
        m_pFkp = nullptr;
    }
    else
    {                                   // there was a Fkp
        // Set PLCF one position back to retrieve the address of the Fkp
        m_pPLCF->SetIdx( ( nIdx >> 8 ) - 1 );
        if (NewFkp())                       // read Fkp again
        {
            sal_uInt8 nFkpIdx = static_cast<sal_uInt8>(nIdx & 0xff);
            m_pFkp->SetIdx(nFkpIdx);          // set Fkp-Pos again
        }
    }
}

bool WW8PLCFx_Fc_FKP::SeekPos(WW8_FC nFcPos)
{
    // StartPos for next Where()
    SetStartFc( nFcPos );

    // find StartPos for next pPLCF->Get()
    bool bRet = m_pPLCF->SeekPos(nFcPos);

    // make FKP invalid?
    WW8_CP nPLCFStart, nPLCFEnd;
    void* pPage;
    if( m_pFkp && m_pPLCF->Get( nPLCFStart, nPLCFEnd, pPage ) )
    {
        tools::Long nPo = SVBT16ToUInt16( static_cast<sal_uInt8 *>(pPage) );
        nPo <<= 9;                                          // shift as LONG
        if (nPo != m_pFkp->GetFilePos())
            m_pFkp = nullptr;
        else
            m_pFkp->SeekPos( nFcPos );
    }
    return bRet;
}

WW8_FC WW8PLCFx_Fc_FKP::Where()
{
    if( !m_pFkp && !NewFkp() )
        return WW8_FC_MAX;
    WW8_FC nP = m_pFkp ? m_pFkp->Where() : WW8_FC_MAX;
    if( nP != WW8_FC_MAX )
        return nP;

    m_pFkp = nullptr;                   // FKP finished -> get new
    return Where();                     // easiest way: do it recursively
}

sal_uInt8* WW8PLCFx_Fc_FKP::GetSprmsAndPos(WW8_FC& rStart, WW8_FC& rEnd, sal_Int32& rLen)
{
    rLen = 0;                               // Default
    rStart = rEnd = WW8_FC_MAX;

    if( !m_pFkp )     // Fkp not there ?
    {
        if( !NewFkp() )
            return nullptr;
    }

    sal_uInt8* pPos = m_pFkp ? m_pFkp->Get( rStart, rEnd, rLen ) : nullptr;
    if( rStart == WW8_FC_MAX )    //Not found
        return nullptr;
    return pPos;
}

void WW8PLCFx_Fc_FKP::advance()
{
    if( !m_pFkp && !NewFkp() )
        return;

    if (!m_pFkp)
        return;

    m_pFkp->advance();
    if( m_pFkp->Where() == WW8_FC_MAX )
        (void)NewFkp();
}

sal_uInt16 WW8PLCFx_Fc_FKP::GetIstd() const
{
    return m_pFkp ? m_pFkp->GetIstd() : 0xFFFF;
}

void WW8PLCFx_Fc_FKP::GetPCDSprms( WW8PLCFxDesc& rDesc )
{
    rDesc.pMemPos   = nullptr;
    rDesc.nSprmsLen = 0;
    if( m_pPCDAttrs )
    {
        if( !m_pFkp )
        {
            OSL_FAIL("+Problem: GetPCDSprms: NewFkp necessary (not possible!)" );
            if( !NewFkp() )
                return;
        }
        m_pPCDAttrs->GetSprms(&rDesc);
    }
}

SprmResult WW8PLCFx_Fc_FKP::HasSprm(sal_uInt16 nId, bool bFindFirst)
{
    // const would be nicer, but for that, NewFkp() would need to be replaced or eliminated
    if( !m_pFkp )
    {
        OSL_FAIL( "+Motz: HasSprm: NewFkp needed ( no const possible )" );
        // happens in BugDoc 31722
        if( !NewFkp() )
            return SprmResult();
    }

    if (!m_pFkp)
        return SprmResult();

    SprmResult aRes = m_pFkp->HasSprm(nId, bFindFirst);

    if (!aRes.pSprm)
    {
        WW8PLCFxDesc aDesc;
        GetPCDSprms( aDesc );

        if (aDesc.pMemPos)
        {
            WW8SprmIter aIter(aDesc.pMemPos, aDesc.nSprmsLen,
                m_pFkp->GetSprmParser());
            aRes = aIter.FindSprm(nId, bFindFirst);
        }
    }

    return aRes;
}

void WW8PLCFx_Fc_FKP::HasSprm(sal_uInt16 nId, std::vector<SprmResult> &rResult)
{
    // const would be nicer, but for that, NewFkp() would need to be replaced or eliminated
    if (!m_pFkp)
    {
       OSL_FAIL( "+Motz: HasSprm: NewFkp needed ( no const possible )" );
       // happens in BugDoc 31722
       if( !NewFkp() )
           return;
    }

    if (!m_pFkp)
        return;

    m_pFkp->HasSprm(nId, rResult);

    WW8PLCFxDesc aDesc;
    GetPCDSprms( aDesc );

    if (!aDesc.pMemPos)
        return;

    const wwSprmParser &rSprmParser = m_pFkp->GetSprmParser();
    WW8SprmIter aIter(aDesc.pMemPos, aDesc.nSprmsLen, rSprmParser);
    while(aIter.GetSprms())
    {
        if (aIter.GetCurrentId() == nId)
        {
            sal_Int32 nFixedLen = rSprmParser.DistanceToData(nId);
            sal_Int32 nL = rSprmParser.GetSprmSize(nId, aIter.GetSprms(), aIter.GetRemLen());
            rResult.emplace_back(aIter.GetCurrentParams(), nL - nFixedLen);
        }
        aIter.advance();
    };
}

WW8PLCFx_Cp_FKP::WW8PLCFx_Cp_FKP( SvStream* pSt, SvStream* pTableSt,
    SvStream* pDataSt, const WW8ScannerBase& rBase, ePLCFT ePl )
    : WW8PLCFx_Fc_FKP(pSt, pTableSt, pDataSt, *rBase.m_pWw8Fib, ePl,
    rBase.WW8Cp2Fc(0)), m_rSBase(rBase), m_nAttrStart(-1), m_nAttrEnd(-1),
    m_bLineEnd(false),
    m_bComplex( (7 < rBase.m_pWw8Fib->m_nVersion) || rBase.m_pWw8Fib->m_fComplex )
{
    ResetAttrStartEnd();

    if (m_rSBase.m_pPiecePLCF)
        m_pPcd.reset( new WW8PLCFx_PCD(GetFIB(), rBase.m_pPiecePLCF.get(), 0, IsSevenMinus(GetFIBVersion())) );

    /*
    Make a copy of the piece attributes for so that the calls to HasSprm on a
    Fc_FKP will be able to take into account the current piece attributes,
    despite the fact that such attributes can only be found through a cp based
    mechanism.
    */
    if (m_pPcd)
    {
        m_pPCDAttrs.reset( m_rSBase.m_pPLCFx_PCDAttrs ? new WW8PLCFx_PCDAttrs(
            *m_rSBase.m_pWw8Fib, m_pPcd.get(), &m_rSBase) : nullptr);
    }

    m_pPieceIter = m_rSBase.m_pPieceIter.get();
}

WW8PLCFx_Cp_FKP::~WW8PLCFx_Cp_FKP()
{
}

void WW8PLCFx_Cp_FKP::ResetAttrStartEnd()
{
    m_nAttrStart = -1;
    m_nAttrEnd   = -1;
    m_bLineEnd   = false;
}

sal_uInt32 WW8PLCFx_Cp_FKP::GetPCDIdx() const
{
    return m_pPcd ? m_pPcd->GetIdx() : 0;
}

bool WW8PLCFx_Cp_FKP::SeekPos(WW8_CP nCpPos)
{
    if( m_pPcd )  // Complex
    {
        if( !m_pPcd->SeekPos( nCpPos ) )  // set piece
            return false;
        if (m_pPCDAttrs && !m_pPCDAttrs->GetIter()->SeekPos(nCpPos))
            return false;
        return WW8PLCFx_Fc_FKP::SeekPos(m_pPcd->CurrentPieceStartCp2Fc(nCpPos));
    }
                                    // NO piece table !!!
    return WW8PLCFx_Fc_FKP::SeekPos( m_rSBase.WW8Cp2Fc(nCpPos) );
}

WW8_CP WW8PLCFx_Cp_FKP::Where()
{
    WW8_FC nFc = WW8PLCFx_Fc_FKP::Where();
    if( m_pPcd )
        return m_pPcd->CurrentPieceStartFc2Cp( nFc ); // identify piece
    return m_rSBase.WW8Fc2Cp( nFc );      // NO piece table !!!
}

void WW8PLCFx_Cp_FKP::GetSprms(WW8PLCFxDesc* p)
{
    WW8_CP nOrigCp = p->nStartPos;

    if (!GetDirty())        //Normal case
    {
        p->pMemPos = WW8PLCFx_Fc_FKP::GetSprmsAndPos(p->nStartPos, p->nEndPos,
            p->nSprmsLen);
    }
    else
    {
        /*
        For the odd case where we have a location in a fastsaved file which
        does not have an entry in the FKP, perhaps its para end is in the next
        piece, or perhaps the cp just doesn't exist at all in this document.
        AdvSprm doesn't know so it sets the PLCF as dirty and we figure out
        in this method what the situation is

        It doesn't exist then the piece iterator will not be able to find it.
        Otherwise our cool fastsave algorithm can be brought to bear on the
        problem.
        */
        if( !m_pPieceIter )
            return;
        const sal_uInt32 nOldPos = m_pPieceIter->GetIdx();
        bool bOk = m_pPieceIter->SeekPos(nOrigCp);
        m_pPieceIter->SetIdx(nOldPos);
        if (!bOk)
            return;
    }

    if( m_pPcd )  // piece table available
    {
        // Init ( no ++ called, yet )
        if( (m_nAttrStart >  m_nAttrEnd) || (m_nAttrStart == -1) )
        {
            p->bRealLineEnd = (m_ePLCF == PAP);

            if ( ((m_ePLCF == PAP ) || (m_ePLCF == CHP)) && (nOrigCp != WW8_CP_MAX) )
            {
                bool bIsUnicode=false;
                /*
                To find the end of a paragraph for a character in a
                complex format file.

                It is necessary to know the piece that contains the
                character and the FC assigned to the character.
                */

                //We set the piece iterator to the piece that contains the
                //character, now we have the correct piece for this character
                sal_uInt32 nOldPos = m_pPieceIter->GetIdx();
                p->nStartPos = nOrigCp;
                m_pPieceIter->SeekPos( p->nStartPos);

                //This is the FC assigned to the character, but we already
                //have the result of the next stage, so we can skip this step
                //WW8_FC nStartFc = rSBase.WW8Cp2Fc(p->nStartPos, &bIsUnicode);

                /*
                Using the FC of the character, first search the FKP that
                describes the character to find the smallest FC in the rgfc
                that is larger than the character FC.
                */
                //But the search has already been done, the next largest FC is
                //p->nEndPos.
                WW8_FC nOldEndPos = p->nEndPos;

                /*
                If the FC found in the FKP is less than or equal to the limit
                FC of the piece, the end of the paragraph that contains the
                character is at the FKP FC minus 1.
                */
                WW8_CP nCpStart, nCpEnd;
                void* pData=nullptr;
                bool bOk = m_pPieceIter->Get(nCpStart, nCpEnd, pData);

                if (!bOk)
                {
                    m_pPieceIter->SetIdx(nOldPos);
                    return;
                }

                WW8_FC nLimitFC = SVBT32ToUInt32( static_cast<WW8_PCD*>(pData)->fc );
                WW8_FC nBeginLimitFC = nLimitFC;
                if (IsEightPlus(GetFIBVersion()))
                {
                    nBeginLimitFC =
                        WW8PLCFx_PCD::TransformPieceAddress(nLimitFC,
                        bIsUnicode);
                }

                WW8_CP nCpLen;
                bool bFail = o3tl::checked_sub(nCpEnd, nCpStart, nCpLen);
                if (bFail)
                {
                    SAL_WARN("sw.ww8", "broken offset, ignoring");
                    p->nStartPos = p->nEndPos = WW8_FC_MAX;
                    m_pPieceIter->SetIdx(nOldPos);
                    return;
                }

                if (bIsUnicode)
                {
                    bFail = o3tl::checked_multiply<WW8_CP>(nCpLen, 2, nCpLen);
                    if (bFail)
                    {
                        SAL_WARN("sw.ww8", "broken offset, ignoring");
                        p->nStartPos = p->nEndPos = WW8_FC_MAX;
                        m_pPieceIter->SetIdx(nOldPos);
                        return;
                    }
                }

                bFail = o3tl::checked_add(nBeginLimitFC, nCpLen, nLimitFC);
                if (bFail)
                {
                    SAL_WARN("sw.ww8", "broken offset, ignoring");
                    p->nStartPos = p->nEndPos = WW8_FC_MAX;
                    m_pPieceIter->SetIdx(nOldPos);
                    return;
                }

                if (nOldEndPos <= nLimitFC)
                {
                    bFail = o3tl::checked_sub(nLimitFC, nOldEndPos, nCpLen);
                    if (bFail)
                    {
                        SAL_WARN("sw.ww8", "broken offset, ignoring");
                        p->nStartPos = p->nEndPos = WW8_FC_MAX;
                        m_pPieceIter->SetIdx(nOldPos);
                        return;
                    }

                    nCpLen /= (bIsUnicode ? 2 : 1);

                    bFail = o3tl::checked_sub(nCpEnd, nCpLen, p->nEndPos);
                    if (bFail)
                    {
                        SAL_WARN("sw.ww8", "broken offset, ignoring");
                        p->nStartPos = p->nEndPos = WW8_FC_MAX;
                        m_pPieceIter->SetIdx(nOldPos);
                        return;
                    }
                }
                else
                {
                    p->nEndPos = nCpEnd;
                    if (m_ePLCF != CHP)
                    {
                        /*
                        If the FKP FC that was found was greater than the FC
                        of the end of the piece, scan piece by piece toward
                        the end of the document until a piece is found that
                        contains a  paragraph end mark.
                        */

                        /*
                        It's possible to check if a piece contains a paragraph
                        mark by using the FC of the beginning of the piece to
                        search in the FKPs for the smallest FC in the FKP rgfc
                        that is greater than the FC of the beginning of the
                        piece. If the FC found is less than or equal to the
                        limit FC of the piece, then the character that ends
                        the paragraph is the character immediately before the
                        FKP fc
                        */

                        m_pPieceIter->advance();

                        for (;m_pPieceIter->GetIdx() < m_pPieceIter->GetIMax();
                            m_pPieceIter->advance())
                        {
                            if( !m_pPieceIter->Get( nCpStart, nCpEnd, pData ) )
                            {
                                OSL_ENSURE( false, "piece iter broken!" );
                                break;
                            }
                            bIsUnicode = false;
                            sal_Int32 nFcStart=SVBT32ToUInt32(static_cast<WW8_PCD*>(pData)->fc);

                            if (IsEightPlus(GetFIBVersion()))
                            {
                                nFcStart =
                                    WW8PLCFx_PCD::TransformPieceAddress(
                                    nFcStart,bIsUnicode );
                            }

                            bFail = o3tl::checked_sub(nCpEnd, nCpStart, nCpLen);
                            if (bFail)
                            {
                                SAL_WARN("sw.ww8", "broken offset, ignoring");
                                p->nStartPos = p->nEndPos = WW8_FC_MAX;
                                continue;
                            }

                            if (bIsUnicode)
                            {
                                bFail = o3tl::checked_multiply<WW8_CP>(nCpLen, 2, nCpLen);
                                if (bFail)
                                {
                                    SAL_WARN("sw.ww8", "broken offset, ignoring");
                                    p->nStartPos = p->nEndPos = WW8_FC_MAX;
                                    continue;
                                }
                            }

                            bFail = o3tl::checked_add(nFcStart, nCpLen, nLimitFC);
                            if (bFail)
                            {
                                SAL_WARN("sw.ww8", "broken offset, ignoring");
                                p->nStartPos = p->nEndPos = WW8_FC_MAX;
                                continue;
                            }

                            //if it doesn't exist, skip it
                            if (!SeekPos(nCpStart))
                                continue;

                            WW8_FC nOne,nSmallest;
                            p->pMemPos = WW8PLCFx_Fc_FKP::GetSprmsAndPos(nOne,
                                nSmallest, p->nSprmsLen);

                            if (nSmallest <= nLimitFC)
                            {
                                WW8_CP nCpDiff;
                                bFail = o3tl::checked_sub(nLimitFC, nSmallest, nCpDiff);
                                if (bFail)
                                {
                                    SAL_WARN("sw.ww8", "broken offset, ignoring");
                                    p->nStartPos = p->nEndPos = WW8_FC_MAX;
                                    continue;
                                }
                                if (bIsUnicode)
                                    nCpDiff /= 2;

                                WW8_CP nEndPos;
                                bFail = o3tl::checked_sub(nCpEnd, nCpDiff, nEndPos);
                                if (bFail)
                                {
                                    SAL_WARN("sw.ww8", "broken offset, ignoring");
                                    p->nStartPos = p->nEndPos = WW8_FC_MAX;
                                    continue;
                                }

                                OSL_ENSURE(nEndPos >= p->nStartPos, "EndPos before StartPos");

                                if (nEndPos >= p->nStartPos)
                                    p->nEndPos = nEndPos;

                                break;
                            }
                        }
                    }
                }
                m_pPieceIter->SetIdx( nOldPos );
            }
            else
                WW8PLCFx_PCD::CurrentPieceFc2Cp( p->nStartPos, p->nEndPos,&m_rSBase );
        }
        else
        {
            p->nStartPos = m_nAttrStart;
            p->nEndPos = m_nAttrEnd;
            p->bRealLineEnd = m_bLineEnd;
        }
    }
    else        // NO piece table !!!
    {
        p->nStartPos = m_rSBase.WW8Fc2Cp( p->nStartPos );
        p->nEndPos   = m_rSBase.WW8Fc2Cp( p->nEndPos );
        p->bRealLineEnd = m_ePLCF == PAP;
    }
}

void WW8PLCFx_Cp_FKP::advance()
{
    WW8PLCFx_Fc_FKP::advance();
    // !pPcd: emergency break
    if ( !m_bComplex || !m_pPcd )
        return;

    if( GetPCDIdx() >= m_pPcd->GetIMax() )           // End of PLCF
    {
        m_nAttrStart = m_nAttrEnd = WW8_CP_MAX;
        return;
    }

    sal_Int32 nFkpLen;                               // Fkp entry
    // get Fkp entry
    WW8PLCFx_Fc_FKP::GetSprmsAndPos(m_nAttrStart, m_nAttrEnd, nFkpLen);

    WW8PLCFx_PCD::CurrentPieceFc2Cp( m_nAttrStart, m_nAttrEnd, &m_rSBase );
    m_bLineEnd = (m_ePLCF == PAP);
}

WW8PLCFx_SEPX::WW8PLCFx_SEPX(SvStream* pSt, SvStream* pTableSt,
    const WW8Fib& rFib, WW8_CP nStartCp)
    : WW8PLCFx(rFib, true), maSprmParser(rFib),
    m_pStrm(pSt), m_nArrMax(256), m_nSprmSiz(0)
{
    if (rFib.m_lcbPlcfsed)
        m_pPLCF.reset( new WW8PLCF(*pTableSt, rFib.m_fcPlcfsed, rFib.m_lcbPlcfsed,
                                 GetFIBVersion() <= ww::eWW2 ? 6 : 12, nStartCp) );

    m_pSprms.reset( new sal_uInt8[m_nArrMax] );     // maximum length
}

WW8PLCFx_SEPX::~WW8PLCFx_SEPX()
{
}

sal_uInt32 WW8PLCFx_SEPX::GetIdx() const
{
    return m_pPLCF ? m_pPLCF->GetIdx() : 0;
}

void WW8PLCFx_SEPX::SetIdx(sal_uInt32 nIdx)
{
    if( m_pPLCF ) m_pPLCF->SetIdx( nIdx );
}

bool WW8PLCFx_SEPX::SeekPos(WW8_CP nCpPos)
{
    return m_pPLCF && m_pPLCF->SeekPos( nCpPos );
}

WW8_CP WW8PLCFx_SEPX::Where()
{
    return m_pPLCF ? m_pPLCF->Where() : 0;
}

void WW8PLCFx_SEPX::GetSprms(WW8PLCFxDesc* p)
{
    if( !m_pPLCF ) return;

    void* pData;

    p->bRealLineEnd = false;
    if (!m_pPLCF->Get( p->nStartPos, p->nEndPos, pData ))
    {
        p->nStartPos = p->nEndPos = WW8_CP_MAX;       // PLCF completely processed
        p->pMemPos = nullptr;
        p->nSprmsLen = 0;
    }
    else
    {
        sal_uInt32 nPo =  SVBT32ToUInt32( static_cast<sal_uInt8*>(pData)+2 );
        if (nPo == 0xFFFFFFFF || !checkSeek(*m_pStrm, nPo))
        {
            p->nStartPos = p->nEndPos = WW8_CP_MAX;   // Sepx empty
            p->pMemPos = nullptr;
            p->nSprmsLen = 0;
        }
        else
        {
            // read len
            if (GetFIBVersion() <= ww::eWW2)    // eWW6 ?, docs say yes, but...
            {
                sal_uInt8 nSiz(0);
                m_pStrm->ReadUChar( nSiz );
                m_nSprmSiz = nSiz;
            }
            else
            {
                m_pStrm->ReadUInt16( m_nSprmSiz );
            }

            std::size_t nRemaining = m_pStrm->remainingSize();
            if (m_nSprmSiz > nRemaining)
                m_nSprmSiz = nRemaining;

            if( m_nSprmSiz > m_nArrMax )
            {               // does not fit
                m_nArrMax = m_nSprmSiz;                 // Get more memory
                m_pSprms.reset( new sal_uInt8[m_nArrMax] );
            }
            m_nSprmSiz = m_pStrm->ReadBytes(m_pSprms.get(), m_nSprmSiz); // read Sprms

            p->nSprmsLen = m_nSprmSiz;
            p->pMemPos = m_pSprms.get();                    // return Position
        }
    }
}

void WW8PLCFx_SEPX::advance()
{
    if (m_pPLCF)
        m_pPLCF->advance();
}

SprmResult WW8PLCFx_SEPX::HasSprm(sal_uInt16 nId) const
{
    return HasSprm(nId, m_pSprms.get(), m_nSprmSiz);
}

SprmResult WW8PLCFx_SEPX::HasSprm( sal_uInt16 nId, const sal_uInt8*  pOtherSprms,
    tools::Long nOtherSprmSiz ) const
{
    SprmResult aRet;
    if (m_pPLCF)
    {
        WW8SprmIter aIter(pOtherSprms, nOtherSprmSiz, maSprmParser);
        aRet = aIter.FindSprm(nId, /*bFindFirst=*/true);
    }
    return aRet;
}

bool WW8PLCFx_SEPX::Find4Sprms(sal_uInt16 nId1,sal_uInt16 nId2,sal_uInt16 nId3,sal_uInt16 nId4,
    SprmResult& r1, SprmResult& r2, SprmResult& r3, SprmResult& r4) const
{
    if( !m_pPLCF )
        return false;

    bool bFound = false;

    sal_uInt8* pSp = m_pSprms.get();
    size_t i = 0;
    while (i + maSprmParser.MinSprmLen() <= m_nSprmSiz)
    {
        // Sprm found?
        const sal_uInt16 nCurrentId = maSprmParser.GetSprmId(pSp);
        sal_Int32 nRemLen = m_nSprmSiz - i;
        const sal_Int32 x = maSprmParser.GetSprmSize(nCurrentId, pSp, nRemLen);
        bool bValid = x <= nRemLen;
        if (!bValid)
        {
            SAL_WARN("sw.ww8", "sprm longer than remaining bytes, doc or parser is wrong");
            break;
        }
        bool bOk = true;
        if( nCurrentId  == nId1 )
        {
            sal_Int32 nFixedLen = maSprmParser.DistanceToData(nId1);
            r1 = SprmResult(pSp + nFixedLen, x - nFixedLen);
        }
        else if( nCurrentId  == nId2 )
        {
            sal_Int32 nFixedLen = maSprmParser.DistanceToData(nId2);
            r2 = SprmResult(pSp + nFixedLen, x - nFixedLen);
        }
        else if( nCurrentId  == nId3 )
        {
            sal_Int32 nFixedLen = maSprmParser.DistanceToData(nId3);
            r3 = SprmResult(pSp + nFixedLen, x - nFixedLen);
        }
        else if( nCurrentId  == nId4 )
        {
            sal_Int32 nFixedLen = maSprmParser.DistanceToData(nId4);
            r4 = SprmResult(pSp + nFixedLen, x - nFixedLen);
        }
        else
            bOk = false;
        bFound |= bOk;
        // increment pointer so that it points to next SPRM
        i += x;
        pSp += x;
    }
    return bFound;
}

SprmResult WW8PLCFx_SEPX::HasSprm( sal_uInt16 nId, sal_uInt8 n2nd ) const
{
    SprmResult aRet;
    if (m_pPLCF)
    {
        WW8SprmIter aIter(m_pSprms.get(), m_nSprmSiz, maSprmParser);
        aRet = aIter.FindSprm(nId, /*bFindFirst=*/true, &n2nd);
    }
    return aRet;
}

WW8PLCFx_SubDoc::WW8PLCFx_SubDoc(SvStream* pSt, const WW8Fib& rFib,
    WW8_CP nStartCp, tools::Long nFcRef, tools::Long nLenRef, tools::Long nFcText, tools::Long nLenText,
    tools::Long nStruct)
    : WW8PLCFx(rFib, true)
{
    if( nLenRef && nLenText )
    {
        m_pRef.reset(new WW8PLCF(*pSt, nFcRef, nLenRef, nStruct, nStartCp));
        m_pText.reset(new WW8PLCF(*pSt, nFcText, nLenText, 0, nStartCp));
    }
}

WW8PLCFx_SubDoc::~WW8PLCFx_SubDoc()
{
    m_pRef.reset();
    m_pText.reset();
}

sal_uInt32 WW8PLCFx_SubDoc::GetIdx() const
{
    // Probably pText ... no need for it
    if( m_pRef )
        return ( m_pRef->GetIdx() << 16 | m_pText->GetIdx() );
    return 0;
}

void WW8PLCFx_SubDoc::SetIdx(sal_uInt32 nIdx)
{
    if( m_pRef )
    {
        m_pRef->SetIdx( nIdx >> 16 );
        // Probably pText ... no need for it
        m_pText->SetIdx( nIdx & 0xFFFF );
    }
}

bool WW8PLCFx_SubDoc::SeekPos( WW8_CP nCpPos )
{
    return m_pRef && m_pRef->SeekPos( nCpPos );
}

WW8_CP WW8PLCFx_SubDoc::Where()
{
    return m_pRef ? m_pRef->Where() : WW8_CP_MAX;
}

void WW8PLCFx_SubDoc::GetSprms(WW8PLCFxDesc* p)
{
    p->nStartPos = p->nEndPos = WW8_CP_MAX;
    p->pMemPos = nullptr;
    p->nSprmsLen = 0;
    p->bRealLineEnd = false;

    if (!m_pRef)
        return;

    sal_uInt32 nNr = m_pRef->GetIdx();

    void *pData;
    WW8_CP nFoo;
    if (!m_pRef->Get(p->nStartPos, nFoo, pData))
    {
        p->nEndPos = p->nStartPos = WW8_CP_MAX;
        return;
    }

    if (o3tl::checked_add<WW8_CP>(p->nStartPos, 1, p->nEndPos))
    {
        SAL_WARN("sw.ww8", "broken offset, ignoring");
        p->nEndPos = p->nStartPos = WW8_CP_MAX;
        return;
    }

    if (!m_pText)
        return;

    m_pText->SetIdx(nNr);

    if (!m_pText->Get(p->nCp2OrIdx, p->nSprmsLen, pData))
    {
        p->nEndPos = p->nStartPos = WW8_CP_MAX;
        p->nSprmsLen = 0;
        return;
    }

    if (p->nCp2OrIdx < 0 || p->nCp2OrIdx > p->nSprmsLen)
    {
        SAL_WARN("sw.ww8", "Document has invalid Cp or Idx, ignoring it");
        p->nEndPos = p->nStartPos = WW8_CP_MAX;
        p->nSprmsLen = 0;
        return;
    }

    p->nSprmsLen -= p->nCp2OrIdx;
}

void WW8PLCFx_SubDoc::advance()
{
    if (m_pRef && m_pText)
    {
        m_pRef->advance();
        m_pText->advance();
    }
}

// fields
WW8PLCFx_FLD::WW8PLCFx_FLD( SvStream* pSt, const WW8Fib& rMyFib, short nType)
    : WW8PLCFx(rMyFib, true), m_rFib(rMyFib)
{
    WW8_FC nFc;
    sal_Int32 nLen;

    switch( nType )
    {
    case MAN_HDFT:
        nFc = m_rFib.m_fcPlcffldHdr;
        nLen = m_rFib.m_lcbPlcffldHdr;
        break;
    case MAN_FTN:
        nFc = m_rFib.m_fcPlcffldFootnote;
        nLen = m_rFib.m_lcbPlcffldFootnote;
        break;
    case MAN_EDN:
        nFc = m_rFib.m_fcPlcffldEdn;
        nLen = m_rFib.m_lcbPlcffldEdn;
        break;
    case MAN_AND:
        nFc = m_rFib.m_fcPlcffldAtn;
        nLen = m_rFib.m_lcbPlcffldAtn;
        break;
    case MAN_TXBX:
        nFc = m_rFib.m_fcPlcffldTxbx;
        nLen = m_rFib.m_lcbPlcffldTxbx;
        break;
    case MAN_TXBX_HDFT:
        nFc = m_rFib.m_fcPlcffldHdrTxbx;
        nLen = m_rFib.m_lcbPlcffldHdrTxbx;
        break;
    default:
        nFc = m_rFib.m_fcPlcffldMom;
        nLen = m_rFib.m_lcbPlcffldMom;
        break;
    }

    if( nLen )
        m_pPLCF.reset( new WW8PLCFspecial( pSt, nFc, nLen, 2 ) );
}

WW8PLCFx_FLD::~WW8PLCFx_FLD()
{
}

sal_uInt32 WW8PLCFx_FLD::GetIdx() const
{
    return m_pPLCF ? m_pPLCF->GetIdx() : 0;
}

void WW8PLCFx_FLD::SetIdx(sal_uInt32 nIdx)
{
    if( m_pPLCF )
        m_pPLCF->SetIdx( nIdx );
}

bool WW8PLCFx_FLD::SeekPos(WW8_CP nCpPos)
{
    return m_pPLCF && m_pPLCF->SeekPosExact( nCpPos );
}

WW8_CP WW8PLCFx_FLD::Where()
{
    return m_pPLCF ? m_pPLCF->Where() : WW8_CP_MAX;
}

bool WW8PLCFx_FLD::StartPosIsFieldStart()
{
    void* pData;
    sal_Int32 nTest;
    return m_pPLCF && m_pPLCF->Get(nTest, pData) && ((static_cast<sal_uInt8*>(pData)[0] & 0x1f) == 0x13);
}

bool WW8PLCFx_FLD::EndPosIsFieldEnd(WW8_CP& nCP)
{
    bool bRet = false;

    if (m_pPLCF)
    {
        tools::Long n = m_pPLCF->GetIdx();

        m_pPLCF->advance();

        void* pData;
        sal_Int32 nTest;
        if ( m_pPLCF->Get(nTest, pData) && ((static_cast<sal_uInt8*>(pData)[0] & 0x1f) == 0x15) )
        {
            nCP = nTest;
            bRet = true;
        }

        m_pPLCF->SetIdx(n);
    }

    return bRet;
}

void WW8PLCFx_FLD::GetSprms(WW8PLCFxDesc* p)
{
    p->nStartPos = p->nEndPos = WW8_CP_MAX;
    p->pMemPos = nullptr;
    p->nSprmsLen = 0;
    p->bRealLineEnd = false;

    if (!m_pPLCF)
    {
        p->nStartPos = WW8_CP_MAX;                    // there are no fields
        return;
    }

    tools::Long n = m_pPLCF->GetIdx();

    sal_Int32 nP;
    void *pData;
    if (!m_pPLCF->Get(nP, pData))             // end of PLCFspecial?
    {
        p->nStartPos = WW8_CP_MAX;            // PLCF completely processed
        return;
    }

    p->nStartPos = nP;

    m_pPLCF->advance();
    if (!m_pPLCF->Get(nP, pData))             // end of PLCFspecial?
    {
        p->nStartPos = WW8_CP_MAX;            // PLCF completely processed
        return;
    }

    p->nEndPos = nP;

    m_pPLCF->SetIdx(n);

    p->nCp2OrIdx = m_pPLCF->GetIdx();
}

void WW8PLCFx_FLD::advance()
{
    SAL_WARN_IF(!m_pPLCF, "sw.ww8", "Call without PLCFspecial field");
    if( !m_pPLCF )
        return;
    m_pPLCF->advance();
}

bool WW8PLCFx_FLD::GetPara(tools::Long nIdx, WW8FieldDesc& rF)
{
    SAL_WARN_IF(!m_pPLCF, "sw.ww8", "Call without PLCFspecial field");
    if( !m_pPLCF )
        return false;

    tools::Long n = m_pPLCF->GetIdx();
    m_pPLCF->SetIdx(nIdx);

    bool bOk = WW8GetFieldPara(*m_pPLCF, rF);

    m_pPLCF->SetIdx(n);
    return bOk;
}

// WW8PLCF_Book
void WW8ReadSTTBF(bool bVer8, SvStream& rStrm, sal_uInt32 nStart, sal_Int32 nLen,
    sal_uInt16 nExtraLen, rtl_TextEncoding eCS, std::vector<OUString> &rArray,
    std::vector<ww::bytes>* pExtraArray, std::vector<OUString>* pValueArray)
{
    if (nLen==0)     // Handle Empty STTBF
        return;

    sal_uInt64 const nOldPos = rStrm.Tell();
    if (checkSeek(rStrm, nStart))
    {
        sal_uInt16 nLen2(0);
        rStrm.ReadUInt16( nLen2 ); // bVer67: total length of structure
                        // bVer8 : count of strings

        if( bVer8 )
        {
            sal_uInt16 nStrings(0);
            bool bUnicode = (0xFFFF == nLen2);
            if (bUnicode)
                rStrm.ReadUInt16( nStrings );
            else
                nStrings = nLen2;

            rStrm.ReadUInt16( nExtraLen );

            const size_t nMinStringLen = bUnicode ? sizeof(sal_uInt16) : sizeof(sal_uInt8);
            const size_t nMinRecordSize = nExtraLen + nMinStringLen;
            assert(nMinRecordSize != 0 && "impossible to be zero");
            const size_t nMaxPossibleStrings = rStrm.remainingSize() / nMinRecordSize;
            if (nStrings > nMaxPossibleStrings)
            {
                SAL_WARN("sw.ww8", "STTBF claims " << nStrings << " entries, but only " << nMaxPossibleStrings << " are possible");
                nStrings = nMaxPossibleStrings;
            }

            if (nExtraLen && nStrings)
            {
                const size_t nMaxExtraLen = (rStrm.remainingSize() - (nStrings * nMinStringLen)) / nStrings;
                if (nExtraLen > nMaxExtraLen)
                {
                    SAL_WARN("sw.ww8", "STTBF claims " << nMaxExtraLen << " extra len, but only " << nMaxExtraLen << " are possible");
                    nExtraLen = nMaxExtraLen;
                }
            }

            for (sal_uInt16 i=0; i < nStrings; ++i)
            {
                if (bUnicode)
                    rArray.push_back(read_uInt16_PascalString(rStrm));
                else
                {
                    OString aTmp = read_uInt8_lenPrefixed_uInt8s_ToOString(rStrm);
                    rArray.push_back(OStringToOUString(aTmp, eCS));
                }

                // Skip the extra data
                if (nExtraLen)
                {
                    if (pExtraArray)
                    {
                        ww::bytes extraData(nExtraLen);
                        rStrm.ReadBytes(extraData.data(), nExtraLen);
                        pExtraArray->push_back(extraData);
                    }
                    else
                        rStrm.SeekRel( nExtraLen );
                }
            }
            // read the value of the document variables, if requested.
            if (pValueArray)
            {
                for (sal_uInt16 i=0; i < nStrings; ++i)
                {
                    if( bUnicode )
                        pValueArray->push_back(read_uInt16_PascalString(rStrm));
                    else
                    {
                        OString aTmp = read_uInt8_lenPrefixed_uInt8s_ToOString(rStrm);
                        pValueArray->push_back(OStringToOUString(aTmp, eCS));
                    }
                }
            }
        }
        else
        {
            if( nLen2 != nLen )
            {
                OSL_ENSURE(nLen2 == nLen,
                    "Fib length and read length are different");
                if (nLen > SAL_MAX_UINT16)
                    nLen = SAL_MAX_UINT16;
                else if (nLen < 2 )
                    nLen = 2;
                nLen2 = o3tl::narrowing<sal_uInt16>(nLen);
            }
            sal_uLong nRead = 2;
            while (nRead < nLen2)
            {
                sal_uInt8 nBChar(0);
                rStrm.ReadUChar( nBChar );
                ++nRead;
                if (nBChar)
                {
                    OString aTmp = read_uInt8s_ToOString(rStrm, nBChar);
                    nRead += aTmp.getLength();
                    rArray.push_back(OStringToOUString(aTmp, eCS));
                }
                else
                    rArray.emplace_back();

                // Skip the extra data (for bVer67 versions this must come from
                // external knowledge)
                if (nExtraLen)
                {
                    if (pExtraArray)
                    {
                        ww::bytes extraData(nExtraLen);
                        rStrm.ReadBytes(extraData.data(), nExtraLen);
                        pExtraArray->push_back(extraData);
                    }
                    else
                        rStrm.SeekRel( nExtraLen );
                    nRead+=nExtraLen;
                }
            }
        }
    }
    rStrm.Seek(nOldPos);
}

WW8PLCFx_Book::WW8PLCFx_Book(SvStream* pTableSt, const WW8Fib& rFib)
    : WW8PLCFx(rFib, false), m_nIsEnd(0), m_nBookmarkId(1)
{
    if( !rFib.m_fcPlcfbkf || !rFib.m_lcbPlcfbkf || !rFib.m_fcPlcfbkl ||
        !rFib.m_lcbPlcfbkl || !rFib.m_fcSttbfbkmk || !rFib.m_lcbSttbfbkmk )
    {
        m_nIMax = 0;
    }
    else
    {
        m_pBook[0].reset( new WW8PLCFspecial(pTableSt,rFib.m_fcPlcfbkf,rFib.m_lcbPlcfbkf,4) );

        m_pBook[1].reset( new WW8PLCFspecial(pTableSt,rFib.m_fcPlcfbkl,rFib.m_lcbPlcfbkl,0) );

        rtl_TextEncoding eStructChrSet = WW8Fib::GetFIBCharset(rFib.m_chseTables, rFib.m_lid);

        WW8ReadSTTBF( (7 < rFib.m_nVersion), *pTableSt, rFib.m_fcSttbfbkmk,
            rFib.m_lcbSttbfbkmk, 0, eStructChrSet, m_aBookNames );

        m_nIMax = m_aBookNames.size();

        if( m_pBook[0]->GetIMax() < m_nIMax )   // Count of Bookmarks
            m_nIMax = m_pBook[0]->GetIMax();
        if( m_pBook[1]->GetIMax() < m_nIMax )
            m_nIMax = m_pBook[1]->GetIMax();
        m_aStatus.resize(m_nIMax);
    }
}

WW8PLCFx_Book::~WW8PLCFx_Book()
{
}

sal_uInt32 WW8PLCFx_Book::GetIdx() const
{
    return m_nIMax ? m_pBook[0]->GetIdx() : 0;
}

void WW8PLCFx_Book::SetIdx(sal_uInt32 nI)
{
    if( m_nIMax )
        m_pBook[0]->SetIdx( nI );
}

sal_uInt32 WW8PLCFx_Book::GetIdx2() const
{
    return m_nIMax ? ( m_pBook[1]->GetIdx() | ( m_nIsEnd ? 0x80000000 : 0 ) ) : 0;
}

void WW8PLCFx_Book::SetIdx2(sal_uInt32 nI)
{
    if( m_nIMax )
    {
        m_pBook[1]->SetIdx( nI & 0x7fffffff );
        m_nIsEnd = o3tl::narrowing<sal_uInt16>( ( nI >> 31 ) & 1 );  // 0 or 1
    }
}

bool WW8PLCFx_Book::SeekPos(WW8_CP nCpPos)
{
    if( !m_pBook[0] )
        return false;

    bool bOk = m_pBook[0]->SeekPosExact( nCpPos );
    bOk &= m_pBook[1]->SeekPosExact( nCpPos );
    m_nIsEnd = 0;

    return bOk;
}

WW8_CP WW8PLCFx_Book::Where()
{
    return m_pBook[m_nIsEnd]->Where();
}

tools::Long WW8PLCFx_Book::GetNoSprms( WW8_CP& rStart, WW8_CP& rEnd, sal_Int32& rLen )
{
    void* pData;
    rEnd = WW8_CP_MAX;
    rLen = 0;

    if (!m_pBook[0] || !m_pBook[1] || !m_nIMax || (m_pBook[m_nIsEnd]->GetIdx()) >= m_nIMax)
    {
        rStart = rEnd = WW8_CP_MAX;
        return -1;
    }

    (void)m_pBook[m_nIsEnd]->Get( rStart, pData );    // query position
    return m_pBook[m_nIsEnd]->GetIdx();
}

// The operator ++ has a pitfall: If 2 bookmarks adjoin each other,
// we should first go to the end of the first one
// and then to the beginning of the second one.
// But if 2 bookmarks with the length of 0 lie on top of each other,
// we *must* first find the start and end of each bookmark.
// The case of: ][
//               [...]
//              ][
// is not solved yet.
// Because I must jump back and forth in the start- and end-indices then.
// This would require one more index or bitfield to remember
// the already processed bookmarks.

void WW8PLCFx_Book::advance()
{
    if( !(m_pBook[0] && m_pBook[1] && m_nIMax) )
        return;

    (*m_pBook[m_nIsEnd]).advance();

    sal_uLong l0 = m_pBook[0]->Where();
    sal_uLong l1 = m_pBook[1]->Where();
    if( l0 < l1 )
        m_nIsEnd = 0;
    else if( l1 < l0 )
        m_nIsEnd = 1;
    else
    {
        const void * p = m_pBook[0]->GetData(m_pBook[0]->GetIdx());
        tools::Long nPairFor = (p == nullptr) ? 0 : SVBT16ToUInt16(*static_cast<SVBT16 const *>(p));
        if (nPairFor == m_pBook[1]->GetIdx())
            m_nIsEnd = 0;
        else
            m_nIsEnd = m_nIsEnd ? 0 : 1;
    }
}

tools::Long WW8PLCFx_Book::GetLen() const
{
    if( m_nIsEnd )
    {
        OSL_ENSURE( false, "Incorrect call (1) of PLCF_Book::GetLen()" );
        return 0;
    }
    void * p;
    WW8_CP nStartPos;
    if( !m_pBook[0]->Get( nStartPos, p ) )
    {
        OSL_ENSURE( false, "Incorrect call (2) of PLCF_Book::GetLen()" );
        return 0;
    }
    const sal_uInt16 nEndIdx = SVBT16ToUInt16( *static_cast<SVBT16*>(p) );
    tools::Long nNum = m_pBook[1]->GetPos( nEndIdx );
    nNum -= nStartPos;
    return nNum;
}

void WW8PLCFx_Book::SetStatus(sal_uInt16 nIndex, eBookStatus eStat)
{
    SAL_WARN_IF(nIndex >= m_nIMax, "sw.ww8",
                "bookmark index " << nIndex << " invalid");
    eBookStatus eStatus = m_aStatus.at(nIndex);
    m_aStatus[nIndex] = static_cast<eBookStatus>(eStatus | eStat);
}

eBookStatus WW8PLCFx_Book::GetStatus() const
{
    if (m_aStatus.empty())
        return BOOK_NORMAL;
    tools::Long nEndIdx = GetHandle();
    return ( nEndIdx < m_nIMax ) ? m_aStatus[nEndIdx] : BOOK_NORMAL;
}

tools::Long WW8PLCFx_Book::GetHandle() const
{
    if( !m_pBook[0] || !m_pBook[1] )
        return LONG_MAX;

    if( m_nIsEnd )
        return m_pBook[1]->GetIdx();
    else
    {
        if (const void* p = m_pBook[0]->GetData(m_pBook[0]->GetIdx()))
            return SVBT16ToUInt16( *static_cast<SVBT16 const *>(p) );
        else
            return LONG_MAX;
    }
}

OUString WW8PLCFx_Book::GetBookmark(tools::Long nStart,tools::Long nEnd, sal_uInt16 &nIndex)
{
    bool bFound = false;
    sal_uInt16 i = 0;
    if (m_pBook[0] && m_pBook[1])
    {
        WW8_CP nStartCurrent, nEndCurrent;
        while (sal::static_int_cast<decltype(m_aBookNames)::size_type>(i) < m_aBookNames.size())
        {
            void* p;
            sal_uInt16 nEndIdx;

            if( m_pBook[0]->GetData( i, nStartCurrent, p ) && p )
                nEndIdx = SVBT16ToUInt16( *static_cast<SVBT16*>(p) );
            else
            {
                OSL_ENSURE( false, "Bookmark-EndIdx not readable" );
                nEndIdx = i;
            }

            nEndCurrent = m_pBook[1]->GetPos( nEndIdx );

            if ((nStartCurrent >= nStart) && (nEndCurrent <= nEnd))
            {
                nIndex = i;
                bFound=true;
                break;
            }
            ++i;
        }
    }
    return bFound ? m_aBookNames[i] : OUString();
}

OUString WW8PLCFx_Book::GetUniqueBookmarkName(const OUString &rSuggestedName)
{
    OUString aRet(rSuggestedName.isEmpty() ? u"Unnamed"_ustr : rSuggestedName);
    size_t i = 0;
    while (i < m_aBookNames.size())
    {
        if (aRet == m_aBookNames[i])
        {
            sal_Int32 len = aRet.getLength();
            sal_Int32 p = len - 1;
            while (p > 0 && aRet[p] >= '0' && aRet[p] <= '9')
                --p;
            aRet = aRet.subView(0, p+1) + OUString::number(m_nBookmarkId++);
            i = 0; // start search from beginning
        }
        else
            ++i;
    }
    return aRet;
}

void WW8PLCFx_Book::MapName(OUString& rName)
{
    if( !m_pBook[0] || !m_pBook[1] )
        return;

    size_t i = 0;
    while (i < m_aBookNames.size())
    {
        if (rName.equalsIgnoreAsciiCase(m_aBookNames[i]))
        {
            rName = m_aBookNames[i];
            break;
        }
        ++i;
    }
}

const OUString* WW8PLCFx_Book::GetName() const
{
    const OUString *pRet = nullptr;
    if (!m_nIsEnd && (m_pBook[0]->GetIdx() < m_nIMax))
        pRet = &(m_aBookNames[m_pBook[0]->GetIdx()]);
    return pRet;
}

WW8PLCFx_AtnBook::WW8PLCFx_AtnBook(SvStream* pTableSt, const WW8Fib& rFib)
    : WW8PLCFx(rFib, /*bSprm=*/false),
    m_bIsEnd(false)
{
    if (!rFib.m_fcPlcfAtnbkf || !rFib.m_lcbPlcfAtnbkf || !rFib.m_fcPlcfAtnbkl || !rFib.m_lcbPlcfAtnbkl)
    {
        m_nIMax = 0;
    }
    else
    {
        m_pBook[0].reset( new WW8PLCFspecial(pTableSt, rFib.m_fcPlcfAtnbkf, rFib.m_lcbPlcfAtnbkf, 4) );
        m_pBook[1].reset( new WW8PLCFspecial(pTableSt, rFib.m_fcPlcfAtnbkl, rFib.m_lcbPlcfAtnbkl, 0) );

        m_nIMax = m_pBook[0]->GetIMax();
        if (m_pBook[1]->GetIMax() < m_nIMax)
            m_nIMax = m_pBook[1]->GetIMax();
    }
}

WW8PLCFx_AtnBook::~WW8PLCFx_AtnBook()
{
}

sal_uInt32 WW8PLCFx_AtnBook::GetIdx() const
{
    return m_nIMax ? m_pBook[0]->GetIdx() : 0;
}

void WW8PLCFx_AtnBook::SetIdx(sal_uInt32 nI)
{
    if( m_nIMax )
        m_pBook[0]->SetIdx( nI );
}

sal_uInt32 WW8PLCFx_AtnBook::GetIdx2() const
{
    if (m_nIMax)
        return m_pBook[1]->GetIdx() | ( m_bIsEnd ? 0x80000000 : 0 );
    else
        return 0;
}

void WW8PLCFx_AtnBook::SetIdx2(sal_uInt32 nI)
{
    if( m_nIMax )
    {
        m_pBook[1]->SetIdx( nI & 0x7fffffff );
        m_bIsEnd = static_cast<bool>(( nI >> 31 ) & 1);
    }
}

bool WW8PLCFx_AtnBook::SeekPos(WW8_CP nCpPos)
{
    if (!m_pBook[0])
        return false;

    bool bOk = m_pBook[0]->SeekPosExact(nCpPos);
    bOk &= m_pBook[1]->SeekPosExact(nCpPos);
    m_bIsEnd = false;

    return bOk;
}

WW8_CP WW8PLCFx_AtnBook::Where()
{
    return m_pBook[static_cast<int>(m_bIsEnd)]->Where();
}

tools::Long WW8PLCFx_AtnBook::GetNoSprms( WW8_CP& rStart, WW8_CP& rEnd, sal_Int32& rLen )
{
    void* pData;
    rEnd = WW8_CP_MAX;
    rLen = 0;

    if (!m_pBook[0] || !m_pBook[1] || !m_nIMax || (m_pBook[static_cast<int>(m_bIsEnd)]->GetIdx()) >= m_nIMax)
    {
        rStart = rEnd = WW8_CP_MAX;
        return -1;
    }

    (void)m_pBook[static_cast<int>(m_bIsEnd)]->Get(rStart, pData);
    return m_pBook[static_cast<int>(m_bIsEnd)]->GetIdx();
}

void WW8PLCFx_AtnBook::advance()
{
    if( !(m_pBook[0] && m_pBook[1] && m_nIMax) )
        return;

    (*m_pBook[static_cast<int>(m_bIsEnd)]).advance();

    sal_uLong l0 = m_pBook[0]->Where();
    sal_uLong l1 = m_pBook[1]->Where();
    if( l0 < l1 )
        m_bIsEnd = false;
    else if( l1 < l0 )
        m_bIsEnd = true;
    else
    {
        const void * p = m_pBook[0]->GetData(m_pBook[0]->GetIdx());
        tools::Long nPairFor = (p == nullptr) ? 0 : SVBT16ToUInt16(*static_cast<SVBT16 const *>(p));
        if (nPairFor == m_pBook[1]->GetIdx())
            m_bIsEnd = false;
        else
            m_bIsEnd = !m_bIsEnd;
    }
}

tools::Long WW8PLCFx_AtnBook::getHandle() const
{
    if (!m_pBook[0] || !m_pBook[1])
        return LONG_MAX;

    if (m_bIsEnd)
        return m_pBook[1]->GetIdx();
    else
    {
        if (const void* p = m_pBook[0]->GetData(m_pBook[0]->GetIdx()))
            return SVBT16ToUInt16(*static_cast<const SVBT16*>(p));
        else
            return LONG_MAX;
    }
}

bool WW8PLCFx_AtnBook::getIsEnd() const
{
    return m_bIsEnd;
}

WW8PLCFx_FactoidBook::WW8PLCFx_FactoidBook(SvStream* pTableSt, const WW8Fib& rFib)
    : WW8PLCFx(rFib, /*bSprm=*/false),
    m_bIsEnd(false)
{
    if (!rFib.m_fcPlcfBkfFactoid || !rFib.m_lcbPlcfBkfFactoid || !rFib.m_fcPlcfBklFactoid || !rFib.m_lcbPlcfBklFactoid)
    {
        m_nIMax = 0;
    }
    else
    {
        m_pBook[0].reset(new WW8PLCFspecial(pTableSt, rFib.m_fcPlcfBkfFactoid, rFib.m_lcbPlcfBkfFactoid, 6));
        m_pBook[1].reset(new WW8PLCFspecial(pTableSt, rFib.m_fcPlcfBklFactoid, rFib.m_lcbPlcfBklFactoid, 4));

        m_nIMax = m_pBook[0]->GetIMax();
        if (m_pBook[1]->GetIMax() < m_nIMax)
            m_nIMax = m_pBook[1]->GetIMax();
    }
}

WW8PLCFx_FactoidBook::~WW8PLCFx_FactoidBook()
{
}

sal_uInt32 WW8PLCFx_FactoidBook::GetIdx() const
{
    return m_nIMax ? m_pBook[0]->GetIdx() : 0;
}

void WW8PLCFx_FactoidBook::SetIdx(sal_uInt32 nI)
{
    if (m_nIMax)
        m_pBook[0]->SetIdx(nI);
}

sal_uInt32 WW8PLCFx_FactoidBook::GetIdx2() const
{
    if (m_nIMax)
        return m_pBook[1]->GetIdx() | (m_bIsEnd ? 0x80000000 : 0);
    else
        return 0;
}

void WW8PLCFx_FactoidBook::SetIdx2(sal_uInt32 nI)
{
    if (m_nIMax)
    {
        m_pBook[1]->SetIdx(nI & 0x7fffffff);
        m_bIsEnd = static_cast<bool>((nI >> 31) & 1);
    }
}

bool WW8PLCFx_FactoidBook::SeekPos(WW8_CP nCpPos)
{
    if (!m_pBook[0])
        return false;

    bool bOk = m_pBook[0]->SeekPosExact(nCpPos);
    bOk &= m_pBook[1]->SeekPosExact(nCpPos);
    m_bIsEnd = false;

    return bOk;
}

WW8_CP WW8PLCFx_FactoidBook::Where()
{
    return m_pBook[static_cast<int>(m_bIsEnd)]->Where();
}

tools::Long WW8PLCFx_FactoidBook::GetNoSprms(WW8_CP& rStart, WW8_CP& rEnd, sal_Int32& rLen)
{
    void* pData;
    rEnd = WW8_CP_MAX;
    rLen = 0;

    if (!m_pBook[0] || !m_pBook[1] || !m_nIMax || (m_pBook[static_cast<int>(m_bIsEnd)]->GetIdx()) >= m_nIMax)
    {
        rStart = rEnd = WW8_CP_MAX;
        return -1;
    }

    (void)m_pBook[static_cast<int>(m_bIsEnd)]->Get(rStart, pData);
    return m_pBook[static_cast<int>(m_bIsEnd)]->GetIdx();
}

void WW8PLCFx_FactoidBook::advance()
{
    if (!(m_pBook[0] && m_pBook[1] && m_nIMax))
        return;

    (*m_pBook[static_cast<int>(m_bIsEnd)]).advance();

    sal_uLong l0 = m_pBook[0]->Where();
    sal_uLong l1 = m_pBook[1]->Where();
    if (l0 < l1)
        m_bIsEnd = false;
    else if (l1 < l0)
        m_bIsEnd = true;
    else
    {
        const void * p = m_pBook[0]->GetData(m_pBook[0]->GetIdx());
        tools::Long nPairFor = (p == nullptr) ? 0 : SVBT16ToUInt16(*static_cast<SVBT16 const *>(p));
        if (nPairFor == m_pBook[1]->GetIdx())
            m_bIsEnd = false;
        else
            m_bIsEnd = !m_bIsEnd;
    }
}

tools::Long WW8PLCFx_FactoidBook::getHandle() const
{
    if (!m_pBook[0] || !m_pBook[1])
        return LONG_MAX;

    if (m_bIsEnd)
        return m_pBook[1]->GetIdx();
    else
    {
        if (const void* p = m_pBook[0]->GetData(m_pBook[0]->GetIdx()))
            return SVBT16ToUInt16(*static_cast<const SVBT16*>(p));
        else
            return LONG_MAX;
    }
}

bool WW8PLCFx_FactoidBook::getIsEnd() const
{
    return m_bIsEnd;
}

// In the end of a paragraph in WW6 the attribute extends after the <CR>.
// This will be reset by one character to be used with SW,
// if we don't expect trouble thereby.
void WW8PLCFMan::AdjustEnds( WW8PLCFxDesc& rDesc )
{
    // might be necessary to do this for pChp and/or pSep as well,
    // but its definitely the case for paragraphs that EndPos > StartPos
    // for a well formed paragraph as those always have a paragraph
    // <cr> in them
    if (&rDesc == m_pPap && rDesc.bRealLineEnd)
    {
        if (rDesc.nStartPos == rDesc.nEndPos && rDesc.nEndPos != WW8_CP_MAX)
        {
            SAL_WARN("sw.ww8", "WW8PLCFxDesc End same as Start, abandoning to avoid looping");
            rDesc.nEndPos = WW8_CP_MAX;
        }
    }

    //Store old end position for supercool new property finder that uses
    //cp instead of fc's as nature intended
    rDesc.nOrigEndPos = rDesc.nEndPos;
    rDesc.nOrigStartPos = rDesc.nStartPos;

    /*
     Normally given ^XXX{para end}^ we don't actually insert a para end
     character into the document, so we clip the para end property one to the
     left to make the para properties end when the paragraph text does. In a
     drawing textbox we actually do insert a para end character, so we don't
     clip it. Making the para end properties end after the para end char.
    */
    if (GetDoingDrawTextBox())
        return;

    if ( (&rDesc == m_pPap) && rDesc.bRealLineEnd )
    {
        if ( m_pPap->nEndPos != WW8_CP_MAX )    // Para adjust
        {
            m_nLineEnd = m_pPap->nEndPos;// nLineEnd points *after* the <CR>
            m_pPap->nEndPos--;        // shorten paragraph end by one character

            // Is there already a sep end, which points to the current paragraph end?
            // Then we also must shorten by one character
            if( m_pSep->nEndPos == m_nLineEnd )
                m_pSep->nEndPos--;
        }
    }
    else if (&rDesc == m_pSep)
    {
        // Sep Adjust if end Char-Attr == paragraph end ...
        if( (rDesc.nEndPos == m_nLineEnd) && (rDesc.nEndPos > rDesc.nStartPos) )
            rDesc.nEndPos--;            // ... then shorten by one character
    }
}

void WW8PLCFxDesc::ReduceByOffset()
{
    SAL_WARN_IF(WW8_CP_MAX != nStartPos && nStartPos > nEndPos, "sw.ww8",
                "End " << nEndPos << " before Start " << nStartPos);

    if( nStartPos != WW8_CP_MAX )
    {
        /*
        ##516##,##517##
        Force the property change to happen at the beginning of this
        subdocument, same as in GetNewNoSprms, except that the target type is
        attributes attached to a piece that might span subdocument boundaries
        */
        if (nCpOfs > nStartPos)
            nStartPos = 0;
        else
            nStartPos -= nCpOfs;
    }
    if (nEndPos != WW8_CP_MAX)
    {
        if (nCpOfs > nEndPos)
        {
            SAL_WARN("sw.ww8", "broken subdocument piece entry");
            nEndPos = WW8_CP_MAX;
        }
        else
            nEndPos -= nCpOfs;
    }
}

void WW8PLCFMan::GetNewSprms( WW8PLCFxDesc& rDesc )
{
    rDesc.pPLCFx->GetSprms(&rDesc);
    rDesc.ReduceByOffset();

    rDesc.bFirstSprm = true;
    AdjustEnds( rDesc );
    rDesc.nOrigSprmsLen = rDesc.nSprmsLen;
}

void WW8PLCFMan::GetNewNoSprms( WW8PLCFxDesc& rDesc )
{
    rDesc.nCp2OrIdx = rDesc.pPLCFx->GetNoSprms(rDesc.nStartPos, rDesc.nEndPos,
        rDesc.nSprmsLen);

    SAL_WARN_IF(WW8_CP_MAX != rDesc.nStartPos && rDesc.nStartPos > rDesc.nEndPos, "sw.ww8",
                "End " << rDesc.nEndPos << " before Start " << rDesc.nStartPos);

    rDesc.ReduceByOffset();

    rDesc.bFirstSprm = true;
    rDesc.nOrigSprmsLen = rDesc.nSprmsLen;
}

sal_uInt16 WW8PLCFMan::GetId(const WW8PLCFxDesc* p) const
{
    sal_uInt16 nId = 0;        // Id = 0 for empty attributes

    if (p == m_pField)
        nId = eFLD;
    else if (p == m_pFootnote)
        nId = eFTN;
    else if (p == m_pEdn)
        nId = eEDN;
    else if (p == m_pAnd)
        nId = eAND;
    else if (p->nSprmsLen >= maSprmParser.MinSprmLen())
        nId = maSprmParser.GetSprmId(p->pMemPos);

    return nId;
}

WW8PLCFMan::WW8PLCFMan(const WW8ScannerBase* pBase, ManTypes nType, tools::Long nStartCp,
    bool bDoingDrawTextBox)
    : maSprmParser(*pBase->m_pWw8Fib),
    m_nLineEnd(WW8_CP_MAX),
    mbDoingDrawTextBox(bDoingDrawTextBox)
{
    m_pWwFib = pBase->m_pWw8Fib;

    m_nManType = nType;

    if( MAN_MAINTEXT == nType )
    {
        // search order of the attributes
        m_nPLCF = MAN_PLCF_COUNT;
        m_pField = &m_aD[0];
        m_pBkm = &m_aD[1];
        m_pEdn = &m_aD[2];
        m_pFootnote = &m_aD[3];
        m_pAnd = &m_aD[4];

        m_pPcd = pBase->m_pPLCFx_PCD ? &m_aD[5] : nullptr;
        //pPcdA index == pPcd index + 1
        m_pPcdA = pBase->m_pPLCFx_PCDAttrs ? &m_aD[6] : nullptr;

        m_pChp = &m_aD[7];
        m_pPap = &m_aD[8];
        m_pSep = &m_aD[9];
        m_pAtnBkm = &m_aD[10];
        m_pFactoidBkm = &m_aD[11];

        m_pSep->pPLCFx = pBase->m_pSepPLCF.get();
        m_pFootnote->pPLCFx = pBase->m_pFootnotePLCF.get();
        m_pEdn->pPLCFx = pBase->m_pEdnPLCF.get();
        m_pBkm->pPLCFx = pBase->m_pBook.get();
        m_pAnd->pPLCFx = pBase->m_pAndPLCF.get();
        m_pAtnBkm->pPLCFx = pBase->m_pAtnBook.get();
        m_pFactoidBkm->pPLCFx = pBase->m_pFactoidBook.get();

    }
    else
    {
        // search order of the attributes
        m_nPLCF = 7;
        m_pField = &m_aD[0];
        m_pBkm = pBase->m_pBook ? &m_aD[1] : nullptr;

        m_pPcd = pBase->m_pPLCFx_PCD ? &m_aD[2] : nullptr;
        //pPcdA index == pPcd index + 1
        m_pPcdA= pBase->m_pPLCFx_PCDAttrs ? &m_aD[3] : nullptr;

        m_pChp = &m_aD[4];
        m_pPap = &m_aD[5];
        m_pSep = &m_aD[6]; // Dummy

        m_pAnd = m_pAtnBkm = m_pFactoidBkm = m_pFootnote = m_pEdn = nullptr;     // not used at SpezText
    }

    m_pChp->pPLCFx = pBase->m_pChpPLCF.get();
    m_pPap->pPLCFx = pBase->m_pPapPLCF.get();
    if( m_pPcd )
        m_pPcd->pPLCFx = pBase->m_pPLCFx_PCD.get();
    if( m_pPcdA )
        m_pPcdA->pPLCFx= pBase->m_pPLCFx_PCDAttrs.get();
    if( m_pBkm )
        m_pBkm->pPLCFx = pBase->m_pBook.get();

    m_pMagicTables = pBase->m_pMagicTables.get();
    m_pSubdocs = pBase->m_pSubdocs.get();
    m_pExtendedAtrds = pBase->m_pExtendedAtrds.get();

    switch( nType )                 // field initialization
    {
        case MAN_HDFT:
            m_pField->pPLCFx = pBase->m_pFieldHdFtPLCF.get();
            m_pFdoa = pBase->m_pHdFtFdoa.get();
            m_pTxbx = pBase->m_pHdFtTxbx.get();
            m_pTxbxBkd = pBase->m_pHdFtTxbxBkd.get();
            break;
        case MAN_FTN:
            m_pField->pPLCFx = pBase->m_pFieldFootnotePLCF.get();
            m_pFdoa = m_pTxbx = m_pTxbxBkd = nullptr;
            break;
        case MAN_EDN:
            m_pField->pPLCFx = pBase->m_pFieldEdnPLCF.get();
            m_pFdoa = m_pTxbx = m_pTxbxBkd = nullptr;
            break;
        case MAN_AND:
            m_pField->pPLCFx = pBase->m_pFieldAndPLCF.get();
            m_pFdoa = m_pTxbx = m_pTxbxBkd = nullptr;
            break;
        case MAN_TXBX:
            m_pField->pPLCFx = pBase->m_pFieldTxbxPLCF.get();
            m_pTxbx = pBase->m_pMainTxbx.get();
            m_pTxbxBkd = pBase->m_pMainTxbxBkd.get();
            m_pFdoa = nullptr;
            break;
        case MAN_TXBX_HDFT:
            m_pField->pPLCFx = pBase->m_pFieldTxbxHdFtPLCF.get();
            m_pTxbx = pBase->m_pHdFtTxbx.get();
            m_pTxbxBkd = pBase->m_pHdFtTxbxBkd.get();
            m_pFdoa = nullptr;
            break;
        default:
            m_pField->pPLCFx = pBase->m_pFieldPLCF.get();
            m_pFdoa = pBase->m_pMainFdoa.get();
            m_pTxbx = pBase->m_pMainTxbx.get();
            m_pTxbxBkd = pBase->m_pMainTxbxBkd.get();
            break;
    }

    WW8_CP cp = 0;
    m_pWwFib->GetBaseCp(nType, &cp); //TODO: check return value
    m_nCpO = cp;

    if( nStartCp || m_nCpO )
        SeekPos( nStartCp );    // adjust PLCFe at text StartPos

    // initialization to the member vars Low-Level
    GetChpPLCF()->ResetAttrStartEnd();
    GetPapPLCF()->ResetAttrStartEnd();
    for( sal_uInt16 i=0; i < m_nPLCF; ++i)
    {
        WW8PLCFxDesc* p = &m_aD[i];

        /*
        ##516##,##517##
        For subdocuments we modify the cp of properties to be relative to
        the beginning of subdocuments, we should also do the same for
        piecetable changes, and piecetable properties, otherwise a piece
        change that happens in a subdocument is lost.
        */
        p->nCpOfs = ( p == m_pChp || p == m_pPap || p == m_pBkm || p == m_pPcd ||
            p == m_pPcdA ) ? m_nCpO : 0;

        p->nCp2OrIdx = 0;
        p->bFirstSprm = false;
        p->xIdStack.reset();

        if ((p == m_pChp) || (p == m_pPap))
            p->nStartPos = p->nEndPos = nStartCp;
        else
            p->nStartPos = p->nEndPos = WW8_CP_MAX;
    }

    // initialization to the member vars High-Level
    for( sal_uInt16 i=0; i<m_nPLCF; ++i){
        WW8PLCFxDesc* p = &m_aD[i];

        if( !p->pPLCFx )
        {
            p->nStartPos = p->nEndPos = WW8_CP_MAX;
            continue;
        }

        if( p->pPLCFx->IsSprm() )
        {
            // Careful: nEndPos must be
            p->xIdStack.emplace();
            if ((p == m_pChp) || (p == m_pPap))
            {
                WW8_CP nTemp = p->nEndPos+p->nCpOfs;
                p->pMemPos = nullptr;
                p->nSprmsLen = 0;
                p->nStartPos = nTemp;
                if (!(*p->pPLCFx).SeekPos(p->nStartPos))
                    p->nEndPos = p->nStartPos = WW8_CP_MAX;
                else
                    GetNewSprms( *p );
            }
            else
                GetNewSprms( *p );      // initialized at all PLCFs
        }
        else if( p->pPLCFx )
            GetNewNoSprms( *p );
    }
}

WW8PLCFMan::~WW8PLCFMan()
{
    for( sal_uInt16 i=0; i<m_nPLCF; i++)
        m_aD[i].xIdStack.reset();
}

// 0. which attr class,
// 1. if it's an attr start,
// 2. CP, where is next attr change
sal_uInt16 WW8PLCFMan::WhereIdx(bool *const pbStart, WW8_CP *const pPos) const
{
    OSL_ENSURE(m_nPLCF,"What the hell");
    WW8_CP nNext = WW8_CP_MAX;  // search order:
    sal_uInt16 nNextIdx = m_nPLCF;// first ending found ( CHP, PAP, ( SEP ) ),
    bool bStart = true;     // now find beginnings ( ( SEP ), PAP, CHP )
    const WW8PLCFxDesc* pD;
    for (sal_uInt16 i=0; i < m_nPLCF; ++i)
    {
        pD = &m_aD[i];
        if (pD != m_pPcdA)
        {
            if( (pD->nEndPos < nNext) && (pD->nStartPos == WW8_CP_MAX) )
            {
                // otherwise start = end
                nNext = pD->nEndPos;
                nNextIdx = i;
                bStart = false;
            }
        }
    }
    for (sal_uInt16 i=m_nPLCF; i > 0; --i)
    {
        pD = &m_aD[i-1];
        if (pD != m_pPcdA && pD->nStartPos < nNext )
        {
            nNext = pD->nStartPos;
            nNextIdx = i-1;
            bStart = true;
        }
    }
    if( pPos )
        *pPos = nNext;
    if( pbStart )
        *pbStart = bStart;
    return nNextIdx;
}

// gives the CP pos of the next attr change
WW8_CP WW8PLCFMan::Where() const
{
    WW8_CP l;
    WhereIdx(nullptr, &l);
    return l;
}

void WW8PLCFMan::SeekPos( tools::Long nNewCp )
{
    m_pChp->pPLCFx->SeekPos( nNewCp + m_nCpO ); // create new attr
    m_pPap->pPLCFx->SeekPos( nNewCp + m_nCpO );
    m_pField->pPLCFx->SeekPos( nNewCp );
    if( m_pPcd )
        m_pPcd->pPLCFx->SeekPos( nNewCp + m_nCpO );
    if( m_pBkm )
        m_pBkm->pPLCFx->SeekPos( nNewCp + m_nCpO );
}

void WW8PLCFMan::SaveAllPLCFx( WW8PLCFxSaveAll& rSave ) const
{
    sal_uInt16 n=0;
    if( m_pPcd )
        m_pPcd->Save(  rSave.aS[n++] );
    if( m_pPcdA )
        m_pPcdA->Save( rSave.aS[n++] );

    for(sal_uInt16 i=0; i<m_nPLCF; ++i)
        if( m_pPcd != &m_aD[i] && m_pPcdA != &m_aD[i] )
            m_aD[i].Save( rSave.aS[n++] );
}

void WW8PLCFMan::RestoreAllPLCFx( const WW8PLCFxSaveAll& rSave )
{
    sal_uInt16 n=0;
    if( m_pPcd )
        m_pPcd->Restore(  rSave.aS[n++] );
    if( m_pPcdA )
        m_pPcdA->Restore( rSave.aS[n++] );

    for(sal_uInt16 i=0; i<m_nPLCF; ++i)
        if( m_pPcd != &m_aD[i] && m_pPcdA != &m_aD[i] )
            m_aD[i].Restore( rSave.aS[n++] );
}

namespace
{
    bool IsSizeLegal(tools::Long nSprmLen, sal_Int32 nSprmsLen)
    {
        if (nSprmLen > nSprmsLen)
        {
            SAL_WARN("sw.ww8", "Short sprm, len " << nSprmLen << " claimed, max possible is " << nSprmsLen);
            return false;
        }
        return true;
    }
}

bool WW8PLCFMan::IsSprmLegalForCategory(sal_uInt16 nSprmId, short nIdx) const
{
    const WW8PLCFxDesc* p = &m_aD[nIdx];
    if (p != m_pSep) // just check sep for now
        return true;

    bool bRet;
    ww::WordVersion eVersion = maSprmParser.GetFIBVersion();
    if (eVersion <= ww::eWW2)
        bRet = nSprmId >= 112 && nSprmId <= 145;
    else if (eVersion < ww::eWW8)
        bRet = nSprmId >= NS_sprm::v6::sprmSScnsPgn && nSprmId <= NS_sprm::v6::sprmSDMPaperReq;
    else
    {
        /*
          Sprm bits: 10-12  sgc   sprm group; type of sprm (PAP, CHP, etc)

          sgc value type of sprm
          1         PAP
          2         CHP
          3         PIC
          4         SEP
          5         TAP
        */
        auto nSGC = ((nSprmId & 0x1C00) >> 10);
        bRet = nSGC == 4;
    }
    if (!bRet)
        SAL_INFO("sw.ww8", "sprm, id " << nSprmId << " wrong category for section properties");
    return bRet;
}

void WW8PLCFMan::GetSprmStart( short nIdx, WW8PLCFManResult* pRes ) const
{
    memset( pRes, 0, sizeof( WW8PLCFManResult ) );

    // verifying !!!

    pRes->nMemLen = 0;

    const WW8PLCFxDesc* p = &m_aD[nIdx];

    // first Sprm in a Group
    if( p->bFirstSprm )
    {
        if( p == m_pPap )
            pRes->nFlags |= MAN_MASK_NEW_PAP;
        else if( p == m_pSep )
            pRes->nFlags |= MAN_MASK_NEW_SEP;
    }
    pRes->pMemPos = p->pMemPos;
    pRes->nSprmId = GetId(p);
    pRes->nCp2OrIdx = p->nCp2OrIdx;
    if ((p == m_pFootnote) || (p == m_pEdn) || (p == m_pAnd))
        pRes->nMemLen = p->nSprmsLen;
    else if (p->nSprmsLen >= maSprmParser.MinSprmLen()) //normal
    {
        // Length of actual sprm
        pRes->nMemLen = maSprmParser.GetSprmSize(pRes->nSprmId, pRes->pMemPos, p->nSprmsLen);
        if (!IsSizeLegal(pRes->nMemLen, p->nSprmsLen) || !IsSprmLegalForCategory(pRes->nSprmId, nIdx))
        {
            pRes->nSprmId = 0;
        }
    }
}

void WW8PLCFMan::GetSprmEnd( short nIdx, WW8PLCFManResult* pRes ) const
{
    memset( pRes, 0, sizeof( WW8PLCFManResult ) );

    const WW8PLCFxDesc* p = &m_aD[nIdx];

    if (!(p->xIdStack->empty()))
        pRes->nSprmId = p->xIdStack->top();       // get end position
    else
    {
        OSL_ENSURE( false, "No Id on the Stack" );
        pRes->nSprmId = 0;
    }
}

void WW8PLCFMan::GetNoSprmStart( short nIdx, WW8PLCFManResult* pRes ) const
{
    const WW8PLCFxDesc* p = &m_aD[nIdx];

    pRes->nCpPos = p->nStartPos;
    pRes->nMemLen = p->nSprmsLen;
    pRes->nCp2OrIdx = p->nCp2OrIdx;

    if( p == m_pField )
        pRes->nSprmId = eFLD;
    else if( p == m_pFootnote )
        pRes->nSprmId = eFTN;
    else if( p == m_pEdn )
        pRes->nSprmId = eEDN;
    else if( p == m_pBkm )
        pRes->nSprmId = eBKN;
    else if (p == m_pAtnBkm)
        pRes->nSprmId = eATNBKN;
    else if (p == m_pFactoidBkm)
        pRes->nSprmId = eFACTOIDBKN;
    else if( p == m_pAnd )
        pRes->nSprmId = eAND;
    else if( p == m_pPcd )
    {
        //We slave the piece table attributes to the piece table, the piece
        //table attribute iterator contains the sprms for this piece.
        GetSprmStart( nIdx+1, pRes );
    }
    else
        pRes->nSprmId = 0;          // default: not found
}

void WW8PLCFMan::GetNoSprmEnd( short nIdx, WW8PLCFManResult* pRes ) const
{
    pRes->nMemLen = -1;     // end tag

    if( &m_aD[nIdx] == m_pBkm )
        pRes->nSprmId = eBKN;
    else if (&m_aD[nIdx] == m_pAtnBkm)
        pRes->nSprmId = eATNBKN;
    else if (&m_aD[nIdx] == m_pFactoidBkm)
        pRes->nSprmId = eFACTOIDBKN;
    else if( &m_aD[nIdx] == m_pPcd )
    {
        //We slave the piece table attributes to the piece table, the piece
        //table attribute iterator contains the sprms for this piece.
        GetSprmEnd( nIdx+1, pRes );
    }
    else
        pRes->nSprmId = 0;
}

void WW8PLCFMan::TransferOpenSprms(std::stack<sal_uInt16> &rStack)
{
    for (sal_uInt16 i = 0; i < m_nPLCF; ++i)
    {
        WW8PLCFxDesc* p = &m_aD[i];
        if (!p || !p->xIdStack)
            continue;
        while (!p->xIdStack->empty())
        {
            rStack.push(p->xIdStack->top());
            p->xIdStack->pop();
        }
    }
}

void WW8PLCFMan::AdvSprm(short nIdx, bool bStart)
{
    WW8PLCFxDesc* p = &m_aD[nIdx];    // determine sprm class(!)

    p->bFirstSprm = false;
    if( bStart )
    {
        const sal_uInt16 nLastId = GetId(p);

        const sal_uInt16 nLastAttribStarted = IsSprmLegalForCategory(nLastId, nIdx) ? nLastId : 0;

        p->xIdStack->push(nLastAttribStarted);   // remember Id for attribute end

        if( p->nSprmsLen )
        {   /*
                Check, if we have to process more sprm(s).
            */
            if( p->pMemPos )
            {
                // Length of last sprm
                const sal_Int32 nSprmL = maSprmParser.GetSprmSize(nLastId, p->pMemPos, p->nSprmsLen);

                // Reduce length of all sprms by length of last sprm
                p->nSprmsLen -= nSprmL;

                // pos of next possible sprm
                if (p->nSprmsLen < maSprmParser.MinSprmLen())
                {
                    // preventively set to 0, because the end follows!
                    p->pMemPos = nullptr;
                    p->nSprmsLen = 0;
                }
                else
                    p->pMemPos += nSprmL;
            }
            else
                p->nSprmsLen = 0;
        }
        if (p->nSprmsLen < maSprmParser.MinSprmLen())
            p->nStartPos = WW8_CP_MAX;    // the ending follows
    }
    else
    {
        if (!(p->xIdStack->empty()))
            p->xIdStack->pop();
        if (p->xIdStack->empty())
        {
            if ( (p == m_pChp) || (p == m_pPap) )
            {
                p->pMemPos = nullptr;
                p->nSprmsLen = 0;
                p->nStartPos = p->nOrigEndPos+p->nCpOfs;

                /*
                On failed seek we have run out of sprms, probably. But if it's
                a fastsaved file (has pPcd) then we may be just in a sprm free
                gap between pieces that have them, so set dirty flag in sprm
                finder to consider than.
                */
                if (!(*p->pPLCFx).SeekPos(p->nStartPos))
                {
                    p->nEndPos = WW8_CP_MAX;
                    p->pPLCFx->SetDirty(true);
                }
                if (!p->pPLCFx->GetDirty() || m_pPcd)
                    GetNewSprms( *p );
                p->pPLCFx->SetDirty(false);

                /*
                #i2325#
                To get the character and paragraph properties you first get
                the pap and chp and then apply the fastsaved pPcd properties
                to the range. If a pap or chp starts inside the pPcd range
                then we must bring the current pPcd range to a halt so as to
                end those sprms, then the pap/chp will be processed, and then
                we must force a restart of the pPcd on that pap/chp starting
                boundary. Doing that effectively means that the pPcd sprms will
                be applied to the new range. Not doing it means that the pPcd
                sprms will only be applied to the first pap/chp set of
                properties contained in the pap/chp range.

                So we bring the pPcd to a halt on this location here, by
                settings its end to the current start, then store the starting
                position of the current range to clipstart. The pPcd sprms
                will end as normal (albeit earlier than originally expected),
                and the existence of a clipstart will force the pPcd iterator
                to reread the current set of sprms instead of advancing to its
                next set. Then the clipstart will be set as the starting
                position which will force them to be applied directly after
                the pap and chps.
                */
                if (m_pPcd && ((p->nStartPos > m_pPcd->nStartPos) ||
                    (m_pPcd->nStartPos == WW8_CP_MAX)) &&
                    (m_pPcd->nEndPos != p->nStartPos))
                {
                    m_pPcd->nEndPos = p->nStartPos;
                    static_cast<WW8PLCFx_PCD *>(m_pPcd->pPLCFx)->SetClipStart(
                        p->nStartPos);
                }

            }
            else
            {
                p->pPLCFx->advance(); // next Group of Sprms
                p->pMemPos = nullptr;       // !!!
                p->nSprmsLen = 0;
                GetNewSprms( *p );
            }
            SAL_WARN_IF(p->nStartPos > p->nEndPos, "sw.ww8",
                        "End " << p->nEndPos << " before Start " << p->nStartPos);
        }
    }
}

void WW8PLCFMan::AdvNoSprm(short nIdx, bool bStart)
{
    /*
    For the case of a piece table we slave the piece table attribute iterator
    to the piece table and access it through that only. They are two separate
    structures, but act together as one logical one. The attributes only go
    to the next entry when the piece changes
    */
    WW8PLCFxDesc* p = &m_aD[nIdx];

    if( p == m_pPcd )
    {
        AdvSprm(nIdx+1,bStart);
        if( bStart )
            p->nStartPos = m_aD[nIdx+1].nStartPos;
        else
        {
            if (m_aD[nIdx+1].xIdStack->empty())
            {
                WW8PLCFx_PCD *pTemp = static_cast<WW8PLCFx_PCD*>(m_pPcd->pPLCFx);
                /*
                #i2325#
                As per normal, go on to the next set of properties, i.e. we
                have traversed over to the next piece.  With a clipstart set
                we are being told to reread the current piece sprms so as to
                reapply them to a new chp or pap range.
                */
                if (pTemp->GetClipStart() == -1)
                    p->pPLCFx->advance();
                p->pMemPos = nullptr;
                p->nSprmsLen = 0;
                GetNewSprms( m_aD[nIdx+1] );
                GetNewNoSprms( *p );
                if (pTemp->GetClipStart() != -1)
                {
                    /*
                    #i2325#, now we will force our starting position to the
                    clipping start so as to force the application of these
                    sprms after the current pap/chp sprms so as to apply the
                    fastsave sprms to the current range.
                    */
                    p->nStartPos = pTemp->GetClipStart();
                    pTemp->SetClipStart(-1);
                }
            }
        }
    }
    else
    {                                  // NoSprm without end
        p->pPLCFx->advance();
        p->pMemPos = nullptr;                     // MemPos invalid
        p->nSprmsLen = 0;
        GetNewNoSprms( *p );
    }
}

void WW8PLCFMan::advance()
{
    bool bStart;
    const sal_uInt16 nIdx = WhereIdx(&bStart);
    if (nIdx < m_nPLCF)
    {
        WW8PLCFxDesc* p = &m_aD[nIdx];

        p->bFirstSprm = true;                       // Default

        if( p->pPLCFx->IsSprm() )
            AdvSprm( nIdx, bStart );
        else                                        // NoSprm
            AdvNoSprm( nIdx, bStart );
    }
}

// return true for the beginning of an attribute or error,
//           false for the end of an attribute
// remaining return values are delivered to the caller from WW8PclxManResults.
bool WW8PLCFMan::Get(WW8PLCFManResult* pRes) const
{
    memset( pRes, 0, sizeof( WW8PLCFManResult ) );
    bool bStart;
    const sal_uInt16 nIdx = WhereIdx(&bStart);

    if( nIdx >= m_nPLCF )
    {
        OSL_ENSURE( false, "Position not found" );
        return true;
    }

    if( m_aD[nIdx].pPLCFx->IsSprm() )
    {
        if( bStart )
        {
            GetSprmStart( nIdx, pRes );
            return true;
        }
        else
        {
            GetSprmEnd( nIdx, pRes );
            return false;
        }
    }
    else
    {
        if( bStart )
        {
            GetNoSprmStart( nIdx, pRes );
            return true;
        }
        else
        {
            GetNoSprmEnd( nIdx, pRes );
            return false;
        }
    }
}

sal_uInt16 WW8PLCFMan::GetColl() const
{
    if( m_pPap->pPLCFx )
        return  m_pPap->pPLCFx->GetIstd();
    else
    {
        OSL_ENSURE( false, "GetColl without PLCF_Pap" );
        return 0;
    }
}

WW8PLCFx_FLD* WW8PLCFMan::GetField() const
{
    return static_cast<WW8PLCFx_FLD*>(m_pField->pPLCFx);
}

SprmResult WW8PLCFMan::HasParaSprm( sal_uInt16 nId ) const
{
    return static_cast<WW8PLCFx_Cp_FKP*>(m_pPap->pPLCFx)->HasSprm( nId );
}

SprmResult WW8PLCFMan::HasCharSprm( sal_uInt16 nId ) const
{
    return static_cast<WW8PLCFx_Cp_FKP*>(m_pChp->pPLCFx)->HasSprm( nId );
}

void WW8PLCFMan::HasCharSprm(sal_uInt16 nId,
    std::vector<SprmResult> &rResult) const
{
    static_cast<WW8PLCFx_Cp_FKP*>(m_pChp->pPLCFx)->HasSprm(nId, rResult);
}

void WW8PLCFx::Save( WW8PLCFxSave1& rSave ) const
{
    rSave.nPLCFxPos    = GetIdx();
    rSave.nPLCFxPos2   = GetIdx2();
    rSave.nPLCFxMemOfs = 0;
    rSave.nStartFC     = GetStartFc();
}

void WW8PLCFx::Restore( const WW8PLCFxSave1& rSave )
{
    SetIdx(     rSave.nPLCFxPos  );
    SetIdx2(    rSave.nPLCFxPos2 );
    SetStartFc( rSave.nStartFC   );
}

sal_uInt32 WW8PLCFx_Cp_FKP::GetIdx2() const
{
    return GetPCDIdx();
}

void WW8PLCFx_Cp_FKP::SetIdx2(sal_uInt32 nIdx)
{
    if( m_pPcd )
        m_pPcd->SetIdx( nIdx );
}

void WW8PLCFx_Cp_FKP::Save( WW8PLCFxSave1& rSave ) const
{
    if (m_pFkp)
        m_pFkp->IncMustRemainCache();
    WW8PLCFx::Save( rSave );

    rSave.nAttrStart = m_nAttrStart;
    rSave.nAttrEnd   = m_nAttrEnd;
    rSave.bLineEnd   = m_bLineEnd;
}

void WW8PLCFx_Cp_FKP::Restore( const WW8PLCFxSave1& rSave )
{
    WW8PLCFx::Restore( rSave );

    m_nAttrStart = rSave.nAttrStart;
    m_nAttrEnd   = rSave.nAttrEnd;
    m_bLineEnd   = rSave.bLineEnd;

    if (m_pFkp)
        m_pFkp->DecMustRemainCache();
}

void WW8PLCFxDesc::Save( WW8PLCFxSave1& rSave ) const
{
    if( !pPLCFx )
        return;

    pPLCFx->Save( rSave );
    if( !pPLCFx->IsSprm() )
        return;

    WW8PLCFxDesc aD;
    aD.nStartPos = nOrigStartPos+nCpOfs;
    aD.nCpOfs = rSave.nCpOfs = nCpOfs;
    if (!(pPLCFx->SeekPos(aD.nStartPos)))
    {
        aD.nEndPos = WW8_CP_MAX;
        pPLCFx->SetDirty(true);
    }
    pPLCFx->GetSprms(&aD);
    pPLCFx->SetDirty(false);
    aD.ReduceByOffset();
    rSave.nStartCp = aD.nStartPos;
    rSave.nPLCFxMemOfs = nOrigSprmsLen - nSprmsLen;
}

void WW8PLCFxDesc::Restore( const WW8PLCFxSave1& rSave )
{
    if( !pPLCFx )
        return;

    pPLCFx->Restore( rSave );
    if( !pPLCFx->IsSprm() )
        return;

    WW8PLCFxDesc aD;
    aD.nStartPos = rSave.nStartCp+rSave.nCpOfs;
    nCpOfs = aD.nCpOfs = rSave.nCpOfs;
    if (!(pPLCFx->SeekPos(aD.nStartPos)))
    {
        aD.nEndPos = WW8_CP_MAX;
        pPLCFx->SetDirty(true);
    }
    pPLCFx->GetSprms(&aD);
    pPLCFx->SetDirty(false);
    aD.ReduceByOffset();

    if (nOrigSprmsLen > aD.nSprmsLen)
    {
        //two entries exist for the same offset, cut and run
        SAL_WARN("sw.ww8", "restored properties don't match saved properties, bailing out");
        nSprmsLen = 0;
        pMemPos = nullptr;
    }
    else
    {
        nSprmsLen = nOrigSprmsLen - rSave.nPLCFxMemOfs;
        pMemPos = aD.pMemPos == nullptr ? nullptr : aD.pMemPos + rSave.nPLCFxMemOfs;
    }
}

namespace
{
    sal_uInt32 Readcb(SvStream& rSt, ww::WordVersion eVer)
    {
        if (eVer <= ww::eWW2)
        {
            sal_uInt16 nShort(0);
            rSt.ReadUInt16(nShort);
            return nShort;
        }
        else
        {
            sal_uInt32 nLong(0);
            rSt.ReadUInt32(nLong);
            return nLong;
        }
    }
}

bool WW8Fib::GetBaseCp(ManTypes nType, WW8_CP * cp) const
{
    assert(cp != nullptr);
    WW8_CP nOffset = 0;

    switch (nType)
    {
        case MAN_TXBX_HDFT:
            if (m_ccpTxbx < 0) {
                return false;
            }
            nOffset = m_ccpTxbx;
            [[fallthrough]];
        case MAN_TXBX:
            if (m_ccpEdn < 0 || m_ccpEdn > std::numeric_limits<WW8_CP>::max() - nOffset) {
                return false;
            }
            nOffset += m_ccpEdn;
            [[fallthrough]];
        case MAN_EDN:
            if (m_ccpAtn < 0 || m_ccpAtn > std::numeric_limits<WW8_CP>::max() - nOffset) {
                return false;
            }
            nOffset += m_ccpAtn;
            [[fallthrough]];
        case MAN_AND:
            if (m_ccpMcr < 0 || m_ccpMcr > std::numeric_limits<WW8_CP>::max() - nOffset) {
                return false;
            }
            nOffset += m_ccpMcr;
        /*
            // fall through

         A subdocument of this kind (MAN_MACRO) probably exists in some defunct
         version of MSWord, but now ccpMcr is always 0. If some example that
         uses this comes to light, this is the likely calculation required

        case MAN_MACRO:
        */
            if (m_ccpHdr < 0 || m_ccpHdr > std::numeric_limits<WW8_CP>::max() - nOffset) {
                return false;
            }
            nOffset += m_ccpHdr;
            [[fallthrough]];
        case MAN_HDFT:
            if (m_ccpFootnote < 0 || m_ccpFootnote > std::numeric_limits<WW8_CP>::max() - nOffset) {
                return false;
            }
            nOffset += m_ccpFootnote;
            [[fallthrough]];
        case MAN_FTN:
            if (m_ccpText < 0 || m_ccpText > std::numeric_limits<WW8_CP>::max() - nOffset) {
                return false;
            }
            nOffset += m_ccpText;
            [[fallthrough]];
        case MAN_MAINTEXT:
            break;
    }
    *cp = nOffset;
    return true;
}

ww::WordVersion WW8Fib::GetFIBVersion() const
{
    ww::WordVersion eVer = ww::eWW8;
    /*
     * Word for Windows 2 I think (1.X might work too if anyone has an example.
     *
     * 0xA59B for Word 1 for Windows
     * 0xA59C for Word 1 for OS/2 "PM Word"
     *
     * Various pages claim that the fileformats of Word 1 and 2 for Windows are
     * equivalent to Word for Macintosh 4 and 5. On the other hand
     *
     * wIdents for Word for Mac versions...
     * 0xFE32 for Word 1
     * 0xFE34 for Word 3
     * 0xFE37 for Word 4 et 5.
     *
     * and this document
     * http://cmsdoc.cern.ch/documents/docformat/CMS_CERN_LetterHead.word is
     * claimed to be "Word 5 for Mac" by Office etc and has that wIdent, but
     * its format isn't the same as that of Word 2 for windows. Nor is it
     * the same as that of Word for DOS/PCWord 5
     */
    if (m_wIdent == 0xa59b || m_wIdent == 0xa59c)
        eVer = ww::eWW1;
    else if (m_wIdent == 0xa5db)
        eVer = ww::eWW2;
    else
    {
        switch (m_nVersion)
        {
            case 6:
                eVer = ww::eWW6;
                break;
            case 7:
                eVer = ww::eWW7;
                break;
            case 8:
                eVer = ww::eWW8;
                break;
        }
    }
    return eVer;
}

WW8Fib::WW8Fib(SvStream& rSt, sal_uInt8 nWantedVersion, sal_uInt32 nOffset):
    m_fDot(false), m_fGlsy(false), m_fComplex(false), m_fHasPic(false), m_cQuickSaves(0),
    m_fEncrypted(false), m_fWhichTableStm(false), m_fReadOnlyRecommended(false),
    m_fWriteReservation(false), m_fExtChar(false), m_fFarEast(false), m_fObfuscated(false),
    m_fMac(false), m_fEmptySpecial(false), m_fLoadOverridePage(false), m_fFuturesavedUndo(false),
    m_fWord97Saved(false), m_fWord2000Saved(false)
        // in C++20 with P06831R1 "Default member initializers for bit-fields (revision 1)", the
        // above bit-field member initializations can be moved to the class definition
{
    // See [MS-DOC] 2.5.15 "How to read the FIB".
    rSt.Seek( nOffset );
    /*
        note desired number, identify file version number
        and check against desired number!
    */
    m_nVersion = nWantedVersion;
    rSt.ReadUInt16( m_wIdent );
    rSt.ReadUInt16( m_nFib );
    rSt.ReadUInt16( m_nProduct );
    if( ERRCODE_NONE != rSt.GetError() )
    {
        sal_Int16 nFibMin;
        sal_Int16 nFibMax;
        // note: 6 stands for "6 OR 7",  7 stands for "ONLY 7"
        switch( m_nVersion )
        {
            case 6:
                nFibMin = 0x0065;   // from 101 WinWord 6.0
                                    //     102    "
                                    // and 103 WinWord 6.0 for Macintosh
                                    //     104    "
                nFibMax = 0x0069;   // to 105 WinWord 95
                break;
            case 7:
                nFibMin = 0x0069;   // from 105 WinWord 95
                nFibMax = 0x0069;   // to 105 WinWord 95
                break;
            case 8:
                nFibMin = 0x006A;   // from 106 WinWord 97
                nFibMax = 0x00c1;   // to 193 WinWord 97 (?)
                break;
            default:
                nFibMin = 0;            // program error!
                nFibMax = 0;
                m_nFib    = 1;
                OSL_ENSURE( false, "nVersion not implemented!" );
                break;
        }
        if ( (m_nFib < nFibMin) || (m_nFib > nFibMax) )
        {
            m_nFibError = ERR_SWG_READ_ERROR; // report error
            return;
        }
    }

    ww::WordVersion eVer = GetFIBVersion();

    // helper vars for Ver67:
    sal_Int16 pnChpFirst_Ver67=0;
    sal_Int16 pnPapFirst_Ver67=0;
    sal_Int16 cpnBteChp_Ver67=0;
    sal_Int16 cpnBtePap_Ver67=0;

    // read FIB
    sal_uInt16 nTmpLid = 0;
    rSt.ReadUInt16(nTmpLid);
    m_lid = LanguageType(nTmpLid);
    rSt.ReadInt16( m_pnNext );
    sal_uInt8 aBits1(0);
    rSt.ReadUChar( aBits1 );
    sal_uInt8 aBits2(0);
    rSt.ReadUChar( aBits2 );
    rSt.ReadUInt16( m_nFibBack );
    rSt.ReadUInt16( m_nHash );
    rSt.ReadUInt16( m_nKey );
    rSt.ReadUChar( m_envr );
    sal_uInt8 aVer8Bits1(0);     // only used starting with WinWord 8
    rSt.ReadUChar( aVer8Bits1 ); // only have an empty reserve field under Ver67
                            // content from aVer8Bits1

                            // sal_uInt8 fMac              :1;
                            // sal_uInt8 fEmptySpecial     :1;
                            // sal_uInt8 fLoadOverridePage :1;
                            // sal_uInt8 fFuturesavedUndo  :1;
                            // sal_uInt8 fWord97Saved      :1;
                            // sal_uInt8 :3;
    rSt.ReadUInt16( m_chse );
    rSt.ReadUInt16( m_chseTables );
    rSt.ReadInt32( m_fcMin );
    rSt.ReadInt32( m_fcMac );

// insertion for WW8
    if (IsEightPlus(eVer))
    {
        rSt.ReadUInt16( m_csw );

        // Marke: "rgsw"  Beginning of the array of shorts
        rSt.ReadUInt16( m_wMagicCreated );
        rSt.ReadUInt16( m_wMagicRevised );
        rSt.ReadUInt16( m_wMagicCreatedPrivate );
        rSt.ReadUInt16( m_wMagicRevisedPrivate );
        rSt.SeekRel( 9 * sizeof( sal_Int16 ) );

        /*
        // these are the 9 unused fields:
        && (bVer67 || WW8ReadINT16(  rSt, pnFbpChpFirst_W6          ))  // 1
        && (bVer67 || WW8ReadINT16(  rSt, pnChpFirst_W6                 ))  // 2
        && (bVer67 || WW8ReadINT16(  rSt, cpnBteChp_W6                  ))  // 3
        && (bVer67 || WW8ReadINT16(  rSt, pnFbpPapFirst_W6          ))  // 4
        && (bVer67 || WW8ReadINT16(  rSt, pnPapFirst_W6                 ))  // 5
        && (bVer67 || WW8ReadINT16(  rSt, cpnBtePap_W6                  ))  // 6
        && (bVer67 || WW8ReadINT16(  rSt, pnFbpLvcFirst_W6          ))  // 7
        && (bVer67 || WW8ReadINT16(  rSt, pnLvcFirst_W6                 ))  // 8
        && (bVer67 || WW8ReadINT16(  rSt, cpnBteLvc_W6                  ))  // 9
        */
        sal_uInt16 nTmpFE = 0;
        rSt.ReadUInt16(nTmpFE);
        m_lidFE = LanguageType(nTmpFE);
        rSt.ReadUInt16( m_clw );
    }

// end of the insertion for WW8

        // Marke: "rglw"  Beginning of the array of longs
    rSt.ReadInt32( m_cbMac );

        // ignore 2 longs, because they are unimportant
    rSt.SeekRel( 2 * sizeof( sal_Int32) );

        // skipping 2 more longs only at Ver67
    if (IsSevenMinus(eVer))
        rSt.SeekRel( 2 * sizeof( sal_Int32) );

    rSt.ReadInt32( m_ccpText );
    rSt.ReadInt32( m_ccpFootnote );
    rSt.ReadInt32( m_ccpHdr );
    rSt.ReadInt32( m_ccpMcr );
    rSt.ReadInt32( m_ccpAtn );
    rSt.ReadInt32( m_ccpEdn );
    rSt.ReadInt32( m_ccpTxbx );
    rSt.ReadInt32( m_ccpHdrTxbx );

        // only skip one more long at Ver67
    if (IsSevenMinus(eVer))
        rSt.SeekRel( 1 * sizeof( sal_Int32) );
    else
    {
// insertion for WW8
        rSt.ReadInt32( m_pnFbpChpFirst );
        rSt.ReadInt32( m_pnChpFirst );
        rSt.ReadInt32( m_cpnBteChp );
        rSt.ReadInt32( m_pnFbpPapFirst );
        rSt.ReadInt32( m_pnPapFirst );
        rSt.ReadInt32( m_cpnBtePap );
        rSt.ReadInt32( m_pnFbpLvcFirst );
        rSt.ReadInt32( m_pnLvcFirst );
        rSt.ReadInt32( m_cpnBteLvc );
        rSt.ReadInt32( m_fcIslandFirst );
        rSt.ReadInt32( m_fcIslandLim );
        rSt.ReadUInt16( m_cfclcb );

        // Read cswNew to find out if nFib should be ignored.
        sal_uInt64 nPos = rSt.Tell();
        rSt.SeekRel(m_cfclcb * 8);
        if (rSt.good() && rSt.remainingSize() >= 2)
        {
            rSt.ReadUInt16(m_cswNew);
        }
        rSt.Seek(nPos);
    }

// end of the insertion for WW8

    // Marke: "rgfclcb" Beginning of array of FC/LCB pairs.
    rSt.ReadInt32( m_fcStshfOrig );
    m_lcbStshfOrig = Readcb(rSt, eVer);
    rSt.ReadInt32( m_fcStshf );
    m_lcbStshf = Readcb(rSt, eVer);
    rSt.ReadInt32( m_fcPlcffndRef );
    m_lcbPlcffndRef = Readcb(rSt, eVer);
    rSt.ReadInt32( m_fcPlcffndText );
    m_lcbPlcffndText = Readcb(rSt, eVer);
    rSt.ReadInt32( m_fcPlcfandRef );
    m_lcbPlcfandRef = Readcb(rSt, eVer);
    rSt.ReadInt32( m_fcPlcfandText );
    m_lcbPlcfandText = Readcb(rSt, eVer);
    rSt.ReadInt32( m_fcPlcfsed );
    m_lcbPlcfsed = Readcb(rSt, eVer);
    rSt.ReadInt32( m_fcPlcfpad );
    m_lcbPlcfpad = Readcb(rSt, eVer);
    rSt.ReadInt32( m_fcPlcfphe );
    m_lcbPlcfphe = Readcb(rSt, eVer);
    rSt.ReadInt32( m_fcSttbfglsy );
    m_lcbSttbfglsy = Readcb(rSt, eVer);
    rSt.ReadInt32( m_fcPlcfglsy );
    m_lcbPlcfglsy = Readcb(rSt, eVer);
    rSt.ReadInt32( m_fcPlcfhdd );
    m_lcbPlcfhdd = Readcb(rSt, eVer);
    rSt.ReadInt32( m_fcPlcfbteChpx );
    m_lcbPlcfbteChpx = Readcb(rSt, eVer);
    rSt.ReadInt32( m_fcPlcfbtePapx );
    m_lcbPlcfbtePapx = Readcb(rSt, eVer);
    rSt.ReadInt32( m_fcPlcfsea );
    m_lcbPlcfsea = Readcb(rSt, eVer);
    rSt.ReadInt32( m_fcSttbfffn );
    m_lcbSttbfffn = Readcb(rSt, eVer);
    rSt.ReadInt32( m_fcPlcffldMom );
    m_lcbPlcffldMom = Readcb(rSt, eVer);
    rSt.ReadInt32( m_fcPlcffldHdr );
    m_lcbPlcffldHdr = Readcb(rSt, eVer);
    rSt.ReadInt32( m_fcPlcffldFootnote );
    m_lcbPlcffldFootnote = Readcb(rSt, eVer);
    rSt.ReadInt32( m_fcPlcffldAtn );
    m_lcbPlcffldAtn = Readcb(rSt, eVer);
    rSt.ReadInt32( m_fcPlcffldMcr );
    m_lcbPlcffldMcr = Readcb(rSt, eVer);
    rSt.ReadInt32( m_fcSttbfbkmk );
    m_lcbSttbfbkmk = Readcb(rSt, eVer);
    rSt.ReadInt32( m_fcPlcfbkf );
    m_lcbPlcfbkf = Readcb(rSt, eVer);
    rSt.ReadInt32( m_fcPlcfbkl );
    m_lcbPlcfbkl = Readcb(rSt, eVer);
    rSt.ReadInt32( m_fcCmds );
    m_lcbCmds = Readcb(rSt, eVer);
    rSt.ReadInt32( m_fcPlcfmcr );
    m_lcbPlcfmcr = Readcb(rSt, eVer);
    rSt.ReadInt32( m_fcSttbfmcr );
    m_lcbSttbfmcr = Readcb(rSt, eVer);
    if (eVer >= ww::eWW2)
    {
        rSt.ReadInt32( m_fcPrDrvr );
        m_lcbPrDrvr = Readcb(rSt, eVer);
        rSt.ReadInt32( m_fcPrEnvPort );
        m_lcbPrEnvPort = Readcb(rSt, eVer);
        rSt.ReadInt32( m_fcPrEnvLand );
        m_lcbPrEnvLand = Readcb(rSt, eVer);
    }
    else
    {
        rSt.ReadInt32( m_fcPrEnvPort );
        m_lcbPrEnvPort = Readcb(rSt, eVer);
    }
    rSt.ReadInt32( m_fcWss );
    m_lcbWss = Readcb(rSt, eVer);
    rSt.ReadInt32( m_fcDop );
    m_lcbDop = Readcb(rSt, eVer);
    rSt.ReadInt32( m_fcSttbfAssoc );
    m_lcbSttbfAssoc = Readcb(rSt, eVer);
    rSt.ReadInt32( m_fcClx );
    m_lcbClx = Readcb(rSt, eVer);
    rSt.ReadInt32( m_fcPlcfpgdFootnote );
    m_lcbPlcfpgdFootnote = Readcb(rSt, eVer);
    rSt.ReadInt32( m_fcAutosaveSource );
    m_lcbAutosaveSource = Readcb(rSt, eVer);
    rSt.ReadInt32( m_fcGrpStAtnOwners );
    m_lcbGrpStAtnOwners = Readcb(rSt, eVer);
    rSt.ReadInt32( m_fcSttbfAtnbkmk );
    m_lcbSttbfAtnbkmk = Readcb(rSt, eVer);

    // only skip more shot at Ver67
    if (IsSevenMinus(eVer))
    {
        if (eVer == ww::eWW1)
            rSt.SeekRel(1*sizeof(sal_Int32));
        rSt.SeekRel(1*sizeof(sal_Int16));

        if (eVer >= ww::eWW2)
        {
            rSt.ReadInt16(pnChpFirst_Ver67);
            rSt.ReadInt16(pnPapFirst_Ver67);
        }
        rSt.ReadInt16(cpnBteChp_Ver67);
        rSt.ReadInt16(cpnBtePap_Ver67);
    }

    if (eVer > ww::eWW2)
    {
        rSt.ReadInt32( m_fcPlcfdoaMom );
        rSt.ReadInt32( m_lcbPlcfdoaMom );
        rSt.ReadInt32( m_fcPlcfdoaHdr );
        rSt.ReadInt32( m_lcbPlcfdoaHdr );
        rSt.ReadInt32( m_fcPlcfspaMom );
        rSt.ReadInt32( m_lcbPlcfspaMom );
        rSt.ReadInt32( m_fcPlcfspaHdr );
        rSt.ReadInt32( m_lcbPlcfspaHdr );

        rSt.ReadInt32( m_fcPlcfAtnbkf );
        rSt.ReadInt32( m_lcbPlcfAtnbkf );
        rSt.ReadInt32( m_fcPlcfAtnbkl );
        rSt.ReadInt32( m_lcbPlcfAtnbkl );
        rSt.ReadInt32( m_fcPms );
        rSt.ReadInt32( m_lcbPMS );
        rSt.ReadInt32( m_fcFormFieldSttbf );
        rSt.ReadInt32( m_lcbFormFieldSttbf );
        rSt.ReadInt32( m_fcPlcfendRef );
        rSt.ReadInt32( m_lcbPlcfendRef );
        rSt.ReadInt32( m_fcPlcfendText );
        rSt.ReadInt32( m_lcbPlcfendText );
        rSt.ReadInt32( m_fcPlcffldEdn );
        rSt.ReadInt32( m_lcbPlcffldEdn );
        rSt.ReadInt32( m_fcPlcfpgdEdn );
        rSt.ReadInt32( m_lcbPlcfpgdEdn );
        rSt.ReadInt32( m_fcDggInfo );
        rSt.ReadInt32( m_lcbDggInfo );
        rSt.ReadInt32( m_fcSttbfRMark );
        rSt.ReadInt32( m_lcbSttbfRMark );
        rSt.ReadInt32( m_fcSttbfCaption );
        rSt.ReadInt32( m_lcbSttbfCaption );
        rSt.ReadInt32( m_fcSttbAutoCaption );
        rSt.ReadInt32( m_lcbSttbAutoCaption );
        rSt.ReadInt32( m_fcPlcfwkb );
        rSt.ReadInt32( m_lcbPlcfwkb );
        rSt.ReadInt32( m_fcPlcfspl );
        rSt.ReadInt32( m_lcbPlcfspl );
        rSt.ReadInt32( m_fcPlcftxbxText );
        rSt.ReadInt32( m_lcbPlcftxbxText );
        rSt.ReadInt32( m_fcPlcffldTxbx );
        rSt.ReadInt32( m_lcbPlcffldTxbx );
        rSt.ReadInt32( m_fcPlcfHdrtxbxText );
        rSt.ReadInt32( m_lcbPlcfHdrtxbxText );
        rSt.ReadInt32( m_fcPlcffldHdrTxbx );
        rSt.ReadInt32( m_lcbPlcffldHdrTxbx );
        rSt.ReadInt32( m_fcStwUser );
        rSt.ReadUInt32( m_lcbStwUser );
        rSt.ReadInt32( m_fcSttbttmbd );
        rSt.ReadUInt32( m_lcbSttbttmbd );
    }

    if( ERRCODE_NONE == rSt.GetError() )
    {
        // set bit flag
        m_fDot        =   aBits1 & 0x01       ;
        m_fGlsy       = ( aBits1 & 0x02 ) >> 1;
        m_fComplex    = ( aBits1 & 0x04 ) >> 2;
        m_fHasPic     = ( aBits1 & 0x08 ) >> 3;
        m_cQuickSaves = ( aBits1 & 0xf0 ) >> 4;
        m_fEncrypted  =   aBits2 & 0x01       ;
        m_fWhichTableStm= ( aBits2 & 0x02 ) >> 1;
        m_fReadOnlyRecommended = (aBits2 & 0x4) >> 2;
        m_fWriteReservation = (aBits2 & 0x8) >> 3;
        m_fExtChar    = ( aBits2 & 0x10 ) >> 4;
        // dummy    = ( aBits2 & 0x20 ) >> 5;
        m_fFarEast    = ( aBits2 & 0x40 ) >> 6; // #i90932#
        // dummy    = ( aBits2 & 0x80 ) >> 7;

        /*
            p.r.n. fill targeted variable with xxx_Ver67
            or set flags
        */
        if (IsSevenMinus(eVer))
        {
            m_pnChpFirst = pnChpFirst_Ver67;
            m_pnPapFirst = pnPapFirst_Ver67;
            m_cpnBteChp = cpnBteChp_Ver67;
            m_cpnBtePap = cpnBtePap_Ver67;
        }
        else if (IsEightPlus(eVer))
        {
            m_fMac              =   aVer8Bits1  & 0x01           ;
            m_fEmptySpecial     = ( aVer8Bits1  & 0x02 ) >> 1;
            m_fLoadOverridePage = ( aVer8Bits1  & 0x04 ) >> 2;
            m_fFuturesavedUndo  = ( aVer8Bits1  & 0x08 ) >> 3;
            m_fWord97Saved      = ( aVer8Bits1  & 0x10 ) >> 4;
            m_fWord2000Saved    = ( aVer8Bits1  & 0x20 ) >> 5;

            /*
                especially for WW8:
                identify the values for PLCF and PLF LFO
                and PLCF for the textbox break descriptors
            */
            sal_uInt64 nOldPos = rSt.Tell();

            rSt.Seek( 0x02da );
            rSt.ReadInt32( m_fcSttbFnm );
            rSt.ReadInt32( m_lcbSttbFnm );
            rSt.ReadInt32( m_fcPlcfLst );
            rSt.ReadInt32( m_lcbPlcfLst );
            rSt.ReadInt32( m_fcPlfLfo );
            rSt.ReadInt32( m_lcbPlfLfo );
            rSt.ReadInt32( m_fcPlcftxbxBkd );
            rSt.ReadInt32( m_lcbPlcftxbxBkd );
            rSt.ReadInt32( m_fcPlcfHdrtxbxBkd );
            rSt.ReadInt32( m_lcbPlcfHdrtxbxBkd );
            if( ERRCODE_NONE != rSt.GetError() )
            {
                m_nFibError = ERR_SWG_READ_ERROR;
            }

            rSt.Seek( 0x372 );          // fcSttbListNames
            rSt.ReadInt32( m_fcSttbListNames );
            rSt.ReadInt32( m_lcbSttbListNames );

            if (m_cfclcb > 93)
            {
                rSt.Seek( 0x382 );          // MagicTables
                rSt.ReadInt32( m_fcPlcfTch );
                rSt.ReadInt32( m_lcbPlcfTch );
            }

            if (m_cfclcb > 113)
            {
                rSt.Seek( 0x41A );          // new ATRD
                rSt.ReadInt32( m_fcAtrdExtra );
                rSt.ReadUInt32( m_lcbAtrdExtra );
            }

            // Factoid bookmarks
            if (m_cfclcb > 134)
            {
                rSt.Seek(0x432);
                rSt.ReadInt32(m_fcPlcfBkfFactoid);
                rSt.ReadUInt32(m_lcbPlcfBkfFactoid);

                rSt.Seek(0x442);
                rSt.ReadInt32(m_fcPlcfBklFactoid);
                rSt.ReadUInt32(m_lcbPlcfBklFactoid);

                rSt.Seek(0x44a);
                rSt.ReadInt32(m_fcFactoidData);
                rSt.ReadUInt32(m_lcbFactoidData);
            }

            if( ERRCODE_NONE != rSt.GetError() )
                m_nFibError = ERR_SWG_READ_ERROR;

            rSt.Seek( 0x5bc );          // Actual nFib introduced in Word 2003
            rSt.ReadUInt16( m_nFib_actual );

            rSt.Seek( nOldPos );
        }
    }
    else
    {
        m_nFibError = ERR_SWG_READ_ERROR;     // report error
    }
}

WW8Fib::WW8Fib(sal_uInt8 nVer, bool bDot):
    m_nVersion(nVer), m_fDot(false), m_fGlsy(false), m_fComplex(false), m_fHasPic(false), m_cQuickSaves(0),
    m_fEncrypted(false), m_fWhichTableStm(false), m_fReadOnlyRecommended(false),
    m_fWriteReservation(false), m_fExtChar(false), m_fFarEast(false), m_fObfuscated(false),
    m_fMac(false), m_fEmptySpecial(false), m_fLoadOverridePage(false), m_fFuturesavedUndo(false),
    m_fWord97Saved(false), m_fWord2000Saved(false)
        // in C++20 with P06831R1 "Default member initializers for bit-fields (revision 1)", the
        // above bit-field member initializations can be moved to the class definition
{
    if (8 == nVer)
    {
        m_fcMin = 0x800;
        m_wIdent = 0xa5ec;
        m_nFib = 0x0101;
        m_nFibBack = 0xbf;
        m_nProduct = 0x204D;
        m_fDot = bDot;

        m_csw = 0x0e;     // Is this really necessary???
        m_cfclcb = 0x88;  //      -""-
        m_clw = 0x16;     //      -""-
        m_pnFbpChpFirst = m_pnFbpPapFirst = m_pnFbpLvcFirst = 0x000fffff;
        m_fExtChar = true;
        m_fWord97Saved = m_fWord2000Saved = true;

        // Just a fancy way to write 'Caolan80'.
        m_wMagicCreated = 0x6143;
        m_wMagicRevised = 0x6C6F;
        m_wMagicCreatedPrivate = 0x6E61;
        m_wMagicRevisedPrivate = 0x3038;
    }
    else
    {
        m_fcMin = 0x300;
        m_wIdent = 0xa5dc;
        m_nFib = m_nFibBack = 0x65;
        m_nProduct = 0xc02d;
    }

    //If nFib is 0x00D9 or greater, then cQuickSaves MUST be 0xF
    m_cQuickSaves = m_nFib >= 0x00D9 ? 0xF : 0;

    // --> #i90932#
    m_lid = LanguageType(0x409); // LANGUAGE_ENGLISH_US

    LanguageType nLang = Application::GetSettings().GetLanguageTag().getLanguageType();
    m_fFarEast = MsLangId::isCJK(nLang);
    if (m_fFarEast)
        m_lidFE = nLang;
    else
        m_lidFE = m_lid;

    LanguageTag aLanguageTag( m_lid );
    LocaleDataWrapper aLocaleWrapper( std::move(aLanguageTag) );
    m_nNumDecimalSep = aLocaleWrapper.getNumDecimalSep()[0];
}


void WW8Fib::WriteHeader(SvStream& rStrm)
{
    bool bVer8 = 8 == m_nVersion;

    size_t nUnencryptedHdr = bVer8 ? 0x44 : 0x24;
    std::unique_ptr<sal_uInt8[]> pDataPtr( new sal_uInt8[ nUnencryptedHdr ] );
    sal_uInt8 *pData = pDataPtr.get();
    memset( pData, 0, nUnencryptedHdr );

    m_cbMac = rStrm.TellEnd();

    Set_UInt16( pData, m_wIdent );
    Set_UInt16( pData, m_nFib );
    Set_UInt16( pData, m_nProduct );
    Set_UInt16( pData, static_cast<sal_uInt16>(m_lid) );
    Set_UInt16( pData, m_pnNext );

    sal_uInt16 nBits16 = 0;
    if( m_fDot )          nBits16 |= 0x0001;
    if( m_fGlsy)          nBits16 |= 0x0002;
    if( m_fComplex )      nBits16 |= 0x0004;
    if( m_fHasPic )       nBits16 |= 0x0008;
    nBits16 |= (0xf0 & ( m_cQuickSaves << 4 ));
    if( m_fEncrypted )    nBits16 |= 0x0100;
    if( m_fWhichTableStm )  nBits16 |= 0x0200;

    if (m_fReadOnlyRecommended)
        nBits16 |= 0x0400;
    if (m_fWriteReservation)
        nBits16 |= 0x0800;

    if( m_fExtChar )      nBits16 |= 0x1000;
    if( m_fFarEast )      nBits16 |= 0x4000;  // #i90932#
    if( m_fObfuscated )   nBits16 |= 0x8000;
    Set_UInt16( pData, nBits16 );

    Set_UInt16( pData, m_nFibBack );
    Set_UInt16( pData, m_nHash );
    Set_UInt16( pData, m_nKey );
    Set_UInt8( pData, m_envr );

    sal_uInt8 nBits8 = 0;
    if( bVer8 )
    {
        if( m_fMac )                  nBits8 |= 0x0001;
        if( m_fEmptySpecial )         nBits8 |= 0x0002;
        if( m_fLoadOverridePage )     nBits8 |= 0x0004;
        if( m_fFuturesavedUndo )      nBits8 |= 0x0008;
        if( m_fWord97Saved )          nBits8 |= 0x0010;
        if( m_fWord2000Saved )        nBits8 |= 0x0020;
    }
    // under Ver67 these are only reserved
    Set_UInt8( pData, nBits8  );

    Set_UInt16( pData, m_chse );
    Set_UInt16( pData, m_chseTables );
    Set_UInt32( pData, m_fcMin );
    Set_UInt32( pData, m_fcMac );

// insertion for WW8

    // Marke: "rgsw"  Beginning of the array of shorts
    if( bVer8 )
    {
        Set_UInt16( pData, m_csw );
        Set_UInt16( pData, m_wMagicCreated );
        Set_UInt16( pData, m_wMagicRevised );
        Set_UInt16( pData, m_wMagicCreatedPrivate );
        Set_UInt16( pData, m_wMagicRevisedPrivate );
        pData += 9 * sizeof( sal_Int16 );
        Set_UInt16( pData, static_cast<sal_uInt16>(m_lidFE) );
        Set_UInt16( pData, m_clw );
    }

// end of the insertion for WW8

    // Marke: "rglw"  Beginning of the array of longs
    Set_UInt32( pData, m_cbMac );

    rStrm.WriteBytes(pDataPtr.get(), nUnencryptedHdr);
}

void WW8Fib::Write(SvStream& rStrm)
{
    bool bVer8 = 8 == m_nVersion;

    WriteHeader( rStrm );

    size_t nUnencryptedHdr = bVer8 ? 0x44 : 0x24;

    std::unique_ptr<sal_uInt8[]> pDataPtr( new sal_uInt8[ m_fcMin - nUnencryptedHdr ] );
    sal_uInt8 *pData = pDataPtr.get();
    memset( pData, 0, m_fcMin - nUnencryptedHdr );

    m_cbMac = rStrm.TellEnd();

    // ignore 2 longs, because they are unimportant
    pData += 2 * sizeof( sal_Int32);

    // skipping 2 more longs only at Ver67
    if( !bVer8 )
        pData += 2 * sizeof( sal_Int32);

    Set_UInt32( pData, m_ccpText );
    Set_UInt32( pData, m_ccpFootnote );
    Set_UInt32( pData, m_ccpHdr );
    Set_UInt32( pData, m_ccpMcr );
    Set_UInt32( pData, m_ccpAtn );
    Set_UInt32( pData, m_ccpEdn );
    Set_UInt32( pData, m_ccpTxbx );
    Set_UInt32( pData, m_ccpHdrTxbx );

        // only skip one more long at Ver67
    if( !bVer8 )
        pData += 1 * sizeof( sal_Int32);

// insertion for WW8
    if( bVer8 )
    {
        Set_UInt32( pData, m_pnFbpChpFirst );
        Set_UInt32( pData, m_pnChpFirst );
        Set_UInt32( pData, m_cpnBteChp );
        Set_UInt32( pData, m_pnFbpPapFirst );
        Set_UInt32( pData, m_pnPapFirst );
        Set_UInt32( pData, m_cpnBtePap );
        Set_UInt32( pData, m_pnFbpLvcFirst );
        Set_UInt32( pData, m_pnLvcFirst );
        Set_UInt32( pData, m_cpnBteLvc );
        Set_UInt32( pData, m_fcIslandFirst );
        Set_UInt32( pData, m_fcIslandLim );
        Set_UInt16( pData, m_cfclcb );
    }
// end of the insertion for WW8

    // Marke: "rgfclcb" Beginning of array of FC/LCB pairs.
    Set_UInt32( pData, m_fcStshfOrig );
    Set_UInt32( pData, m_lcbStshfOrig );
    Set_UInt32( pData, m_fcStshf );
    Set_UInt32( pData, m_lcbStshf );
    Set_UInt32( pData, m_fcPlcffndRef );
    Set_UInt32( pData, m_lcbPlcffndRef );
    Set_UInt32( pData, m_fcPlcffndText );
    Set_UInt32( pData, m_lcbPlcffndText );
    Set_UInt32( pData, m_fcPlcfandRef );
    Set_UInt32( pData, m_lcbPlcfandRef );
    Set_UInt32( pData, m_fcPlcfandText );
    Set_UInt32( pData, m_lcbPlcfandText );
    Set_UInt32( pData, m_fcPlcfsed );
    Set_UInt32( pData, m_lcbPlcfsed );
    Set_UInt32( pData, m_fcPlcfpad );
    Set_UInt32( pData, m_lcbPlcfpad );
    Set_UInt32( pData, m_fcPlcfphe );
    Set_UInt32( pData, m_lcbPlcfphe );
    Set_UInt32( pData, m_fcSttbfglsy );
    Set_UInt32( pData, m_lcbSttbfglsy );
    Set_UInt32( pData, m_fcPlcfglsy );
    Set_UInt32( pData, m_lcbPlcfglsy );
    Set_UInt32( pData, m_fcPlcfhdd );
    Set_UInt32( pData, m_lcbPlcfhdd );
    Set_UInt32( pData, m_fcPlcfbteChpx );
    Set_UInt32( pData, m_lcbPlcfbteChpx );
    Set_UInt32( pData, m_fcPlcfbtePapx );
    Set_UInt32( pData, m_lcbPlcfbtePapx );
    Set_UInt32( pData, m_fcPlcfsea );
    Set_UInt32( pData, m_lcbPlcfsea );
    Set_UInt32( pData, m_fcSttbfffn );
    Set_UInt32( pData, m_lcbSttbfffn );
    Set_UInt32( pData, m_fcPlcffldMom );
    Set_UInt32( pData, m_lcbPlcffldMom );
    Set_UInt32( pData, m_fcPlcffldHdr );
    Set_UInt32( pData, m_lcbPlcffldHdr );
    Set_UInt32( pData, m_fcPlcffldFootnote );
    Set_UInt32( pData, m_lcbPlcffldFootnote );
    Set_UInt32( pData, m_fcPlcffldAtn );
    Set_UInt32( pData, m_lcbPlcffldAtn );
    Set_UInt32( pData, m_fcPlcffldMcr );
    Set_UInt32( pData, m_lcbPlcffldMcr );
    Set_UInt32( pData, m_fcSttbfbkmk );
    Set_UInt32( pData, m_lcbSttbfbkmk );
    Set_UInt32( pData, m_fcPlcfbkf );
    Set_UInt32( pData, m_lcbPlcfbkf );
    Set_UInt32( pData, m_fcPlcfbkl );
    Set_UInt32( pData, m_lcbPlcfbkl );
    Set_UInt32( pData, m_fcCmds );
    Set_UInt32( pData, m_lcbCmds );
    Set_UInt32( pData, m_fcPlcfmcr );
    Set_UInt32( pData, m_lcbPlcfmcr );
    Set_UInt32( pData, m_fcSttbfmcr );
    Set_UInt32( pData, m_lcbSttbfmcr );
    Set_UInt32( pData, m_fcPrDrvr );
    Set_UInt32( pData, m_lcbPrDrvr );
    Set_UInt32( pData, m_fcPrEnvPort );
    Set_UInt32( pData, m_lcbPrEnvPort );
    Set_UInt32( pData, m_fcPrEnvLand );
    Set_UInt32( pData, m_lcbPrEnvLand );
    Set_UInt32( pData, m_fcWss );
    Set_UInt32( pData, m_lcbWss );
    Set_UInt32( pData, m_fcDop );
    Set_UInt32( pData, m_lcbDop );
    Set_UInt32( pData, m_fcSttbfAssoc );
    Set_UInt32( pData, m_lcbSttbfAssoc );
    Set_UInt32( pData, m_fcClx );
    Set_UInt32( pData, m_lcbClx );
    Set_UInt32( pData, m_fcPlcfpgdFootnote );
    Set_UInt32( pData, m_lcbPlcfpgdFootnote );
    Set_UInt32( pData, m_fcAutosaveSource );
    Set_UInt32( pData, m_lcbAutosaveSource );
    Set_UInt32( pData, m_fcGrpStAtnOwners );
    Set_UInt32( pData, m_lcbGrpStAtnOwners );
    Set_UInt32( pData, m_fcSttbfAtnbkmk );
    Set_UInt32( pData, m_lcbSttbfAtnbkmk );

    // only skip one more short at Ver67
    if( !bVer8 )
    {
        pData += 1*sizeof( sal_Int16);
        Set_UInt16( pData, o3tl::narrowing<sal_uInt16>(m_pnChpFirst) );
        Set_UInt16( pData, o3tl::narrowing<sal_uInt16>(m_pnPapFirst) );
        Set_UInt16( pData, o3tl::narrowing<sal_uInt16>(m_cpnBteChp) );
        Set_UInt16( pData, o3tl::narrowing<sal_uInt16>(m_cpnBtePap) );
    }

    Set_UInt32( pData, m_fcPlcfdoaMom ); // only at Ver67, in Ver8 unused
    Set_UInt32( pData, m_lcbPlcfdoaMom ); // only at Ver67, in Ver8 unused
    Set_UInt32( pData, m_fcPlcfdoaHdr ); // only at Ver67, in Ver8 unused
    Set_UInt32( pData, m_lcbPlcfdoaHdr ); // only at Ver67, in Ver8 unused

    Set_UInt32( pData, m_fcPlcfspaMom ); // in Ver67 empty reserve
    Set_UInt32( pData, m_lcbPlcfspaMom ); // in Ver67 empty reserve
    Set_UInt32( pData, m_fcPlcfspaHdr ); // in Ver67 empty reserve
    Set_UInt32( pData, m_lcbPlcfspaHdr ); // in Ver67 empty reserve

    Set_UInt32( pData, m_fcPlcfAtnbkf );
    Set_UInt32( pData, m_lcbPlcfAtnbkf );
    Set_UInt32( pData, m_fcPlcfAtnbkl );
    Set_UInt32( pData, m_lcbPlcfAtnbkl );
    Set_UInt32( pData, m_fcPms );
    Set_UInt32( pData, m_lcbPMS );
    Set_UInt32( pData, m_fcFormFieldSttbf );
    Set_UInt32( pData, m_lcbFormFieldSttbf );
    Set_UInt32( pData, m_fcPlcfendRef );
    Set_UInt32( pData, m_lcbPlcfendRef );
    Set_UInt32( pData, m_fcPlcfendText );
    Set_UInt32( pData, m_lcbPlcfendText );
    Set_UInt32( pData, m_fcPlcffldEdn );
    Set_UInt32( pData, m_lcbPlcffldEdn );
    Set_UInt32( pData, m_fcPlcfpgdEdn );
    Set_UInt32( pData, m_lcbPlcfpgdEdn );
    Set_UInt32( pData, m_fcDggInfo ); // in Ver67 empty reserve
    Set_UInt32( pData, m_lcbDggInfo ); // in Ver67 empty reserve
    Set_UInt32( pData, m_fcSttbfRMark );
    Set_UInt32( pData, m_lcbSttbfRMark );
    Set_UInt32( pData, m_fcSttbfCaption );
    Set_UInt32( pData, m_lcbSttbfCaption );
    Set_UInt32( pData, m_fcSttbAutoCaption );
    Set_UInt32( pData, m_lcbSttbAutoCaption );
    Set_UInt32( pData, m_fcPlcfwkb );
    Set_UInt32( pData, m_lcbPlcfwkb );
    Set_UInt32( pData, m_fcPlcfspl ); // in Ver67 empty reserve
    Set_UInt32( pData, m_lcbPlcfspl ); // in Ver67 empty reserve
    Set_UInt32( pData, m_fcPlcftxbxText );
    Set_UInt32( pData, m_lcbPlcftxbxText );
    Set_UInt32( pData, m_fcPlcffldTxbx );
    Set_UInt32( pData, m_lcbPlcffldTxbx );
    Set_UInt32( pData, m_fcPlcfHdrtxbxText );
    Set_UInt32( pData, m_lcbPlcfHdrtxbxText );
    Set_UInt32( pData, m_fcPlcffldHdrTxbx );
    Set_UInt32( pData, m_lcbPlcffldHdrTxbx );

    if( bVer8 )
    {
        pData += 0x2da - 0x27a;         // Pos + Offset (fcPlcfLst - fcStwUser)
        Set_UInt32( pData, m_fcSttbFnm);
        Set_UInt32( pData, m_lcbSttbFnm);
        Set_UInt32( pData, m_fcPlcfLst );
        Set_UInt32( pData, m_lcbPlcfLst );
        Set_UInt32( pData, m_fcPlfLfo );
        Set_UInt32( pData, m_lcbPlfLfo );
        Set_UInt32( pData, m_fcPlcftxbxBkd );
        Set_UInt32( pData, m_lcbPlcftxbxBkd );
        Set_UInt32( pData, m_fcPlcfHdrtxbxBkd );
        Set_UInt32( pData, m_lcbPlcfHdrtxbxBkd );

        pData += 0x372 - 0x302; // Pos + Offset (fcSttbListNames - fcDocUndo)
        Set_UInt32( pData, m_fcSttbListNames );
        Set_UInt32( pData, m_lcbSttbListNames );

        pData += 0x382 - 0x37A;
        Set_UInt32( pData, m_fcPlcfTch );
        Set_UInt32( pData, m_lcbPlcfTch );

        pData += 0x3FA - 0x38A;
        Set_UInt16( pData, sal_uInt16(0x0002));
        Set_UInt16( pData, sal_uInt16(0x00D9));

        pData += 0x41A - 0x3FE;
        Set_UInt32( pData, m_fcAtrdExtra );
        Set_UInt32( pData, m_lcbAtrdExtra );

        pData += 0x42a - 0x422;
        Set_UInt32(pData, m_fcSttbfBkmkFactoid);
        Set_UInt32(pData, m_lcbSttbfBkmkFactoid);
        Set_UInt32(pData, m_fcPlcfBkfFactoid);
        Set_UInt32(pData, m_lcbPlcfBkfFactoid);

        pData += 0x442 - 0x43A;
        Set_UInt32(pData, m_fcPlcfBklFactoid);
        Set_UInt32(pData, m_lcbPlcfBklFactoid);
        Set_UInt32(pData, m_fcFactoidData);
        Set_UInt32(pData, m_lcbFactoidData);

        pData += 0x4BA - 0x452;
        Set_UInt32(pData, m_fcPlcffactoid);
        Set_UInt32(pData, m_lcbPlcffactoid);

        pData += 0x4DA - 0x4c2;
        Set_UInt32( pData, m_fcHplxsdr );
        Set_UInt32( pData, 0);
    }

    rStrm.WriteBytes(pDataPtr.get(), m_fcMin - nUnencryptedHdr);
}

rtl_TextEncoding WW8Fib::GetFIBCharset(sal_uInt16 chs, LanguageType nLidLocale)
{
    OSL_ENSURE(chs <= 0x100, "overflowed winword charset set");
    if (chs == 0x0100)
        return RTL_TEXTENCODING_APPLE_ROMAN;
    if (chs == 0 && static_cast<sal_uInt16>(nLidLocale) >= 999)
    {
        /*
         nLidLocale:
            language stamp -- localized version In pre-WinWord 2.0 files this
            value was the nLocale. If value is < 999, then it is the nLocale,
            otherwise it is the lid.
        */
        css::lang::Locale aLocale(LanguageTag::convertToLocale(nLidLocale));
        return msfilter::util::getBestTextEncodingFromLocale(aLocale);
    }
    return rtl_getTextEncodingFromWindowsCharset(static_cast<sal_uInt8>(chs));
}

MSOFactoidType::MSOFactoidType()
    : m_nId(0)
{
}

namespace MSOPBString
{
static OUString Read(SvStream& rStream)
{
    OUString aRet;

    sal_uInt16 nBuf(0);
    rStream.ReadUInt16(nBuf);
    sal_uInt16 nCch = nBuf & 0x7fff; // Bits 1..15.
    bool bAnsiString = (nBuf & (1 << 15)) >> 15; // 16th bit.
    if (bAnsiString)
        aRet = OStringToOUString(read_uInt8s_ToOString(rStream, nCch), RTL_TEXTENCODING_ASCII_US);
    else
        aRet = read_uInt16s_ToOUString(rStream, nCch);

    return aRet;
}

static void Write(std::u16string_view aString, SvStream& rStream)
{
    sal_uInt16 nBuf = 0;
    nBuf |= sal_Int32(aString.size()); // cch, 0..14th bits.
    nBuf |= 0x8000; // fAnsiString, 15th bit.
    rStream.WriteUInt16(nBuf);
    SwWW8Writer::WriteString8(rStream, aString, false, RTL_TEXTENCODING_ASCII_US);
}
};

void MSOFactoidType::Read(SvStream& rStream)
{
    sal_uInt32 cbFactoid(0);
    rStream.ReadUInt32(cbFactoid);
    rStream.ReadUInt32(m_nId);
    m_aUri = MSOPBString::Read(rStream);
    m_aTag = MSOPBString::Read(rStream);
    MSOPBString::Read(rStream); // rgbDownloadURL
}

void MSOFactoidType::Write(WW8Export& rExport)
{
    SvStream& rStream = *rExport.m_pTableStrm;

    SvMemoryStream aStream;
    aStream.WriteUInt32(m_nId); // id
    MSOPBString::Write(m_aUri, aStream);
    MSOPBString::Write(m_aTag, aStream);
    MSOPBString::Write(u"", aStream); // rgbDownloadURL
    rStream.WriteUInt32(aStream.Tell());
    aStream.Seek(0);
    rStream.WriteStream(aStream);
}

void MSOPropertyBagStore::Read(SvStream& rStream)
{
    sal_uInt32 cFactoidType(0);
    rStream.ReadUInt32(cFactoidType);
    for (sal_uInt32 i = 0; i < cFactoidType && rStream.good(); ++i)
    {
        MSOFactoidType aFactoidType;
        aFactoidType.Read(rStream);
        m_aFactoidTypes.push_back(aFactoidType);
    }
    sal_uInt16 cbHdr(0);
    rStream.ReadUInt16(cbHdr);
    SAL_WARN_IF(cbHdr != 0xc, "sw.ww8", "MSOPropertyBagStore::Read: unexpected cbHdr");
    sal_uInt16 nVer(0);
    rStream.ReadUInt16(nVer);
    SAL_WARN_IF(nVer != 0x0100, "sw.ww8", "MSOPropertyBagStore::Read: unexpected nVer");
    rStream.SeekRel(4); // cfactoid
    sal_uInt32 nCste(0);
    rStream.ReadUInt32(nCste);

    //each string has a 2 byte len record at the start
    const size_t nMaxPossibleRecords = rStream.remainingSize() / sizeof(sal_uInt16);
    if (nCste > nMaxPossibleRecords)
    {
        SAL_WARN("sw.ww8", nCste << " records claimed, but max possible is " << nMaxPossibleRecords);
        nCste = nMaxPossibleRecords;
    }

    for (sal_uInt32 i = 0; i < nCste; ++i)
    {
        OUString aString = MSOPBString::Read(rStream);
        m_aStringTable.push_back(aString);
    }
}

void MSOPropertyBagStore::Write(WW8Export& rExport)
{
    SvStream& rStream = *rExport.m_pTableStrm;
    rStream.WriteUInt32(m_aFactoidTypes.size()); // cFactoidType
    for (MSOFactoidType& rType : m_aFactoidTypes)
        rType.Write(rExport);
    rStream.WriteUInt16(0xc); // cbHdr
    rStream.WriteUInt16(0x0100); // sVer
    rStream.WriteUInt32(0); // cfactoid
    rStream.WriteUInt32(m_aStringTable.size()); // cste
    for (const OUString& rString : m_aStringTable)
        MSOPBString::Write(rString, rStream);
}

MSOProperty::MSOProperty()
    : m_nKey(0)
    , m_nValue(0)
{
}

void MSOProperty::Read(SvStream& rStream)
{
    rStream.ReadUInt32(m_nKey);
    rStream.ReadUInt32(m_nValue);
}

void MSOProperty::Write(SvStream& rStream)
{
    rStream.WriteUInt32(m_nKey);
    rStream.WriteUInt32(m_nValue);
}

MSOPropertyBag::MSOPropertyBag()
    : m_nId(0)
{
}

bool MSOPropertyBag::Read(SvStream& rStream)
{
    rStream.ReadUInt16(m_nId);
    sal_uInt16 cProp(0);
    rStream.ReadUInt16(cProp);
    if (!rStream.good())
        return false;
    rStream.SeekRel(2); // cbUnknown
    //each MSOProperty is 8 bytes in size
    const size_t nMaxPossibleRecords = rStream.remainingSize() / 8;
    if (cProp > nMaxPossibleRecords)
    {
        SAL_WARN("sw.ww8", cProp << " records claimed, but max possible is " << nMaxPossibleRecords);
        cProp = nMaxPossibleRecords;
    }
    for (sal_uInt16 i = 0; i < cProp && rStream.good(); ++i)
    {
        MSOProperty aProperty;
        aProperty.Read(rStream);
        m_aProperties.push_back(aProperty);
    }
    return rStream.good();
}

void MSOPropertyBag::Write(WW8Export& rExport)
{
    SvStream& rStream = *rExport.m_pTableStrm;
    rStream.WriteUInt16(m_nId);
    rStream.WriteUInt16(m_aProperties.size());
    rStream.WriteUInt16(0); // cbUnknown
    for (MSOProperty& rProperty : m_aProperties)
        rProperty.Write(rStream);
}

void WW8SmartTagData::Read(SvStream& rStream, WW8_FC fcFactoidData, sal_uInt32 lcbFactoidData)
{
    sal_uInt64 nOldPosition = rStream.Tell();
    if (!checkSeek(rStream, fcFactoidData))
        return;

    m_aPropBagStore.Read(rStream);
    while (rStream.good() && rStream.Tell() < fcFactoidData + lcbFactoidData)
    {
        MSOPropertyBag aPropertyBag;
        if (!aPropertyBag.Read(rStream))
            break;
        m_aPropBags.push_back(aPropertyBag);
    }

    rStream.Seek(nOldPosition);
}

void WW8SmartTagData::Write(WW8Export& rExport)
{
    m_aPropBagStore.Write(rExport);
    for (MSOPropertyBag& rPropertyBag : m_aPropBags)
        rPropertyBag.Write(rExport);
}

WW8Style::WW8Style(SvStream& rStream, WW8Fib& rFibPara)
    : m_rFib(rFibPara), m_rStream(rStream), m_cstd(0), m_cbSTDBaseInFile(0), m_fStdStylenamesWritten(0)
    , m_stiMaxWhenSaved(0), m_istdMaxFixedWhenSaved(0), m_nVerBuiltInNamesWhenSaved(0)
    , m_ftcAsci(0), m_ftcFE(0), m_ftcOther(0), m_ftcBi(0)
{
    if (!checkSeek(m_rStream, m_rFib.m_fcStshf))
        return;

    sal_uInt16 cbStshi = 0; //  2 bytes size of the following STSHI structure
    sal_uInt32 nRemaining = m_rFib.m_lcbStshf;
    const sal_uInt32 nMinValidStshi = 4;

    if (m_rFib.GetFIBVersion() <= ww::eWW2)
    {
        cbStshi = 0;
        m_cstd = 256;
    }
    else
    {
        if (m_rFib.m_nFib < 67) // old Version ? (need to find this again to fix)
            cbStshi = nMinValidStshi;
        else    // new version
        {
            if (nRemaining < sizeof(cbStshi))
                return;
            // reads the length of the structure in the file
            m_rStream.ReadUInt16( cbStshi );
            nRemaining-=2;
        }
    }

    cbStshi = std::min(static_cast<sal_uInt32>(cbStshi), nRemaining);
    if (cbStshi < nMinValidStshi)
        return;

    const sal_uInt16 nRead = cbStshi;
    do
    {
        m_rStream.ReadUInt16( m_cstd );

        m_rStream.ReadUInt16( m_cbSTDBaseInFile );

        if(  6 > nRead ) break;

        sal_uInt16 a16Bit;
        m_rStream.ReadUInt16( a16Bit );
        m_fStdStylenamesWritten = a16Bit & 0x0001;

        if(  8 > nRead ) break;
        m_rStream.ReadUInt16( m_stiMaxWhenSaved );

        if( 10 > nRead ) break;
        m_rStream.ReadUInt16( m_istdMaxFixedWhenSaved );

        if( 12 > nRead ) break;
        m_rStream.ReadUInt16( m_nVerBuiltInNamesWhenSaved );

        if( 14 > nRead ) break;
        m_rStream.ReadUInt16( m_ftcAsci );

        if( 16 > nRead ) break;
        m_rStream.ReadUInt16( m_ftcFE );

        if ( 18 > nRead ) break;
        m_rStream.ReadUInt16( m_ftcOther );

        m_ftcBi = m_ftcOther;

        if ( 20 > nRead ) break;
        m_rStream.ReadUInt16( m_ftcBi );

        // p.r.n. ignore the rest
        if( 20 < nRead )
            m_rStream.SeekRel( nRead-20 );
    }
    while( false ); // trick: the block above will be passed through exactly one time
                //            and that's why we can early exit with "break".

    nRemaining -= cbStshi;

    //There will be stshi.cstd (cbSTD, STD) pairs in the file following the
    //STSHI. Note that styles can be empty, i.e. cbSTD == 0
    const sal_uInt32 nMinRecordSize = sizeof(sal_uInt16);
    const sal_uInt16 nMaxPossibleRecords = nRemaining/nMinRecordSize;

    OSL_ENSURE(m_cstd <= nMaxPossibleRecords,
        "allegedly more styles that available data");
    m_cstd = o3tl::sanitizing_min(m_cstd, nMaxPossibleRecords);
}

// Read1STDFixed() reads a style. If the style is completely existent,
// so it has no empty slot, we should allocate memory and a pointer should
// reference to STD (perhaps filled with 0). If the slot is empty,
// it will return a null pointer.
std::unique_ptr<WW8_STD> WW8Style::Read1STDFixed(sal_uInt16& rSkip)
{
    if (m_rStream.remainingSize() < 2)
    {
        rSkip = 0;
        return nullptr;
    }

    std::unique_ptr<WW8_STD> pStd;

    sal_uInt16 cbStd(0);
    m_rStream.ReadUInt16(cbStd);   // read length

    if( cbStd >= m_cbSTDBaseInFile )
    {
        // Fixed part completely available

        // read fixed part of STD
        pStd.reset(new WW8_STD);
        memset( pStd.get(), 0, sizeof( *pStd ) );

        do
        {
            if( 2 > m_cbSTDBaseInFile ) break;

            sal_uInt16 a16Bit = 0;
            m_rStream.ReadUInt16( a16Bit );
            pStd->sti          =        a16Bit & 0x0fff  ;
            pStd->fScratch     = sal_uInt16(0 != ( a16Bit & 0x1000 ));
            pStd->fInvalHeight = sal_uInt16(0 != ( a16Bit & 0x2000 ));
            pStd->fHasUpe      = sal_uInt16(0 != ( a16Bit & 0x4000 ));
            pStd->fMassCopy    = sal_uInt16(0 != ( a16Bit & 0x8000 ));

            if( 4 > m_cbSTDBaseInFile ) break;
            a16Bit = 0;
            m_rStream.ReadUInt16( a16Bit );
            pStd->sgc      =   a16Bit & 0x000f       ;
            pStd->istdBase = ( a16Bit & 0xfff0 ) >> 4;

            if( 6 > m_cbSTDBaseInFile ) break;
            a16Bit = 0;
            m_rStream.ReadUInt16( a16Bit );
            pStd->cupx     =   a16Bit & 0x000f       ;
            pStd->istdNext = ( a16Bit & 0xfff0 ) >> 4;

            if( 8 > m_cbSTDBaseInFile ) break;
            m_rStream.ReadUInt16( pStd->bchUpe );

            // from Ver8 this two fields should be added:
            if (10 > m_cbSTDBaseInFile) break;
            a16Bit = 0;
            m_rStream.ReadUInt16( a16Bit );
            pStd->fAutoRedef =   a16Bit & 0x0001       ;
            pStd->fHidden    = ( a16Bit & 0x0002 ) >> 1;
            // You never know: cautionary skipped
            if (m_cbSTDBaseInFile > 10)
            {
                auto nSkip = std::min<sal_uInt64>(m_cbSTDBaseInFile - 10, m_rStream.remainingSize());
                m_rStream.Seek(m_rStream.Tell() + nSkip);
            }
        }
        while( false ); // trick: the block above will passed through exactly one time
                    //   and can be left early with a "break"

        if (!m_rStream.good() || !m_cbSTDBaseInFile)
        {
            pStd.reset(); // report error with NULL
        }

        rSkip = cbStd - m_cbSTDBaseInFile;
    }
    else
    {           // Fixed part too short
        if( cbStd )
            m_rStream.SeekRel( cbStd );           // skip leftovers
        rSkip = 0;
    }
    return pStd;
}

std::unique_ptr<WW8_STD> WW8Style::Read1Style(sal_uInt16& rSkip, OUString* pString)
{
    // Attention: MacWord-Documents have their Stylenames
    // always in ANSI, even if eStructCharSet == CHARSET_MAC !!

    std::unique_ptr<WW8_STD> pStd = Read1STDFixed(rSkip);         // read STD

    // string desired?
    if( pString )
    {   // real style?
        if ( pStd )
        {
            sal_Int32 nLenStringBytes = 0;
            switch( m_rFib.m_nVersion )
            {
                case 6:
                case 7:
                    // read pascal string
                    *pString = read_uInt8_BeltAndBracesString(m_rStream, RTL_TEXTENCODING_MS_1252);
                    // leading len and trailing zero --> 2
                    nLenStringBytes = pString->getLength() + 2;
                    break;
                case 8:
                    // handle Unicode-String with leading length short and
                    // trailing zero
                    if (TestBeltAndBraces(m_rStream))
                    {
                        *pString = read_uInt16_BeltAndBracesString(m_rStream);
                        nLenStringBytes = (pString->getLength() + 2) * 2;
                    }
                    else
                    {
                        /*
                        #i8114#
                        This is supposed to be impossible, it's just supposed
                        to be 16 bit count followed by the string and ending
                        in a 0 short. But "Lotus SmartSuite Product: Word Pro"
                        is creating invalid style names in ww7- format. So we
                        use the belt and braces of the ms strings to see if
                        they are not corrupt. If they are then we try them as
                        8bit ones
                        */
                        *pString = read_uInt8_BeltAndBracesString(m_rStream,RTL_TEXTENCODING_MS_1252);
                        // leading len and trailing zero --> 2
                        nLenStringBytes = pString->getLength() + 2;
                    }
                    break;
                default:
                    OSL_ENSURE(false, "It was forgotten to code nVersion!");
                    break;
            }
            if (nLenStringBytes > rSkip)
            {
                SAL_WARN("sw.ww8", "WW8Style structure corrupt");
                nLenStringBytes = rSkip;
            }
            rSkip -= nLenStringBytes;
        }
        else
            pString->clear();   // can not return a name
    }
    return pStd;
}

namespace {
const sal_uInt16 maxStrSize = 65;

struct WW8_FFN_Ver6
{
    WW8_FFN_BASE base;
    // from Ver6
    char szFfn[maxStrSize]; // 0x6 or 0x40 from Ver8 on zero terminated string that
                        // records name of font.
                        // Maximal size of szFfn is 65 characters.
                        // Attention: This array can also be smaller!!!
                        // Possibly followed by a second sz which records the
                        // name of an alternate font to use if the first named
                        // font does not exist on this system.
};

}

// #i43762# check font name for illegal characters
static void lcl_checkFontname( OUString& sString )
{
    // for efficiency, we'd like to use String methods as far as possible.
    // Hence, we will:
    // 1) convert all invalid chars to \u0001
    // 2) then erase all \u0001 chars (if anywhere found), and
    // 3) erase leading/trailing ';', in case a font name was
    //    completely removed

    // convert all invalid chars to \u0001
    OUStringBuffer aBuf(sString);
    const sal_Int32 nLen = aBuf.getLength();
    bool bFound = false;
    for ( sal_Int32 n = 0; n < nLen; ++n )
    {
        if ( aBuf[n] < 0x20 )
        {
            aBuf[n] = 1;
            bFound = true;
        }
    }
    sString = aBuf.makeStringAndClear();

    // if anything was found, remove \u0001 + leading/trailing ';'
    if( bFound )
    {
        sString = comphelper::string::strip(sString.replaceAll("\001", ""), ';');
    }
}

namespace
{
    sal_uInt16 calcMaxFonts(sal_uInt8 *p, sal_Int32 nFFn)
    {
        // Figure out the max number of fonts defined here
        sal_uInt16 nMax = 0;
        sal_Int32 nRemaining = nFFn;
        while (nRemaining)
        {
            //p[0] is cbFfnM1, the alleged total length of FFN - 1.
            //i.e. length after cbFfnM1
            const sal_uInt16 cbFfnM1 = *p++;
            --nRemaining;

            if (cbFfnM1 > nRemaining)
                break;

            nMax++;
            nRemaining -= cbFfnM1;
            p += cbFfnM1;
        }
        return nMax;
    }

    template<typename T> bool readU8(
        sal_uInt8 const * p, std::size_t offset, sal_uInt8 const * pEnd,
        T * value)
    {
        assert(p <= pEnd);
        assert(value != nullptr);
        if (offset >= o3tl::make_unsigned(pEnd - p)) {
            return false;
        }
        *value = p[offset];
        return true;
    }

    bool readS16(
        sal_uInt8 const * p, std::size_t offset, sal_uInt8 const * pEnd,
        short * value)
    {
        assert(p <= pEnd);
        assert(value != nullptr);
        if (offset > o3tl::make_unsigned(pEnd - p)
            || static_cast<std::size_t>(pEnd - p) - offset < 2)
        {
            return false;
        }
        *value = unsigned(p[offset]) + (unsigned(p[offset + 1]) << 8);
        return true;
    }

    sal_Int32 getStringLengthWithMax(
        sal_uInt8 const * p, std::size_t offset, sal_uInt8 const * pEnd, std::size_t maxchars)
    {
        assert(p <= pEnd);
        assert(pEnd - p <= SAL_MAX_INT32);
        if (offset >= o3tl::make_unsigned(pEnd - p)) {
            return -1;
        }
        std::size_t nbytes = static_cast<std::size_t>(pEnd - p) - offset;
        std::size_t nsearch = std::min(nbytes, maxchars + 1);
        void const * p2 = std::memchr(p + offset, 0, nsearch);
        if (p2 == nullptr) {
            return -1;
        }
        return static_cast<sal_uInt8 const *>(p2) - (p + offset);
    }
}

WW8Fonts::WW8Fonts( SvStream& rSt, WW8Fib const & rFib )
{
    // Attention: MacWord-Documents have their Fontnames
    // always in ANSI, even if eStructCharSet == CHARSET_MAC !!
    if( rFib.m_lcbSttbfffn <= 2 )
    {
        OSL_ENSURE( false, "font table is broken! (rFib.lcbSttbfffn < 2)" );
        return;
    }

    if (!checkSeek(rSt, rFib.m_fcSttbfffn))
        return;

    sal_Int32 nFFn = rFib.m_lcbSttbfffn - 2;

    const sal_uInt64 nMaxPossible = rSt.remainingSize();
    if (o3tl::make_unsigned(nFFn) > nMaxPossible)
    {
        SAL_WARN("sw.ww8", "FFN structure longer than available data");
        nFFn = nMaxPossible;
    }

    // allocate Font Array
    std::vector<sal_uInt8> aA(nFFn);
    memset(aA.data(), 0, nFFn);

    ww::WordVersion eVersion = rFib.GetFIBVersion();

    sal_uInt16 nMax(0);
    if( eVersion >= ww::eWW8 )
    {
        // bVer8: read the count of strings in nMax
        rSt.ReadUInt16(nMax);
    }

    // Ver8:  skip undefined uint16
    // Ver67: skip the herein stored total byte of structure
    //        - we already got that information in rFib.lcbSttbfffn
    rSt.SeekRel( 2 );

    // read all font information
    nFFn = rSt.ReadBytes(aA.data(), nFFn);
    sal_uInt8 * const pEnd = aA.data() + nFFn;
    const sal_uInt16 nCalcMax = calcMaxFonts(aA.data(), nFFn);

    if (eVersion < ww::eWW8)
        nMax = nCalcMax;
    else
    {
        //newer versions include supportive count of fonts, so take min of that
        //and calced max
        nMax = std::min(nMax, nCalcMax);
    }

    if (nMax)
    {
        // allocate Index Array
        m_aFontA.resize(nMax);
        WW8_FFN* p = m_aFontA.data();

        if( eVersion <= ww::eWW2 )
        {
            sal_uInt8 const * pVer2 = aA.data();
            sal_uInt16 i = 0;
            for(; i<nMax; ++i, ++p)
            {
                if (!readU8(
                        pVer2, offsetof(WW8_FFN_BASE, cbFfnM1), pEnd,
                        &p->aFFNBase.cbFfnM1))
                {
                    break;
                }

                p->aFFNBase.prg       =  0;
                p->aFFNBase.fTrueType = 0;
                p->aFFNBase.ff        = 0;

                if (!(readU8(pVer2, 1, pEnd, &p->aFFNBase.wWeight)
                      && readU8(pVer2, 2, pEnd, &p->aFFNBase.chs)))
                {
                    break;
                }
                /*
                 #i8726# 7- seems to encode the name in the same encoding as
                 the font, e.g load the doc in 97 and save to see the unicode
                 ver of the asian fontnames in that example to confirm.
                */
                rtl_TextEncoding eEnc = WW8Fib::GetFIBCharset(p->aFFNBase.chs, rFib.m_lid);
                if ((eEnc == RTL_TEXTENCODING_SYMBOL) || (eEnc == RTL_TEXTENCODING_DONTKNOW))
                    eEnc = RTL_TEXTENCODING_MS_1252;

                const size_t nStringOffset = 1 + 2;
                sal_Int32 n = getStringLengthWithMax(pVer2, nStringOffset, pEnd, maxStrSize);
                if (n == -1) {
                    break;
                }
                p->sFontname = OUString(
                    reinterpret_cast<char const *>(pVer2 + nStringOffset), n, eEnc);
                pVer2 = pVer2 + p->aFFNBase.cbFfnM1 + 1;
            }
            nMax = i;
        }
        else if( eVersion < ww::eWW8 )
        {
            sal_uInt8 const * pVer6 = aA.data();
            sal_uInt16 i = 0;
            for(; i<nMax; ++i, ++p)
            {
                if (!readU8(
                        pVer6, offsetof(WW8_FFN_BASE, cbFfnM1), pEnd,
                        &p->aFFNBase.cbFfnM1))
                {
                    break;
                }
                sal_uInt8 c2;
                if (!readU8(pVer6, 1, pEnd, &c2)) {
                    break;
                }

                p->aFFNBase.prg       =  c2 & 0x02;
                p->aFFNBase.fTrueType = (c2 & 0x04) >> 2;
                // skip a reserve bit
                p->aFFNBase.ff        = (c2 & 0x70) >> 4;

                if (!(readS16(
                          pVer6, offsetof(WW8_FFN_BASE, wWeight), pEnd,
                          &p->aFFNBase.wWeight)
                      && readU8(
                          pVer6, offsetof(WW8_FFN_BASE, chs), pEnd, &p->aFFNBase.chs)
                      && readU8(
                          pVer6, offsetof(WW8_FFN_BASE, ibszAlt), pEnd,
                          &p->aFFNBase.ibszAlt)))
                {
                    break;
                }
                /*
                 #i8726# 7- seems to encode the name in the same encoding as
                 the font, e.g load the doc in 97 and save to see the unicode
                 ver of the asian fontnames in that example to confirm.
                 */
                rtl_TextEncoding eEnc = WW8Fib::GetFIBCharset(p->aFFNBase.chs, rFib.m_lid);
                if ((eEnc == RTL_TEXTENCODING_SYMBOL) || (eEnc == RTL_TEXTENCODING_DONTKNOW))
                    eEnc = RTL_TEXTENCODING_MS_1252;
                const size_t nStringOffset = offsetof(WW8_FFN_Ver6, szFfn);
                sal_Int32 n = getStringLengthWithMax(pVer6, nStringOffset, pEnd, maxStrSize);
                if (n == -1) {
                    break;
                }
                p->sFontname = OUString(reinterpret_cast<char const*>(pVer6 + nStringOffset), n, eEnc);
                if (p->aFFNBase.ibszAlt && p->aFFNBase.ibszAlt < maxStrSize) //don't start after end of string
                {
                    const size_t nAltStringOffset = offsetof(WW8_FFN_Ver6, szFfn) + p->aFFNBase.ibszAlt;
                    n = getStringLengthWithMax(pVer6, nAltStringOffset, pEnd, maxStrSize);
                    if (n == -1) {
                        break;
                    }
                    p->sFontname += ";" + OUString(reinterpret_cast<char const*>(pVer6 + nAltStringOffset),
                        n, eEnc);
                }
                else
                {
                    //#i18369# if it's a symbol font set Symbol as fallback
                    if (
                         RTL_TEXTENCODING_SYMBOL == WW8Fib::GetFIBCharset(p->aFFNBase.chs, rFib.m_lid)
                         && p->sFontname!="Symbol"
                       )
                    {
                        p->sFontname += ";Symbol";
                    }
                }
                pVer6 = pVer6 + p->aFFNBase.cbFfnM1 + 1;
            }
            nMax = i;
        }
        else
        {
            //count of bytes in minimum FontFamilyInformation payload
            const sal_uInt8 cbMinFFNPayload = 41;
            sal_uInt16 nValidFonts = 0;
            sal_Int32 nRemainingFFn = nFFn;
            sal_uInt8* pRaw = aA.data();
            for (sal_uInt16 i=0; i < nMax && nRemainingFFn; ++i, ++p)
            {
                //pRaw[0] is cbFfnM1, the alleged total length of FFN - 1
                //i.e. length after cbFfnM1
                sal_uInt8 cbFfnM1 = *pRaw++;
                --nRemainingFFn;

                if (cbFfnM1 > nRemainingFFn)
                    break;

                if (cbFfnM1 < cbMinFFNPayload)
                    break;

                p->aFFNBase.cbFfnM1 = cbFfnM1;

                sal_uInt8 *pVer8 = pRaw;

                sal_uInt8 c2 = *pVer8++;
                --cbFfnM1;

                p->aFFNBase.prg = c2 & 0x02;
                p->aFFNBase.fTrueType = (c2 & 0x04) >> 2;
                // skip a reserve bit
                p->aFFNBase.ff = (c2 & 0x70) >> 4;

                p->aFFNBase.wWeight = SVBT16ToUInt16(*reinterpret_cast<SVBT16*>(pVer8));
                pVer8+=2;
                cbFfnM1-=2;

                p->aFFNBase.chs = *pVer8++;
                --cbFfnM1;

                p->aFFNBase.ibszAlt = *pVer8++;
                --cbFfnM1;

                pVer8 += 10; //PANOSE
                cbFfnM1-=10;
                pVer8 += 24; //FONTSIGNATURE
                cbFfnM1-=24;

                assert(cbFfnM1 >= 2);

                sal_uInt8 nMaxNullTerminatedPossible = cbFfnM1/2 - 1;
                sal_Unicode *pPrimary = reinterpret_cast<sal_Unicode*>(pVer8);
                pPrimary[nMaxNullTerminatedPossible] = 0;
#ifdef OSL_BIGENDIAN
                swapEndian(pPrimary);
#endif
                p->sFontname = pPrimary;
                if (p->aFFNBase.ibszAlt && p->aFFNBase.ibszAlt < nMaxNullTerminatedPossible)
                {
                    sal_Unicode *pSecondary = pPrimary + p->aFFNBase.ibszAlt;
#ifdef OSL_BIGENDIAN
                    swapEndian(pSecondary);
#endif
                    p->sFontname += OUString::Concat(";") + pSecondary;
                }

                // #i43762# check font name for illegal characters
                lcl_checkFontname( p->sFontname );

                // set pointer one font back to original array
                pRaw += p->aFFNBase.cbFfnM1;
                nRemainingFFn -= p->aFFNBase.cbFfnM1;
                ++nValidFonts;
            }
            OSL_ENSURE(nMax == nValidFonts, "Font count differs with availability");
            nMax = std::min(nMax, nValidFonts);
        }
    }
    m_aFontA.resize(nMax);
    m_aFontA.shrink_to_fit();
}

const WW8_FFN* WW8Fonts::GetFont( sal_uInt16 nNum ) const
{
    if (nNum >= m_aFontA.size())
        return nullptr;

    return &m_aFontA[nNum];
}

// Search after a header/footer for an index in the ww list from header/footer

// specials for WinWord6 and -7:
//
// 1) At the start of reading we must build WWPLCF_HdFt with Fib and Dop
// 2) The main text must be read sequentially over all sections
// 3) For every header/footer in the main text, we must call UpdateIndex()
//    exactly once with the parameter from the attribute.
//    (per section can be maximally one). This call must take place *after*
//    the last call from GetTextPos().
// 4) GetTextPos() can be called with exactly one flag
//    out of WW8_{FOOTER,HEADER}_{ODD,EVEN,FIRST} (Do not change!)
//  -> maybe we can get a right result then

WW8PLCF_HdFt::WW8PLCF_HdFt( SvStream* pSt, WW8Fib const & rFib, WW8Dop const & rDop )
    : m_aPLCF(*pSt, rFib.m_fcPlcfhdd , rFib.m_lcbPlcfhdd , 0)
{
    m_nIdxOffset = 0;

     /*
      This dop.grpfIhdt has a bit set for each special
      footnote *and endnote!!* separator,continuation separator, and
      continuation notice entry, the documentation does not mention the
      endnote separators, the documentation also gets the index numbers
      backwards when specifying which bits to test. The bottom six bits
      of this value must be tested and skipped over. Each section's
      grpfIhdt is then tested for the existence of the appropriate headers
      and footers, at the end of each section the nIdxOffset must be updated
      to point to the beginning of the next section's group of headers and
      footers in this PLCF, UpdateIndex does that task.
      */
    for( sal_uInt8 nI = 0x1; nI <= 0x20; nI <<= 1 )
        if( nI & rDop.grpfIhdt )                // bit set?
            m_nIdxOffset++;
}

bool WW8PLCF_HdFt::GetTextPos(sal_uInt8 grpfIhdt, sal_uInt8 nWhich, WW8_CP& rStart,
    WW8_CP& rLen)
{
    sal_uInt8 nI = 0x01;
    short nIdx = m_nIdxOffset;
    while (true)
    {
        if( nI & nWhich )
            break;                      // found
        if( grpfIhdt & nI )
            nIdx++;                     // uninteresting Header / Footer
        nI <<= 1;                       // text next bit
        if( nI > 0x20 )
            return false;               // not found
    }
                                        // nIdx is HdFt-Index
    WW8_CP nEnd;
    void* pData;

    m_aPLCF.SetIdx( nIdx );               // Lookup suitable CP
    m_aPLCF.Get( rStart, nEnd, pData );
    if (nEnd < rStart)
    {
        SAL_WARN("sw.ww8", "End " << nEnd << " before Start " << rStart);
        return false;
    }

    bool bFail = o3tl::checked_sub(nEnd, rStart, rLen);
    if (bFail)
    {
        SAL_WARN("sw.ww8", "broken offset, ignoring");
        return false;
    }

    m_aPLCF.advance();

    return true;
}

void WW8PLCF_HdFt::GetTextPosExact(short nIdx, WW8_CP& rStart, WW8_CP& rLen)
{
    WW8_CP nEnd;
    void* pData;

    m_aPLCF.SetIdx( nIdx );               // Lookup suitable CP
    m_aPLCF.Get( rStart, nEnd, pData );
    if (nEnd < rStart)
    {
        SAL_WARN("sw.ww8", "End " << nEnd << " before Start " << rStart);
        rLen = 0;
        return;
    }
    if (o3tl::checked_sub(nEnd, rStart, rLen))
    {
        SAL_WARN("sw.ww8", "GetTextPosExact overflow");
        rLen = 0;
    }
}

void WW8PLCF_HdFt::UpdateIndex( sal_uInt8 grpfIhdt )
{
    // Caution: Description is not correct
    for( sal_uInt8 nI = 0x01; nI <= 0x20; nI <<= 1 )
        if( nI & grpfIhdt )
            m_nIdxOffset++;
}

WW8Dop::WW8Dop(SvStream& rSt, sal_Int16 nFib, sal_Int32 nPos, sal_uInt32 nSize):
    fFacingPages(false), fWidowControl(false), fPMHMainDoc(false), grfSuppression(0), fpc(0),
    grpfIhdt(0), rncFootnote(0), nFootnote(0), fOutlineDirtySave(false), fOnlyMacPics(false),
    fOnlyWinPics(false), fLabelDoc(false), fHyphCapitals(false), fAutoHyphen(false),
    fFormNoFields(false), fLinkStyles(false), fRevMarking(false), fBackup(false),
    fExactCWords(false), fPagHidden(false), fPagResults(false), fLockAtn(false),
    fMirrorMargins(false), fReadOnlyRecommended(false), fDfltTrueType(false),
    fPagSuppressTopSpacing(false), fProtEnabled(false), fDispFormFieldSel(false), fRMView(false),
    fRMPrint(false), fWriteReservation(false), fLockRev(false), fEmbedFonts(false),
    copts_fNoTabForInd(false), copts_fNoSpaceRaiseLower(false), copts_fSuppressSpbfAfterPgBrk(false),
    copts_fWrapTrailSpaces(false), copts_fMapPrintTextColor(false), copts_fNoColumnBalance(false),
    copts_fConvMailMergeEsc(false), copts_fSuppressTopSpacing(false),
    copts_fOrigWordTableRules(false), copts_fTransparentMetafiles(false),
    copts_fShowBreaksInFrames(false), copts_fSwapBordersFacingPgs(false), copts_fExpShRtn(false),
    rncEdn(0), nEdn(0), epc(0), fPrintFormData(false), fSaveFormData(false), fShadeFormData(false),
    fWCFootnoteEdn(false), wvkSaved(0), wScaleSaved(0), zkSaved(0), fRotateFontW6(false),
    iGutterPos(false), fNoTabForInd(false), fNoSpaceRaiseLower(false),
    fSuppressSpbfAfterPageBreak(false), fWrapTrailSpaces(false), fMapPrintTextColor(false),
    fNoColumnBalance(false), fConvMailMergeEsc(false), fSuppressTopSpacing(false),
    fOrigWordTableRules(false), fTransparentMetafiles(false), fShowBreaksInFrames(false),
    fSwapBordersFacingPgs(false), fCompatibilityOptions_Unknown1_13(false), fExpShRtn(false),
    fCompatibilityOptions_Unknown1_15(false), fDntBlnSbDbWid(false),
    fSuppressTopSpacingMac5(false), fTruncDxaExpand(false), fPrintBodyBeforeHdr(false),
    fNoLeading(false), fCompatibilityOptions_Unknown1_21(false), fMWSmallCaps(false),
    fCompatibilityOptions_Unknown1_23(false), fCompatibilityOptions_Unknown1_24(false),
    fCompatibilityOptions_Unknown1_25(false), fCompatibilityOptions_Unknown1_26(false),
    fCompatibilityOptions_Unknown1_27(false), fCompatibilityOptions_Unknown1_28(false),
    fCompatibilityOptions_Unknown1_29(false), fCompatibilityOptions_Unknown1_30(false),
    fCompatibilityOptions_Unknown1_31(false), fUsePrinterMetrics(false), lvl(0), fHtmlDoc(false),
    fSnapBorder(false), fIncludeHeader(false), fIncludeFooter(false), fForcePageSizePag(false),
    fMinFontSizePag(false), fHaveVersions(false), fAutoVersion(false),
    fUnknown3(0), fUseBackGroundInAllmodes(false), fDoNotEmbedSystemFont(false), fWordCompat(false),
    fLiveRecover(false), fEmbedFactoids(false), fFactoidXML(false), fFactoidAllDone(false),
    fFolioPrint(false), fReverseFolio(false), iTextLineEnding(0), fHideFcc(false),
    fAcetateShowMarkup(false), fAcetateShowAtn(false), fAcetateShowInsDel(false),
    fAcetateShowProps(false)
        // in C++20 with P06831R1 "Default member initializers for bit-fields (revision 1)", the
        // above bit-field member initializations can be moved to the class definition
{
    fDontUseHTMLAutoSpacing = true; //default
    fAcetateShowAtn = true; //default
    const sal_uInt32 nMaxDopSize = 0x268;
    std::unique_ptr<sal_uInt8[]> pDataPtr( new sal_uInt8[ nMaxDopSize ] );
    sal_uInt8* pData = pDataPtr.get();

    sal_uInt32 nRead = std::min(nMaxDopSize, nSize);
    if (nSize < 2 || !checkSeek(rSt, nPos) || nRead != rSt.ReadBytes(pData, nRead))
        nDopError = ERR_SWG_READ_ERROR;     // report error
    else
    {
        if (nMaxDopSize > nRead)
            memset( pData + nRead, 0, nMaxDopSize - nRead );

        // interpret the data
        sal_uInt32 a32Bit;
        sal_uInt16 a16Bit;
        sal_uInt8   a8Bit;

        a16Bit = Get_UShort( pData );        // 0 0x00
        fFacingPages        = 0 != ( a16Bit  &  0x0001 )     ;
        fWidowControl       = 0 != ( a16Bit  &  0x0002 )     ;
        fPMHMainDoc         = 0 != ( a16Bit  &  0x0004 )     ;
        grfSuppression      =      ( a16Bit  &  0x0018 ) >> 3;
        fpc                 =      ( a16Bit  &  0x0060 ) >> 5;
        grpfIhdt            =      ( a16Bit  &  0xff00 ) >> 8;

        a16Bit = Get_UShort( pData );        // 2 0x02
        rncFootnote              =   a16Bit  &  0x0003        ;
        nFootnote                = ( a16Bit  & ~0x0003 ) >> 2 ;

        a8Bit = Get_Byte( pData );           // 4 0x04
        fOutlineDirtySave      = 0 != ( a8Bit  &  0x01   );

        a8Bit = Get_Byte( pData );           // 5 0x05
        fOnlyMacPics           = 0 != ( a8Bit  &  0x01   );
        fOnlyWinPics           = 0 != ( a8Bit  &  0x02   );
        fLabelDoc              = 0 != ( a8Bit  &  0x04   );
        fHyphCapitals          = 0 != ( a8Bit  &  0x08   );
        fAutoHyphen            = 0 != ( a8Bit  &  0x10   );
        fFormNoFields          = 0 != ( a8Bit  &  0x20   );
        fLinkStyles            = 0 != ( a8Bit  &  0x40   );
        fRevMarking            = 0 != ( a8Bit  &  0x80   );

        a8Bit = Get_Byte( pData );           // 6 0x06
        fBackup                = 0 != ( a8Bit  &  0x01   );
        fExactCWords           = 0 != ( a8Bit  &  0x02   );
        fPagHidden             = 0 != ( a8Bit  &  0x04   );
        fPagResults            = 0 != ( a8Bit  &  0x08   );
        fLockAtn               = 0 != ( a8Bit  &  0x10   );
        fMirrorMargins         = 0 != ( a8Bit  &  0x20   );
        fReadOnlyRecommended   = 0 != ( a8Bit  &  0x40   );
        fDfltTrueType          = 0 != ( a8Bit  &  0x80   );

        a8Bit = Get_Byte( pData );           // 7 0x07
        fPagSuppressTopSpacing = 0 != ( a8Bit  &  0x01   );
        fProtEnabled           = 0 != ( a8Bit  &  0x02   );
        fDispFormFieldSel        = 0 != ( a8Bit  &  0x04   );
        fRMView                = 0 != ( a8Bit  &  0x08   );
        fRMPrint               = 0 != ( a8Bit  &  0x10   );
        fWriteReservation      = 0 != ( a8Bit  &  0x20   );
        fLockRev               = 0 != ( a8Bit  &  0x40   );
        fEmbedFonts            = 0 != ( a8Bit  &  0x80   );

        a8Bit = Get_Byte( pData );           // 8 0x08
        copts_fNoTabForInd           = 0 != ( a8Bit  &  0x01   );
        copts_fNoSpaceRaiseLower     = 0 != ( a8Bit  &  0x02   );
        copts_fSuppressSpbfAfterPgBrk = 0 != ( a8Bit  &  0x04   );
        copts_fWrapTrailSpaces       = 0 != ( a8Bit  &  0x08   );
        copts_fMapPrintTextColor     = 0 != ( a8Bit  &  0x10   );
        copts_fNoColumnBalance       = 0 != ( a8Bit  &  0x20   );
        copts_fConvMailMergeEsc      = 0 != ( a8Bit  &  0x40   );
        copts_fSuppressTopSpacing    = 0 != ( a8Bit  &  0x80   );

        a8Bit = Get_Byte( pData );           // 9 0x09
        copts_fOrigWordTableRules    = 0 != ( a8Bit  &  0x01   );
        copts_fTransparentMetafiles  = 0 != ( a8Bit  &  0x02   );
        copts_fShowBreaksInFrames    = 0 != ( a8Bit  &  0x04   );
        copts_fSwapBordersFacingPgs  = 0 != ( a8Bit  &  0x08   );
        copts_fExpShRtn              = 0 != ( a8Bit  &  0x20   );  // #i56856#
        copts_fDntBlnSbDbWid         = 0 != ( a8Bit  &  0x80   );  // tdf#88908

        dxaTab = Get_Short( pData );         // 10 0x0a
        wSpare = Get_UShort( pData );        // 12 0x0c
        dxaHotZ = Get_UShort( pData );       // 14 0x0e
        cConsecHypLim = Get_UShort( pData ); // 16 0x10
        wSpare2 = Get_UShort( pData );       // 18 0x12
        dttmCreated = Get_Long( pData );     // 20 0x14
        dttmRevised = Get_Long( pData );     // 24 0x18
        dttmLastPrint = Get_Long( pData );   // 28 0x1c
        nRevision = Get_Short( pData );      // 32 0x20
        tmEdited = Get_Long( pData );        // 34 0x22
        cWords = Get_Long( pData );          // 38 0x26
        cCh = Get_Long( pData );             // 42 0x2a
        cPg = Get_Short( pData );            // 46 0x2e
        cParas = Get_Long( pData );          // 48 0x30

        a16Bit = Get_UShort( pData );        // 52 0x34
        rncEdn =   a16Bit &  0x0003       ;
        nEdn   = ( a16Bit & ~0x0003 ) >> 2;

        a16Bit = Get_UShort( pData );        // 54 0x36
        epc            =   a16Bit &  0x0003       ;
        nfcFootnoteRef      = ( a16Bit &  0x003c ) >> 2;
        nfcEdnRef      = ( a16Bit &  0x03c0 ) >> 6;
        fPrintFormData = 0 != ( a16Bit &  0x0400 );
        fSaveFormData  = 0 != ( a16Bit &  0x0800 );
        fShadeFormData = 0 != ( a16Bit &  0x1000 );
        fWCFootnoteEdn      = 0 != ( a16Bit &  0x8000 );

        cLines = Get_Long( pData );          // 56 0x38
        cWordsFootnoteEnd = Get_Long( pData );    // 60 0x3c
        cChFootnoteEdn = Get_Long( pData );       // 64 0x40
        cPgFootnoteEdn = Get_Short( pData );      // 68 0x44
        cParasFootnoteEdn = Get_Long( pData );    // 70 0x46
        cLinesFootnoteEdn = Get_Long( pData );    // 74 0x4a
        lKeyProtDoc = Get_Long( pData );     // 78 0x4e

        a16Bit = Get_UShort( pData );        // 82 0x52
        wvkSaved    =   a16Bit & 0x0007        ;
        wScaleSaved = ( a16Bit & 0x0ff8 ) >> 3 ;
        zkSaved     = ( a16Bit & 0x3000 ) >> 12;
        fRotateFontW6 = ( a16Bit & 0x4000 ) >> 14;
        iGutterPos = ( a16Bit &  0x8000 ) >> 15;

        if (nFib >= 103) // Word 6/32bit, 95, 97, 2000, 2002, 2003, 2007
        {
            a32Bit = Get_ULong( pData );     // 84 0x54
            SetCompatibilityOptions(a32Bit);
        }

        //#i22436#, for all WW7- documents
        if (nFib <= 104) // Word 95
            fUsePrinterMetrics = true;

        if (nFib > 105) // Word 97, 2000, 2002, 2003, 2007
        {
            adt = Get_Short( pData );            // 88 0x58

            doptypography.ReadFromMem(pData);    // 90 0x5a

            memcpy( &dogrid, pData, sizeof( WW8_DOGRID )); // 400 0x190
            pData += sizeof( WW8_DOGRID );

            a16Bit = Get_UShort( pData );        // 410 0x19a
            // the following 9 bit are uninteresting
            fHtmlDoc                = ( a16Bit &  0x0200 ) >>  9 ;
            fSnapBorder             = ( a16Bit &  0x0800 ) >> 11 ;
            fIncludeHeader          = ( a16Bit &  0x1000 ) >> 12 ;
            fIncludeFooter          = ( a16Bit &  0x2000 ) >> 13 ;
            fForcePageSizePag       = ( a16Bit &  0x4000 ) >> 14 ;
            fMinFontSizePag         = ( a16Bit &  0x8000 ) >> 15 ;

            a16Bit = Get_UShort( pData );        // 412 0x19c
            fHaveVersions   = 0 != ( a16Bit  &  0x0001 );
            fAutoVersion    = 0 != ( a16Bit  &  0x0002 );

            pData += 12;                         // 414 0x19e

            cChWS = Get_Long( pData );           // 426 0x1aa
            cChWSFootnoteEdn = Get_Long( pData );     // 430 0x1ae
            grfDocEvents = Get_Long( pData );    // 434 0x1b2

            pData += 4+30+8;  // 438 0x1b6; 442 0x1ba; 472 0x1d8; 476 0x1dc

            cDBC = Get_Long( pData );            // 480 0x1e0
            cDBCFootnoteEdn = Get_Long( pData );      // 484 0x1e4

            pData += 1 * sizeof( sal_Int32);         // 488 0x1e8

            nfcFootnoteRef = Get_Short( pData );      // 492 0x1ec
            nfcEdnRef = Get_Short( pData );      // 494 0x1ee
            hpsZoomFontPag = Get_Short( pData ); // 496 0x1f0
            dywDispPag = Get_Short( pData );     // 498 0x1f2

            if (nRead >= 516)
            {
                //500 -> 508, Appear to be repeated here in 2000+
                pData += 8;                      // 500 0x1f4
                a32Bit = Get_Long( pData );      // 508 0x1fc
                SetCompatibilityOptions(a32Bit);
                a32Bit = Get_Long( pData );      // 512 0x200

                // i#78591#
                SetCompatibilityOptions2(a32Bit);
            }
            if (nRead >= 550)
            {
                pData += 32;
                a16Bit = Get_UShort( pData );
                fDoNotEmbedSystemFont = ( a16Bit &  0x0001 );
                fWordCompat = ( a16Bit &  0x0002 ) >> 1;
                fLiveRecover = ( a16Bit &  0x0004 ) >> 2;
                fEmbedFactoids = ( a16Bit &  0x0008 ) >> 3;
                fFactoidXML = ( a16Bit &  0x00010 ) >> 4;
                fFactoidAllDone = ( a16Bit &  0x0020 ) >> 5;
                fFolioPrint = ( a16Bit &  0x0040 ) >> 6;
                fReverseFolio = ( a16Bit &  0x0080 ) >> 7;
                iTextLineEnding = ( a16Bit &  0x0700 ) >> 8;
                fHideFcc = ( a16Bit &  0x0800 ) >> 11;
                fAcetateShowMarkup = ( a16Bit &  0x1000 ) >> 12;
                fAcetateShowAtn = ( a16Bit &  0x2000 ) >> 13;
                fAcetateShowInsDel = ( a16Bit &  0x4000 ) >> 14;
                fAcetateShowProps = ( a16Bit &  0x8000 ) >> 15;
            }
            if (nRead >= 600)
            {
                pData += 48;
                a16Bit = Get_Short(pData);
                fUseBackGroundInAllmodes = (a16Bit & 0x0080) >> 7;
            }
        }
    }
}

WW8Dop::WW8Dop():
    fFacingPages(false), fWidowControl(true), fPMHMainDoc(false), grfSuppression(0), fpc(1),
    grpfIhdt(0), rncFootnote(0), nFootnote(1), fOutlineDirtySave(true), fOnlyMacPics(false),
    fOnlyWinPics(false), fLabelDoc(false), fHyphCapitals(true), fAutoHyphen(false),
    fFormNoFields(false), fLinkStyles(false), fRevMarking(false), fBackup(true),
    fExactCWords(false), fPagHidden(true), fPagResults(true), fLockAtn(false),
    fMirrorMargins(false), fReadOnlyRecommended(false), fDfltTrueType(true),
    fPagSuppressTopSpacing(false), fProtEnabled(false), fDispFormFieldSel(false), fRMView(true),
    fRMPrint(true), fWriteReservation(false), fLockRev(false), fEmbedFonts(false),
    copts_fNoTabForInd(false), copts_fNoSpaceRaiseLower(false), copts_fSuppressSpbfAfterPgBrk(false),
    copts_fWrapTrailSpaces(false), copts_fMapPrintTextColor(false), copts_fNoColumnBalance(false),
    copts_fConvMailMergeEsc(false), copts_fSuppressTopSpacing(false),
    copts_fOrigWordTableRules(false), copts_fTransparentMetafiles(false),
    copts_fShowBreaksInFrames(false), copts_fSwapBordersFacingPgs(false), copts_fExpShRtn(false),
    dxaTab(0x2d0), dxaHotZ(0x168), nRevision(1),
    rncEdn(0), nEdn(1), epc(3), fPrintFormData(false), fSaveFormData(false), fShadeFormData(true),
    fWCFootnoteEdn(false), wvkSaved(2), wScaleSaved(100), zkSaved(0), fRotateFontW6(false),
    iGutterPos(false), fNoTabForInd(false), fNoSpaceRaiseLower(false),
    fSuppressSpbfAfterPageBreak(false), fWrapTrailSpaces(false), fMapPrintTextColor(false),
    fNoColumnBalance(false), fConvMailMergeEsc(false), fSuppressTopSpacing(false),
    fOrigWordTableRules(false), fTransparentMetafiles(false), fShowBreaksInFrames(false),
    fSwapBordersFacingPgs(false), fCompatibilityOptions_Unknown1_13(false), fExpShRtn(false),
    fCompatibilityOptions_Unknown1_15(false), fDntBlnSbDbWid(false),
    fSuppressTopSpacingMac5(false), fTruncDxaExpand(false), fPrintBodyBeforeHdr(false),
    fNoLeading(true), fCompatibilityOptions_Unknown1_21(false), fMWSmallCaps(false),
    fCompatibilityOptions_Unknown1_23(false), fCompatibilityOptions_Unknown1_24(false),
    fCompatibilityOptions_Unknown1_25(false), fCompatibilityOptions_Unknown1_26(false),
    fCompatibilityOptions_Unknown1_27(false), fCompatibilityOptions_Unknown1_28(false),
    fCompatibilityOptions_Unknown1_29(false), fCompatibilityOptions_Unknown1_30(false),
    fCompatibilityOptions_Unknown1_31(false), fUsePrinterMetrics(true), lvl(9), fHtmlDoc(false),
    fSnapBorder(false), fIncludeHeader(true), fIncludeFooter(true), fForcePageSizePag(false),
    fMinFontSizePag(false), fHaveVersions(false), fAutoVersion(false),
    cChWS(0), cChWSFootnoteEdn(0), cDBC(0), cDBCFootnoteEdn(0), nfcEdnRef(2),
    fUnknown3(0), fUseBackGroundInAllmodes(false), fDoNotEmbedSystemFont(false), fWordCompat(false),
    fLiveRecover(false), fEmbedFactoids(false), fFactoidXML(false), fFactoidAllDone(false),
    fFolioPrint(false), fReverseFolio(false), iTextLineEnding(0), fHideFcc(false),
    fAcetateShowMarkup(false), fAcetateShowAtn(true), fAcetateShowInsDel(false),
    fAcetateShowProps(false)
        // in C++20 with P06831R1 "Default member initializers for bit-fields (revision 1)", the
        // above bit-field member initializations can be moved to the class definition
{
    /*
    Writer acts like this all the time at the moment, ideally we need an
    option for these two as well to import word docs that are not like
    this by default
    */
    // put in initialization list
    // fNoLeading = true;
    //fUsePrinterMetrics = true;
}

void WW8Dop::SetCompatibilityOptions(sal_uInt32 a32Bit)
{
    fNoTabForInd                = ( a32Bit &  0x00000001 )       ;
    fNoSpaceRaiseLower          = ( a32Bit &  0x00000002 ) >>  1 ;
    fSuppressSpbfAfterPageBreak = ( a32Bit &  0x00000004 ) >>  2 ;
    fWrapTrailSpaces            = ( a32Bit &  0x00000008 ) >>  3 ;
    fMapPrintTextColor          = ( a32Bit &  0x00000010 ) >>  4 ;
    fNoColumnBalance            = ( a32Bit &  0x00000020 ) >>  5 ;
    fConvMailMergeEsc           = ( a32Bit &  0x00000040 ) >>  6 ;
    fSuppressTopSpacing         = ( a32Bit &  0x00000080 ) >>  7 ;
    fOrigWordTableRules         = ( a32Bit &  0x00000100 ) >>  8 ;
    fTransparentMetafiles       = ( a32Bit &  0x00000200 ) >>  9 ;
    fShowBreaksInFrames         = ( a32Bit &  0x00000400 ) >> 10 ;
    fSwapBordersFacingPgs       = ( a32Bit &  0x00000800 ) >> 11 ;
    fCompatibilityOptions_Unknown1_13       = ( a32Bit &  0x00001000 ) >> 12 ;
    fExpShRtn                   = ( a32Bit &  0x00002000 ) >> 13 ; // #i56856#
    fCompatibilityOptions_Unknown1_15       = ( a32Bit &  0x00004000 ) >> 14 ;
    fDntBlnSbDbWid              = ( a32Bit &  0x00008000 ) >> 15 ; // tdf#88908
    fSuppressTopSpacingMac5     = ( a32Bit &  0x00010000 ) >> 16 ;
    fTruncDxaExpand             = ( a32Bit &  0x00020000 ) >> 17 ;
    fPrintBodyBeforeHdr         = ( a32Bit &  0x00040000 ) >> 18 ;
    fNoLeading                  = ( a32Bit &  0x00080000 ) >> 19 ;
    fCompatibilityOptions_Unknown1_21       = ( a32Bit &  0x00100000 ) >> 20 ;
    fMWSmallCaps                = ( a32Bit &  0x00200000 ) >> 21 ;
    fCompatibilityOptions_Unknown1_23       = ( a32Bit &  0x00400000 ) >> 22 ;
    fCompatibilityOptions_Unknown1_24       = ( a32Bit &  0x00800800 ) >> 23 ;
    fCompatibilityOptions_Unknown1_25       = ( a32Bit &  0x01000000 ) >> 24 ;
    fCompatibilityOptions_Unknown1_26       = ( a32Bit &  0x02000000 ) >> 25 ;
    fCompatibilityOptions_Unknown1_27       = ( a32Bit &  0x04000000 ) >> 26 ;
    fCompatibilityOptions_Unknown1_28       = ( a32Bit &  0x08000000 ) >> 27 ;
    fCompatibilityOptions_Unknown1_29       = ( a32Bit &  0x10000000 ) >> 28 ;
    fCompatibilityOptions_Unknown1_30       = ( a32Bit &  0x20000000 ) >> 29 ;
    fCompatibilityOptions_Unknown1_31       = ( a32Bit &  0x40000000 ) >> 30 ;

    fUsePrinterMetrics          = ( a32Bit &  0x80000000 ) >> 31 ;
}

sal_uInt32 WW8Dop::GetCompatibilityOptions() const
{
    sal_uInt32 a32Bit = 0;
    if (fNoTabForInd)                   a32Bit |= 0x00000001;
    if (fNoSpaceRaiseLower)             a32Bit |= 0x00000002;
    if (fSuppressSpbfAfterPageBreak)    a32Bit |= 0x00000004;
    if (fWrapTrailSpaces)               a32Bit |= 0x00000008;
    if (fMapPrintTextColor)             a32Bit |= 0x00000010;
    if (fNoColumnBalance)               a32Bit |= 0x00000020;
    if (fConvMailMergeEsc)              a32Bit |= 0x00000040;
    if (fSuppressTopSpacing)            a32Bit |= 0x00000080;
    if (fOrigWordTableRules)            a32Bit |= 0x00000100;
    if (fTransparentMetafiles)          a32Bit |= 0x00000200;
    if (fShowBreaksInFrames)            a32Bit |= 0x00000400;
    if (fSwapBordersFacingPgs)          a32Bit |= 0x00000800;
    if (fCompatibilityOptions_Unknown1_13)          a32Bit |= 0x00001000;
    if (fExpShRtn)                      a32Bit |= 0x00002000; // #i56856#
    if (fCompatibilityOptions_Unknown1_15)          a32Bit |= 0x00004000;
    if (fDntBlnSbDbWid)                 a32Bit |= 0x00008000; // tdf#88908
    if (fSuppressTopSpacingMac5)        a32Bit |= 0x00010000;
    if (fTruncDxaExpand)                a32Bit |= 0x00020000;
    if (fPrintBodyBeforeHdr)            a32Bit |= 0x00040000;
    if (fNoLeading)                     a32Bit |= 0x00080000;
    if (fCompatibilityOptions_Unknown1_21)          a32Bit |= 0x00100000;
    if (fMWSmallCaps)                   a32Bit |= 0x00200000;
    if (fCompatibilityOptions_Unknown1_23)          a32Bit |= 0x00400000;
    if (fCompatibilityOptions_Unknown1_24)          a32Bit |= 0x00800000;
    if (fCompatibilityOptions_Unknown1_25)          a32Bit |= 0x01000000;
    if (fCompatibilityOptions_Unknown1_26)          a32Bit |= 0x02000000;
    if (fCompatibilityOptions_Unknown1_27)          a32Bit |= 0x04000000;
    if (fCompatibilityOptions_Unknown1_28)          a32Bit |= 0x08000000;
    if (fCompatibilityOptions_Unknown1_29)          a32Bit |= 0x10000000;
    if (fCompatibilityOptions_Unknown1_30)          a32Bit |= 0x20000000;
    if (fCompatibilityOptions_Unknown1_31)          a32Bit |= 0x40000000;
    if (fUsePrinterMetrics)             a32Bit |= 0x80000000;
    return a32Bit;
}

// i#78591#
void WW8Dop::SetCompatibilityOptions2(sal_uInt32 a32Bit)
{
    fSpLayoutLikeWW8                                     = ( a32Bit &  0x00000001 );
    fFtnLayoutLikeWW8                                    = ( a32Bit &  0x00000002 ) >>  1 ;
    fDontUseHTMLAutoSpacing     = ( a32Bit &  0x00000004 ) >>  2 ;
    fDontAdjustLineHeightInTable                         = ( a32Bit &  0x00000008 ) >>  3 ;
    fForgetLastTabAlign                                  = ( a32Bit &  0x00000010 ) >>  4 ;
    fUseAutospaceForFullWidthAlpha                       = ( a32Bit &  0x00000020 ) >>  5 ;
    fAlignTablesRowByRow                                 = ( a32Bit &  0x00000040 ) >>  6 ;
    fLayoutRawTableWidth                                 = ( a32Bit &  0x00000080 ) >>  7 ;
    fLayoutTableRowsApart                                = ( a32Bit &  0x00000100 ) >>  8 ;
    fUseWord97LineBreakingRules                          = ( a32Bit &  0x00000200 ) >>  9 ;
    fDontBreakWrappedTables                              = ( a32Bit &  0x00000400 ) >> 10 ;
    fDontSnapToGridInCell                                = ( a32Bit &  0x00000800 ) >> 11 ;
    fDontAllowFieldEndSelect                             = ( a32Bit &  0x00001000 ) >> 12 ;
    fApplyBreakingRules                                  = ( a32Bit &  0x00002000 ) >> 13 ;
    fDontWrapTextWithPunct                               = ( a32Bit &  0x00004000 ) >> 14 ;
    fDontUseAsianBreakRules                              = ( a32Bit &  0x00008000 ) >> 15 ;
    fUseWord2002TableStyleRules                          = ( a32Bit &  0x00010000 ) >> 16 ;
    fGrowAutoFit                                         = ( a32Bit &  0x00020000 ) >> 17 ;
    fUseNormalStyleForList                               = ( a32Bit &  0x00040000 ) >> 18 ;
    fDontUseIndentAsNumberingTabStop                     = ( a32Bit &  0x00080000 ) >> 19 ;
    fFELineBreak11                                       = ( a32Bit &  0x00100000 ) >> 20 ;
    fAllowSpaceOfSameStyleInTable                        = ( a32Bit &  0x00200000 ) >> 21 ;
    fWW11IndentRules                                     = ( a32Bit &  0x00400000 ) >> 22 ;
    fDontAutofitConstrainedTables                        = ( a32Bit &  0x00800800 ) >> 23 ;
    fAutofitLikeWW11                                     = ( a32Bit &  0x01000800 ) >> 24 ;
    fUnderlineTabInNumList                               = ( a32Bit &  0x02000800 ) >> 25 ;
    fHangulWidthLikeWW11                                 = ( a32Bit &  0x04000800 ) >> 26 ;
    fSplitPgBreakAndParaMark                             = ( a32Bit &  0x08000800 ) >> 27 ;
    fDontVertAlignCellWithSp = true; // always true      = ( a32Bit &  0x10000800 ) >> 28 ;
    fDontBreakConstrainedForcedTables                    = ( a32Bit &  0x20000800 ) >> 29 ;
    fDontVertAlignInTxbx                                 = ( a32Bit &  0x40000800 ) >> 30 ;
    fWord11KerningPairs                                  = ( a32Bit &  0x80000000 ) >> 31 ;
}

sal_uInt32 WW8Dop::GetCompatibilityOptions2() const
{
    sal_uInt32 a32Bit = 0;
    if (fSpLayoutLikeWW8)                           a32Bit |= 0x00000001;
    if (fFtnLayoutLikeWW8)                          a32Bit |= 0x00000002;
    if (fDontUseHTMLAutoSpacing)     a32Bit |= 0x00000004;
    if (fDontAdjustLineHeightInTable)               a32Bit |= 0x00000008;
    if (fForgetLastTabAlign)                        a32Bit |= 0x00000010;
    if (fUseAutospaceForFullWidthAlpha)             a32Bit |= 0x00000020;
    if (fAlignTablesRowByRow)                       a32Bit |= 0x00000040;
    if (fLayoutRawTableWidth)                       a32Bit |= 0x00000080;
    if (fLayoutTableRowsApart)                      a32Bit |= 0x00000100;
    if (fUseWord97LineBreakingRules)                a32Bit |= 0x00000200;
    if (fDontBreakWrappedTables)                    a32Bit |= 0x00000400;
    if (fDontSnapToGridInCell)                      a32Bit |= 0x00000800;
    if (fDontAllowFieldEndSelect)                   a32Bit |= 0x00001000;
    //#i42909# set thai "line breaking rules" compatibility option
    // pflin, wonder whether bUseThaiLineBreakingRules is correct
    // when importing word document.
    if (bUseThaiLineBreakingRules)          a32Bit |= 0x00002000;
    else if (fApplyBreakingRules)                   a32Bit |= 0x00002000;
    if (fDontWrapTextWithPunct)                     a32Bit |= 0x00004000;
    if (fDontUseAsianBreakRules)                    a32Bit |= 0x00008000;
    if (fUseWord2002TableStyleRules)                a32Bit |= 0x00010000;
    if (fGrowAutoFit)                               a32Bit |= 0x00020000;
    if (fUseNormalStyleForList)                     a32Bit |= 0x00040000;
    if (fDontUseIndentAsNumberingTabStop)           a32Bit |= 0x00080000;
    if (fFELineBreak11)                             a32Bit |= 0x00100000;
    if (fAllowSpaceOfSameStyleInTable)              a32Bit |= 0x00200000;
    if (fWW11IndentRules)                           a32Bit |= 0x00400000;
    if (fDontAutofitConstrainedTables)              a32Bit |= 0x00800000;
    if (fAutofitLikeWW11)                           a32Bit |= 0x01000000;
    if (fUnderlineTabInNumList)                     a32Bit |= 0x02000000;
    if (fHangulWidthLikeWW11)                       a32Bit |= 0x04000000;
    if (fSplitPgBreakAndParaMark)                   a32Bit |= 0x08000000;
    if (fDontVertAlignCellWithSp)                   a32Bit |= 0x10000000;
    if (fDontBreakConstrainedForcedTables)          a32Bit |= 0x20000000;
    if (fDontVertAlignInTxbx)                       a32Bit |= 0x40000000;
    if (fWord11KerningPairs)                        a32Bit |= 0x80000000;
    return a32Bit;
}

void WW8Dop::Write(SvStream& rStrm, WW8Fib& rFib) const
{
    const int nMaxDopLen = 610;
    sal_uInt32 nLen = 8 == rFib.m_nVersion ? nMaxDopLen : 84;
    rFib.m_fcDop =  rStrm.Tell();
    rFib.m_lcbDop = nLen;

    sal_uInt8 aData[ nMaxDopLen ] = {};
    sal_uInt8* pData = aData;

    // analyse the data
    sal_uInt16 a16Bit;
    sal_uInt8   a8Bit;

    a16Bit = 0;                         // 0 0x00
    if (fFacingPages)
        a16Bit |= 0x0001;
    if (fWidowControl)
        a16Bit |= 0x0002;
    if (fPMHMainDoc)
        a16Bit |= 0x0004;
    a16Bit |= ( 0x0018 & (grfSuppression << 3));
    a16Bit |= ( 0x0060 & (fpc << 5));
    a16Bit |= ( 0xff00 & (grpfIhdt << 8));
    Set_UInt16( pData, a16Bit );

    a16Bit = 0;                         // 2 0x02
    a16Bit |= ( 0x0003 & rncFootnote );
    a16Bit |= ( ~0x0003 & (nFootnote << 2));
    Set_UInt16( pData, a16Bit );

    a8Bit = 0;                          // 4 0x04
    if( fOutlineDirtySave ) a8Bit |= 0x01;
    Set_UInt8( pData, a8Bit );

    a8Bit = 0;                          // 5 0x05
    if( fOnlyMacPics )  a8Bit |= 0x01;
    if( fOnlyWinPics )  a8Bit |= 0x02;
    if( fLabelDoc )     a8Bit |= 0x04;
    if( fHyphCapitals ) a8Bit |= 0x08;
    if( fAutoHyphen )   a8Bit |= 0x10;
    if( fFormNoFields ) a8Bit |= 0x20;
    if( fLinkStyles )   a8Bit |= 0x40;
    if( fRevMarking )   a8Bit |= 0x80;
    Set_UInt8( pData, a8Bit );

    a8Bit = 0;                          // 6 0x06
    if( fBackup )               a8Bit |= 0x01;
    if( fExactCWords )          a8Bit |= 0x02;
    if( fPagHidden )            a8Bit |= 0x04;
    if( fPagResults )           a8Bit |= 0x08;
    if( fLockAtn )              a8Bit |= 0x10;
    if( fMirrorMargins )        a8Bit |= 0x20;
    if( fReadOnlyRecommended )  a8Bit |= 0x40;
    if( fDfltTrueType )         a8Bit |= 0x80;
    Set_UInt8( pData, a8Bit );

    a8Bit = 0;                          // 7 0x07
    if( fPagSuppressTopSpacing )    a8Bit |= 0x01;
    if( fProtEnabled )              a8Bit |= 0x02;
    if( fDispFormFieldSel )           a8Bit |= 0x04;
    if( fRMView )                   a8Bit |= 0x08;
    if( fRMPrint )                  a8Bit |= 0x10;
    if( fWriteReservation )         a8Bit |= 0x20;
    if( fLockRev )                  a8Bit |= 0x40;
    if( fEmbedFonts )               a8Bit |= 0x80;
    Set_UInt8( pData, a8Bit );

    a8Bit = 0;                          // 8 0x08
    if( copts_fNoTabForInd )            a8Bit |= 0x01;
    if( copts_fNoSpaceRaiseLower )      a8Bit |= 0x02;
    if( copts_fSuppressSpbfAfterPgBrk ) a8Bit |= 0x04;
    if( copts_fWrapTrailSpaces )        a8Bit |= 0x08;
    if( copts_fMapPrintTextColor )      a8Bit |= 0x10;
    if( copts_fNoColumnBalance )        a8Bit |= 0x20;
    if( copts_fConvMailMergeEsc )       a8Bit |= 0x40;
    if( copts_fSuppressTopSpacing )     a8Bit |= 0x80;
    Set_UInt8( pData, a8Bit );

    a8Bit = 0;                          // 9 0x09
    if( copts_fOrigWordTableRules )     a8Bit |= 0x01;
    if( copts_fTransparentMetafiles )   a8Bit |= 0x02;
    if( copts_fShowBreaksInFrames )     a8Bit |= 0x04;
    if( copts_fSwapBordersFacingPgs )   a8Bit |= 0x08;
    if( copts_fExpShRtn )               a8Bit |= 0x20;  // #i56856#
    Set_UInt8( pData, a8Bit );

    Set_UInt16( pData, dxaTab );        // 10 0x0a
    Set_UInt16( pData, wSpare );        // 12 0x0c
    Set_UInt16( pData, dxaHotZ );       // 14 0x0e
    Set_UInt16( pData, cConsecHypLim ); // 16 0x10
    Set_UInt16( pData, wSpare2 );       // 18 0x12
    Set_UInt32( pData, dttmCreated );   // 20 0x14
    Set_UInt32( pData, dttmRevised );   // 24 0x18
    Set_UInt32( pData, dttmLastPrint ); // 28 0x1c
    Set_UInt16( pData, nRevision );     // 32 0x20
    Set_UInt32( pData, tmEdited );      // 34 0x22
    Set_UInt32( pData, cWords );        // 38 0x26
    Set_UInt32( pData, cCh );           // 42 0x2a
    Set_UInt16( pData, cPg );           // 46 0x2e
    Set_UInt32( pData, cParas );        // 48 0x30

    a16Bit = 0;                         // 52 0x34
    a16Bit |= ( 0x0003 & rncEdn );
    a16Bit |= (~0x0003 & ( nEdn << 2));
    Set_UInt16( pData, a16Bit );

    a16Bit = 0;                         // 54 0x36
    a16Bit |= (0x0003 & epc );
    a16Bit |= (0x003c & (nfcFootnoteRef << 2));
    a16Bit |= (0x03c0 & (nfcEdnRef << 6));
    if( fPrintFormData )    a16Bit |= 0x0400;
    if( fSaveFormData )     a16Bit |= 0x0800;
    if( fShadeFormData )    a16Bit |= 0x1000;
    if( fWCFootnoteEdn )         a16Bit |= 0x8000;
    Set_UInt16( pData, a16Bit );

    Set_UInt32( pData, cLines );        // 56 0x38
    Set_UInt32( pData, cWordsFootnoteEnd );  // 60 0x3c
    Set_UInt32( pData, cChFootnoteEdn );     // 64 0x40
    Set_UInt16( pData, cPgFootnoteEdn );     // 68 0x44
    Set_UInt32( pData, cParasFootnoteEdn );  // 70 0x46
    Set_UInt32( pData, cLinesFootnoteEdn );  // 74 0x4a
    Set_UInt32( pData, lKeyProtDoc );   // 78 0x4e

    a16Bit = 0;                         // 82 0x52
    if (wvkSaved)
        a16Bit |= 0x0007;
    a16Bit |= (0x0ff8 & (wScaleSaved << 3));
    a16Bit |= (0x3000 & (zkSaved << 12));
    if (iGutterPos)
    {
        // Last bit: gutter at top.
        a16Bit |= 0x8000;
    }
    Set_UInt16( pData, a16Bit );

    if( 8 == rFib.m_nVersion )
    {
        Set_UInt32(pData, GetCompatibilityOptions());  // 84 0x54

        Set_UInt16( pData, adt );                      // 88 0x58

        doptypography.WriteToMem(pData);               // 400 0x190

        memcpy( pData, &dogrid, sizeof( WW8_DOGRID ));
        pData += sizeof( WW8_DOGRID );

        a16Bit = 0x12;      // set lvl to 9            // 410 0x19a
        if( fHtmlDoc )          a16Bit |= 0x0200;
        if( fSnapBorder )       a16Bit |= 0x0800;
        if( fIncludeHeader )    a16Bit |= 0x1000;
        if( fIncludeFooter )    a16Bit |= 0x2000;
        if( fForcePageSizePag ) a16Bit |= 0x4000;
        if( fMinFontSizePag )   a16Bit |= 0x8000;
        Set_UInt16( pData, a16Bit );

        a16Bit = 0;                                    // 412 0x19c
        if( fHaveVersions ) a16Bit |= 0x0001;
        if( fAutoVersion )  a16Bit |= 0x0002;
        Set_UInt16( pData, a16Bit );

        pData += 12;                                   // 414 0x19e

        Set_UInt32( pData, cChWS );                    // 426 0x1aa
        Set_UInt32( pData, cChWSFootnoteEdn );              // 430 0x1ae
        Set_UInt32( pData, grfDocEvents );             // 434 0x1b2

        pData += 4+30+8;  // 438 0x1b6; 442 0x1ba; 472 0x1d8; 476 0x1dc

        Set_UInt32( pData, cDBC );                     // 480 0x1e0
        Set_UInt32( pData, cDBCFootnoteEdn );               // 484 0x1e4

        pData += 1 * sizeof( sal_Int32);                   // 488 0x1e8

        Set_UInt16( pData, nfcFootnoteRef );                // 492 0x1ec
        Set_UInt16( pData, nfcEdnRef );                // 494 0x1ee
        Set_UInt16( pData, hpsZoomFontPag );           // 496 0x1f0
        Set_UInt16( pData, dywDispPag );               // 498 0x1f2

        //500 -> 508, Appear to be repeated here in 2000+
        pData += 8;
        Set_UInt32(pData, GetCompatibilityOptions());
        Set_UInt32(pData, GetCompatibilityOptions2());
        pData += 32;

        a16Bit = 0;
        if (fEmbedFactoids)
            a16Bit |= 0x8;
        if (fAcetateShowMarkup)
            a16Bit |= 0x1000;
        //Word XP at least requires fAcetateShowMarkup to honour fAcetateShowAtn
        if (fAcetateShowAtn)
        {
            a16Bit |= 0x1000;
            a16Bit |= 0x2000;
        }
        Set_UInt16(pData, a16Bit);

        pData += 48;
        a16Bit = 0x0080;
        Set_UInt16(pData, a16Bit);
    }
    rStrm.WriteBytes(aData, nLen);
}

void WW8DopTypography::ReadFromMem(sal_uInt8 *&pData)
{
    sal_uInt16 a16Bit = Get_UShort(pData);
    m_fKerningPunct = (a16Bit & 0x0001);
    m_iJustification = (a16Bit & 0x0006) >>  1;
    m_iLevelOfKinsoku = (a16Bit & 0x0018) >>  3;
    m_f2on1 = (a16Bit & 0x0020) >>  5;
    m_reserved1 = (a16Bit & 0x03C0) >>  6;
    m_reserved2 = (a16Bit & 0xFC00) >>  10;

    m_cchFollowingPunct = Get_Short(pData);
    m_cchLeadingPunct = Get_Short(pData);

    sal_Int16 i;
    for (i=0; i < nMaxFollowing; ++i)
        m_rgxchFPunct[i] = Get_Short(pData);
    for (i=0; i < nMaxLeading; ++i)
        m_rgxchLPunct[i] = Get_Short(pData);

    if (m_cchFollowingPunct >= 0 && m_cchFollowingPunct < nMaxFollowing)
        m_rgxchFPunct[m_cchFollowingPunct]=0;
    else
        m_rgxchFPunct[nMaxFollowing - 1]=0;

    if (m_cchLeadingPunct >= 0 && m_cchLeadingPunct < nMaxLeading)
        m_rgxchLPunct[m_cchLeadingPunct]=0;
    else
        m_rgxchLPunct[nMaxLeading - 1]=0;

}

void WW8DopTypography::WriteToMem(sal_uInt8 *&pData) const
{
    sal_uInt16 a16Bit = sal_uInt16(m_fKerningPunct);
    a16Bit |= (m_iJustification << 1) & 0x0006;
    a16Bit |= (m_iLevelOfKinsoku << 3) & 0x0018;
    a16Bit |= (int(m_f2on1) << 5) & 0x0020;
    a16Bit |= (m_reserved1 << 6) & 0x03C0;
    a16Bit |= (m_reserved2 << 10) & 0xFC00;
    Set_UInt16(pData,a16Bit);

    Set_UInt16(pData,m_cchFollowingPunct);
    Set_UInt16(pData,m_cchLeadingPunct);

    sal_Int16 i;
    for (i=0; i < nMaxFollowing; ++i)
        Set_UInt16(pData,m_rgxchFPunct[i]);
    for (i=0; i < nMaxLeading; ++i)
        Set_UInt16(pData,m_rgxchLPunct[i]);
}

LanguageType WW8DopTypography::GetConvertedLang() const
{
    LanguageType nLang;
    //I have assumed people's republic/taiwan == simplified/traditional

    //This isn't a documented issue, so we might have it all wrong,
    //i.e. i.e. what's with the powers of two ?

    /*
    One example of 3 for reserved1 which was really Japanese, perhaps last bit
    is for some other use ?, or redundant. If more examples trigger the assert
    we might be able to figure it out.
    */
    switch(m_reserved1 & 0xE)
    {
        case 2:     //Japan
            nLang = LANGUAGE_JAPANESE;
            break;
        case 4:     //Chinese (People's Republic)
            nLang = LANGUAGE_CHINESE_SIMPLIFIED;
            break;
        case 6:     //Korean
            nLang = LANGUAGE_KOREAN;
            break;
        case 8:     //Chinese (Taiwan)
            nLang = LANGUAGE_CHINESE_TRADITIONAL;
            break;
        default:
            OSL_ENSURE(false, "Unknown MS Asian Typography language, report");
            nLang = LANGUAGE_CHINESE_SIMPLIFIED_LEGACY;
            break;
        case 0:
            //And here we have the possibility that it says 2, but it's really
            //a bug and only japanese level 2 has been selected after a custom
            //version was chosen on last save!
            nLang = LANGUAGE_JAPANESE;
            break;
    }
    return nLang;
}

//              Sprms

sal_uInt16 wwSprmParser::GetSprmTailLen(sal_uInt16 nId, const sal_uInt8* pSprm, sal_Int32 nRemLen)
    const
{
    SprmInfo aSprm = GetSprmInfo(nId);
    sal_uInt16 nL = 0;                      // number of Bytes to read

    //sprmPChgTabs
    switch( nId )
    {
        case 23:
        case 0xC615:
            if( pSprm[1 + mnDelta] != 255 )
                nL = static_cast< sal_uInt16 >(pSprm[1 + mnDelta] + aSprm.nLen);
            else
            {
                sal_uInt8 nDelIdx = 2 + mnDelta;
                sal_uInt8 nDel = nDelIdx < nRemLen ? pSprm[nDelIdx] : 0;
                sal_uInt8 nInsIdx = 3 + mnDelta + 4 * nDel;
                sal_uInt8 nIns = nInsIdx < nRemLen ? pSprm[nInsIdx] : 0;

                nL = 2 + 4 * nDel + 3 * nIns;
            }
            break;
        default:
            switch (aSprm.nVari)
            {
                case L_FIX:
                    nL = aSprm.nLen;        // Excl. Token
                    break;
                case L_VAR:
                    // Variable 1-Byte Length
                    // parameter length (i.e. excluding token and length byte)
                    nL = static_cast< sal_uInt16 >(pSprm[1 + mnDelta] + aSprm.nLen);
                    break;
                case L_VAR2:
                {
                    // Variable 2-Byte Length
                    // For sprmTDefTable and sprmTDefTable10, the length of the
                    // parameter plus 1 is recorded in the two bytes beginning
                    // at offset (WW7-) 1 or (WW8+) 2
                    sal_uInt8 nIndex = 1 + mnDelta;
                    sal_uInt16 nCount;
                    if (nIndex + 1 >= nRemLen)
                    {
                        SAL_WARN("sw.ww8", "sprm longer than remaining bytes, doc or parser is wrong");
                        nCount = 0;
                    }
                    else
                    {
                        nCount = SVBT16ToUInt16(&pSprm[nIndex]);
                        SAL_WARN_IF(nCount < 1, "sw.ww8", "length should have been at least 1");
                        if (nCount)
                            --nCount;
                    }
                    nL = static_cast<sal_uInt16>(nCount + aSprm.nLen);
                    break;
                }
                default:
                    OSL_ENSURE(false, "Unknown sprm variant");
                    break;
            }
            break;
    }
    return nL;
}

// one or two bytes at the beginning at the sprm id
sal_uInt16 wwSprmParser::GetSprmId(const sal_uInt8* pSp) const
{
    OSL_ENSURE(pSp, "Why GetSprmId with pSp of 0");
    if (!pSp)
        return 0;

    sal_uInt16 nId = 0;

    if (ww::IsSevenMinus(meVersion))
    {
        nId = *pSp; // [0..0xff]
    }
    else
    {
        nId = SVBT16ToUInt16(pSp);
        if (0x0800 > nId)
            nId = 0;
    }

    return nId;
}

// with tokens and length byte
sal_Int32 wwSprmParser::GetSprmSize(sal_uInt16 nId, const sal_uInt8* pSprm, sal_Int32 nRemLen) const
{
    return GetSprmTailLen(nId, pSprm, nRemLen) + 1 + mnDelta + SprmDataOfs(nId);
}

sal_uInt8 wwSprmParser::SprmDataOfs(sal_uInt16 nId) const
{
    return GetSprmInfo(nId).nVari;
}

sal_Int32 wwSprmParser::DistanceToData(sal_uInt16 nId) const
{
    return 1 + mnDelta + SprmDataOfs(nId);
}

SprmResult wwSprmParser::findSprmData(sal_uInt16 nId, sal_uInt8* pSprms,
    sal_Int32 nLen) const
{
    while (nLen >= MinSprmLen())
    {
        const sal_uInt16 nCurrentId = GetSprmId(pSprms);
        // set pointer to data
        sal_Int32 nSize = GetSprmSize(nCurrentId, pSprms, nLen);

        bool bValid = nSize <= nLen;

        SAL_WARN_IF(!bValid, "sw.ww8",
            "sprm 0x" << std::hex << nCurrentId << std::dec << " longer than remaining bytes, " <<
            nSize << " vs " << nLen << "doc or parser is wrong");

        if (nCurrentId == nId && bValid) // Sprm found
        {
            sal_Int32 nFixedLen = DistanceToData(nId);
            return SprmResult(pSprms + nFixedLen, nSize - nFixedLen);
        }

        //Clip to available size if wrong
        nSize = std::min(nSize, nLen);
        pSprms += nSize;
        nLen -= nSize;
    }
    // Sprm not found
    return SprmResult();
}

SEPr::SEPr() :
    bkc(2), fTitlePage(0), fAutoPgn(0), nfcPgn(0), fUnlocked(0), cnsPgn(0),
    fPgnRestart(0), fEndNote(1), lnc(0), grpfIhdt(0), nLnnMod(0), dxaLnn(0),
    dxaPgn(720), dyaPgn(720), fLBetween(0), vjc(0), dmBinFirst(0),
    dmBinOther(0), dmPaperReq(0), fPropRMark(0), ibstPropRMark(0),
    dttmPropRMark(0), dxtCharSpace(0), dyaLinePitch(0), clm(0), reserved1(0),
    dmOrientPage(0), iHeadingPgn(0), pgnStart(1), lnnMin(0), wTextFlow(0),
    reserved2(0), pgbApplyTo(0), pgbPageDepth(0), pgbOffsetFrom(0),
    xaPage(lLetterWidth), yaPage(lLetterHeight), xaPageNUp(lLetterWidth), yaPageNUp(lLetterHeight),
    dxaLeft(1800), dxaRight(1800), dyaTop(1440), dyaBottom(1440), dzaGutter(0),
    dyaHdrTop(720), dyaHdrBottom(720), ccolM1(0), fEvenlySpaced(1),
    reserved3(0), fBiDi(0), fFacingCol(0), fRTLGutter(0), fRTLAlignment(0),
    dxaColumns(720), dxaColumnWidth(0), dmOrientFirst(0), fLayout(0),
    reserved4(0)
{
}

bool checkRead(SvStream &rSt, void *pDest, sal_uInt32 nLength)
{
    return (rSt.ReadBytes(pDest, nLength) == static_cast<std::size_t>(nLength));
}

#ifdef OSL_BIGENDIAN
void swapEndian(sal_Unicode *pString)
{
    for (sal_Unicode *pWork = pString; *pWork; ++pWork)
        *pWork = OSL_SWAPWORD(*pWork);
}
#endif

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
