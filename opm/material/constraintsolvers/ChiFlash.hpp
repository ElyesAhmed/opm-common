// -*- mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
// vi: set et ts=4 sw=4 sts=4:
/*
  This file is part of the Open Porous Media project (OPM).

  OPM is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 2 of the License, or
  (at your option) any later version.
  OPM is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with OPM.  If not, see <http://www.gnu.org/licenses/>.

  Consult the COPYING file in the top-level source directory of this
  module for the precise wording of the license and the list of
  copyright holders.
*/
/*!
 * \file
 * \copydoc Opm::ChiFlash
 */
#ifndef OPM_CHI_FLASH_HPP
#define OPM_CHI_FLASH_HPP

#include <opm/material/fluidmatrixinteractions/NullMaterial.hpp>
#include <opm/material/fluidmatrixinteractions/MaterialTraits.hpp>
#include <opm/material/fluidstates/CompositionalFluidState.hpp>
#include <opm/material/densead/Evaluation.hpp>
#include <opm/material/densead/Math.hpp>
#include <opm/material/common/MathToolbox.hpp>
#include <opm/material/common/Valgrind.hpp>
#include <opm/material/Constants.hpp>
#include <opm/material/eos/PengRobinsonMixture.hpp>

#include <opm/material/common/Exceptions.hpp>

#include <dune/common/fvector.hh>
#include <dune/common/fmatrix.hh>
#include <dune/common/version.hh>
#include <dune/common/classname.hh>

#include <limits>
#include <iostream>
#include <iomanip>
#include <type_traits>

namespace Opm {

/*!
 * \brief Determines the phase compositions, pressures and saturations
 *        given the total mass of all components for the chiwoms problem.
 *
 */
template <class Scalar, class FluidSystem>
class ChiFlash
{
    //using Problem = GetPropType<TypeTag, Properties::Problem>;
    enum { numPhases = FluidSystem::numPhases };
    enum { numComponents = FluidSystem::numComponents };
    // enum { Comp0Idx = FluidSystem::Comp0Idx }; //rename for generic ?
    // enum { Comp1Idx = FluidSystem::Comp1Idx }; //rename for generic ?
    enum { oilPhaseIdx = FluidSystem::oilPhaseIdx};
    enum { gasPhaseIdx = FluidSystem::gasPhaseIdx};
    enum { numMiscibleComponents = 3}; //octane, co2 // should be brine instead of brine here.
    enum { numMisciblePhases = 2}; //oil, gas
    enum {
        numEq =
        numMisciblePhases+
        numMisciblePhases*numMiscibleComponents
    };//pressure, saturation, composition

    /* enum {
        // p0PvIdx = 0, // pressure first phase primary variable index
        // S0PvIdx = 1, // saturation first phase primary variable index
        // x00PvIdx = S0PvIdx + 1, // molefraction first phase first component primary variable index
        //numMiscibleComponennets*numMisciblePhases-1 molefractions/primvar follow
    }; */

public:
    /*!
     * \brief Calculates the fluid state from the global mole fractions of the components and the phase pressures
     *
     */
    template <class FluidState>
    static void solve(FluidState& fluid_state,
                      const Dune::FieldVector<typename FluidState::Scalar, numComponents>& z,
                      int spatialIdx,
                      int verbosity,
                      std::string twoPhaseMethod,
                      Scalar tolerance)
    {

        using InputEval = typename FluidState::Scalar;
        using ComponentVector = Dune::FieldVector<typename FluidState::Scalar, numComponents>;

#if ! DUNE_VERSION_NEWER(DUNE_COMMON, 2,7)
        Dune::FMatrixPrecision<InputEval>::set_singular_limit(1e-35);
#endif

        if (tolerance <= 0)
            tolerance = std::min<Scalar>(1e-3, 1e8*std::numeric_limits<Scalar>::epsilon());
        
        //K and L from previous timestep (wilson and -1 initially)
        ComponentVector K;
        for(int compIdx = 0; compIdx < numComponents; ++compIdx) {
            K[compIdx] = fluid_state.K(compIdx);
        }
        InputEval L;
        L = fluid_state.L(0);

        // Print header
        if (verbosity >= 1) {
            std::cout << "********" << std::endl;
            std::cout << "Flash calculations on Cell " << spatialIdx << std::endl;
            std::cout << "Inputs are K = [" << K << "], L = [" << L << "], z = [" << z << "], P = " << fluid_state.pressure(0) << ", and T = " << fluid_state.temperature(0) << std::endl;
        }

        using ScalarVector = Dune::FieldVector<Scalar, numComponents>;
        Scalar L_scalar = Opm::getValue(L);
        ScalarVector z_scalar;
        ScalarVector K_scalar;
        for (unsigned i = 0; i < numComponents; ++i) {
            z_scalar[i] = Opm::getValue(z[i]);
            K_scalar[i] = Opm::getValue(K[i]);
        }
        using ScalarFluidState = CompositionalFluidState<Scalar, FluidSystem>;
        ScalarFluidState fluid_state_scalar;

        for (unsigned compIdx = 0; compIdx < numComponents; ++compIdx) {
            fluid_state_scalar.setMoleFraction(oilPhaseIdx, compIdx, Opm::getValue(fluid_state.moleFraction(oilPhaseIdx, compIdx)));
            fluid_state_scalar.setMoleFraction(gasPhaseIdx, compIdx, Opm::getValue(fluid_state.moleFraction(gasPhaseIdx, compIdx)));
            fluid_state_scalar.setKvalue(compIdx, Opm::getValue(fluid_state.K(compIdx)));
        }

        fluid_state_scalar.setLvalue(L_scalar);
        // other values need to be Scalar, but I guess the fluidstate does not support it yet.
        fluid_state_scalar.setPressure(FluidSystem::oilPhaseIdx,
                                       Opm::getValue(fluid_state.pressure(FluidSystem::oilPhaseIdx)));
        fluid_state_scalar.setPressure(FluidSystem::gasPhaseIdx,
                                       Opm::getValue(fluid_state.pressure(FluidSystem::gasPhaseIdx)));

        // TODO: not sure whether we need to set the saturations
        fluid_state_scalar.setSaturation(FluidSystem::gasPhaseIdx,
                                         Opm::getValue(fluid_state_scalar.saturation(FluidSystem::gasPhaseIdx)));
        fluid_state_scalar.setSaturation(FluidSystem::oilPhaseIdx,
                                         Opm::getValue(fluid_state_scalar.saturation(FluidSystem::oilPhaseIdx)));
        fluid_state_scalar.setTemperature(Opm::getValue(fluid_state.temperature(0)));

        // Rachford Rice equation to get initial L for composition solver
        L_scalar = solveRachfordRice_g_(K_scalar, z_scalar, verbosity);


        // Do a stability test to check if cell is single-phase (do for all cells the first time).
        bool isStable = false;
        if ( L <= 0 || L == 1 ) {
             if (verbosity >= 1) {
                 std::cout << "Perform stability test (L <= 0 or L == 1)!" << std::endl;
             }
            phaseStabilityTest_(isStable, K_scalar, fluid_state_scalar, z_scalar, verbosity);
        }
        if (verbosity >= 1) {
            std::cout << "Inputs after stability test are K = [" << K_scalar << "], L = [" << L_scalar << "], z = [" << z_scalar << "], P = " << fluid_state.pressure(0) << ", and T = " << fluid_state.temperature(0) << std::endl;
        }

        // Update the composition if cell is two-phase
        if ( !isStable) {
            flash_2ph(z_scalar, twoPhaseMethod, K_scalar, L_scalar, fluid_state_scalar, verbosity);
        } else {
            // Cell is one-phase. Use Li's phase labeling method to see if it's liquid or vapor
            L = li_single_phase_label_(fluid_state, z, verbosity);
        }

        // Print footer
        if (verbosity >= 1) {
            std::cout << "********" << std::endl;
        }

        // Update phases
        /* typename FluidSystem::template ParameterCache<InputEval> paramCache;
        paramCache.updatePhase(fluid_state, oilPhaseIdx);
        paramCache.updatePhase(fluid_state, gasPhaseIdx); */

        /* // Calculate compressibility factor
        const Scalar R = Opm::Constants<Scalar>::R;
        InputEval Z_L = (paramCache.molarVolume(oilPhaseIdx) * fluid_state.pressure(oilPhaseIdx) ) /
                        (R * fluid_state.temperature(oilPhaseIdx));
        InputEval Z_V = (paramCache.molarVolume(gasPhaseIdx) * fluid_state.pressure(gasPhaseIdx) ) /
                        (R * fluid_state.temperature(gasPhaseIdx));

        std::cout << " the type of InputEval here is " << Dune::className<InputEval>() << std::endl;
        // Update saturation
        InputEval So = L*Z_L/(L*Z_L+(1-L)*Z_V);
        InputEval Sg = 1-So;
        
        fluid_state.setSaturation(oilPhaseIdx, So);
        fluid_state.setSaturation(gasPhaseIdx, Sg);

        //Update L and K to the problem for the next flash
        for (int compIdx = 0; compIdx < numComponents; ++compIdx){
            fluid_state.setKvalue(compIdx, K[compIdx]);
        }
        fluid_state.setCompressFactor(oilPhaseIdx, Z_L);
        fluid_state.setCompressFactor(gasPhaseIdx, Z_V);
        fluid_state.setLvalue(L); */


        // Print saturation

        /* std::cout << " output the molefraction derivatives" << std::endl;
        std::cout << " for vapor comp 1 "  << std::endl;
        fluid_state.moleFraction(gasPhaseIdx, 0).print();
        std::cout << std::endl << " for vapor comp 2 " << std::endl;
        fluid_state.moleFraction(gasPhaseIdx, 1).print();
        std::cout << std::endl << " for vapor comp 3 " << std::endl;
        fluid_state.moleFraction(gasPhaseIdx, 2).print();
        std::cout << std::endl;
        std::cout << " for oil comp 1 "  << std::endl;
        fluid_state.moleFraction(oilPhaseIdx, 0).print();
        std::cout << std::endl << " for oil comp 2 " << std::endl;
        fluid_state.moleFraction(oilPhaseIdx, 1).print();
        std::cout << std::endl << " for oil comp 3 " << std::endl;
        fluid_state.moleFraction(oilPhaseIdx, 2).print();
        std::cout << std::endl;
        std::cout << " for pressure " << std::endl;
        fluid_state.pressure(0).print();
        std::cout<< std::endl;
        fluid_state.pressure(1).print();
        std::cout<< std::endl; */

        // Update densities
       //  fluid_state.setDensity(oilPhaseIdx, FluidSystem::density(fluid_state, paramCache, oilPhaseIdx));
       //  fluid_state.setDensity(gasPhaseIdx, FluidSystem::density(fluid_state, paramCache, gasPhaseIdx));

        // check the residuals of the equations
        /* using NewtonVector = Dune::FieldVector<InputEval, numMisciblePhases*numMiscibleComponents+1>;
        NewtonVector newtonX;
        NewtonVector newtonB;
        for (int compIdx=0; compIdx<numComponents; ++compIdx){
            newtonX[compIdx] = Opm::getValue(fluid_state.moleFraction(oilPhaseIdx, compIdx));
            newtonX[compIdx + numMiscibleComponents] = Opm::getValue(fluid_state.moleFraction(gasPhaseIdx, compIdx));
        }
        newtonX[numMisciblePhases*numMiscibleComponents] = Opm::getValue(L);

        evalDefect_(newtonB, newtonX, fluid_state, z); */
        /* std::cout << " the residuals of the equations is " << std::endl;
        for (unsigned i = 0; i < newtonB.N(); ++i) {
            std::cout << newtonB[i] << std::endl;
        }
        std::cout << std::endl;
        if (verbosity >= 5) {
            std::cout << " mole fractions for oil " << std::endl;
            for (int i = 0; i < numComponents; ++i) {
                std::cout << " i " << i << "  " << fluid_state.moleFraction(oilPhaseIdx, i) << std::endl;
            }
            std::cout << " mole fractions for gas " << std::endl;
            for (int i = 0; i < numComponents; ++i) {
                std::cout << " i " << i << "  " << fluid_state.moleFraction(gasPhaseIdx, i) << std::endl;
            }
            std::cout << " K " << std::endl;
            for (int i = 0; i < numComponents; ++i) {
                std::cout << " i " << K[i] << std::endl;
            }
            std::cout << "Z_L = " << Z_L <<std::endl;
            std::cout << "Z_V = " << Z_V <<std::endl;
            std::cout << " fugacity for oil phase " << std::endl;
            for (int i = 0; i < numComponents; ++i) {
                auto phi = FluidSystem::fugacityCoefficient(fluid_state, paramCache, oilPhaseIdx, i);
                std::cout << " i " << i << " " << phi << std::endl;
            }
            std::cout << " fugacity for gas phase " << std::endl;
            for (int i = 0; i < numComponents; ++i) {
                auto phi = FluidSystem::fugacityCoefficient(fluid_state, paramCache, gasPhaseIdx, i);
                std::cout << " i " << i << " " << phi << std::endl;
            }
            std::cout << " density for oil phase " << std::endl;
            std::cout << FluidSystem::density(fluid_state, paramCache, oilPhaseIdx) << std::endl;
            std::cout << " density for gas phase " << std::endl;
            std::cout << FluidSystem::density(fluid_state, paramCache, gasPhaseIdx) << std::endl;
            std::cout << " viscosities for oil " << std::endl;
            std::cout << FluidSystem::viscosity(fluid_state, paramCache, oilPhaseIdx) << std::endl;
            std::cout << " viscosities for gas " << std::endl;
            std::cout << FluidSystem::viscosity(fluid_state, paramCache, gasPhaseIdx) << std::endl;
            std::cout << "So = " << So <<std::endl;
            std::cout << "Sg = " << Sg <<std::endl;
        } */
    }//end solve

    /*!
     * \brief Calculates the chemical equilibrium from the component
     *        fugacities in a phase.
     *
     * This is a convenience method which assumes that the capillary pressure is
     * zero...
     */
    template <class FluidState, class ComponentVector>
    static void solve(FluidState& fluidState,
                      const ComponentVector& globalMolarities,
                      Scalar tolerance = 0.0)
    {
        using MaterialTraits = NullMaterialTraits<Scalar, numPhases>;
        using MaterialLaw = NullMaterial<MaterialTraits>;
        using MaterialLawParams = typename MaterialLaw::Params;

        MaterialLawParams matParams;
        solve<MaterialLaw>(fluidState, matParams, globalMolarities, tolerance);
    }


protected:

    template <class FlashFluidState>
    static typename FlashFluidState::Scalar wilsonK_(const FlashFluidState& fluidState, int compIdx)
    {
        const auto& acf = FluidSystem::acentricFactor(compIdx);
        const auto& T_crit = FluidSystem::criticalTemperature(compIdx);
        const auto& T = fluidState.temperature(0);
        const auto& p_crit = FluidSystem::criticalPressure(compIdx);
        const auto& p = fluidState.pressure(0); //for now assume no capillary pressure

        const auto& tmp = Opm::exp(5.3727 * (1+acf) * (1-T_crit/T)) * (p_crit/p);
        return tmp;
    }

    template <class Vector, class FlashFluidState>
    static typename Vector::field_type li_single_phase_label_(const FlashFluidState& fluidState, const Vector& globalComposition, int verbosity)
    {
        // Calculate intermediate sum
        typename Vector::field_type sumVz = 0.0;
        for (int compIdx=0; compIdx<numComponents; ++compIdx){
            // Get component information
            const auto& V_crit = FluidSystem::criticalVolume(compIdx);

            // Sum calculation
            sumVz += (V_crit * globalComposition[compIdx]);
        }

        // Calculate approximate (pseudo) critical temperature using Li's method
        typename Vector::field_type Tc_est = 0.0;
        for (int compIdx=0; compIdx<numComponents; ++compIdx){
            // Get component information
            const auto& V_crit = FluidSystem::criticalVolume(compIdx);
            const auto& T_crit = FluidSystem::criticalTemperature(compIdx);

            // Sum calculation
            Tc_est += (V_crit * T_crit * globalComposition[compIdx] / sumVz);
        }

        // Get temperature
        const auto& T = fluidState.temperature(0);
        
        // If temperature is below estimated critical temperature --> phase = liquid; else vapor
        typename Vector::field_type L;
        if (T < Tc_est) {
            // Liquid
            L = 1.0;

            // Print
            if (verbosity >= 1) {
                std::cout << "Cell is single-phase, liquid (L = 1.0) due to Li's phase labeling method giving T < Tc_est (" << T << " < " << Tc_est << ")!" << std::endl;
            }
        }
        else {
            // Vapor
            L = 0.0;
            
            // Print
            if (verbosity >= 1) {
                std::cout << "Cell is single-phase, vapor (L = 0.0) due to Li's phase labeling method giving T >= Tc_est (" << T << " >= " << Tc_est << ")!" << std::endl;
            }
        }
        

        return L;
    }

    // TODO: not totally sure whether ValueType can be obtained from Vector::field_type
    template <class Vector>
    static typename Vector::field_type rachfordRice_g_(const Vector& K, typename Vector::field_type L, const Vector& globalComposition)
    {
        typename Vector::field_type g=0;
        for (int compIdx=0; compIdx<numComponents; ++compIdx){
            g += (globalComposition[compIdx]*(K[compIdx]-1))/(K[compIdx]-L*(K[compIdx]-1));
        }
        return g;
    }

    template <class Vector>
    static typename Vector::field_type rachfordRice_dg_dL_(const Vector& K, const typename Vector::field_type L, const Vector& globalComposition)
    {
        typename Vector::field_type dg=0;
        for (int compIdx=0; compIdx<numComponents; ++compIdx){
            dg += (globalComposition[compIdx]*(K[compIdx]-1)*(K[compIdx]-1))/((K[compIdx]-L*(K[compIdx]-1))*(K[compIdx]-L*(K[compIdx]-1)));
        }
        return dg;
    }

    template <class Vector>
    static typename Vector::field_type solveRachfordRice_g_(const Vector& K, const Vector& globalComposition, int verbosity)
    {
        // Find min and max K. Have to do a laborious for loop to avoid water component (where K=0)
        // TODO: Replace loop with Dune::min_value() and Dune::max_value() when water component is properly handled
        typename Vector::field_type Kmin = K[0];
        typename Vector::field_type Kmax = K[0];
        for (int compIdx=1; compIdx<numComponents; ++compIdx){
            if (K[compIdx] < Kmin)
                Kmin = K[compIdx];
            else if (K[compIdx] >= Kmax)
                Kmax = K[compIdx];
        }

        // Lower and upper bound for solution
        auto Lmin = (Kmin / (Kmin - 1));
        auto Lmax = Kmax / (Kmax - 1);

        // Check if Lmin < Lmax, and switch if not
        if (Lmin > Lmax)
        {
            auto Ltmp = Lmin;
            Lmin = Lmax;
            Lmax = Ltmp;
        }

        // Initial guess
        auto L = (Lmin + Lmax)/2;

        // Print initial guess and header
        if (verbosity == 3 || verbosity == 4) {
            std::cout << "Initial guess: L = " << L << " and [Lmin, Lmax] = [" << Lmin << ", " << Lmax << "]" << std::endl;
            std::cout << std::setw(10) << "Iteration" << std::setw(16) << "abs(step)" << std::setw(16) << "L" << std::endl;
        }

        // Newton-Raphson loop
        for (int iteration=1; iteration<100; ++iteration){
            // Calculate function and derivative values
            auto g = rachfordRice_g_(K, L, globalComposition);
            auto dg_dL = rachfordRice_dg_dL_(K, L, globalComposition);

            // Lnew = Lold - g/dg;
            auto delta = g/dg_dL;
            L -= delta;

            // Check if L is within the bounds, and if not, we apply bisection method
            if (L < Lmin || L > Lmax)
                {
                    // Print info
                    if (verbosity == 3 || verbosity == 4) {
                        std::cout << "L is not within the the range [Lmin, Lmax], solve using Bisection method!" << std::endl;
                    }

                    // Run bisection
                    L = bisection_g_(K, Lmin, Lmax, globalComposition, verbosity);
        
                    // Ensure that L is in the range (0, 1)
                    L = Opm::min(Opm::max(L, 0.0), 1.0);

                    // Print final result
                    if (verbosity >= 1) {
                        std::cout << "Rachford-Rice (Bisection) converged to final solution L = " << L << std::endl;
                    }
                    return L;
                }

            // Print iteration info
            if (verbosity == 3 || verbosity == 4) {
                std::cout << std::setw(10) << iteration << std::setw(16) << Opm::abs(delta) << std::setw(16) << L << std::endl;
            }
            // Check for convergence
            if ( Opm::abs(delta) < 1e-10 ) {
                // Ensure that L is in the range (0, 1)
                L = Opm::min(Opm::max(L, 0.0), 1.0);

                // Print final result
                if (verbosity >= 1) {
                    std::cout << "Rachford-Rice converged to final solution L = " << L << std::endl;
                }
                return L;
            }
        }
        // Throw error if Rachford-Rice fails
        throw std::runtime_error(" Rachford-Rice did not converge within maximum number of iterations" );
    }

    template <class Vector>
    static typename Vector::field_type bisection_g_(const Vector& K, typename Vector::field_type Lmin,
                                                    typename Vector::field_type Lmax, const Vector& globalComposition, int verbosity)
    {
        // Calculate for g(Lmin) for first comparison with gMid = g(L)
        typename Vector::field_type gLmin = rachfordRice_g_(K, Lmin, globalComposition);
      
        // Print new header
        if (verbosity == 3 || verbosity == 4) {
                std::cout << std::setw(10) << "Iteration" << std::setw(16) << "g(Lmid)" << std::setw(16) << "L" << std::endl;
        }
            
        // Bisection loop
        for (int iteration=1; iteration<100; ++iteration){
            // New midpoint
            auto L = (Lmin + Lmax) / 2;
            auto gMid = rachfordRice_g_(K, L, globalComposition);
            if (verbosity == 3 || verbosity == 4) {
                std::cout << std::setw(10) << iteration << std::setw(16) << gMid << std::setw(16) << L << std::endl;
            }

            // Check if midpoint fulfills g=0 or L - Lmin is sufficiently small
            if (Opm::abs(gMid) < 1e-10 || Opm::abs((Lmax - Lmin) / 2) < 1e-10){
                return L;
            }
            
            // Else we repeat with midpoint being either Lmin og Lmax (depending on the signs).
            else if (Dune::sign(gMid) != Dune::sign(gLmin)) {
                // gMid has same sign as gLmax, so we set L as the new Lmax
                Lmax = L; 
            }
            else {
                // gMid and gLmin have same sign so we set L as the new Lmin 
                Lmin = L;
                gLmin = gMid;
            }
        }
        throw std::runtime_error(" Rachford-Rice with bisection failed!");
    }

    template <class FlashFluidState, class ComponentVector>
    static void phaseStabilityTest_(bool& isStable, ComponentVector& K, FlashFluidState& fluidState, const ComponentVector& globalComposition, int verbosity)
    {
        // Declarations
        bool isTrivialL, isTrivialV;
        ComponentVector x, y;
        typename FlashFluidState::Scalar S_l, S_v;
        ComponentVector K0 = K;
        ComponentVector K1 = K;

        // Check for vapour instable phase
        if (verbosity == 3 || verbosity == 4) {
            std::cout << "Stability test for vapor phase:" << std::endl;
        }
        checkStability_(fluidState, isTrivialV, K0, y, S_v, globalComposition, /*isGas=*/true, verbosity);
        bool V_unstable = (S_v < (1.0 + 1e-5)) || isTrivialV;

        // Check for liquids stable phase
        if (verbosity == 3 || verbosity == 4) {
            std::cout << "Stability test for liquid phase:" << std::endl;
        }
        checkStability_(fluidState, isTrivialL, K1, x, S_l, globalComposition, /*isGas=*/false, verbosity);
        bool L_stable = (S_l < (1.0 + 1e-5)) || isTrivialL;

        // L-stable means success in making liquid, V-unstable means no success in making vapour
        isStable = L_stable && V_unstable; 
        if (isStable) {
            // Single phase, i.e. phase composition is equivalent to the global composition
            // Update fluidstate with mole fraction
            for (int compIdx=0; compIdx<numComponents; ++compIdx){
                fluidState.setMoleFraction(gasPhaseIdx, compIdx, globalComposition[compIdx]);
                fluidState.setMoleFraction(oilPhaseIdx, compIdx, globalComposition[compIdx]);
            }
        }
        // If not stable: use the mole fractions from Michelsen's test to update K
        else {
            for (int compIdx = 0; compIdx<numComponents; ++compIdx) {
                K[compIdx] = y[compIdx] / x[compIdx];
            }
        }
    }

    template <class FlashFluidState, class ComponentVector>
    static void checkStability_(const FlashFluidState& fluidState, bool& isTrivial, ComponentVector& K, ComponentVector& xy_loc,
                                typename FlashFluidState::Scalar& S_loc, const ComponentVector& globalComposition, bool isGas, int verbosity)
    {
        using FlashEval = typename FlashFluidState::Scalar;
        using PengRobinsonMixture = typename Opm::PengRobinsonMixture<Scalar, FluidSystem>;

        // Declarations 
        FlashFluidState fluidState_fake = fluidState;
        FlashFluidState fluidState_global = fluidState;

        // Setup output
        if (verbosity >= 3 || verbosity >= 4) {
            std::cout << std::setw(10) << "Iteration" << std::setw(16) << "K-Norm" << std::setw(16) << "R-Norm" << std::endl;
        }

        // Michelsens stability test.
        // Make two fake phases "inside" one phase and check for positive volume
        for (int i = 0; i < 20000; ++i) {
            S_loc = 0.0;
            if (isGas) {
                for (int compIdx=0; compIdx<numComponents; ++compIdx){
                    xy_loc[compIdx] = K[compIdx] * globalComposition[compIdx];
                    S_loc += xy_loc[compIdx];
                }
                for (int compIdx=0; compIdx<numComponents; ++compIdx){
                    xy_loc[compIdx] /= S_loc;
                    fluidState_fake.setMoleFraction(gasPhaseIdx, compIdx, xy_loc[compIdx]);
                }
            }
            else {
                for (int compIdx=0; compIdx<numComponents; ++compIdx){
                    xy_loc[compIdx] = globalComposition[compIdx]/K[compIdx];
                    S_loc += xy_loc[compIdx];
                }
                for (int compIdx=0; compIdx<numComponents; ++compIdx){
                    xy_loc[compIdx] /= S_loc;
                    fluidState_fake.setMoleFraction(oilPhaseIdx, compIdx, xy_loc[compIdx]);
                }
            }

            int phaseIdx = (isGas ? static_cast<int>(gasPhaseIdx) : static_cast<int>(oilPhaseIdx));
            int phaseIdx2 = (isGas ? static_cast<int>(oilPhaseIdx) : static_cast<int>(gasPhaseIdx));
            // TODO: not sure the following makes sense
            for (int compIdx=0; compIdx<numComponents; ++compIdx){
                fluidState_global.setMoleFraction(phaseIdx2, compIdx, globalComposition[compIdx]);
            }

            typename FluidSystem::template ParameterCache<FlashEval> paramCache_fake;
            paramCache_fake.updatePhase(fluidState_fake, phaseIdx);

            typename FluidSystem::template ParameterCache<FlashEval> paramCache_global;
            paramCache_global.updatePhase(fluidState_global, phaseIdx2);

            //fugacity for fake phases each component
            for (int compIdx=0; compIdx<numComponents; ++compIdx){
                auto phiFake = PengRobinsonMixture::computeFugacityCoefficient(fluidState_fake, paramCache_fake, phaseIdx, compIdx);
                auto phiGlobal = PengRobinsonMixture::computeFugacityCoefficient(fluidState_global, paramCache_global, phaseIdx2, compIdx);

                fluidState_fake.setFugacityCoefficient(phaseIdx, compIdx, phiFake);
                fluidState_global.setFugacityCoefficient(phaseIdx2, compIdx, phiGlobal);
            }

           
            ComponentVector R;
            for (int compIdx=0; compIdx<numComponents; ++compIdx){
                if (isGas){
                    auto fug_fake = fluidState_fake.fugacity(phaseIdx, compIdx);
                    auto fug_global = fluidState_global.fugacity(phaseIdx2, compIdx);
                    auto fug_ratio = fug_global / fug_fake;
                    R[compIdx] = fug_ratio / S_loc;
                }
                else{
                    auto fug_fake = fluidState_fake.fugacity(phaseIdx, compIdx);
                    auto fug_global = fluidState_global.fugacity(phaseIdx2, compIdx);
                    auto fug_ratio = fug_fake / fug_global;
                    R[compIdx] = fug_ratio * S_loc;
                }
            }

            for (int compIdx=0; compIdx<numComponents; ++compIdx){
                K[compIdx] *= R[compIdx];
            }
            Scalar R_norm = 0.0;
            Scalar K_norm = 0.0;
            for (int compIdx=0; compIdx<numComponents; ++compIdx){
                auto a = Opm::getValue(R[compIdx]) - 1.0;
                auto b = Opm::log(Opm::getValue(K[compIdx]));
                R_norm += a*a;
                K_norm += b*b;
            }

            // Print iteration info
            if (verbosity >= 3 || verbosity >= 4) {
                std::cout << std::setw(10) << i << std::setw(16) << K_norm << std::setw(16) << R_norm << std::endl;
            }

            // Check convergence
            isTrivial = (K_norm < 1e-5);
            if (isTrivial || R_norm < 1e-10)
                return;
            //todo: make sure that no mole fraction is smaller than 1e-8 ?
            //todo: take care of water!
        }
        throw std::runtime_error(" Stability test did not converge");
    }//end checkStability

    // TODO: basically FlashFluidState and ComponentVector are both depending on the one Scalar type
    template <class FlashFluidState, class ComponentVector>
    static void computeLiquidVapor_(FlashFluidState& fluidState, typename FlashFluidState::Scalar& L, ComponentVector& K, const ComponentVector& globalComposition)
    {
        // Calculate x and y, and normalize
        ComponentVector x;
        ComponentVector y;
        typename FlashFluidState::Scalar sumx=0;
        typename FlashFluidState::Scalar sumy=0;
        for (int compIdx=0; compIdx<numComponents; ++compIdx){
            x[compIdx] = globalComposition[compIdx]/(L + (1-L)*K[compIdx]);
            sumx += x[compIdx];
            y[compIdx] = (K[compIdx]*globalComposition[compIdx])/(L + (1-L)*K[compIdx]);
            sumy += y[compIdx];
        }
        x /= sumx;
        y /= sumy;

        for (int compIdx=0; compIdx<numComponents; ++compIdx){
            fluidState.setMoleFraction(oilPhaseIdx, compIdx, x[compIdx]);
            fluidState.setMoleFraction(gasPhaseIdx, compIdx, y[compIdx]);
            //SET k ??

        }
    }

    // TODO: refactoring the template parameter later
    // For the function flash_2ph, we should carry the derivatives in, and
    // return with the correct and updated derivatives
    template <class FluidState, class ComponentVector>
    static void flash_2ph(const ComponentVector& z_scalar,
                          const std::string& flash_2p_method,
                          ComponentVector& K_scalar,
                          typename FluidState::Scalar& L_scalar,
                          FluidState& fluid_state_scalar,
                          int verbosity = 0) {
        if (verbosity >= 1) {
            std::cout << "Cell is two-phase! Solve Rachford-Rice with initial K = [" << K_scalar << "]" << std::endl;
        }

        // Calculate composition using nonlinear solver
        // Newton
        if (flash_2p_method == "newton") {
            if (verbosity >= 1) {
                std::cout << "Calculate composition using Newton." << std::endl;
            }
            newtonCompositionUpdate_(K_scalar, L_scalar, fluid_state_scalar, z_scalar, verbosity);
        } else if (flash_2p_method == "ssi") {
            // Successive substitution
            if (verbosity >= 1) {
                std::cout << "Calculate composition using Succcessive Substitution." << std::endl;
            }
            successiveSubstitutionComposition_(K_scalar, L_scalar, fluid_state_scalar, z_scalar, false, verbosity);
        } else if (flash_2p_method == "ssi+newton") {
            successiveSubstitutionComposition_(K_scalar, L_scalar, fluid_state_scalar, z_scalar, true, verbosity);
            newtonCompositionUpdate_(K_scalar, L_scalar, fluid_state_scalar, z_scalar, verbosity);
        } else {
            throw std::runtime_error("unknown two phase flash method " + flash_2p_method + " is specified");
        }
    }

    template <class FlashFluidState, class ComponentVector>
    static void newtonCompositionUpdate_(ComponentVector& K, typename FlashFluidState::Scalar& L,
                                         FlashFluidState& fluidState, const ComponentVector& globalComposition,
                                         int verbosity)
    {
        constexpr size_t num_equations = numMisciblePhases * numMiscibleComponents + 1;
        using NewtonVector = Dune::FieldVector<Scalar, num_equations>;
        using NewtonMatrix = Dune::FieldMatrix<Scalar, num_equations, num_equations>;
        constexpr Scalar tolerance = 1.e-8;

        NewtonVector soln(0.);
        NewtonVector res(0.);
        NewtonMatrix jac (0.);

        // Compute x and y from K, L and Z
        computeLiquidVapor_(fluidState, L, K, globalComposition);
        std::cout << " the current L is " << Opm::getValue(L) << std::endl;

        // Print initial condition
        if (verbosity >= 1) {
            std::cout << "Initial guess: x = [";
            for (int compIdx=0; compIdx<numComponents; ++compIdx){
                if (compIdx < numComponents - 1)
                    std::cout << fluidState.moleFraction(oilPhaseIdx, compIdx) << " ";
                else
                    std::cout << fluidState.moleFraction(oilPhaseIdx, compIdx);
            }
            std::cout << "], y = [";
            for (int compIdx=0; compIdx<numComponents; ++compIdx){
                if (compIdx < numComponents - 1)
                    std::cout << fluidState.moleFraction(gasPhaseIdx, compIdx) << " ";
                else
                    std::cout << fluidState.moleFraction(gasPhaseIdx, compIdx);
            }
            std::cout << "], and " << "L = " << L << std::endl;
        }

        // Print header
        if (verbosity == 2 || verbosity == 4) {
            std::cout << std::setw(10) << "Iteration" << std::setw(16) << "Norm2(step)" << std::setw(16) << "Norm2(Residual)" << std::endl;
        }

        // AD type
        using Eval = DenseAd::Evaluation<Scalar, num_equations>;
        // TODO: we might need to use numMiscibleComponents here
        std::vector<Eval> x(num_equations), y(num_equations);
        Eval l;

        // TODO: I might not need to set soln anything here.
        for (unsigned compIdx = 0; compIdx < numComponents; ++compIdx) {
            x[compIdx] = Eval(Opm::getValue(fluidState.moleFraction(oilPhaseIdx, compIdx)), compIdx);
            const unsigned idx = compIdx + numComponents;
            y[compIdx] = Eval(Opm::getValue(fluidState.moleFraction(gasPhaseIdx, compIdx)), idx);
        }
        l = Eval(Opm::getValue(L), num_equations - 1);

        // it is created for the AD calculation for the flash calculation
        CompositionalFluidState<Eval, FluidSystem> flash_fluid_state;
        for (unsigned compIdx = 0; compIdx < numComponents; ++compIdx) {
            flash_fluid_state.setMoleFraction(FluidSystem::oilPhaseIdx, compIdx, x[compIdx]);
            flash_fluid_state.setMoleFraction(FluidSystem::gasPhaseIdx, compIdx, y[compIdx]);
            // TODO: should we use wilsonK_ here?
            flash_fluid_state.setKvalue(compIdx, y[compIdx] / x[compIdx]);
        }
        flash_fluid_state.setLvalue(l);
        // other values need to be Scalar, but I guess the fluidstate does not support it yet.
        flash_fluid_state.setPressure(FluidSystem::oilPhaseIdx,
                                      Opm::getValue(fluidState.pressure(FluidSystem::oilPhaseIdx)));
        flash_fluid_state.setPressure(FluidSystem::gasPhaseIdx,
                                      Opm::getValue(fluidState.pressure(FluidSystem::gasPhaseIdx)));

        // TODO: not sure whether we need to set the saturations
        flash_fluid_state.setSaturation(FluidSystem::gasPhaseIdx,
                                        Opm::getValue(fluidState.saturation(FluidSystem::gasPhaseIdx)));
        flash_fluid_state.setSaturation(FluidSystem::oilPhaseIdx,
                                        Opm::getValue(fluidState.saturation(FluidSystem::oilPhaseIdx)));
        flash_fluid_state.setTemperature(Opm::getValue(fluidState.temperature(0)));

        using ParamCache = typename FluidSystem::template ParameterCache<typename CompositionalFluidState<Eval, FluidSystem>::Scalar>;
        ParamCache paramCache;

        for (unsigned phaseIdx = 0; phaseIdx < numPhases; ++phaseIdx) {
            paramCache.updatePhase(flash_fluid_state, phaseIdx);
            for (unsigned compIdx = 0; compIdx < numComponents; ++compIdx) {
                Eval phi = FluidSystem::fugacityCoefficient(flash_fluid_state, paramCache, phaseIdx, compIdx);
                flash_fluid_state.setFugacityCoefficient(phaseIdx, compIdx, phi);
            }
        }
        bool converged = false;
        unsigned iter = 0;
        constexpr unsigned max_iter = 1000;
        while (!converged && iter < max_iter) {

            // assembling the Jacobian and residuals
            for (unsigned compIdx = 0; compIdx < numComponents; ++compIdx) {
                {
                    // z - L*x - (1-L) * y
                    auto local_res = -Opm::getValue(globalComposition[compIdx]) + l * x[compIdx] + (1 - l) * y[compIdx];
                    res[compIdx] = Opm::getValue(local_res);
                    for (unsigned i = 0; i < num_equations; ++i) {
                        jac[compIdx][i] = local_res.derivative(i);
                    }
                }

                {
                    // (f_liquid/f_vapor) - 1 = 0
                    auto local_res = (flash_fluid_state.fugacity(oilPhaseIdx, compIdx) /
                                      flash_fluid_state.fugacity(gasPhaseIdx, compIdx)) - 1.0;
                    res[compIdx + numComponents] = Opm::getValue(local_res);
                    for (unsigned i = 0; i < num_equations; ++i) {
                        jac[compIdx + numComponents][i] = local_res.derivative(i);
                    }
                }
            }
            // sum(x) - sum(y) = 0
            Eval sumx = 0.;
            Eval sumy = 0.;
            for (unsigned compIdx = 0; compIdx < numComponents; ++compIdx) {
                sumx += x[compIdx];
                sumy += y[compIdx];
            }
            auto local_res = sumx - sumy;
            res[num_equations - 1] = Opm::getValue(local_res);
            for (unsigned i = 0; i < num_equations; ++i) {
                jac[num_equations - 1][i] = local_res.derivative(i);
            }

            std::cout << " newton residual is " << res.two_norm() << std::endl;
            converged = res.two_norm() < tolerance;
            if (converged) {
                break;
            }

            jac.solve(soln, res);
            constexpr Scalar damping_factor = 1.0;
            // updating x and y
            for (unsigned compIdx = 0; compIdx < numComponents; ++compIdx) {
                x[compIdx] -= soln[compIdx] * damping_factor;
                y[compIdx] -= soln[compIdx + numComponents] * damping_factor;
            }
            l -= soln[num_equations - 1] * damping_factor;

            for (unsigned compIdx = 0; compIdx < numComponents; ++compIdx) {
                flash_fluid_state.setMoleFraction(FluidSystem::oilPhaseIdx, compIdx, x[compIdx]);
                flash_fluid_state.setMoleFraction(FluidSystem::gasPhaseIdx, compIdx, y[compIdx]);
                // TODO: should we use wilsonK_ here?
                flash_fluid_state.setKvalue(compIdx, y[compIdx] / x[compIdx]);
            }
            flash_fluid_state.setLvalue(l);

            for (unsigned phaseIdx = 0; phaseIdx < numPhases; ++phaseIdx) {
                paramCache.updatePhase(flash_fluid_state, phaseIdx);
                for (unsigned compIdx = 0; compIdx < numComponents; ++compIdx) {
                    Eval phi = FluidSystem::fugacityCoefficient(flash_fluid_state, paramCache, phaseIdx, compIdx);
                    flash_fluid_state.setFugacityCoefficient(phaseIdx, compIdx, phi);
                }
            }
        }
        if (!converged) {
            throw std::runtime_error(" Newton composition update did not converge within maxIterations");
        }
        // TODO: we need to update the K, L, fluidState, not sure about the derivatives
    }

    template <class DefectVector>
    static void updateCurrentSol_(DefectVector& x, DefectVector& d)
    {
        // Find smallest percentage update
        Scalar w = 1.0;
        for (size_t i=0; i<x.size(); ++i){
            Scalar w_tmp = Opm::getValue(Opm::min(Opm::max(x[i] + d[i], 0.0), 1.0) - x[i]) / Opm::getValue(d[i]);
            w = Opm::min(w, w_tmp);
        }

        // Loop over the solution vector and apply the smallest percentage update
        for (size_t i=0; i<x.size(); ++i){
            x[i] += w*d[i];
        }
    }

    template <class DefectVector>
    static bool checkFugacityEquil_(DefectVector& b)
    {
        // Init. fugacity vector
        DefectVector fugVec;

        // Loop over b and find the fugacity equilibrium
        // OBS: If the equations in b changes in evalDefect_ then you have to change here as well!
        for (int compIdx=0; compIdx<numComponents; ++compIdx){
            fugVec[compIdx] = 0.0;
            fugVec[compIdx+numMiscibleComponents] = b[compIdx+numMiscibleComponents];
        }
        fugVec[numMiscibleComponents*numMisciblePhases] = 0.0;
        
        // Check if norm(fugVec) is less than tolerance
        bool conv = fugVec.two_norm() < 1e-6;
        return conv;
    }

    /* template <class Vector, class Matrix, class Eval, class ComponentVector>
    static void evalJacobian(const ComponentVector& globalComposition,
                             const Vector& x,
                             const Vector& y,
                             const Eval& l,
                             Vector& b,
                             Matrix& m)
    {
        // TODO: all the things are going through the FluidState, which makes it difficult to get the AD correct.
        FluidState fluidState(fluidStateIn);
        ComponentVector K;
        for (int compIdx=0; compIdx<numComponents; ++compIdx){
            fluidState.setMoleFraction(oilPhaseIdx, compIdx, x[compIdx]);
            fluidState.setMoleFraction(gasPhaseIdx, compIdx, x[compIdx + numMiscibleComponents]);
        }


        // Compute fugacities
        using ValueType = typename FluidState::Scalar;
        using ParamCache = typename FluidSystem::template ParameterCache<typename FluidState::Scalar>;
        ParamCache paramCache;
        for (int phaseIdx=0; phaseIdx<numPhases; ++phaseIdx){
            paramCache.updatePhase(fluidState, phaseIdx);
            for (int compIdx=0; compIdx<numComponents; ++compIdx){
                ValueType phi = FluidSystem::fugacityCoefficient(fluidState, paramCache, phaseIdx, compIdx);
                fluidState.setFugacityCoefficient(phaseIdx, compIdx, phi);
            }
        }

        // Compute residuals for Newton update. Primary variables are: x, y, and L
        // TODO: Make this AD
        // Extract L
        ValueType L = x[numMiscibleComponents*numMisciblePhases];

        // Residuals
        // OBS: the residuals are negative in the newton system!
        for (int compIdx=0; compIdx<numComponents; ++compIdx){
            // z - L*x - (1-L) * y
            b[compIdx] = -globalComposition[compIdx] + L*x[compIdx] + (1-L)*x[compIdx + numMiscibleComponents];

            // (f_liquid/f_vapor) - 1 = 0
            b[compIdx + numMiscibleComponents] = -(fluidState.fugacity(oilPhaseIdx, compIdx) / fluidState.fugacity(gasPhaseIdx, compIdx)) + 1.0;

            // sum(x) - sum(y) = 0
            b[numMiscibleComponents*numMisciblePhases] += -x[compIdx] + x[compIdx + numMiscibleComponents];
        }
    }//end valDefect */



    template <class FluidState, class DefectVector, class ComponentVector>
    static void evalDefect_(DefectVector& b,
                            DefectVector& x,
                            const FluidState& fluidStateIn,
                            const ComponentVector& globalComposition)
    {
        // Put x and y in a FluidState instance for fugacity calculations
        FluidState fluidState(fluidStateIn);
        ComponentVector K;
        for (int compIdx=0; compIdx<numComponents; ++compIdx){
            fluidState.setMoleFraction(oilPhaseIdx, compIdx, x[compIdx]);
            fluidState.setMoleFraction(gasPhaseIdx, compIdx, x[compIdx + numMiscibleComponents]);
        }
        

        // Compute fugacities
        using ValueType = typename FluidState::Scalar;
        using ParamCache = typename FluidSystem::template ParameterCache<typename FluidState::Scalar>;
        ParamCache paramCache;
        for (int phaseIdx=0; phaseIdx<numPhases; ++phaseIdx){
            paramCache.updatePhase(fluidState, phaseIdx);
            for (int compIdx=0; compIdx<numComponents; ++compIdx){
                ValueType phi = FluidSystem::fugacityCoefficient(fluidState, paramCache, phaseIdx, compIdx);
                fluidState.setFugacityCoefficient(phaseIdx, compIdx, phi);
            }
        }

        // Compute residuals for Newton update. Primary variables are: x, y, and L
        // TODO: Make this AD
        // Extract L
        ValueType L = x[numMiscibleComponents*numMisciblePhases];
        
        // Residuals
        // OBS: the residuals are negative in the newton system!
        for (int compIdx=0; compIdx<numComponents; ++compIdx){
            // z - L*x - (1-L) * y
            b[compIdx] = -globalComposition[compIdx] + L*x[compIdx] + (1-L)*x[compIdx + numMiscibleComponents];
            
            // (f_liquid/f_vapor) - 1 = 0
            b[compIdx + numMiscibleComponents] = -(fluidState.fugacity(oilPhaseIdx, compIdx) / fluidState.fugacity(gasPhaseIdx, compIdx)) + 1.0;
        
            // sum(x) - sum(y) = 0
            b[numMiscibleComponents*numMisciblePhases] += -x[compIdx] + x[compIdx + numMiscibleComponents];
        }
    }//end valDefect

    template <class FluidState, class DefectVector, class DefectMatrix, class ComponentVector>
    static void evalJacobian_(DefectMatrix& A,
                              const DefectVector& xIn,
                              const FluidState& fluidStateIn,
                              const ComponentVector& globalComposition)
    {
        // TODO: Use AD instead
        // Calculate response of current state x
        DefectVector x;
        DefectVector b0;
        for(size_t j=0; j<xIn.size(); ++j){
            x[j] = xIn[j];
        }
        
        evalDefect_(b0, x, fluidStateIn, globalComposition);

        // Make the jacobian A in Newton system Ax=b
        Scalar epsilon = 1e-10;
        for(size_t i=0; i<b0.size(); ++i){
            // Permutate x and calculate response
            x[i] += epsilon;
            DefectVector bEps;
            evalDefect_(bEps, x, fluidStateIn, globalComposition);
            x[i] -= epsilon;

            // Forward difference of all eqs wrt primary variable i
            DefectVector derivI;
            for(size_t j=0; j<b0.size(); ++j){
                derivI[j] = bEps[j];
                derivI[j] -= b0[j];
                derivI[j] /= epsilon;
                A[j][i] = derivI[j];
            }
        }
    }//end evalJacobian

    // TODO: or use typename FlashFluidState::Scalar
    template <class FlashFluidState, class ComponentVector>
    static void successiveSubstitutionComposition_(ComponentVector& K, typename ComponentVector::field_type& L, FlashFluidState& fluidState, const ComponentVector& globalComposition,
                                                   const bool newton_afterwards, const int verbosity)
    {
        // std::cout << " Evaluation in successiveSubstitutionComposition_  is " << Dune::className(L) << std::endl;
        // Determine max. iterations based on if it will be used as a standalone flash or as a pre-process to Newton (or other) method.
        const int maxIterations = newton_afterwards ? 3 : 10;

        // Store cout format before manipulation
        std::ios_base::fmtflags f(std::cout.flags());
        
        // Print initial guess
        if (verbosity >= 1)
            std::cout << "Initial guess: K = [" << K << "] and L = " << L << std::endl;

        if (verbosity == 2 || verbosity == 4) {
            // Print header
            int fugWidth = (numComponents * 12)/2;
            int convWidth = fugWidth + 7;
            std::cout << std::setw(10) << "Iteration" << std::setw(fugWidth) << "fL/fV" << std::setw(convWidth) << "norm2(fL/fv-1)" << std::endl;
        }
        // 
        // Successive substitution loop
        // 
        for (int i=0; i < maxIterations; ++i){
            // Compute (normalized) liquid and vapor mole fractions
            computeLiquidVapor_(fluidState, L, K, globalComposition);

            // Calculate fugacity coefficient
            using ParamCache = typename FluidSystem::template ParameterCache<typename FlashFluidState::Scalar>;
            ParamCache paramCache;
            for (int phaseIdx=0; phaseIdx<numPhases; ++phaseIdx){
                paramCache.updatePhase(fluidState, phaseIdx);
                for (int compIdx=0; compIdx<numComponents; ++compIdx){
                    auto phi = FluidSystem::fugacityCoefficient(fluidState, paramCache, phaseIdx, compIdx);
                    fluidState.setFugacityCoefficient(phaseIdx, compIdx, phi);
                }
            }
            
            // Calculate fugacity ratio
            ComponentVector newFugRatio;
            ComponentVector convFugRatio;
            for (int compIdx=0; compIdx<numComponents; ++compIdx){
                newFugRatio[compIdx] = fluidState.fugacity(oilPhaseIdx, compIdx)/fluidState.fugacity(gasPhaseIdx, compIdx);
                convFugRatio[compIdx] = newFugRatio[compIdx] - 1.0;
            }

            // Print iteration info
            if (verbosity == 2 || verbosity == 4) {
                int prec = 5;
                int fugWidth = (prec + 3);
                int convWidth = prec + 9;
                std::cout << std::defaultfloat;
                std::cout << std::fixed;
                std::cout << std::setw(5) << i;
                std::cout << std::setw(fugWidth);
                std::cout << std::setprecision(prec);
                std::cout << newFugRatio;
                std::cout << std::scientific;
                std::cout << std::setw(convWidth) << convFugRatio.two_norm() << std::endl;
            }

            // Check convergence
            if (convFugRatio.two_norm() < 1e-6){
                // Restore cout format
                std::cout.flags(f); 

                // Print info
                if (verbosity >= 1) {
                    std::cout << "Solution converged to the following result :" << std::endl;
                    std::cout << "x = [";
                    for (int compIdx=0; compIdx<numComponents; ++compIdx){
                        if (compIdx < numComponents - 1)
                            std::cout << fluidState.moleFraction(oilPhaseIdx, compIdx) << " ";
                        else
                            std::cout << fluidState.moleFraction(oilPhaseIdx, compIdx);
                    }
                    std::cout << "]" << std::endl;
                    std::cout << "y = [";
                    for (int compIdx=0; compIdx<numComponents; ++compIdx){
                        if (compIdx < numComponents - 1)
                            std::cout << fluidState.moleFraction(gasPhaseIdx, compIdx) << " ";
                        else
                            std::cout << fluidState.moleFraction(gasPhaseIdx, compIdx);
                    }
                    std::cout << "]" << std::endl;
                    std::cout << "K = [" << K << "]" << std::endl;
                    std::cout << "L = " << L << std::endl;
                }
                // Restore cout format format
                return;
            }
            
            //  If convergence is not met, K is updated in a successive substitution manner
            else{
                // Update K
                for (int compIdx=0; compIdx<numComponents; ++compIdx){
                    K[compIdx] *= newFugRatio[compIdx];
                }

                // Solve Rachford-Rice to get L from updated K
                L = solveRachfordRice_g_(K, globalComposition, 0);
            }

        }
    //     throw std::runtime_error("Successive substitution composition update did not converge within maxIterations");
    }
    
};//end ChiFlash

} // namespace Opm

#endif
