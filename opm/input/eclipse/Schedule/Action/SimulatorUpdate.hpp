/*
  Copyright 2021 Equinor ASA.

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

#ifndef SIMULATOR_UPDATE_HPP
#define SIMULATOR_UPDATE_HPP

#include <string>
#include <unordered_set>

namespace Opm {

/// This struct is used to communicate back from the Schedule::applyAction()
/// what needs to be updated in the simulator when execution is returned to
/// the simulator code.

struct SimulatorUpdate
{
    static SimulatorUpdate serializationTestObject()
    {
        SimulatorUpdate simulatorUpdate;
        simulatorUpdate.tran_update = true;
        simulatorUpdate.well_structure_changed = true;
        simulatorUpdate.affected_wells = {"test"};
        simulatorUpdate.welpi_wells.insert("I-45");
        return simulatorUpdate;
    }

    template<class Serializer>
    void serializeOp(Serializer& serializer)
    {
        serializer(affected_wells);
        serializer(welpi_wells);
        serializer(tran_update);
        serializer(well_structure_changed);
    }

    /// Wells affected by ACTIONX and for which the simulator needs to
    /// reapply rates and state from the newly updated Schedule object.
    std::unordered_set<std::string> affected_wells{};

    /// Wells affected only by WELPI for which the simulator needs to update
    /// its internal notion of the connection transmissibility factors.
    std::unordered_set<std::string> welpi_wells{};

    /// Whether or not a transmissibility multiplier keyword was invoked in
    /// an ACTIONX block.
    ///
    /// If so, the simulator needs to recalculate the transmissibilities.
    bool tran_update{false};

    /// Whether or not well structure changed in processing an ACTIONX
    /// block.
    ///
    /// Typically because of a keyword like WELSPECS, COMPDAT, and/or
    /// WELOPEN.
    bool well_structure_changed{false};

    void append(const SimulatorUpdate& otherSimUpdate)
    {
        this->tran_update = this->tran_update || otherSimUpdate.tran_update;

        this->well_structure_changed = this->well_structure_changed
            || otherSimUpdate.well_structure_changed;

        this->affected_wells.insert(otherSimUpdate.affected_wells.begin(),
                                    otherSimUpdate.affected_wells.end());

        this->welpi_wells.insert(otherSimUpdate.welpi_wells.begin(),
                                 otherSimUpdate.welpi_wells.end());
    }

    void reset()
    {
        this->tran_update = false;
        this->well_structure_changed = false;
        this->affected_wells.clear();
        this->welpi_wells.clear();
    }

    bool operator==(const SimulatorUpdate& that) const
    {
        return (this->tran_update == that.tran_update)
            && (this->well_structure_changed == that.well_structure_changed)
            && (this->affected_wells == that.affected_wells)
            && (this->welpi_wells == that.welpi_wells)
            ;
    }
};

} // namespace Opm

#endif // SIMULATOR_UPDATE_HPP
