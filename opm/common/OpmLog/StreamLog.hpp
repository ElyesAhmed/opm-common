/*
  Copyright 2015 Statoil ASA.

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

#ifndef STREAMLOG_H
#define STREAMLOG_H

#include <fstream>
#include <cstdint>

#include <opm/common/OpmLog/LogBackend.hpp>

namespace Opm {

class StreamLog : public LogBackend {

public:
    StreamLog(const std::string& logFile , int64_t messageMask, bool append = false);
    StreamLog(std::ostream& os , int64_t messageMask);
    ~StreamLog() override;

protected:
    void addMessageUnconditionally(int64_t messageType, const std::string& message) override;

private:
    void close();

    std::ofstream   m_ofstream;
    std::ostream  * m_ostream;
    bool m_streamOwner;
};
}

#endif
