/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef INCLUDED_VCL_FONT_FEATURE_HXX
#define INCLUDED_VCL_FONT_FEATURE_HXX

#include <vcl/dllapi.h>
#include <rtl/character.hxx>
#include <rtl/ustring.hxx>
#include <unotools/resmgr.hxx>
#include <cstdint>
#include <vector>

namespace vcl::font
{
constexpr uint32_t featureCode(const char sFeature[4])
{
    return static_cast<uint32_t>(sFeature[0]) << 24U | static_cast<uint32_t>(sFeature[1]) << 16U
           | static_cast<uint32_t>(sFeature[2]) << 8U | static_cast<uint32_t>(sFeature[3]);
}

VCL_DLLPUBLIC OUString featureCodeAsString(uint32_t nFeature);

enum class FeatureParameterType
{
    BOOL,
    ENUM
};

enum class FeatureType
{
    OpenType,
    Graphite
};

struct VCL_DLLPUBLIC FeatureParameter
{
private:
    uint32_t m_nCode;
    OUString m_sDescription;
    TranslateId m_pDescriptionID;

public:
    FeatureParameter(uint32_t nCode, OUString aDescription);
    FeatureParameter(uint32_t nCode, TranslateId pDescriptionID);

    uint32_t getCode() const;
    OUString getDescription() const;
};

class VCL_DLLPUBLIC FeatureDefinition
{
private:
    OUString m_sDescription;
    TranslateId m_pDescriptionID;
    OUString m_sNumericPart;
    uint32_t m_nCode;
    int32_t m_nDefault;
    FeatureParameterType m_eType;
    // the index of the parameter defines the enum value, string is the description
    std::vector<FeatureParameter> m_aEnumParameters;

public:
    FeatureDefinition();
    FeatureDefinition(uint32_t nCode, OUString aDescription,
                      FeatureParameterType eType = FeatureParameterType::BOOL,
                      std::vector<FeatureParameter>&& rEnumParameters
                      = std::vector<FeatureParameter>{},
                      int32_t nDefault = -1);
    FeatureDefinition(uint32_t nCode, TranslateId pDescriptionID,
                      OUString aNumericPart = OUString());
    FeatureDefinition(uint32_t nCode, TranslateId pDescriptionID,
                      std::vector<FeatureParameter> aEnumParameters);

    const std::vector<FeatureParameter>& getEnumParameters() const;
    uint32_t getCode() const;
    OUString getDescription() const;
    FeatureParameterType getType() const;
    int32_t getDefault() const;

    operator bool() const;
};

struct Feature
{
    Feature();
    Feature(uint32_t const nCode, FeatureType eType);

    bool isCharacterVariant() const
    {
        return ((m_nCode >> 24) & 0xFF) == 'c' && ((m_nCode >> 16) & 0xFF) == 'v'
               && rtl::isAsciiDigit((m_nCode >> 8) & 0xFF)
               && rtl::isAsciiDigit((m_nCode >> 0) & 0xFF);
    }

    bool isStylisticSet() const
    {
        return ((m_nCode >> 24) & 0xFF) == 's' && ((m_nCode >> 16) & 0xFF) == 's'
               && rtl::isAsciiDigit((m_nCode >> 8) & 0xFF)
               && rtl::isAsciiDigit((m_nCode >> 0) & 0xFF);
    }

    uint32_t m_nCode;
    FeatureType m_eType;
    FeatureDefinition m_aDefinition;
};

// This is basically duplicates hb_feature_t to avoid including HarfBuzz
// headers here, so the member types should remain compatible.
struct FeatureSetting
{
    FeatureSetting(const OString& feature);

    uint32_t m_nTag;
    uint32_t m_nValue;
    unsigned int m_nStart;
    unsigned int m_nEnd;
};

} // namespace vcl::font

#endif // INCLUDED_VCL_FONT_FEATURE_HXX

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
