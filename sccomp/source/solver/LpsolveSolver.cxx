/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*************************************************************************
 *
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * Copyright 2000, 2010 Oracle and/or its affiliates.
 *
 * OpenOffice.org - a multi-platform office productivity suite
 *
 * This file is part of OpenOffice.org.
 *
 * OpenOffice.org is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version 3
 * only, as published by the Free Software Foundation.
 *
 * OpenOffice.org is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License version 3 for more details
 * (a copy is included in the LICENSE file that accompanied this code).
 *
 * You should have received a copy of the GNU Lesser General Public License
 * version 3 along with OpenOffice.org.  If not, see
 * <http://www.openoffice.org/license.html>
 * for a copy of the LGPLv3 License.
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
 *
 ************************************************************************/

#include <sal/config.h>
#include <sal/log.hxx>

#undef LANGUAGE_NONE
#if defined _WIN32
#define WINAPI __stdcall
#endif
#define LoadInverseLib FALSE
#define LoadLanguageLib FALSE
#ifdef SYSTEM_LPSOLVE
#include <lpsolve/lp_lib.h>
#else
#include <lp_lib.h>
#endif
#undef LANGUAGE_NONE

#include "SolverComponent.hxx"
#include <strings.hrc>

#include <com/sun/star/frame/XModel.hpp>
#include <com/sun/star/table/CellAddress.hpp>
#include <rtl/math.hxx>
#include <algorithm>
#include <memory>
#include <vector>

namespace com::sun::star::uno { class XComponentContext; }

using namespace com::sun::star;

namespace {

class LpsolveSolver : public SolverComponent
{
public:
    LpsolveSolver() {}

private:
    virtual void SAL_CALL solve() override;
    virtual OUString SAL_CALL getImplementationName() override
    {
        return u"com.sun.star.comp.Calc.LpsolveSolver"_ustr;
    }
    virtual OUString SAL_CALL getComponentDescription() override
    {
        return SolverComponent::GetResourceString( RID_SOLVER_COMPONENT );
    }
};

}

void SAL_CALL LpsolveSolver::solve()
{
    uno::Reference<frame::XModel> xModel( mxDoc, uno::UNO_QUERY_THROW );

    maStatus.clear();
    mbSuccess = false;

    if ( mnEpsilonLevel < EPS_TIGHT || mnEpsilonLevel > EPS_BAGGY )
    {
        maStatus = SolverComponent::GetResourceString( RID_ERROR_EPSILONLEVEL );
        return;
    }

    xModel->lockControllers();

    // collect variables in vector (?)

    const auto & aVariableCells = maVariables;
    size_t nVariables = aVariableCells.size();
    size_t nVar = 0;

    // Store all RHS values
    sal_uInt32 nConstraints = maConstraints.size();
    m_aConstrRHS.realloc(nConstraints);

    // collect all dependent cells

    ScSolverCellHashMap aCellsHash;
    aCellsHash[maObjective].reserve( nVariables + 1 );                  // objective function

    for (const auto& rConstr : maConstraints)
    {
        table::CellAddress aCellAddr = rConstr.Left;
        aCellsHash[aCellAddr].reserve( nVariables + 1 );                // constraints: left hand side

        if ( rConstr.Right >>= aCellAddr )
            aCellsHash[aCellAddr].reserve( nVariables + 1 );            // constraints: right hand side
    }

    // set all variables to zero
    //! store old values?
    //! use old values as initial values?
    for ( const auto& rVarCell : aVariableCells )
    {
        SolverComponent::SetValue( mxDoc, rVarCell, 0.0 );
    }

    // read initial values from all dependent cells
    for ( auto& rEntry : aCellsHash )
    {
        double fValue = SolverComponent::GetValue( mxDoc, rEntry.first );
        rEntry.second.push_back( fValue );                         // store as first element, as-is
    }

    // loop through variables
    for ( const auto& rVarCell : aVariableCells )
    {
        SolverComponent::SetValue( mxDoc, rVarCell, 1.0 );      // set to 1 to examine influence

        // read value change from all dependent cells
        for ( auto& rEntry : aCellsHash )
        {
            double fChanged = SolverComponent::GetValue( mxDoc, rEntry.first );
            double fInitial = rEntry.second.front();
            rEntry.second.push_back( fChanged - fInitial );
        }

        SolverComponent::SetValue( mxDoc, rVarCell, 2.0 );      // minimal test for linearity

        for ( const auto& rEntry : aCellsHash )
        {
            double fInitial = rEntry.second.front();
            double fCoeff   = rEntry.second.back();       // last appended: coefficient for this variable
            double fTwo     = SolverComponent::GetValue( mxDoc, rEntry.first );

            bool bLinear = rtl::math::approxEqual( fTwo, fInitial + 2.0 * fCoeff ) ||
                           rtl::math::approxEqual( fInitial, fTwo - 2.0 * fCoeff );
            // second comparison is needed in case fTwo is zero
            if ( !bLinear )
                maStatus = SolverComponent::GetResourceString( RID_ERROR_NONLINEAR );
        }

        SolverComponent::SetValue( mxDoc, rVarCell, 0.0 );      // set back to zero for examining next variable
    }

    xModel->unlockControllers();

    if ( !maStatus.isEmpty() )
        return;


    // build lp_solve model


    lprec* lp = make_lp( 0, nVariables );
    if ( !lp )
        return;

    set_outputfile( lp, const_cast<char*>( "" ) );  // no output

    // set objective function

    const std::vector<double>& rObjCoeff = aCellsHash[maObjective];
    std::unique_ptr<REAL[]> pObjVal(new REAL[nVariables+1]);
    pObjVal[0] = 0.0;                           // ignored
    for (nVar=0; nVar<nVariables; nVar++)
        pObjVal[nVar+1] = rObjCoeff[nVar+1];
    set_obj_fn( lp, pObjVal.get() );
    pObjVal.reset();
    set_rh( lp, 0, rObjCoeff[0] );              // constant term of objective

    // add rows

    set_add_rowmode(lp, TRUE);

    sal_uInt32 nConstrCount(0);
    double* pConstrRHS = m_aConstrRHS.getArray();

    for (const auto& rConstr : maConstraints)
    {
        // integer constraints are set later
        sheet::SolverConstraintOperator eOp = rConstr.Operator;
        if ( eOp == sheet::SolverConstraintOperator_LESS_EQUAL ||
             eOp == sheet::SolverConstraintOperator_GREATER_EQUAL ||
             eOp == sheet::SolverConstraintOperator_EQUAL )
        {
            double fDirectValue = 0.0;
            bool bRightCell = false;
            table::CellAddress aRightAddr;
            const uno::Any& rRightAny = rConstr.Right;
            if ( rRightAny >>= aRightAddr )
                bRightCell = true;                  // cell specified as right-hand side
            else
                rRightAny >>= fDirectValue;         // constant value

            table::CellAddress aLeftAddr = rConstr.Left;

            const std::vector<double>& rLeftCoeff = aCellsHash[aLeftAddr];
            std::unique_ptr<REAL[]> pValues(new REAL[nVariables+1] );
            pValues[0] = 0.0;                               // ignored?
            for (nVar=0; nVar<nVariables; nVar++)
                pValues[nVar+1] = rLeftCoeff[nVar+1];

            // if left hand cell has a constant term, put into rhs value
            double fRightValue = -rLeftCoeff[0];

            if ( bRightCell )
            {
                const std::vector<double>& rRightCoeff = aCellsHash[aRightAddr];
                // modify pValues with rhs coefficients
                for (nVar=0; nVar<nVariables; nVar++)
                    pValues[nVar+1] -= rRightCoeff[nVar+1];

                fRightValue += rRightCoeff[0];      // constant term
            }
            else
                fRightValue += fDirectValue;

            // Remember the RHS value used for sensitivity analysis later
            pConstrRHS[nConstrCount] = fRightValue;

            int nConstrType = LE;
            switch ( eOp )
            {
                case sheet::SolverConstraintOperator_LESS_EQUAL:    nConstrType = LE; break;
                case sheet::SolverConstraintOperator_GREATER_EQUAL: nConstrType = GE; break;
                case sheet::SolverConstraintOperator_EQUAL:         nConstrType = EQ; break;
                default:
                    OSL_FAIL( "unexpected enum type" );
            }
            add_constraint( lp, pValues.get(), nConstrType, fRightValue );
            nConstrCount++;
        }
    }

    set_add_rowmode(lp, FALSE);

    // apply settings to all variables

    for (nVar=0; nVar<nVariables; nVar++)
    {
        if ( !mbNonNegative )
            set_unbounded(lp, nVar+1);          // allow negative (default is non-negative)
                                                //! collect bounds from constraints?
        if ( mbInteger )
            set_int(lp, nVar+1, TRUE);
    }

    // apply single-var integer constraints

    for (const auto& rConstr : maConstraints)
    {
        sheet::SolverConstraintOperator eOp = rConstr.Operator;
        if ( eOp == sheet::SolverConstraintOperator_INTEGER ||
             eOp == sheet::SolverConstraintOperator_BINARY )
        {
            table::CellAddress aLeftAddr = rConstr.Left;
            // find variable index for cell
            for (nVar=0; nVar<nVariables; nVar++)
                if ( AddressEqual( aVariableCells[nVar], aLeftAddr ) )
                {
                    if ( eOp == sheet::SolverConstraintOperator_INTEGER )
                        set_int(lp, nVar+1, TRUE);
                    else
                        set_binary(lp, nVar+1, TRUE);
                }
        }
    }

    if ( mbMaximize )
        set_maxim(lp);
    else
        set_minim(lp);

    if ( !mbLimitBBDepth )
        set_bb_depthlimit( lp, 0 );

    set_epslevel( lp, mnEpsilonLevel );
    set_timeout( lp, mnTimeout );

    // solve model

    int nResult = ::solve( lp );

    mbSuccess = ( nResult == OPTIMAL );
    if ( mbSuccess )
    {
        // get solution

        maSolution.realloc( nVariables );

        REAL* pResultVar = nullptr;
        get_ptr_variables( lp, &pResultVar );
        std::copy_n(pResultVar, nVariables, maSolution.getArray());

        mfResultValue = get_objective( lp );

        // Initially set to false because getting the report might fail
        m_aSensitivityReport.HasReport = false;

        // Get sensitivity report if the user set SensitivityReport parameter to true
        if (mbGenSensitivity)
        {
            // Get sensitivity data about the objective function
            // LpSolve returns an interval for the coefficients of the objective function
            // instead of returning an allowable increase/decrease (which is what we want to show
            // in the sensitivity report; so we these from/till values are converted into increase
            // and decrease values later)
            REAL* pObjFrom = nullptr;
            REAL* pObjTill = nullptr;
            bool bHasObjReport = false;
            bHasObjReport = get_ptr_sensitivity_obj(lp, &pObjFrom, &pObjTill);

            // Get sensitivity data about constraints
            // Similarly to the objective function, the sensitivity values returned for the
            // constraints are in the form from/till and are later converted to increase and
            // decrease values later
            REAL* pConstrValue = nullptr;
            REAL* pConstrDual = nullptr;
            REAL* pConstrFrom = nullptr;
            REAL* pConstrTill = nullptr;
            bool bHasConstrReport = false;
            bHasConstrReport = get_ptr_sensitivity_rhs(lp, &pConstrDual, &pConstrFrom, &pConstrTill);

            // When successful, store sensitivity data in the solver component
            if (bHasObjReport && bHasConstrReport)
            {
                m_aSensitivityReport.HasReport = true;
                m_aObjDecrease.realloc(nVariables);
                m_aObjIncrease.realloc(nVariables);
                double* pObjDecrease = m_aObjDecrease.getArray();
                double* pObjIncrease = m_aObjIncrease.getArray();
                for (size_t i = 0; i < nVariables; i++)
                {
                    // Allowed decrease. Note that the indices of rObjCoeff are offset by 1
                    // because of the objective function
                    if (static_cast<bool>(is_infinite(lp, pObjFrom[i])))
                        pObjDecrease[i] = get_infinite(lp);
                    else
                        pObjDecrease[i] = rObjCoeff[i + 1] - pObjFrom[i];

                    // Allowed increase
                    if (static_cast<bool>(is_infinite(lp, pObjTill[i])))
                        pObjIncrease[i] = get_infinite(lp);
                    else
                        pObjIncrease[i] = pObjTill[i] - rObjCoeff[i + 1];
                }

                // Save objective coefficients for the sensitivity report
                double* pObjCoefficients(new double[nVariables]);
                for (size_t i = 0; i < nVariables; i++)
                    pObjCoefficients[i] = rObjCoeff[i + 1];
                m_aObjCoefficients.realloc(nVariables);
                std::copy_n(pObjCoefficients, nVariables, m_aObjCoefficients.getArray());

                // The reduced costs are in pConstrDual after the constraints
                double* pObjRedCost(new double[nVariables]);
                for (size_t i = 0; i < nVariables; i++)
                    pObjRedCost[i] = pConstrDual[nConstraints + i];
                m_aObjRedCost.realloc(nVariables);
                std::copy_n(pObjRedCost, nVariables, m_aObjRedCost.getArray());

                // Final value of constraints
                get_ptr_constraints(lp, &pConstrValue);
                m_aConstrValue.realloc(nConstraints);
                std::copy_n(pConstrValue, nConstraints, m_aConstrValue.getArray());

                // The RHS contains information for each constraint
                m_aConstrDual.realloc(nConstraints);
                m_aConstrDecrease.realloc(nConstraints);
                m_aConstrIncrease.realloc(nConstraints);
                std::copy_n(pConstrDual, nConstraints, m_aConstrDual.getArray());
                double* pConstrDecrease = m_aConstrDecrease.getArray();
                double* pConstrIncrease = m_aConstrIncrease.getArray();

                for (sal_uInt32 i = 0; i < nConstraints; i++)
                {
                    // Allowed decrease
                    pConstrDecrease[i] = m_aConstrRHS[i] - pConstrFrom[i];
                    if (static_cast<bool>(is_infinite(lp, pConstrFrom[i]))
                        && maConstraints[i].Operator == sheet::SolverConstraintOperator_LESS_EQUAL)
                        pConstrDecrease[i] = m_aConstrRHS[i] - m_aConstrValue[i];

                    // Allowed increase
                    pConstrIncrease[i] = pConstrTill[i] - m_aConstrRHS[i];
                    if (static_cast<bool>(is_infinite(lp, pConstrTill[i]))
                        && maConstraints[i].Operator == sheet::SolverConstraintOperator_GREATER_EQUAL)
                        pConstrIncrease[i] = m_aConstrValue[i] - m_aConstrRHS[i];
                }

                // Set all values of the SensitivityReport object
                m_aSensitivityReport.ObjCoefficients = m_aObjCoefficients;
                m_aSensitivityReport.ObjReducedCosts = m_aObjRedCost;
                m_aSensitivityReport.ObjAllowableDecreases = m_aObjDecrease;
                m_aSensitivityReport.ObjAllowableIncreases = m_aObjIncrease;
                m_aSensitivityReport.ConstrValues = m_aConstrValue;
                m_aSensitivityReport.ConstrRHS = m_aConstrRHS;
                m_aSensitivityReport.ConstrShadowPrices = m_aConstrDual;
                m_aSensitivityReport.ConstrAllowableDecreases = m_aConstrDecrease;
                m_aSensitivityReport.ConstrAllowableIncreases = m_aConstrIncrease;
            }
        }
    }
    else if ( nResult == INFEASIBLE )
        maStatus = SolverComponent::GetResourceString( RID_ERROR_INFEASIBLE );
    else if ( nResult == UNBOUNDED )
        maStatus = SolverComponent::GetResourceString( RID_ERROR_UNBOUNDED );
    else if ( nResult == TIMEOUT || nResult == SUBOPTIMAL )
        maStatus = SolverComponent::GetResourceString( RID_ERROR_TIMEOUT );
    // SUBOPTIMAL is assumed to be caused by a timeout, and reported as an error

    delete_lp( lp );
}

extern "C" SAL_DLLPUBLIC_EXPORT css::uno::XInterface *
com_sun_star_comp_Calc_LpsolveSolver_get_implementation(
    css::uno::XComponentContext *,
    css::uno::Sequence<css::uno::Any> const &)
{
    return cppu::acquire(new LpsolveSolver());
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
