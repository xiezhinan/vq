// Copyright (c) 2012-2014 Eric M. Heien, Michael K. Sachs, John B. Rundle
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS IN THE SOFTWARE.

#include "BadFaultKill.h"

void BadFaultKill::initDesc(const SimFramework *_sim) const {
    const VCSimulation          *sim = static_cast<const VCSimulation *>(_sim);

    sim->console() << "# Will kill faults if CFF drops below " << sim->getFaultKillCFF() << "." << std::endl;
}

SimRequest BadFaultKill::run(SimFramework *_sim) {
    VCSimulation    *sim = static_cast<VCSimulation *>(_sim);
    BlockID         gid;
    int             lid, i;
    double          cff, min_cff;

    min_cff = sim->getFaultKillCFF();

    for (lid=0; lid<sim->numLocalBlocks(); ++lid) {
        gid = sim->getGlobalBID(lid);
        cff = sim->getBlock(gid).getCFF();

        if (cff < min_cff) {
            for (i=0; i<sim->numGlobalBlocks(); ++i) {
                sim->setGreens(gid, i, 0, 0);
            }

            sim->console() << "# WARNING: Killed segment " << gid << " (" << sim->getBlock(gid).getFaultName() << ") due to low CFF (" << cff << ")" << std::endl;
        }
    }

    return SIM_STOP_OK;
}
