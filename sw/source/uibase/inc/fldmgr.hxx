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
#ifndef INCLUDED_SW_SOURCE_UIBASE_INC_FLDMGR_HXX
#define INCLUDED_SW_SOURCE_UIBASE_INC_FLDMGR_HXX

#include <fldbas.hxx>
#include <pam.hxx>
#include <swdllapi.h>
#include <names.hxx>
#include <com/sun/star/uno/Reference.h>
#include <com/sun/star/uno/Any.h>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace com::sun::star{
    namespace text{
        class XNumberingTypeInfo;
    }
}

class SwWrtShell;
class SwField;
class SwFieldType;
class SvNumberFormatter;
namespace weld { class Widget; class Window; }
enum class SwFieldIds : sal_uInt16;

// the groups of fields
enum SwFieldGroups
{
    GRP_DOC,
    GRP_FKT,
    GRP_REF,
    GRP_REG,
    GRP_DB,
    GRP_VAR
};

struct SwFieldGroupRgn
{
    sal_uInt16 nStart;
    sal_uInt16 nEnd;
};

// the field manager handles the insertation of fields
// with command strings
struct SwInsertField_Data
{
    SwFieldTypesEnum m_nTypeId;
    sal_uInt16 m_nSubType;
    const OUString m_sPar1;
    const OUString m_sPar2;
    sal_uInt32 m_nFormatId;
    SwWrtShell* m_pSh;
    sal_Unicode m_cSeparator;
    bool m_bIsAutomaticLanguage;
    css::uno::Any m_aDBDataSource;
    css::uno::Any m_aDBConnection;
    css::uno::Any m_aDBColumn;
    weld::Widget* m_pParent; // parent widget used for SwWrtShell::StartInputFieldDlg()
    /// Marks the PostIt field's annotation start/end if it differs from the cursor selection.
    std::optional<SwPaM> m_oAnnotationRange;
    std::optional<std::tuple<sal_uInt32, sal_uInt32, SwMarkName>> m_oParentId;
    bool m_bNeverExpand;

    SwInsertField_Data(SwFieldTypesEnum nType, sal_uInt16 nSub, OUString aPar1, OUString aPar2,
                       sal_uInt32 nFormatId, SwWrtShell* pShell = nullptr, sal_Unicode cSep = ' ',
                       bool bIsAutoLanguage = true, bool bNeverExpand = false)
        : m_nTypeId(nType)
        , m_nSubType(nSub)
        , m_sPar1(std::move(aPar1))
        , m_sPar2(std::move(aPar2))
        , m_nFormatId(nFormatId)
        , m_pSh(pShell)
        , m_cSeparator(cSep)
        , m_bIsAutomaticLanguage(bIsAutoLanguage)
        , m_pParent(nullptr)
        , m_bNeverExpand(bNeverExpand)
    {
    }
};

class SW_DLLPUBLIC SwFieldMgr
{
private:
    SwField*            m_pCurField;
    SwWrtShell*         m_pWrtShell; // can be ZERO too!
    OUString          m_aCurPar1;
    OUString          m_aCurPar2;
    OUString          m_sCurFrame;

    OUString          m_sMacroPath;
    OUString          m_sMacroName;

    bool            m_bEvalExp;

    SAL_DLLPRIVATE LanguageType    GetCurrLanguage() const;

    css::uno::Reference<css::text::XNumberingTypeInfo> m_xNumberingInfo;
    SAL_DLLPRIVATE css::uno::Reference<css::text::XNumberingTypeInfo> const & GetNumberingInfo()const;

public:
    explicit SwFieldMgr(SwWrtShell* pSh = nullptr);
    ~SwFieldMgr();

    void                SetWrtShell( SwWrtShell* pShell )
                        {   m_pWrtShell = pShell;     }

     // insert field using TypeID (TYP_ ...)
    bool InsertField( SwInsertField_Data& rData );

    // change the current field directly
    void            UpdateCurField(sal_uInt32 nFormat,
                                 const OUString& rPar1,
                                 const OUString& rPar2,
                                 std::unique_ptr<SwField> _pField = nullptr);

    const OUString& GetCurFieldPar1() const { return m_aCurPar1; }
    const OUString& GetCurFieldPar2() const { return m_aCurPar2; }

    // determine a field
    SwField*        GetCurField();

    void            InsertFieldType(SwFieldType const & rType);

    bool            ChooseMacro(weld::Window* pDialogParent);
    void            SetMacroPath(const OUString& rPath);
    const OUString& GetMacroPath() const         { return m_sMacroPath; }
    const OUString& GetMacroName() const         { return m_sMacroName; }

    // previous and next of the same type
    bool GoNextPrev( bool bNext = true, SwFieldType* pTyp = nullptr );
    bool GoNext()    { return GoNextPrev(); }
    bool GoPrev()    { return GoNextPrev( false ); }

    bool            IsDBNumeric(const OUString& rDBName, const OUString& rTableQryName,
                                    bool bIsTable, const OUString& rFieldName);

    // organise RefMark with names
    bool            CanInsertRefMark( const SwMarkName& rStr );

    // access to field types via ResId
    size_t          GetFieldTypeCount() const;
    SwFieldType*    GetFieldType(SwFieldIds nResId, size_t nField = 0) const;
    SwFieldType*    GetFieldType(SwFieldIds nResId, const OUString& rName) const;

    void            RemoveFieldType(SwFieldIds nResId, const OUString& rName);

    // access via TypeId from the dialog
    // Ids for a range of fields
    static const SwFieldGroupRgn& GetGroupRange(bool bHtmlMode, sal_uInt16 nGrpId);
    static sal_uInt16           GetGroup(SwFieldTypesEnum nTypeId, sal_uInt16 nSubType);

    // the current field's TypeId
    SwFieldTypesEnum    GetCurTypeId() const;

    // TypeId for a concrete position in the list
    static SwFieldTypesEnum GetTypeId(sal_uInt16 nPos);
    // name of the type in the list of fields
    static const OUString & GetTypeStr(sal_uInt16 nPos);

    // Pos in the list of fields
    static sal_uInt16   GetPos(SwFieldTypesEnum nTypeId);

    // subtypes to a type
    void                GetSubTypes(SwFieldTypesEnum nId, std::vector<OUString>& rToFill);

    // format to a type
    sal_uInt16          GetFormatCount(SwFieldTypesEnum nTypeId, bool bHtmlMode) const;
    OUString            GetFormatStr(const SwField&) const;
    OUString            GetFormatStr(SwFieldTypesEnum nTypeId, sal_uInt32 nFormatId) const;
    sal_uInt16          GetFormatId(SwFieldTypesEnum nTypeId, sal_uInt32 nFormatId) const;
    sal_uInt32          GetDefaultFormat(SwFieldTypesEnum nTypeId, bool bIsText, SvNumberFormatter* pFormatter);

    // turn off evaluation of expression fields for insertation
    // of many expression fields (see labels)

    inline void     SetEvalExpFields(bool bEval);
    void            EvalExpFields(SwWrtShell* pSh);
};

inline void SwFieldMgr::SetEvalExpFields(bool bEval)
    { m_bEvalExp = bEval; }

#endif

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
