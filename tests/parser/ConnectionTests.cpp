/*
  Copyright 2013 Statoil ASA.

  This file is part of the Open Porous Media project (OPM).

  OPM is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  OPM is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the

  GNU General Public License for more details.
  You should have received a copy of the GNU General Public License
  along with OPM.  If not, see <http://www.gnu.org/licenses/>.
 */

#define BOOST_TEST_MODULE CompletionTests
#include <boost/test/unit_test.hpp>

#include <opm/input/eclipse/Schedule/Well/Connection.hpp>

#include <opm/common/utility/ActiveGridCells.hpp>
#include <opm/common/utility/OpmInputError.hpp>

#include <opm/input/eclipse/Python/Python.hpp>

#include <opm/input/eclipse/EclipseState/EclipseState.hpp>
#include <opm/input/eclipse/EclipseState/Grid/EclipseGrid.hpp>
#include <opm/input/eclipse/EclipseState/Grid/FieldPropsManager.hpp>
#include <opm/input/eclipse/EclipseState/Tables/TableManager.hpp>

#include <opm/input/eclipse/Schedule/CompletedCells.hpp>
#include <opm/input/eclipse/Schedule/Schedule.hpp>
#include <opm/input/eclipse/Schedule/ScheduleGrid.hpp>
#include <opm/input/eclipse/Schedule/Well/Well.hpp>
#include <opm/input/eclipse/Schedule/Well/WDFAC.hpp>
#include <opm/input/eclipse/Schedule/Well/WellConnections.hpp>

#include <opm/common/OpmLog/KeywordLocation.hpp>

#include <opm/input/eclipse/Units/Units.hpp>
#include <opm/input/eclipse/Units/UnitSystem.hpp>

#include <opm/input/eclipse/Deck/Deck.hpp>

#include <opm/input/eclipse/Parser/ErrorGuard.hpp>
#include <opm/input/eclipse/Parser/InputErrorAction.hpp>
#include <opm/input/eclipse/Parser/ParseContext.hpp>

#include <opm/input/eclipse/Parser/Parser.hpp>

#include <opm/input/eclipse/Schedule/WellTraj/RigEclipseWellLogExtractorGrid.hpp>
#include <external/resinsight/ReservoirDataModel/RigWellPath.h>
#include <external/resinsight/ReservoirDataModel/cvfGeometryTools.h>

#include <array>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <ostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
    double cp_rm3_per_db()
    {
        return Opm::prefix::centi*Opm::unit::Poise * Opm::unit::cubic(Opm::unit::meter)
            / (Opm::unit::day * Opm::unit::barsa);
    }

    Opm::WellConnections
    loadCOMPDAT(const std::string& compdat_keyword)
    {
        Opm::WellConnections connections {
            Opm::Connection::Order::TRACK, 10, 10
        };

        const auto deck = Opm::Parser{}.parseString(compdat_keyword);
        const auto wdfac = Opm::WDFAC{};
        const auto loc = Opm::KeywordLocation{};

        Opm::EclipseGrid grid { 10, 10, 10 };
        const Opm::FieldPropsManager field_props {
            deck, Opm::Phases{true, true, true}, grid, Opm::TableManager{}
        };

        const auto ctx = Opm::ParseContext{};
        auto errors = Opm::ErrorGuard{};

        // Must be mutable.
        Opm::CompletedCells completed_cells(grid);
        const auto sg = Opm::ScheduleGrid { grid, field_props, completed_cells };

        std::vector<int> requested_open_complnums;
        std::vector<int> requested_shut_complnums;
        for (const auto& rec : deck["COMPDAT"][0]) {
            connections.loadCOMPDAT(rec, "WELL", wdfac, sg, loc, ctx, errors,
                                    requested_open_complnums,
                                    requested_shut_complnums);
        }

        return connections;
    }
}

namespace Opm {

inline std::ostream& operator<<( std::ostream& stream, const Connection& c ) {
    return stream << "(" << c.getI() << "," << c.getJ() << "," << c.getK() << ")";
}

inline std::ostream& operator<<( std::ostream& stream, const WellConnections& cs ) {
    stream << "{ ";
    for( const auto& c : cs ) stream << c << " ";
    return stream << "}";
}

}





BOOST_AUTO_TEST_CASE(CreateWellConnectionsOK) {
    Opm::WellConnections completionSet(Opm::Connection::Order::TRACK, 1,1);
    BOOST_CHECK_MESSAGE( completionSet.empty(), "Default-constructed completion set must be empty" );
    BOOST_CHECK_EQUAL( 0U , completionSet.size() );
    BOOST_CHECK(!completionSet.allConnectionsShut());
}



BOOST_AUTO_TEST_CASE(AddCompletionSizeCorrect)
{
    const auto dir = Opm::Connection::Direction::Z;
    const auto kind = Opm::Connection::CTFKind::DeckValue;
    const auto depth = 0.0;

    auto ctf_props = Opm::Connection::CTFProperties{};

    ctf_props.CF = 99.88;
    ctf_props.Kh = 355.113;
    ctf_props.rw = 0.25;

    const auto completion1 = Opm::Connection { 10,10,10, 100, 1, Opm::Connection::State::OPEN, dir, kind, 0, depth, ctf_props, 0, true };
    const auto completion2 = Opm::Connection { 10,10,11, 102, 1, Opm::Connection::State::SHUT, dir, kind, 0, depth, ctf_props, 0, true };

    Opm::WellConnections completionSet(Opm::Connection::Order::TRACK, 1,1);
    completionSet.add( completion1 );
    BOOST_CHECK_EQUAL( 1U , completionSet.size() );
    BOOST_CHECK_MESSAGE( !completionSet.empty(), "Non-empty completion set must not be empty" );

    completionSet.add( completion2 );
    BOOST_CHECK_EQUAL( 2U , completionSet.size() );

    BOOST_CHECK_EQUAL( completion1 , completionSet.get(0) );
}


BOOST_AUTO_TEST_CASE(WellConnectionsGetOutOfRangeThrows)
{
    const auto dir = Opm::Connection::Direction::Z;
    const auto kind = Opm::Connection::CTFKind::DeckValue;
    const auto depth = 0.0;

    auto ctf_props = Opm::Connection::CTFProperties{};

    ctf_props.CF = 99.88;
    ctf_props.Kh = 355.113;
    ctf_props.rw = 0.25;

    const auto completion1 = Opm::Connection { 10,10,10, 100, 1, Opm::Connection::State::OPEN, dir, kind, 0, depth, ctf_props, 0, true };
    const auto completion2 = Opm::Connection { 10,10,11, 102, 1, Opm::Connection::State::SHUT, dir, kind, 0, depth, ctf_props, 0, true };

    Opm::WellConnections completionSet(Opm::Connection::Order::TRACK, 1,1);
    completionSet.add( completion1 );
    BOOST_CHECK_EQUAL( 1U , completionSet.size() );

    completionSet.add( completion2 );
    BOOST_CHECK_EQUAL( 2U , completionSet.size() );

    BOOST_CHECK_THROW( completionSet.get(10) , std::out_of_range );
}


BOOST_AUTO_TEST_CASE(Compdat_Direction) {
    BOOST_CHECK_MESSAGE(Opm::Connection::DirectionFromString("X") == Opm::Connection::Direction::X,
                        R"(Direction "X" must be Direction::X)");
    BOOST_CHECK_MESSAGE(Opm::Connection::DirectionFromString("x") == Opm::Connection::Direction::X,
                        R"(Direction "x" must be Direction::X)");
    BOOST_CHECK_MESSAGE(Opm::Connection::DirectionFromString("Y") == Opm::Connection::Direction::Y,
                        R"(Direction "Y" must be Direction::Y)");
    BOOST_CHECK_MESSAGE(Opm::Connection::DirectionFromString("y") == Opm::Connection::Direction::Y,
                        R"(Direction "y" must be Direction::Y)");
    BOOST_CHECK_MESSAGE(Opm::Connection::DirectionFromString("Z") == Opm::Connection::Direction::Z,
                        R"(Direction "Z" must be Direction::Z)");
    BOOST_CHECK_MESSAGE(Opm::Connection::DirectionFromString("z") == Opm::Connection::Direction::Z,
                        R"(Direction "z" must be Direction::Z)");

    BOOST_CHECK_THROW(Opm::Connection::DirectionFromString(""), std::invalid_argument);
    BOOST_CHECK_THROW(Opm::Connection::DirectionFromString("XX"), std::invalid_argument);
    BOOST_CHECK_THROW(Opm::Connection::DirectionFromString("X-"), std::invalid_argument);
    BOOST_CHECK_THROW(Opm::Connection::DirectionFromString("HeLlo"), std::invalid_argument);
}


BOOST_AUTO_TEST_CASE(AddCompletionCopy)
{
    const auto dir = Opm::Connection::Direction::Z;
    const auto kind = Opm::Connection::CTFKind::DeckValue;
    const auto depth = 0.0;

    auto ctf_props = Opm::Connection::CTFProperties{};

    ctf_props.CF = 99.88;
    ctf_props.Kh = 355.113;
    ctf_props.rw = 0.25;

    const auto completion1 = Opm::Connection { 10,10,10, 100, 1, Opm::Connection::State::OPEN, dir, kind, 0, depth, ctf_props, 0, true };
    const auto completion2 = Opm::Connection { 10,10,11, 101, 1, Opm::Connection::State::SHUT, dir, kind, 0, depth, ctf_props, 0, true };
    const auto completion3 = Opm::Connection { 10,10,12, 102, 1, Opm::Connection::State::SHUT, dir, kind, 0, depth, ctf_props, 0, true };

    Opm::WellConnections completionSet(Opm::Connection::Order::TRACK, 10,10);
    completionSet.add( completion1 );
    completionSet.add( completion2 );
    completionSet.add( completion3 );
    BOOST_CHECK_EQUAL( 3U , completionSet.size() );

    auto copy = completionSet;
    BOOST_CHECK_EQUAL( 3U , copy.size() );

    BOOST_CHECK_EQUAL( completion1 , copy.get(0));
    BOOST_CHECK_EQUAL( completion2 , copy.get(1));
    BOOST_CHECK_EQUAL( completion3 , copy.get(2));
}


BOOST_AUTO_TEST_CASE(ActiveCompletions)
{
    const auto dir = Opm::Connection::Direction::Z;
    const auto kind = Opm::Connection::CTFKind::DeckValue;
    const auto depth = 0.0;

    auto ctf_props = Opm::Connection::CTFProperties{};

    ctf_props.CF = 99.88;
    ctf_props.Kh = 355.113;
    ctf_props.rw = 0.25;

    Opm::EclipseGrid grid { 10, 20, 20 };

    const auto completion1 = Opm::Connection { 0,0,0, grid.getGlobalIndex(0,0,0), 1, Opm::Connection::State::OPEN, dir, kind, 0, depth, ctf_props, 0, true };
    const auto completion2 = Opm::Connection { 0,0,1, grid.getGlobalIndex(0,0,1), 1, Opm::Connection::State::SHUT, dir, kind, 0, depth, ctf_props, 0, true };
    const auto completion3 = Opm::Connection { 0,0,2, grid.getGlobalIndex(0,0,2), 1, Opm::Connection::State::SHUT, dir, kind, 0, depth, ctf_props, 0, true };

    Opm::WellConnections completions(Opm::Connection::Order::TRACK, 10,10);
    completions.add( completion1 );
    completions.add( completion2 );
    completions.add( completion3 );

    std::vector<int> actnum(grid.getCartesianSize(), 1);
    actnum[0] = 0;
    grid.resetACTNUM(actnum);

    const Opm::WellConnections active_completions(completions, grid);
    BOOST_CHECK_EQUAL( active_completions.size() , 2U);
    BOOST_CHECK_EQUAL( completion2, active_completions.get(0));
    BOOST_CHECK_EQUAL( completion3, active_completions.get(1));
}

BOOST_AUTO_TEST_CASE(loadCOMPDATTEST)
{
    const Opm::UnitSystem units(Opm::UnitSystem::UnitType::UNIT_TYPE_METRIC); // Unit system used in deck FIRST_SIM.DATA.

    {
        const std::string deck = R"(GRID

PERMX
  1000*0.10 /

COPY
  'PERMX' 'PERMZ' /
  'PERMX' 'PERMY' /
/

PORO
  1000*0.3 /

SCHEDULE

COMPDAT
--                                    CF      Diam    Kh      Skin   Df
    'WELL'  1  1   1   1 'OPEN' 1*    1.168   0.311   107.872 1*     1*  'Z'  21.925 /
/)";

        const Opm::WellConnections connections = loadCOMPDAT(deck);
        const auto& conn0 = connections[0];
        BOOST_CHECK_EQUAL(conn0.CF(), units.to_si(Opm::UnitSystem::measure::transmissibility, 1.168));
        BOOST_CHECK_EQUAL(conn0.Kh(), units.to_si(Opm::UnitSystem::measure::effective_Kh, 107.872));
        BOOST_CHECK_MESSAGE(conn0.ctfAssignedFromInput(), "CTF Must be Assigned From Input");
    }

    {
        const std::string deck = R"(GRID

PERMX
  1000*0.10 /

COPY
  'PERMX' 'PERMZ' /
  'PERMX' 'PERMY' /
/

PORO
  1000*0.3 /

SCHEDULE

COMPDAT
--                                CF      Diam    Kh      Skin   Df
'WELL'  1  1   1   1 'OPEN' 1*    1.168   0.311   0       1*     1*  'Z'  21.925 /
/)";

        const Opm::WellConnections connections = loadCOMPDAT(deck);
        const auto& conn0 = connections[0];
        BOOST_CHECK_EQUAL(conn0.CF(), units.to_si(Opm::UnitSystem::measure::transmissibility, 1.168));
        BOOST_CHECK_EQUAL(conn0.Kh(), units.to_si(Opm::UnitSystem::measure::effective_Kh, 0.10 * 1.0));
    }
}


BOOST_AUTO_TEST_CASE(loadCOMPDATTESTSPE1) {
    Opm::Parser parser;

    const auto deck = parser.parseFile("SPE1CASE1.DATA");
    auto python = std::make_shared<Opm::Python>();
    Opm::EclipseState state(deck);
    Opm::Schedule sched(deck, state, python);
    const auto& units = deck.getActiveUnitSystem();

    const auto& prod = sched.getWell("PROD", 0);
    const auto& connections = prod.getConnections();
    const auto& conn0 = connections[0];
    /* Expected values come from Eclipse simulation. */
    BOOST_CHECK_CLOSE(conn0.CF(), units.to_si(Opm::UnitSystem::measure::transmissibility, 10.609), 2e-2);
    BOOST_CHECK_CLOSE(conn0.Kh(), units.to_si(Opm::UnitSystem::measure::effective_Kh, 10000), 1e-6);
    BOOST_CHECK_MESSAGE(!conn0.ctfAssignedFromInput(), "Calculated CTF must NOT be assigned from input");
}


struct exp_conn {
    std::string well;
    int ci;
    double CF;
    double Kh;
};

BOOST_AUTO_TEST_CASE(loadCOMPDATTESTSPE9) {
    Opm::Parser parser;

    const auto deck = parser.parseFile("SPE9_CP_PACKED.DATA");
    auto python = std::make_shared<Opm::Python>();
    Opm::EclipseState state(deck);
    Opm::Schedule sched(deck, state, python);
    const auto& units = deck.getActiveUnitSystem();
/*
  The list of the expected values come from the PRT file in an ECLIPSE simulation.
*/
    std::vector<exp_conn> expected = {
  {"INJE1"   ,1 ,     0.166,    111.9},
  {"INJE1"   ,2 ,     0.597,    402.6},
  {"INJE1"   ,3 ,     1.866,   1259.2},
  {"INJE1"   ,4 ,    12.442,   8394.2},
  {"INJE1"   ,5 ,     6.974,   4705.3},

  {"PRODU2"  ,1 ,     0.893,    602.8},
  {"PRODU2"  ,2 ,     3.828,   2582.8},
  {"PRODU2"  ,3 ,     0.563,    380.0},

  {"PRODU3"  ,1 ,     1.322,    892.1},
  {"PRODU3"  ,2 ,     3.416,   2304.4},

  {"PRODU4"  ,1 ,     4.137,   2791.2},
  {"PRODU4"  ,2 ,    66.455,  44834.7},

  {"PRODU5"  ,1 ,     0.391,    264.0},
  {"PRODU5"  ,2 ,     7.282,   4912.6},
  {"PRODU5"  ,3 ,     1.374,    927.3},

  {"PRODU6"  ,1 ,     1.463,    987.3},
  {"PRODU6"  ,2 ,     1.891,   1275.8},

  {"PRODU7"  ,1 ,     1.061,    716.1},
  {"PRODU7"  ,2 ,     5.902,   3982.0},
  {"PRODU7"  ,3 ,     0.593,    400.1},

  {"PRODU8"  ,1 ,     0.993,    670.1},
  {"PRODU8"  ,2 ,    17.759,  11981.5},

  {"PRODU9"  ,1 ,     0.996,    671.9},
  {"PRODU9"  ,2 ,     2.548,   1719.0},

  {"PRODU10" ,1 ,    11.641,   7853.9},
  {"PRODU10" ,2 ,     7.358,   4964.1},
  {"PRODU10" ,3 ,     0.390,    262.8},

  {"PRODU11" ,2 ,     3.536,   2385.6},

  {"PRODU12" ,1 ,     3.028,   2043.1},
  {"PRODU12" ,2 ,     0.301,    202.7},
  {"PRODU12" ,3 ,     0.279,    188.3},

  {"PRODU13" ,2 ,     5.837,   3938.1},

  {"PRODU14" ,1 ,   180.976, 122098.1},
  {"PRODU14" ,2 ,    25.134,  16957.0},
  {"PRODU14" ,3 ,     0.532,    358.7},

  {"PRODU15" ,1 ,     4.125,   2783.1},
  {"PRODU15" ,2 ,     6.431,   4338.7},

  {"PRODU16" ,2 ,     5.892,   3975.0},

  {"PRODU17" ,1 ,    80.655,  54414.9},
  {"PRODU17" ,2 ,     9.098,   6138.3},

  {"PRODU18" ,1 ,     1.267,    855.1},
  {"PRODU18" ,2 ,    18.556,  12518.9},

  {"PRODU19" ,1 ,    15.589,  10517.2},
  {"PRODU19" ,3 ,     1.273,    859.1},

  {"PRODU20" ,1 ,     3.410,   2300.5},
  {"PRODU20" ,2 ,     0.191,    128.8},
  {"PRODU20" ,3 ,     0.249,    168.1},

  {"PRODU21" ,1 ,     0.596,    402.0},
  {"PRODU21" ,2 ,     0.163,    109.9},

  {"PRODU22" ,1 ,     4.021,   2712.8},
  {"PRODU22" ,2 ,     0.663,    447.1},

  {"PRODU23" ,1 ,     1.542,   1040.2},

  {"PRODU24" ,1 ,    78.939,  53257.0},
  {"PRODU24" ,3 ,    17.517,  11817.8},

  {"PRODU25" ,1 ,     3.038,   2049.5},
  {"PRODU25" ,2 ,     0.926,    624.9},
  {"PRODU25" ,3 ,     0.891,    601.3},

  {"PRODU26" ,1 ,     0.770,    519.6},
  {"PRODU26" ,3 ,     0.176,    118.6}};

   for (const auto& ec : expected) {
     const auto& well = sched.getWell(ec.well, 0);
       const auto& connections = well.getConnections();
       const auto& conn = connections[ec.ci - 1];

       BOOST_CHECK_CLOSE( conn.CF(), units.to_si(Opm::UnitSystem::measure::transmissibility, ec.CF), 2e-1);
       BOOST_CHECK_CLOSE( conn.Kh(), units.to_si(Opm::UnitSystem::measure::effective_Kh, ec.Kh), 1e-1);
       BOOST_CHECK_MESSAGE( !conn.ctfAssignedFromInput(), "Calculated SPE9 CTF values must NOT be assigned from input");
   }
}

BOOST_AUTO_TEST_CASE(ApplyWellPI) {
    const auto deck = Opm::Parser{}.parseString(R"(RUNSPEC
DIMENS
10 10 3 /

START
  5 OCT 2020 /

GRID
DXV
  10*100 /
DYV
  10*100 /
DZV
  3*10 /
DEPTHZ
  121*2000 /

ACTNUM
  100*1
  99*1 0
  100*1
/

PERMX
  300*100 /
PERMY
  300*100 /
PERMZ
  300*100 /
PORO
  300*0.3 /

SCHEDULE
WELSPECS
  'P' 'G' 10 10 2005 'LIQ' /
/

COMPDAT
  'P' 0 0 1 3 OPEN 1 100 /
/

TSTEP
  10
/

END
)");

    const auto es    = Opm::EclipseState{ deck };
    const auto sched = Opm::Schedule{ deck, es };

    const auto expectCF = 100.0*cp_rm3_per_db();

    auto connP = sched.getWell("P", 0).getConnections();
    for (const auto& conn : connP) {
        BOOST_CHECK_CLOSE(conn.CF(), expectCF, 1.0e-10);
    }

    {
        std::vector<bool> scalingApplicable;

        connP.applyWellPIScaling(2.0, scalingApplicable);  // No "prepare" -> no change.
        for (const auto& conn : connP) {
            BOOST_CHECK_CLOSE(conn.CF(), expectCF, 1.0e-10);
        }
    }

    // All CFs scaled by factor 2.
    BOOST_CHECK_MESSAGE( connP.prepareWellPIScaling(), "First call to prepareWellPIScaling must be a state change");
    BOOST_CHECK_MESSAGE(!connP.prepareWellPIScaling(), "Second call to prepareWellPIScaling must NOT be a state change");

    {
        std::vector<bool> scalingApplicable;

        connP.applyWellPIScaling(2.0, scalingApplicable);  // No "prepare" -> no change.
        for (const auto& conn : connP) {
            BOOST_CHECK_CLOSE(conn.CF(), 2.0*expectCF, 1.0e-10);
        }
    }

    // Reset CF -- simulating COMPDAT record (inactive cell)
    auto ctf_props = Opm::Connection::CTFProperties{};
    ctf_props.CF = 50.0*cp_rm3_per_db();
    ctf_props.Kh = 0.123;
    ctf_props.rw = 0.234;
    ctf_props.r0 = 0.157;

    connP.addConnection(8, 9, 0, // 9, 10, 1
                        199,
                        Opm::Connection::State::OPEN,
                        2015.0, ctf_props, 1);

    BOOST_REQUIRE_EQUAL(connP.size(), std::size_t{3});

    BOOST_CHECK_CLOSE(connP[0].CF(),  2.0*expectCF       , 1.0e-10);
    BOOST_CHECK_CLOSE(connP[1].CF(),  2.0*expectCF       , 1.0e-10);
    BOOST_CHECK_CLOSE(connP[2].CF(), 50.0*cp_rm3_per_db(), 1.0e-10);

    // Should not apply to connection whose CF was manually specified
    {
        std::vector<bool> scalingApplicable;
        connP.applyWellPIScaling(2.0, scalingApplicable);

        BOOST_CHECK_CLOSE(connP[0].CF(),  4.0*expectCF       , 1.0e-10);
        BOOST_CHECK_CLOSE(connP[1].CF(),  4.0*expectCF       , 1.0e-10);
        BOOST_CHECK_CLOSE(connP[2].CF(), 50.0*cp_rm3_per_db(), 1.0e-10);
    }

    // Prepare new scaling.  Simulating new WELPI record.
    // New scaling applies to all connections.
    BOOST_CHECK_MESSAGE(connP.prepareWellPIScaling(), "Third call to prepareWellPIScaling must be a state change");

    {
        std::vector<bool> scalingApplicable;
        connP.applyWellPIScaling(2.0, scalingApplicable);

        BOOST_CHECK_CLOSE(connP[0].CF(),   8.0*expectCF       , 1.0e-10);
        BOOST_CHECK_CLOSE(connP[1].CF(),   8.0*expectCF       , 1.0e-10);
        BOOST_CHECK_CLOSE(connP[2].CF(), 100.0*cp_rm3_per_db(), 1.0e-10);
    }

    // Reset CF -- simulating COMPDAT record (active cell)
    connP.addConnection(8, 9, 1, // 9, 10, 2
                        198,
                        Opm::Connection::State::OPEN,
                        2015.0, ctf_props, 1);

    BOOST_REQUIRE_EQUAL(connP.size(), std::size_t{4});

    {
        std::vector<bool> scalingApplicable;
        connP.applyWellPIScaling(2.0, scalingApplicable);

        BOOST_CHECK_CLOSE(connP[0].CF(),  16.0*expectCF       , 1.0e-10);
        BOOST_CHECK_CLOSE(connP[1].CF(),  16.0*expectCF       , 1.0e-10);
        BOOST_CHECK_CLOSE(connP[2].CF(), 200.0*cp_rm3_per_db(), 1.0e-10);
        BOOST_CHECK_CLOSE(connP[3].CF(),  50.0*cp_rm3_per_db(), 1.0e-10);
    }

    {
        std::vector<bool> scalingApplicable;

        connP.applyWellPIScaling(2.0, scalingApplicable);
        BOOST_CHECK_CLOSE(connP[0].CF(), 32.0*expectCF       , 1.0e-10);
        BOOST_CHECK_CLOSE(connP[1].CF(), 32.0*expectCF       , 1.0e-10);
        BOOST_CHECK_CLOSE(connP[2].CF(), 400.0*cp_rm3_per_db(), 1.0e-10);
        BOOST_CHECK_CLOSE(connP[3].CF(), 50.0*cp_rm3_per_db(), 1.0e-10);
    }
}

BOOST_AUTO_TEST_CASE(Completion_From_Global_Connection_Index) {
    const auto deck = Opm::Parser{}.parseString(R"(RUNSPEC
START
7 OCT 2020 /

DIMENS
  10 10 3 /

GRID
DXV
  10*100.0 /
DYV
  10*100.0 /
DZV
  3*10.0 /

DEPTHZ
  121*2000.0 /

PERMX
  300*100.0 /
PERMY
  300*100.0 /
PERMZ
  300*10.0 /
PORO
  300*0.3 /

SCHEDULE
WELSPECS
  'P' 'G' 10 10 2005 'LIQ' /
/
COMPDAT
  'P' 0 0 1 1 OPEN 1 100 /
/

TSTEP
  10
/

COMPDAT
  'P' 0 0 2 2 OPEN 1 50 /
/

TSTEP
  10
/

END
)");

    const auto es    = Opm::EclipseState{ deck };
    const auto sched = Opm::Schedule{ deck, es };

    {
        const auto connP = sched.getWell("P", 0).getConnections();

        const auto complnum_100 =
            getCompletionNumberFromGlobalConnectionIndex(connP, 100 - 1);
        const auto complnum_200 =
            getCompletionNumberFromGlobalConnectionIndex(connP, 200 - 1);

        BOOST_CHECK_MESSAGE(  complnum_100.has_value(), "Completion number must be defined at time 0 for connection in cell (10,10,1)");
        BOOST_CHECK_MESSAGE(! complnum_200.has_value(), "Completion number must NOT be defined at time 0 for connection in cell (10,10,2)");

        BOOST_CHECK_EQUAL(complnum_100.value(), 1);
    }

    {
        const auto connP = sched.getWell("P", 1).getConnections();

        const auto complnum_100 =
            getCompletionNumberFromGlobalConnectionIndex(connP, 100 - 1);
        const auto complnum_200 =
            getCompletionNumberFromGlobalConnectionIndex(connP, 200 - 1);

        BOOST_CHECK_MESSAGE(complnum_100.has_value(), "Completion number must be defined at time 0 for connection in cell (10,10,1)");
        BOOST_CHECK_MESSAGE(complnum_200.has_value(), "Completion number must be defined at time 0 for connection in cell (10,10,2)");

        BOOST_CHECK_EQUAL(complnum_100.value(), 1);
        BOOST_CHECK_EQUAL(complnum_200.value(), 2);
    }
}

BOOST_AUTO_TEST_CASE(testReAndConnectionLength) {
    Opm::Parser parser;

    const auto deck = parser.parseFile("SPE1CASE1.DATA");
    auto python = std::make_shared<Opm::Python>();
    Opm::EclipseState state(deck);
    Opm::Schedule sched(deck, state, python);

    const auto& prod = sched.getWell("PROD", 0);
    const auto& connections = prod.getConnections();
    const auto& conn0 = connections[0];
    BOOST_CHECK_CLOSE(conn0.re(), 171.96498506535622 , 2e-2);
    BOOST_CHECK_CLOSE(conn0.connectionLength(),15.239999999999782, 1e-6);
}

BOOST_AUTO_TEST_CASE(loadCOMPTRAJTESTSPE1) {
    Opm::Parser parser;

    const auto deck = parser.parseFile("SPE1CASE1_WELTRAJ.DATA");
    auto python = std::make_shared<Opm::Python>();
    Opm::EclipseState state(deck);
    Opm::Schedule sched(deck, state, python);
    const auto& units = deck.getActiveUnitSystem();

    const auto& inj = sched.getWell("INJ", 0);
    const auto& connections = inj.getConnections();

    /* Comparison values (CFs and intersected cells) are from ResInsight through importing a deviation file with contents
          WELLNAME: 'INJ1'
          # X   Y    TVDMSL   MDMSL
          500   500  -100.0   0.0
          500   500   8325.0  8325.0
          2500  2500  8425.0  8450.0
       and adjusting the completion data in agreement with the COMPTRAJ data in the input file
     */
    const std::array<double, 4> connection_factor{311.783, 7.79428, 38.9674, 62.3465};
    const std::array<int, 4> global_index{0, 100, 111, 211};
    BOOST_CHECK_EQUAL(connections.size(), 4);
    for (std::size_t i = 0 ; i < connections.size();  ++i ) {
         BOOST_CHECK_CLOSE(connections[i].CF(), units.to_si(Opm::UnitSystem::measure::transmissibility, connection_factor[i]), 2e-2);
         BOOST_CHECK_EQUAL(connections[i].global_index(), global_index[i]);
    }
}

BOOST_AUTO_TEST_CASE(loadCOMPTRAJTESTSPE1_2) {
    Opm::Parser parser;

    const auto deck = parser.parseFile("SPE1CASE1_WELTRAJ_2.DATA");
    auto python = std::make_shared<Opm::Python>();
    Opm::EclipseState state(deck);
    Opm::Schedule sched(deck, state, python);
    const auto& units = deck.getActiveUnitSystem();

    const auto& inj = sched.getWell("INJ", 0);
    const auto& connections = inj.getConnections();

    /* Comparison values (CFs and intersected cells) are from ResInsight through importing a deviation file with contents
          WELLNAME: 'INJ1'
          # X   Y    TVDMSL   MDMSL
          2500   3500  -100.0   0.0
          2500   3500   8325.0  8325.0
          2750   3750   8375.0  8375.0
          3500   4500   8400.0  8400.0
          4500   5500   8425.0  8425.0
          -999
       and adjusting the completion data in agreement with the COMPTRAJ data in the input file
     */
    const std::array<double, 5> connection_factor{78.5921, 11.7884, 77.9007, 311.585, 155.784};
    const std::array<int, 5> global_index{0, 100, 200, 211, 222};
    BOOST_CHECK_EQUAL(connections.size(), 5);
    for (std::size_t i = 0 ; i < connections.size();  ++i ) {
         BOOST_CHECK_CLOSE(connections[i].CF(), units.to_si(Opm::UnitSystem::measure::transmissibility, connection_factor[i]), 2e-2);
         BOOST_CHECK_EQUAL(connections[i].global_index(), global_index[i]);
    }
}

namespace {

// Geometry callbacks for WellConnections::recomputeTrajectoryConnections()
// built from an EclipseGrid, keyed by GLOBAL cell index (all coarse cells).
struct GridReplayGeom {
    std::vector<std::array<std::array<double,3>, 8>> corners;
    const Opm::EclipseGrid* grid{};

    explicit GridReplayGeom(const Opm::EclipseGrid& g)
        : grid(&g)
    {
        const std::size_t n = g.getCartesianSize();
        corners.resize(n);
        for (std::size_t gi = 0; gi < n; ++gi) {
            const auto ijk = g.getIJK(gi);
            for (std::size_t l = 0; l < 8; ++l) {
                corners[gi][l] = g.getCornerPos(static_cast<std::size_t>(ijk[0]),
                                                static_cast<std::size_t>(ijk[1]),
                                                static_cast<std::size_t>(ijk[2]), l);
            }
        }
    }

    // Collapse one cell's geometry to a near-zero-thickness sliver, so the
    // trajectory replay grazes it and the sliver filter drops every candidate.
    void flatten(std::size_t i, std::size_t j, std::size_t k)
    {
        const std::size_t gi = grid->getGlobalIndex(i, j, k);
        double zc = 0.0;
        for (const auto& c : corners[gi]) { zc += c[2]; }
        zc /= 8.0;
        for (std::size_t l = 0; l < 8; ++l) {
            corners[gi][l][2] = zc + (l < 4 ? -5.0e-6 : 5.0e-6);
        }
    }

    std::function<std::optional<Opm::WellConnections::TrajectoryCell>(std::size_t)>
    cellInfo() const
    {
        const auto* g = grid;
        return [g](std::size_t gi)
            -> std::optional<Opm::WellConnections::TrajectoryCell>
        {
            if (gi >= g->getCartesianSize()) { return std::nullopt; }
            const auto ijk = g->getIJK(gi);
            Opm::WellConnections::TrajectoryCell tc;
            tc.ijk = { static_cast<int>(ijk[0]),
                       static_cast<int>(ijk[1]),
                       static_cast<int>(ijk[2]) };
            tc.global_index = gi;
            tc.depth        = g->getCellDepth(gi);
            tc.dimensions   = g->getCellDims(gi);
            tc.perm         = { 100.0 * 9.869233e-16, 100.0 * 9.869233e-16,
                                 10.0 * 9.869233e-16 };  // 100/10 mD in SI
            tc.ntg          = 1.0;
            tc.satnum       = 0;
            tc.lgr_name.clear();
            tc.lgr_grid     = 0;
            return tc;
        };
    }

    std::function<std::array<double,3>(std::size_t)> center() const
    {
        const auto* g = grid;
        return [g](std::size_t gi) { return g->getCellCenter(gi); };
    }
    std::function<std::array<double,3>(std::size_t)> extent() const
    {
        const auto* g = grid;
        return [g](std::size_t gi) { return g->getCellDims(gi); };
    }
};

constexpr const char* STACKED_COMPLETION_DECK = R"(RUNSPEC
START
 1 JAN 2026 /
OIL
WATER
DIMENS
 3 3 3 /
TABDIMS
/
GRID
DXV
 3*100 /
DYV
 3*100 /
DZV
 3*10 /
DEPTHZ
 16*2000 /
EQUALS
 PERMX 100 /
 PERMY 100 /
 PERMZ  10 /
 PORO   0.3 /
/
PROPS
DENSITY
 800 1000 1 /
SOLUTION
SCHEDULE
WELSPECS
 'P' 'G' 2 2 2005.0 OIL /
/
COMPDAT
 'P' 2 2 1 3 OPEN 1 1* 0.3 /
/
TSTEP
 3*10 /
END
)";

} // anonymous namespace

// A synthetic (COMPDAT-derived) trajectory that is replayed against a grid
// with NO refinement must reproduce every original completion -- the replay is
// only a device to discover child cells where a parent was refined, and must
// never silently drop a completion (regression: SPE9 PRODU10 lost the middle
// of its three stacked completions to the intersection extractor whenever
// --adaptive-lgr / a dynamic rebuild triggered the trajectory replay, even
// for a refinement box far away from the well).
BOOST_AUTO_TEST_CASE(RecomputeTrajectory_SyntheticIdentityKeepsAllCompletions)
{
    const auto deck = Opm::Parser{}.parseString(STACKED_COMPLETION_DECK);
    const auto es   = Opm::EclipseState { deck };
    auto sched = Opm::Schedule { deck, es, std::make_shared<Opm::Python>() };
    const auto& eg = es.getInputGrid();

    const auto baseline = sched.getWell("P", 0).getConnections();
    BOOST_REQUIRE_EQUAL(baseline.size(), 3u);
    const auto cf0 = std::array { baseline[0].CF(), baseline[1].CF(), baseline[2].CF() };

    GridReplayGeom geom(eg);

    auto run = [&](bool flattenMiddle) {
        auto wc = baseline;   // fresh copy
        BOOST_REQUIRE(wc.synthesizeTrajectory(geom.center(), geom.extent()));
        BOOST_REQUIRE(wc.hasTrajectory());

        GridReplayGeom g2(eg);
        if (flattenMiddle) {
            g2.flatten(1, 1, 1);   // 0-based middle completion cell (2,2,2)
        }
        const auto lgrNames =
            wc.recomputeTrajectoryConnections(g2.corners, g2.cellInfo());

        BOOST_CHECK(lgrNames.empty());     // nothing refined -> stays GLOBAL
        BOOST_REQUIRE_EQUAL(wc.size(), 3u);

        for (std::size_t c = 0; c < 3; ++c) {
            BOOST_CHECK_EQUAL(wc[c].getI(), 1);
            BOOST_CHECK_EQUAL(wc[c].getJ(), 1);
            BOOST_CHECK_EQUAL(wc[c].getK(), static_cast<int>(c));      // K order preserved
            BOOST_CHECK_EQUAL(wc[c].get_lgr_level(), 0);
            BOOST_CHECK_EQUAL(wc[c].complnum(), baseline[c].complnum()); // no renumbering
            BOOST_CHECK(wc[c].CF() > 0.0);
            // A synthetic well untouched by refinement is restored verbatim:
            // the connection factor is bit-for-bit the original.
            BOOST_CHECK_CLOSE(wc[c].CF(), cf0[c], 1.0e-12);
        }
    };

    run(/*flattenMiddle=*/false);   // identity replay
    run(/*flattenMiddle=*/true);    // middle cell grazed -> fallback restores it
}

BOOST_AUTO_TEST_CASE(Compdat_Zero_Perm_Dflt_Action)
{
    const auto deck = Opm::Parser{}.parseString(R"(RUNSPEC
START
  18 MAR 2026 /
OIL
WATER
DIMENS
3 1 3 /
TABDIMS
/
EQLDIMS
/
WELLDIMS
 1 3 1 1 /
GRID
DXV
 3*100 /
DYV
 100 /
DZV
 3*10 /
DEPTHZ
 8*2000 /
EQUALS
 PERMX 100 /
 PERMY 100 /
 PERMZ  10 /
 PORO    0.3 /
/
-- Kx(2,1,2) = Ky(2,1,2) = 0.
EQUALS
 PERMX 0  2 2  1 1  2 2 /
 PERMY 0 /
/
PROPS
DENSITY
  800 1000 1 /
SOLUTION
EQUIL
2010 200 2010 1.23 1995 0.0 1* 1* -5 /
SCHEDULE
WELSPECS
  'P' 'G' 2 2 2005.0 LIQ /
/
COMPDAT
  'P'  2  1  1  3  OPEN  1  1*  0.3048  /
/
TSTEP
  5*10 /
END
)");

    const auto ctx = Opm::ParseContext{};
    auto errors = Opm::ErrorGuard{};

    const auto es = Opm::EclipseState { deck };
    const auto sched = Opm::Schedule {
        deck, es, ctx, errors,
        std::make_shared<Opm::Python>()
    };

    const auto& well_p = sched.back().wells("P");

    BOOST_REQUIRE_MESSAGE(well_p.hasConnections(),
                          R"(Well "P" must have connections at end of simulation)");

    BOOST_REQUIRE_EQUAL(well_p.getConnections().size(), std::size_t{2});

    {
        const auto& c0 = well_p.getConnections()[0];

        BOOST_CHECK_EQUAL(c0.getI(), 1);
        BOOST_CHECK_EQUAL(c0.getJ(), 0);
        BOOST_CHECK_EQUAL(c0.getK(), 0);
    }

    {
        const auto& c1 = well_p.getConnections()[1];

        BOOST_CHECK_EQUAL(c1.getI(), 1);
        BOOST_CHECK_EQUAL(c1.getJ(), 0);
        BOOST_CHECK_EQUAL(c1.getK(), 2);
    }
}

BOOST_AUTO_TEST_CASE(Compdat_Zero_Perm_Throw)
{
    const auto deck = Opm::Parser{}.parseString(R"(RUNSPEC
START
  18 MAR 2026 /
OIL
WATER
DIMENS
3 1 3 /
TABDIMS
/
EQLDIMS
/
WELLDIMS
 1 3 1 1 /
GRID
DXV
 3*100 /
DYV
 100 /
DZV
 3*10 /
DEPTHZ
 8*2000 /
EQUALS
 PERMX 100 /
 PERMY 100 /
 PERMZ  10 /
 PORO    0.3 /
/
-- Kx(2,1,2) = Ky(2,1,2) = 0.
EQUALS
 PERMX 0  2 2  1 1  2 2 /
 PERMY 0 /
/
PROPS
DENSITY
  800 1000 1 /
SOLUTION
EQUIL
2010 200 2010 1.23 1995 0.0 1* 1* -5 /
SCHEDULE
WELSPECS
  'P' 'G' 2 2 2005.0 LIQ /
/
COMPDAT
  'P'  2  1  1  3  OPEN  1  1*  0.3048  /
/
TSTEP
  5*10 /
END
)");

    const auto ctx = Opm::ParseContext {
        std::vector {
            std::pair { Opm::ParseContext::SCHEDULE_COMPDAT_ZERO_PERM,
                        Opm::InputErrorAction::THROW_EXCEPTION },
        }
    };

    auto errors = Opm::ErrorGuard{};

    const auto es = Opm::EclipseState { deck };

    BOOST_CHECK_THROW(Opm::Schedule(deck, es, ctx, errors, std::make_shared<Opm::Python>()),
                      Opm::OpmInputError);
}

BOOST_AUTO_TEST_CASE(Compdat_Zero_Perm_Diagnostic_Text)
{
    const auto deck = Opm::Parser{}.parseString(R"(RUNSPEC
START
  18 MAR 2026 /
OIL
WATER
DIMENS
3 1 3 /
TABDIMS
/
EQLDIMS
/
WELLDIMS
 1 3 1 1 /
GRID
DXV
 3*100 /
DYV
 100 /
DZV
 3*10 /
DEPTHZ
 8*2000 /
EQUALS
 PERMX 100 /
 PERMY 100 /
 PERMZ  10 /
 PORO    0.3 /
/
-- Kx(2,1,2) = Ky(2,1,2) = 0.
EQUALS
 PERMX 0  2 2  1 1  2 2 /
 PERMY 0 /
/
PROPS
DENSITY
  800 1000 1 /
SOLUTION
EQUIL
2010 200 2010 1.23 1995 0.0 1* 1* -5 /
SCHEDULE
WELSPECS
  'P' 'G' 2 2 2005.0 LIQ /
/
COMPDAT
  'P'  2  1  1  3  OPEN  1  1*  0.3048  /
/
TSTEP
  5*10 /
END
)");

    const auto ctx = Opm::ParseContext {
        std::vector {
            std::pair { Opm::ParseContext::SCHEDULE_COMPDAT_ZERO_PERM,
                        Opm::InputErrorAction::DELAYED_EXIT1 },
        }
    };

    auto errors = Opm::ErrorGuard{};

    const auto es = Opm::EclipseState { deck };
    const auto sched = Opm::Schedule {
        deck, es, ctx, errors,
        std::make_shared<Opm::Python>()
    };

    const auto diagnostic = errors.formattedErrors();
    errors.clear();

    // Note: Leading newline ("R(\n)) added by ErrorGuard::formattedErrors().
    BOOST_CHECK_EQUAL(diagnostic, R"(
Problem with keyword COMPDAT
In <memory string> line 44
Connection (2,1,2) (direction 'Z') for well P ignored because
   PERMX=0.000e+00 mD and PERMY=0.000e+00 mD.)");
}

BOOST_AUTO_TEST_CASE(Compdat_Zero_Perm_Diagnostic_Text_Kx10)
{
    const auto deck = Opm::Parser{}.parseString(R"(RUNSPEC
START
  18 MAR 2026 /
OIL
WATER
DIMENS
3 1 3 /
TABDIMS
/
EQLDIMS
/
WELLDIMS
 1 3 1 1 /
GRID
DXV
 3*100 /
DYV
 100 /
DZV
 3*10 /
DEPTHZ
 8*2000 /
EQUALS
 PERMX 100 /
 PERMY 100 /
 PERMZ  10 /
 PORO    0.3 /
/
-- Kx(2,1,2) = Ky(2,1,2) = 0.
EQUALS
 PERMX 10  2 2  1 1  2 2 /
 PERMY  0 /
/
PROPS
DENSITY
  800 1000 1 /
SOLUTION
EQUIL
2010 200 2010 1.23 1995 0.0 1* 1* -5 /
SCHEDULE
WELSPECS
  'P' 'G' 2 2 2005.0 LIQ /
/
COMPDAT
  'P'  2  1  1  3  OPEN  1  1*  0.3048  /
/
TSTEP
  5*10 /
END
)");

    const auto ctx = Opm::ParseContext {
        std::vector {
            std::pair { Opm::ParseContext::SCHEDULE_COMPDAT_ZERO_PERM,
                        Opm::InputErrorAction::DELAYED_EXIT1 },
        }
    };

    auto errors = Opm::ErrorGuard{};

    const auto es = Opm::EclipseState { deck };
    const auto sched = Opm::Schedule {
        deck, es, ctx, errors,
        std::make_shared<Opm::Python>()
    };

    const auto diagnostic = errors.formattedErrors();
    errors.clear();

    // Note: Leading newline ("R(\n)) added by ErrorGuard::formattedErrors().
    BOOST_CHECK_EQUAL(diagnostic, R"(
Problem with keyword COMPDAT
In <memory string> line 44
Connection (2,1,2) (direction 'Z') for well P ignored because
   PERMX=1.000e+01 mD and PERMY=0.000e+00 mD.)");
}

BOOST_AUTO_TEST_CASE(Compdat_Zero_Perm_Diagnostic_Text_Ky10)
{
    const auto deck = Opm::Parser{}.parseString(R"(RUNSPEC
START
  18 MAR 2026 /
OIL
WATER
DIMENS
3 1 3 /
TABDIMS
/
EQLDIMS
/
WELLDIMS
 1 3 1 1 /
GRID
DXV
 3*100 /
DYV
 100 /
DZV
 3*10 /
DEPTHZ
 8*2000 /
EQUALS
 PERMX 100 /
 PERMY 100 /
 PERMZ  10 /
 PORO    0.3 /
/
-- Kx(2,1,2) = Ky(2,1,2) = 0.
EQUALS
 PERMX  0  2 2  1 1  2 2 /
 PERMY 10 /
/
PROPS
DENSITY
  800 1000 1 /
SOLUTION
EQUIL
2010 200 2010 1.23 1995 0.0 1* 1* -5 /
SCHEDULE
WELSPECS
  'P' 'G' 2 2 2005.0 LIQ /
/
COMPDAT
  'P'  2  1  1  3  OPEN  1  1*  0.3048  /
/
TSTEP
  5*10 /
END
)");

    const auto ctx = Opm::ParseContext {
        std::vector {
            std::pair { Opm::ParseContext::SCHEDULE_COMPDAT_ZERO_PERM,
                        Opm::InputErrorAction::DELAYED_EXIT1 },
        }
    };

    auto errors = Opm::ErrorGuard{};

    const auto es = Opm::EclipseState { deck };
    const auto sched = Opm::Schedule {
        deck, es, ctx, errors,
        std::make_shared<Opm::Python>()
    };

    const auto diagnostic = errors.formattedErrors();
    errors.clear();

    // Note: Leading newline ("R(\n)) added by ErrorGuard::formattedErrors().
    BOOST_CHECK_EQUAL(diagnostic, R"(
Problem with keyword COMPDAT
In <memory string> line 44
Connection (2,1,2) (direction 'Z') for well P ignored because
   PERMX=0.000e+00 mD and PERMY=1.000e+01 mD.)");
}

BOOST_AUTO_TEST_CASE(Compdat_Zero_Perm_Diagnostic_Text_DirX)
{
    const auto deck = Opm::Parser{}.parseString(R"(RUNSPEC
START
  18 MAR 2026 /
OIL
WATER
DIMENS
3 1 3 /
TABDIMS
/
EQLDIMS
/
WELLDIMS
 1 3 1 1 /
GRID
DXV
 3*100 /
DYV
 100 /
DZV
 3*10 /
DEPTHZ
 8*2000 /
EQUALS
 PERMX 100 /
 PERMY 100 /
 PERMZ  10 /
 PORO    0.3 /
/
-- Ky(2,1,2) = Kz(2,1,2) = 0.
EQUALS
 PERMY 0  2 2  1 1  2 2 /
 PERMZ 0 /
/
PROPS
DENSITY
  800 1000 1 /
SOLUTION
EQUIL
2010 200 2010 1.23 1995 0.0 1* 1* -5 /
SCHEDULE
WELSPECS
  'P' 'G' 2 2 2005.0 LIQ /
/
COMPDAT
  'P'  2  1  1  1  OPEN  1  1*  0.3048  /
  'P'  2  1  2  2  OPEN  1  1*  0.3048 1* 1* 1* 'X' /
  'P'  2  1  3  3  OPEN  1  1*  0.3048  /
/
TSTEP
  5*10 /
END
)");

    const auto ctx = Opm::ParseContext {
        std::vector {
            std::pair { Opm::ParseContext::SCHEDULE_COMPDAT_ZERO_PERM,
                        Opm::InputErrorAction::DELAYED_EXIT1 },
        }
    };

    auto errors = Opm::ErrorGuard{};

    const auto es = Opm::EclipseState { deck };
    const auto sched = Opm::Schedule {
        deck, es, ctx, errors,
        std::make_shared<Opm::Python>()
    };

    const auto diagnostic = errors.formattedErrors();
    errors.clear();

    // Note: Leading newline ("R(\n)) added by ErrorGuard::formattedErrors().
    BOOST_CHECK_EQUAL(diagnostic, R"(
Problem with keyword COMPDAT
In <memory string> line 44
Connection (2,1,2) (direction 'X') for well P ignored because
   PERMY=0.000e+00 mD and PERMZ=0.000e+00 mD.)");
}

BOOST_AUTO_TEST_CASE(Compdat_Zero_Perm_Diagnostic_Text_DirY)
{
    const auto deck = Opm::Parser{}.parseString(R"(RUNSPEC
START
  18 MAR 2026 /
OIL
WATER
DIMENS
3 1 3 /
TABDIMS
/
EQLDIMS
/
WELLDIMS
 1 3 1 1 /
GRID
DXV
 3*100 /
DYV
 100 /
DZV
 3*10 /
DEPTHZ
 8*2000 /
EQUALS
 PERMX 100 /
 PERMY 100 /
 PERMZ  10 /
 PORO    0.3 /
/
-- Kx(2,1,2) = Kz(2,1,2) = 0.
EQUALS
 PERMX 0  2 2  1 1  2 2 /
 PERMZ 0 /
/
PROPS
DENSITY
  800 1000 1 /
SOLUTION
EQUIL
2010 200 2010 1.23 1995 0.0 1* 1* -5 /
SCHEDULE
WELSPECS
  'P' 'G' 2 2 2005.0 LIQ /
/
COMPDAT
  'P'  2  1  1  1  OPEN  1  1*  0.3048  /
  'P'  2  1  2  2  OPEN  1  1*  0.3048 1* 1* 1* 'Y' /
  'P'  2  1  3  3  OPEN  1  1*  0.3048  /
/
TSTEP
  5*10 /
END
)");

    const auto ctx = Opm::ParseContext {
        std::vector {
            std::pair { Opm::ParseContext::SCHEDULE_COMPDAT_ZERO_PERM,
                        Opm::InputErrorAction::DELAYED_EXIT1 },
        }
    };

    auto errors = Opm::ErrorGuard{};

    const auto es = Opm::EclipseState { deck };
    const auto sched = Opm::Schedule {
        deck, es, ctx, errors,
        std::make_shared<Opm::Python>()
    };

    const auto diagnostic = errors.formattedErrors();
    errors.clear();

    // Note: Leading newline ("R(\n)) added by ErrorGuard::formattedErrors().
    BOOST_CHECK_EQUAL(diagnostic, R"(
Problem with keyword COMPDAT
In <memory string> line 44
Connection (2,1,2) (direction 'Y') for well P ignored because
   PERMZ=0.000e+00 mD and PERMX=0.000e+00 mD.)");
}

// A synthetic centre vertex may coincide with an internal refined-cell face.
// Replaying a straight completion must retain both children of every parent.
BOOST_AUTO_TEST_CASE(RecomputeTrajectory_SyntheticRefinedKeepsAllChildren)
{
    const auto deck = Opm::Parser{}.parseString(STACKED_COMPLETION_DECK);
    const auto es = Opm::EclipseState { deck };
    auto sched = Opm::Schedule { deck, es, std::make_shared<Opm::Python>() };
    const auto& coarse = es.getInputGrid();
    GridReplayGeom original(coarse);
    const Opm::EclipseGrid fine(6, 6, 6, 50.0, 50.0, 5.0, 2000.0);
    GridReplayGeom refined(fine);
    const auto lookup = refined.cellInfo();
    auto wc = sched.getWell("P", 0).getConnections();
    BOOST_REQUIRE(wc.synthesizeTrajectory(original.center(), original.extent()));
    const auto names = wc.recomputeTrajectoryConnections(refined.corners,
        [&lookup](std::size_t idx) {
            auto cell = lookup(idx);
            if (cell) { cell->lgr_grid = 1; cell->lgr_name = "FINE"; }
            return cell;
        });
    BOOST_REQUIRE_EQUAL(names.size(), 1u);
    BOOST_CHECK_EQUAL(*names.begin(), "FINE");
    BOOST_REQUIRE_EQUAL(wc.size(), 6u);
    for (int k = 0; k < 6; ++k) {
        const auto& c = wc[k];
        BOOST_CHECK_EQUAL(c.getI(), 3);
        BOOST_CHECK_EQUAL(c.getJ(), 3);
        BOOST_CHECK_EQUAL(c.getK(), k);
        BOOST_CHECK_EQUAL(c.get_lgr_level(), 1);
        BOOST_CHECK(c.CF() > 0.0);
        BOOST_CHECK(c.Kh() > 0.0);
    }
}

// Exact six-child geometry from SPE9 PRODU11. Roundoff in the shared face
// corners puts the vertical path on a face-triangle edge: no child may vanish.
BOOST_AUTO_TEST_CASE(RecomputeTrajectory_RefinedFaceDiagonal)
{
    const std::vector<std::array<external::cvf::Vec3d, 8>> corners {
        {{
            {1051.5600000000002, 868.68000000000006, 2926.652730267288},
            {1097.2800000000002, 868.67999999999995, 2926.652730267288},
            {1051.5600000000002, 914.40000000000009, 2926.652730267288},
            {1097.2800000000002, 914.39999999999998, 2926.652730267288},
            {1051.5600000000002, 868.68000000000006, 2928.9387302672881},
            {1097.2800000000004, 868.68000000000018, 2928.9387302672881},
            {1051.5600000000002, 914.40000000000009, 2928.9387302672881},
            {1097.2800000000004, 914.4000000000002, 2928.9387302672881},
        }},
        {{
            {1051.5600000000002, 868.68000000000006, 2928.9387302672881},
            {1097.2800000000004, 868.68000000000018, 2928.9387302672881},
            {1051.5600000000002, 914.40000000000009, 2928.9387302672881},
            {1097.2800000000004, 914.4000000000002, 2928.9387302672881},
            {1051.5600000000002, 868.68000000000018, 2931.2247302672881},
            {1097.2800000000002, 868.68000000000006, 2931.2247302672881},
            {1051.5600000000002, 914.40000000000009, 2931.2247302672881},
            {1097.2800000000002, 914.40000000000009, 2931.2247302672881},
        }},
        {{
            {1051.5600000000002, 868.68000000000018, 2931.2247302672881},
            {1097.2800000000002, 868.68000000000006, 2931.2247302672881},
            {1051.5600000000002, 914.40000000000009, 2931.2247302672881},
            {1097.2800000000002, 914.40000000000009, 2931.2247302672881},
            {1051.5600000000002, 868.68000000000018, 2935.187130267288},
            {1097.2800000000002, 868.67999999999995, 2935.187130267288},
            {1051.5600000000002, 914.40000000000009, 2935.187130267288},
            {1097.2800000000002, 914.40000000000009, 2935.187130267288},
        }},
        {{
            {1051.5600000000002, 868.68000000000018, 2935.187130267288},
            {1097.2800000000002, 868.67999999999995, 2935.187130267288},
            {1051.5600000000002, 914.40000000000009, 2935.187130267288},
            {1097.2800000000002, 914.40000000000009, 2935.187130267288},
            {1051.5600000000002, 868.68000000000006, 2939.1495302672879},
            {1097.2800000000002, 868.67999999999995, 2939.1495302672879},
            {1051.5600000000002, 914.4000000000002, 2939.1495302672879},
            {1097.2800000000002, 914.40000000000009, 2939.1495302672879},
        }},
        {{
            {1051.5600000000002, 868.68000000000006, 2939.1495302672879},
            {1097.2800000000002, 868.67999999999995, 2939.1495302672879},
            {1051.5600000000002, 914.4000000000002, 2939.1495302672879},
            {1097.2800000000002, 914.40000000000009, 2939.1495302672879},
            {1051.5600000000002, 868.68000000000006, 2941.435530267288},
            {1097.2800000000002, 868.68000000000006, 2941.435530267288},
            {1051.5600000000002, 914.40000000000009, 2941.435530267288},
            {1097.2800000000002, 914.40000000000009, 2941.435530267288},
        }},
        {{
            {1051.5600000000002, 868.68000000000006, 2941.435530267288},
            {1097.2800000000002, 868.68000000000006, 2941.435530267288},
            {1051.5600000000002, 914.40000000000009, 2941.435530267288},
            {1097.2800000000002, 914.40000000000009, 2941.435530267288},
            {1051.5600000000002, 868.68000000000006, 2943.721530267288},
            {1097.2800000000004, 868.68000000000006, 2943.721530267288},
            {1051.5600000000002, 914.40000000000009, 2943.721530267288},
            {1097.2800000000004, 914.4000000000002, 2943.721530267288},
        }},
    };
    const std::vector<external::cvf::Vec3d> points {
        {1051.6514400000001, 868.7714400000001, 2926.6504442672881},
        {1051.6514400000001, 868.7714400000001, 2928.9387302672881},
        {1051.6514400000001, 868.7714400000001, 2935.187130267288},
        {1051.6514400000001, 868.7714400000001, 2941.435530267288},
        {1051.6514400000001, 868.7714400000001, 2943.7238162672879},
    };
    const std::vector<double> md { 0, 2.288285999999971, 8.5366859999999178, 14.785085999999865, 17.073371999999836 };
    for (bool reverse : {false, true}) {
        auto pathPoints = points;
        auto pathMd = md;
        if (reverse) {
            std::reverse(pathPoints.begin(), pathPoints.end());
            for (std::size_t i = 0; i < md.size(); ++i)
                pathMd[i] = md.back() - md[md.size() - 1 - i];
        }
        external::cvf::ref<external::RigWellPath> path {new external::RigWellPath};
        path->setWellPathPoints(pathPoints);
        path->setMeasuredDepths(pathMd);
        external::cvf::ref<external::cvf::BoundingBoxTree> tree;
        external::RigEclipseWellLogExtractorGrid extractor(path.p(), corners, tree);
        const auto hits = extractor.cellIntersectionInfosAlongWellPath();
        BOOST_REQUIRE_EQUAL(hits.size(), 6u);
        std::array<int, 6> count{};
        double length = 0.0;
        for (const auto& hit : hits) {
            BOOST_REQUIRE_LT(hit.globCellIndex, count.size());
            ++count[hit.globCellIndex];
            BOOST_CHECK_GT(hit.endMD, hit.startMD);
            length += hit.endMD - hit.startMD;
        }
        for (int n : count) BOOST_CHECK_EQUAL(n, 1);
        BOOST_CHECK_SMALL(length - 17.0688, 1e-9);
    }
}

BOOST_AUTO_TEST_CASE(TrajectoryTriangle_BoundaryAndOutside)
{
    using external::cvf::Vec3d;
    const Vec3d a(0, 0, 0), b(1, 0, 0), c(0, 1, 0);
    for (bool reverse : {false, true}) {
        const double start = reverse ? 1.0 : -1.0;
        Vec3d intersection;
        bool entering = false;
        for (const auto& xy : {std::array<double, 2>{0.5, 0.5}, {0.0, 0.0}, {0.2, 0.2}})
            BOOST_CHECK_EQUAL(external::cvf::GeometryTools::intersectLineSegmentTriangle(
                Vec3d(xy[0], xy[1], start), Vec3d(xy[0], xy[1], -start),
                a, b, c, &intersection, &entering), 1);
        for (const auto& xy : {std::array<double, 2>{-1e-8, 0.5}, {0.5, 0.5 + 1e-8}})
            BOOST_CHECK_EQUAL(external::cvf::GeometryTools::intersectLineSegmentTriangle(
                Vec3d(xy[0], xy[1], start), Vec3d(xy[0], xy[1], -start),
                a, b, c, &intersection, &entering), 0);
    }
}
