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

#include "EqSimFileOutput.h"

void EqSimFileOutput::initDesc(const SimFramework *_sim) const {
    const VCSimulation          *sim = static_cast<const VCSimulation *>(_sim);

    sim->console() << "# EqSim events output file: " << sim->getEqSimOutputFile() << std::endl;
}

/*!
 Initialize the EqSim event output file by setting the header, title, etc.
 */
void EqSimFileOutput::init(SimFramework *_sim) {
    VCSimulation        *sim = static_cast<VCSimulation *>(_sim);
    std::string         file_name = sim->getEqSimOutputFile().c_str();

    eqsim_event_file.open(file_name.c_str());
}

/*!
 Add the current event to the EqSim output file. This takes a little work because multi-fault
 ruptures are single events in VC but must be split along fault lines in EqSim.
 */
SimRequest EqSimFileOutput::run(SimFramework *_sim) {
    VCSimulation                *sim = static_cast<VCSimulation *>(_sim);
    BlockIDSet                  event_blocks, slip_rect_blocks;
    BlockIDSet::iterator        bit, bit2;
    SectionBlockMapping         event_sections;
    double                      event_magnitude, fault_rupture_area, mean_slip, normalized_year;
    double                      low_depth, high_depth, low_das, high_das;
    double                      shear_stress, shear_stress0, normal_stress, normal_stress0;
    SectionIDList               section_ordering;
    SectionIDList::iterator     it;
    quakelib::SectionID                 sid;
    BlockID                     hypocenter_bid;
    quakelib::Conversion        convert;

    // Don't start recording until the specified year
    if (sim->getRecordingStart() > sim->getYear()) return SIM_STOP_OK;

    // In Virtual California, events may occur over multiple faults,
    // but this is not supported in the EqSim format.
    // Therefore, we may need to split a VC event into multiple EqSim events.

    // Get the blocks involved in this event
    sim->getCurrentEvent().getInvolvedBlocks(event_blocks);

    // Figure out what faults IDs these blocks correspond to
    sim->getSectionBlockMapping(event_sections, event_blocks);

    // Get the total magnitude of the event (among all faults)
    event_magnitude = sim->getCurrentEvent().getMagnitude(event_blocks);

    // Order the faults by rupture progression
    sim->getCurrentEvent().orderSectionsByRupture(section_ordering, event_sections);

    // For each fault ID, output the necessary statistics
    for (it=section_ordering.begin(); it!=section_ordering.end(); ++it) {
        quakelib::EQSimEventSummary event_summary;

        sid = *it;
        normalized_year = sim->getCurrentEvent().getEventYear() - sim->getRecordingStart();
        sim->depthDASBounds(event_sections[sid], low_depth, high_depth, low_das, high_das);
        sim->sumStresses(event_sections[sid], shear_stress, shear_stress0, normal_stress, normal_stress0);
        hypocenter_bid = sim->getCurrentEvent().getHypocenter(event_sections[sid]);
        sim->getCurrentEvent().getTotalSlipAndArea(event_sections[sid], mean_slip, fault_rupture_area);
        mean_slip /= event_sections[sid].size();

        event_summary.set_event_id(sim->getEventCount());
        event_summary.set_magnitude(event_magnitude);
        event_summary.set_time(convert.year2sec(normalized_year));
        event_summary.set_duration(0.0);
        event_summary.set_sid(sid);
        event_summary.set_depth_lo(low_depth);
        event_summary.set_depth_hi(high_depth);
        event_summary.set_das_lo(low_das);
        event_summary.set_das_hi(high_das);
        event_summary.set_hypo_depth(sim->getBlock(hypocenter_bid).avg_depth());
        event_summary.set_hypo_das(convert.km2m(sim->getBlock(hypocenter_bid).avg_das()));
        event_summary.set_area(convert.sqkm2sqm(fault_rupture_area));
        event_summary.set_mean_slip(convert.cm2m(mean_slip));
        event_summary.set_moment(convert.sqkm2sqm(fault_rupture_area)*convert.cm2m(mean_slip)*3.0e10);
        event_summary.set_shear_before(convert.bar2pascal(shear_stress0));
        event_summary.set_shear_after(convert.bar2pascal(shear_stress));
        event_summary.set_normal_before(convert.bar2pascal(normal_stress0));
        event_summary.set_normal_after(convert.bar2pascal(normal_stress));

        // Write out a slip map for events that are over the specified magnitude
        if (event_magnitude > sim->getEqSimOutputSlipMapMag()) {
            for (bit=event_sections[sid].begin(); bit!=event_sections[sid].end(); ++bit) {
                slip_rect_blocks.clear();

                if (sim->isTopOfSlipRectangle(*bit, event_sections[sid])) {
                    quakelib::EQSimEventSlipMap event_slips;

                    sim->getSlipRectangleBlocks(slip_rect_blocks, *bit, event_sections[sid]);
                    sim->sumStresses(slip_rect_blocks, shear_stress, shear_stress0, normal_stress, normal_stress0);
                    sim->depthDASBounds(slip_rect_blocks, low_depth, high_depth, low_das, high_das);

                    sim->getCurrentEvent().getTotalSlipAndArea(slip_rect_blocks, mean_slip, fault_rupture_area);
                    mean_slip /= slip_rect_blocks.size();

                    event_slips.set_depth_lo(-1.0*convert.km2m(low_depth));
                    event_slips.set_depth_hi(-1.0*convert.km2m(high_depth));
                    event_slips.set_das_lo(convert.km2m(low_das));
                    event_slips.set_das_hi(convert.km2m(high_das));
                    event_slips.set_area(convert.sqkm2sqm(fault_rupture_area));
                    event_slips.set_mean_slip(convert.cm2m(mean_slip));
                    event_slips.set_moment(convert.sqkm2sqm(fault_rupture_area)*convert.cm2m(mean_slip)*3.0e10);
                    event_slips.set_shear_before(convert.bar2pascal(shear_stress0));
                    event_slips.set_shear_after(convert.bar2pascal(shear_stress));
                    event_slips.set_normal_before(convert.bar2pascal(normal_stress0));
                    event_slips.set_normal_after(convert.bar2pascal(normal_stress));

                    for (bit2=slip_rect_blocks.begin(); bit2!=slip_rect_blocks.end(); ++bit2)
                        event_slips.add_slip_entry(quakelib::EQSimEventSlipElement(*bit2+1));

                    event_summary.add_slip_map(event_slips);
                }
            }
        }

        // Add the generated event summary to the EqSim output class and flush it to the file
        eqsim_event_file.add_event_summary(event_summary);
        eqsim_event_file.flush();
    }

    return SIM_STOP_OK;
}

void EqSimFileOutput::finish(SimFramework *_sim) {
    eqsim_event_file.close();
}
