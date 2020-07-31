#include "L1Trigger/CSCTriggerPrimitives/interface/CSCCathodeLCTProcessor.h"
#include "L1Trigger/CSCTriggerPrimitives/interface/CSCLCTTools.h"

#include <iomanip>
#include <iostream>

// Default values of configuration parameters.
const unsigned int CSCCathodeLCTProcessor::def_fifo_tbins = 12;
const unsigned int CSCCathodeLCTProcessor::def_fifo_pretrig = 7;
const unsigned int CSCCathodeLCTProcessor::def_hit_persist = 6;
const unsigned int CSCCathodeLCTProcessor::def_drift_delay = 2;
const unsigned int CSCCathodeLCTProcessor::def_nplanes_hit_pretrig = 2;
const unsigned int CSCCathodeLCTProcessor::def_nplanes_hit_pattern = 4;
const unsigned int CSCCathodeLCTProcessor::def_pid_thresh_pretrig = 2;
const unsigned int CSCCathodeLCTProcessor::def_min_separation = 10;
const unsigned int CSCCathodeLCTProcessor::def_tmb_l1a_window_size = 7;

//----------------
// Constructors --
//----------------

CSCCathodeLCTProcessor::CSCCathodeLCTProcessor(unsigned endcap,
                                               unsigned station,
                                               unsigned sector,
                                               unsigned subsector,
                                               unsigned chamber,
                                               const edm::ParameterSet& conf)
    : CSCBaseboard(endcap, station, sector, subsector, chamber, conf) {
  static std::atomic<bool> config_dumped{false};

  // CLCT configuration parameters.
  fifo_tbins = clctParams_.getParameter<unsigned int>("clctFifoTbins");
  hit_persist = clctParams_.getParameter<unsigned int>("clctHitPersist");
  drift_delay = clctParams_.getParameter<unsigned int>("clctDriftDelay");
  nplanes_hit_pretrig = clctParams_.getParameter<unsigned int>("clctNplanesHitPretrig");
  nplanes_hit_pattern = clctParams_.getParameter<unsigned int>("clctNplanesHitPattern");

  // Not used yet.
  fifo_pretrig = clctParams_.getParameter<unsigned int>("clctFifoPretrig");

  pid_thresh_pretrig = clctParams_.getParameter<unsigned int>("clctPidThreshPretrig");
  min_separation = clctParams_.getParameter<unsigned int>("clctMinSeparation");

  start_bx_shift = clctParams_.getParameter<int>("clctStartBxShift");

  // Motherboard parameters: common for all configurations.
  tmb_l1a_window_size =  // Common to CLCT and TMB
      tmbParams_.getParameter<unsigned int>("tmbL1aWindowSize");

  // separate handle for early time bins
  early_tbins = tmbParams_.getParameter<int>("tmbEarlyTbins");
  if (early_tbins < 0)
    early_tbins = fifo_pretrig - CSCConstants::CLCT_EMUL_TIME_OFFSET;

  // wether to readout only the earliest two LCTs in readout window
  readout_earliest_2 = tmbParams_.getParameter<bool>("tmbReadoutEarliest2");

  // Verbosity level, set to 0 (no print) by default.
  infoV = clctParams_.getParameter<int>("verbosity");

  use_run3_patterns_ = clctParams_.getParameter<bool>("useRun3Patterns");

  use_comparator_codes_ = clctParams_.getParameter<bool>("useComparatorCodes");

  // Check and print configuration parameters.
  checkConfigParameters();
  if ((infoV > 0) && !config_dumped) {
    dumpConfigParams();
    config_dumped = true;
  }

  numStrips = 0;  // Will be set later.
  // Provisional, but should be OK for all stations except ME1.
  for (int i_layer = 0; i_layer < CSCConstants::NUM_LAYERS; i_layer++) {
    if ((i_layer + 1) % 2 == 0)
      stagger[i_layer] = 0;
    else
      stagger[i_layer] = 1;
  }

  // which patterns should we use?
  if (use_run3_patterns_) {
    clct_pattern_ = CSCPatternBank::clct_pattern_run3_;
  } else {
    clct_pattern_ = CSCPatternBank::clct_pattern_legacy_;
  }

  thePreTriggerDigis.clear();
}

CSCCathodeLCTProcessor::CSCCathodeLCTProcessor() : CSCBaseboard() {
  // constructor for debugging.
  static std::atomic<bool> config_dumped{false};

  // CLCT configuration parameters.
  setDefaultConfigParameters();
  infoV = 2;

  early_tbins = 4;

  start_bx_shift = 0;

  // Check and print configuration parameters.
  checkConfigParameters();
  if (!config_dumped) {
    dumpConfigParams();
    config_dumped = true;
  }

  numStrips = CSCConstants::MAX_NUM_STRIPS;
  // Should be OK for all stations except ME1.
  for (int i_layer = 0; i_layer < CSCConstants::NUM_LAYERS; i_layer++) {
    if ((i_layer + 1) % 2 == 0)
      stagger[i_layer] = 0;
    else
      stagger[i_layer] = 1;
  }

  thePreTriggerDigis.clear();
}

void CSCCathodeLCTProcessor::setDefaultConfigParameters() {
  // Set default values for configuration parameters.
  fifo_tbins = def_fifo_tbins;
  fifo_pretrig = def_fifo_pretrig;
  hit_persist = def_hit_persist;
  drift_delay = def_drift_delay;
  nplanes_hit_pretrig = def_nplanes_hit_pretrig;
  nplanes_hit_pattern = def_nplanes_hit_pattern;
  pid_thresh_pretrig = def_pid_thresh_pretrig;
  min_separation = def_min_separation;
  tmb_l1a_window_size = def_tmb_l1a_window_size;
}

// Set configuration parameters obtained via EventSetup mechanism.
void CSCCathodeLCTProcessor::setConfigParameters(const CSCDBL1TPParameters* conf) {
  static std::atomic<bool> config_dumped{false};

  fifo_tbins = conf->clctFifoTbins();
  fifo_pretrig = conf->clctFifoPretrig();
  hit_persist = conf->clctHitPersist();
  drift_delay = conf->clctDriftDelay();
  nplanes_hit_pretrig = conf->clctNplanesHitPretrig();
  nplanes_hit_pattern = conf->clctNplanesHitPattern();
  pid_thresh_pretrig = conf->clctPidThreshPretrig();
  min_separation = conf->clctMinSeparation();

  // Check and print configuration parameters.
  checkConfigParameters();
  if (!config_dumped) {
    dumpConfigParams();
    config_dumped = true;
  }
}

void CSCCathodeLCTProcessor::checkConfigParameters() {
  // Make sure that the parameter values are within the allowed range.

  // Max expected values.
  static const unsigned int max_fifo_tbins = 1 << 5;
  static const unsigned int max_fifo_pretrig = 1 << 5;
  static const unsigned int max_hit_persist = 1 << 4;
  static const unsigned int max_drift_delay = 1 << 2;
  static const unsigned int max_nplanes_hit_pretrig = 1 << 3;
  static const unsigned int max_nplanes_hit_pattern = 1 << 3;
  static const unsigned int max_pid_thresh_pretrig = 1 << 4;
  static const unsigned int max_min_separation = CSCConstants::NUM_HALF_STRIPS_7CFEBS;
  static const unsigned int max_tmb_l1a_window_size = 1 << 4;

  // Checks.
  CSCBaseboard::checkConfigParameters(fifo_tbins, max_fifo_tbins, def_fifo_tbins, "fifo_tbins");
  CSCBaseboard::checkConfigParameters(fifo_pretrig, max_fifo_pretrig, def_fifo_pretrig, "fifo_pretrig");
  CSCBaseboard::checkConfigParameters(hit_persist, max_hit_persist, def_hit_persist, "hit_persist");
  CSCBaseboard::checkConfigParameters(drift_delay, max_drift_delay, def_drift_delay, "drift_delay");
  CSCBaseboard::checkConfigParameters(
      nplanes_hit_pretrig, max_nplanes_hit_pretrig, def_nplanes_hit_pretrig, "nplanes_hit_pretrig");
  CSCBaseboard::checkConfigParameters(
      nplanes_hit_pattern, max_nplanes_hit_pattern, def_nplanes_hit_pattern, "nplanes_hit_pattern");
  CSCBaseboard::checkConfigParameters(
      pid_thresh_pretrig, max_pid_thresh_pretrig, def_pid_thresh_pretrig, "pid_thresh_pretrig");
  CSCBaseboard::checkConfigParameters(min_separation, max_min_separation, def_min_separation, "min_separation");
  CSCBaseboard::checkConfigParameters(
      tmb_l1a_window_size, max_tmb_l1a_window_size, def_tmb_l1a_window_size, "tmb_l1a_window_size");
}

void CSCCathodeLCTProcessor::clear() {
  thePreTriggerDigis.clear();
  thePreTriggerBXs.clear();
  CLCTContainer_.clear();
  CLCTContainer_.resize(CSCConstants::MAX_CLCT_TBINS);
}

std::vector<CSCCLCTDigi> CSCCathodeLCTProcessor::run(const CSCComparatorDigiCollection* compdc) {
  // This is the version of the run() function that is called when running
  // over the entire detector.  It gets the comparator & timing info from the
  // comparator digis and then passes them on to another run() function.

  static std::atomic<bool> config_dumped{false};
  if ((infoV > 0) && !config_dumped) {
    dumpConfigParams();
    config_dumped = true;
  }

  // Get the number of strips and stagger of layers for the given chamber.
  // Do it only once per chamber.
  if (numStrips <= 0 or numStrips > CSCConstants::MAX_NUM_STRIPS_7CFEBS) {
    if (cscChamber_) {
      numStrips = cscChamber_->layer(1)->geometry()->numberOfStrips();
      // ME1/a is known to the readout hardware as strips 65-80 of ME1/1.
      // Still need to decide whether we do any special adjustments to
      // reconstruct LCTs in this region (3:1 ganged strips); for now, we
      // simply allow for hits in ME1/a and apply standard reconstruction
      // to them.
      // For SLHC ME1/1 is set to have 4 CFEBs in ME1/b and 3 CFEBs in ME1/a
      if (isME11_) {
        if (theRing == 4) {
          edm::LogError("CSCCathodeLCTProcessor|SetupError")
              << "+++ Invalid ring number for this processor " << theRing << " was set in the config."
              << " +++\n"
              << "+++ CSC geometry looks garbled; no emulation possible +++\n";
        }
        if (!disableME1a_ && theRing == 1 && !gangedME1a_)
          numStrips = CSCConstants::MAX_NUM_STRIPS_7CFEBS;
        if (!disableME1a_ && theRing == 1 && gangedME1a_)
          numStrips = CSCConstants::MAX_NUM_STRIPS;
        if (disableME1a_ && theRing == 1)
          numStrips = CSCConstants::MAX_NUM_STRIPS_ME1B;
      }

      if (numStrips > CSCConstants::MAX_NUM_STRIPS_7CFEBS) {
        edm::LogError("CSCCathodeLCTProcessor|SetupError")
            << "+++ Number of strips, " << numStrips << " found in " << theCSCName_ << " (sector " << theSector
            << " subsector " << theSubsector << " trig id. " << theTrigChamber << ")"
            << " exceeds max expected, " << CSCConstants::MAX_NUM_STRIPS_7CFEBS << " +++\n"
            << "+++ CSC geometry looks garbled; no emulation possible +++\n";
        numStrips = -1;
      }
      // The strips for a given layer may be offset from the adjacent layers.
      // This was done in order to improve resolution.  We need to find the
      // 'staggering' for each layer and make necessary conversions in our
      // arrays.  -JM
      // In the TMB-07 firmware, half-strips in odd layers (layers are
      // counted as ly0-ly5) are shifted by -1 half-strip, whereas in
      // the previous firmware versions half-strips in even layers
      // were shifted by +1 half-strip.  This difference is due to a
      // change from ly3 to ly2 in the choice of the key layer, and
      // the intention to keep half-strips in the key layer unchanged.
      // In the emulator, we use the old way for both cases, to avoid
      // negative half-strip numbers.  This will necessitate a
      // subtraction of 1 half-strip for TMB-07 later on. -SV.
      for (int i_layer = 0; i_layer < CSCConstants::NUM_LAYERS; i_layer++) {
        stagger[i_layer] = (cscChamber_->layer(i_layer + 1)->geometry()->stagger() + 1) / 2;
      }
    } else {
      edm::LogError("CSCCathodeLCTProcessor|ConfigError")
          << " " << theCSCName_ << " (sector " << theSector << " subsector " << theSubsector << " trig id. "
          << theTrigChamber << ")"
          << " is not defined in current geometry! +++\n"
          << "+++ CSC geometry looks garbled; no emulation possible +++\n";
      numStrips = -1;
    }
  }

  if (numStrips <= 0 or 2 * (unsigned)numStrips > csctp::get_csc_max_halfstrip(theStation, theRing)) {
    edm::LogError("CSCCathodeLCTProcessor|ConfigError")
        << " " << theCSCName_ << " (sector " << theSector << " subsector " << theSubsector << " trig id. "
        << theTrigChamber << "):"
        << " numStrips = " << numStrips << "; CLCT emulation skipped! +++";
    std::vector<CSCCLCTDigi> emptyV;
    return emptyV;
  }

  // Get comparator digis in this chamber.
  bool noDigis = getDigis(compdc);

  if (!noDigis) {
    // Get halfstrip times from comparator digis.
    std::vector<int> halfstrip[CSCConstants::NUM_LAYERS][CSCConstants::NUM_HALF_STRIPS_7CFEBS];
    readComparatorDigis(halfstrip);

    // Pass arrays of halfstrips on to another run() doing the
    // LCT search.
    // If the number of layers containing digis is smaller than that
    // required to trigger, quit right away.  (If LCT-based digi suppression
    // is implemented one day, this condition will have to be changed
    // to the number of planes required to pre-trigger.)
    unsigned int layersHit = 0;
    for (int i_layer = 0; i_layer < CSCConstants::NUM_LAYERS; i_layer++) {
      for (int i_hstrip = 0; i_hstrip < CSCConstants::NUM_HALF_STRIPS_7CFEBS; i_hstrip++) {
        if (!halfstrip[i_layer][i_hstrip].empty()) {
          layersHit++;
          break;
        }
      }
    }
    // Run the algorithm only if the probability for the pre-trigger
    // to fire is not null.  (Pre-trigger decisions are used for the
    // strip read-out conditions in DigiToRaw.)
    if (layersHit >= nplanes_hit_pretrig)
      run(halfstrip);
  }

  // Return vector of CLCTs.
  std::vector<CSCCLCTDigi> tmpV = getCLCTs();

  // shift the BX from 7 to 8
  // the unpacked real data CLCTs have central BX at bin 7
  // however in simulation the central BX  is bin 8
  // to make a proper comparison with ALCTs we need
  // CLCT and ALCT to have the central BX in the same bin
  // this shift does not affect the readout of the CLCTs
  // emulated CLCTs put in the event should be centered at bin 7 (as in data)
  for (auto& p : tmpV) {
    p.setBX(p.getBX() + alctClctOffset_);
  }

  return tmpV;
}

void CSCCathodeLCTProcessor::run(
    const std::vector<int> halfstrip[CSCConstants::NUM_LAYERS][CSCConstants::NUM_HALF_STRIPS_7CFEBS]) {
  // This version of the run() function can either be called in a standalone
  // test, being passed the halfstrip times, or called by the
  // run() function above.  It uses the findLCTs() method to find vectors
  // of LCT candidates. These candidates are sorted and the best two per bx
  // are returned.
  std::vector<CSCCLCTDigi> CLCTlist = findLCTs(halfstrip);

  // LCT sorting.
  if (CLCTlist.size() > 1)
    sort(CLCTlist.begin(), CLCTlist.end(), std::greater<CSCCLCTDigi>());

  for (const auto& p : CLCTlist) {
    // only consider valid CLCTs
    if (!p.isValid())
      continue;

    const int bx = p.getBX();
    if (bx >= CSCConstants::MAX_CLCT_TBINS) {
      if (infoV > 0)
        edm::LogWarning("L1CSCTPEmulatorOutOfTimeCLCT")
            << "+++ Bx of CLCT candidate, " << bx << ", exceeds max allowed, " << CSCConstants::MAX_CLCT_TBINS - 1
            << "; skipping it... +++\n";
      continue;
    }

    CLCTContainer_.at(bx).push_back(p);
  }

  for (int bx = 0; bx < CSCConstants::MAX_CLCT_TBINS; bx++) {
    for (unsigned iCLCT = 0; iCLCT < CLCTContainer_.at(bx).size(); iCLCT++) {
      // only consider valid CLCTs
      if (CLCTContainer_.at(bx).at(iCLCT).isValid()) {
        CLCTContainer_.at(bx).at(iCLCT).setTrknmb(iCLCT + 1);

        // check if the LCT is valid
        checkValid(CLCTContainer_.at(bx).at(iCLCT));

        if (infoV > 0) {
          LogDebug("CSCCathodeLCTProcessor")
              << CLCTContainer_.at(bx).at(iCLCT) << " found in " << theCSCName_ << " (sector " << theSector
              << " subsector " << theSubsector << " trig id. " << theTrigChamber << ")"
              << "\n";
        }
      }
    }
  }
  // Now that we have our best CLCTs, they get correlated with the best
  // ALCTs and then get sent to the MotherBoard.  -JM
}

bool CSCCathodeLCTProcessor::getDigis(const CSCComparatorDigiCollection* compdc) {
  bool noDigis = true;

  // Loop over layers and save comparator digis on each one into digiV[layer].
  for (int i_layer = 0; i_layer < CSCConstants::NUM_LAYERS; i_layer++) {
    digiV[i_layer].clear();

    CSCDetId detid(theEndcap, theStation, theRing, theChamber, i_layer + 1);
    getDigis(compdc, detid);

    if (isME11_ && !disableME1a_) {
      CSCDetId detid_me1a(theEndcap, theStation, 4, theChamber, i_layer + 1);
      getDigis(compdc, detid_me1a);
    }

    if (!digiV[i_layer].empty()) {
      noDigis = false;
      if (infoV > 1) {
        LogTrace("CSCCathodeLCTProcessor") << "found " << digiV[i_layer].size() << " comparator digi(s) in layer "
                                           << i_layer << " of " << detid.chamberName() << " (trig. sector " << theSector
                                           << " subsector " << theSubsector << " id " << theTrigChamber << ")";
      }
    }
  }

  return noDigis;
}

void CSCCathodeLCTProcessor::getDigis(const CSCComparatorDigiCollection* compdc, const CSCDetId& id) {
  const bool me1a = (id.station() == 1) && (id.ring() == 4);
  const CSCComparatorDigiCollection::Range rcompd = compdc->get(id);
  for (CSCComparatorDigiCollection::const_iterator digiIt = rcompd.first; digiIt != rcompd.second; ++digiIt) {
    const unsigned int origStrip = digiIt->getStrip();
    const unsigned int maxStripsME1a =
        gangedME1a_ ? CSCConstants::MAX_NUM_STRIPS_ME1A_GANGED : CSCConstants::MAX_NUM_STRIPS_ME1A_UNGANGED;
    // this special case can only be reached in MC
    // in real data, the comparator digis have always ring==1
    if (me1a && origStrip <= maxStripsME1a && !disableME1a_) {
      // Move ME1/A comparators from CFEB=0 to CFEB=4 if this has not
      // been done already.
      CSCComparatorDigi digi_corr(
          origStrip + CSCConstants::MAX_NUM_STRIPS_ME1B, digiIt->getComparator(), digiIt->getTimeBinWord());
      digiV[id.layer() - 1].push_back(digi_corr);
    } else {
      digiV[id.layer() - 1].push_back(*digiIt);
    }
  }
}

void CSCCathodeLCTProcessor::readComparatorDigis(
    std::vector<int> halfstrip[CSCConstants::NUM_LAYERS][CSCConstants::NUM_HALF_STRIPS_7CFEBS]) {
  for (int i_layer = 0; i_layer < CSCConstants::NUM_LAYERS; i_layer++) {
    int i_digi = 0;  // digi counter, for dumps.
    for (std::vector<CSCComparatorDigi>::iterator pld = digiV[i_layer].begin(); pld != digiV[i_layer].end();
         pld++, i_digi++) {
      // Dump raw digi info.
      if (infoV > 1) {
        std::ostringstream strstrm;
        strstrm << "Comparator digi: comparator = " << pld->getComparator() << " strip #" << pld->getStrip()
                << " time bins on:";
        std::vector<int> bx_times = pld->getTimeBinsOn();
        for (unsigned int tbin = 0; tbin < bx_times.size(); tbin++)
          strstrm << " " << bx_times[tbin];
        LogTrace("CSCCathodeLCTProcessor") << strstrm.str();
      }

      // Get comparator: 0/1 for left/right halfstrip for each comparator
      // that fired.
      int thisComparator = pld->getComparator();
      if (thisComparator != 0 && thisComparator != 1) {
        if (infoV >= 0)
          edm::LogWarning("L1CSCTPEmulatorWrongInput")
              << "+++ station " << theStation << " ring " << theRing << " chamber " << theChamber
              << " Found comparator digi with wrong comparator value = " << thisComparator << "; skipping it... +++\n";
        continue;
      }

      // Get strip number.
      int thisStrip = pld->getStrip() - 1;  // count from 0
      if (thisStrip < 0 || thisStrip >= numStrips) {
        if (infoV >= 0)
          edm::LogWarning("L1CSCTPEmulatorWrongInput")
              << "+++ station " << theStation << " ring " << theRing << " chamber " << theChamber
              << " Found comparator digi with wrong strip number = " << thisStrip << " (max strips = " << numStrips
              << "); skipping it... +++\n";
        continue;
      }
      // 2*strip: convert strip to 1/2 strip
      // comp   : comparator output
      // stagger: stagger for this layer
      int thisHalfstrip = 2 * thisStrip + thisComparator + stagger[i_layer];
      if (thisHalfstrip >= 2 * numStrips + 1) {
        if (infoV >= 0)
          edm::LogWarning("L1CSCTPEmulatorWrongInput")
              << "+++ station " << theStation << " ring " << theRing << " chamber " << theChamber
              << " Found wrong halfstrip number = " << thisHalfstrip << "; skipping this digi... +++\n";
        continue;
      }

      // Get bx times on this digi and check that they are within the bounds.
      std::vector<int> bx_times = pld->getTimeBinsOn();
      for (unsigned int i = 0; i < bx_times.size(); i++) {
        // Total number of time bins in DAQ readout is given by fifo_tbins,
        // which thus determines the maximum length of time interval.
        //
        // In data, only the CLCT in the time bin that was matched with L1A are read out
        // while comparator digi is read out by 12 time bin, which includes 12 time bin info
        // in other word, CLCTs emulated from comparator digis usually showed the OTMB behavior in 12 time bin
        // while CLCT from data only showed 1 time bin OTMB behavior
        // the CLCT emulated from comparator digis usually is centering at time bin 7 (BX7) and
        // it is definitly safe to ignore any CLCTs in bx 0 or 1 and those CLCTs will never impacts on any triggers
        if (bx_times[i] > 1 && bx_times[i] < static_cast<int>(fifo_tbins)) {
          if (i == 0 || (i > 0 && bx_times[i] - bx_times[i - 1] >= static_cast<int>(hit_persist))) {
            // A later hit on the same strip is ignored during the
            // number of clocks defined by the "hit_persist" parameter
            // (i.e., 6 bx's by default).
            if (infoV > 1)
              LogTrace("CSCCathodeLCTProcessor")
                  << "Comp digi: layer " << i_layer + 1 << " digi #" << i_digi + 1 << " strip " << thisStrip
                  << " halfstrip " << thisHalfstrip << " time " << bx_times[i] << " comparator " << thisComparator
                  << " stagger " << stagger[i_layer];
            halfstrip[i_layer][thisHalfstrip].push_back(bx_times[i]);
          } else if (i > 0) {
            if (infoV > 1)
              LogTrace("CSCCathodeLCTProcessor")
                  << "+++ station " << theStation << " ring " << theRing << " chamber " << theChamber
                  << " Skipping comparator digi: strip = " << thisStrip << ", layer = " << i_layer + 1
                  << ", bx = " << bx_times[i] << ", bx of previous hit = " << bx_times[i - 1];
          }
        } else {
          if (infoV > 1)
            LogTrace("CSCCathodeLCTProcessor") << "+++ station " << theStation << " ring " << theRing << " chamber "
                                               << theChamber << "+++ Skipping comparator digi: strip = " << thisStrip
                                               << ", layer = " << i_layer + 1 << ", bx = " << bx_times[i] << " +++";
        }
      }
    }
  }
}

// TMB-07 version.
std::vector<CSCCLCTDigi> CSCCathodeLCTProcessor::findLCTs(
    const std::vector<int> halfstrip[CSCConstants::NUM_LAYERS][CSCConstants::NUM_HALF_STRIPS_7CFEBS]) {
  std::vector<CSCCLCTDigi> lctList;

  // Max. number of half-strips for this chamber.
  const int maxHalfStrips = 2 * numStrips + 1;

  if (infoV > 1)
    dumpDigis(halfstrip, maxHalfStrips);

  // 2 possible LCTs per CSC x 7 LCT quantities
  int keystrip_data[CSCConstants::MAX_CLCTS_PER_PROCESSOR][CLCT_NUM_QUANTITIES] = {{0}};
  PulseArray pulse;

  // Fire half-strip one-shots for hit_persist bx's (4 bx's by default).
  pulseExtension(halfstrip, maxHalfStrips, pulse);

  unsigned int start_bx = start_bx_shift;
  // Stop drift_delay bx's short of fifo_tbins since at later bx's we will
  // not have a full set of hits to start pattern search anyway.
  unsigned int stop_bx = fifo_tbins - drift_delay;
  // Allow for more than one pass over the hits in the time window.
  while (start_bx < stop_bx) {
    // All half-strip pattern envelopes are evaluated simultaneously, on every
    // clock cycle.
    int first_bx = 999;
    bool pre_trig = preTrigger(pulse, start_bx, first_bx);

    // If any of half-strip envelopes has enough layers hit in it, TMB
    // will pre-trigger.
    if (pre_trig) {
      thePreTriggerBXs.push_back(first_bx);
      if (infoV > 1)
        LogTrace("CSCCathodeLCTProcessor") << "..... pretrigger at bx = " << first_bx << "; waiting drift delay .....";

      // TMB latches LCTs drift_delay clocks after pretrigger.
      // in the configuration the drift_delay is set to 2bx by default
      // this is the time that is required for the electrons to drift to the
      // cathode strips. 15ns drift time --> 45 ns is 3 sigma for the delay
      // this corresponds to 2bx
      int latch_bx = first_bx + drift_delay;

      // define a new pattern map
      // for each key half strip, and for each pattern, store the 2D collection of fired comparator digis
      std::map<int, std::map<int, CSCCLCTDigi::ComparatorContainer>> hits_in_patterns;
      hits_in_patterns.clear();

      // We check if there is at least one key half strip for which at least
      // one pattern id has at least the minimum number of hits
      bool hits_in_time = patternFinding(pulse, maxHalfStrips, latch_bx, hits_in_patterns);
      if (infoV > 1) {
        if (hits_in_time) {
          for (int hstrip = stagger[CSCConstants::KEY_CLCT_LAYER - 1]; hstrip < maxHalfStrips; hstrip++) {
            if (nhits[hstrip] > 0) {
              LogTrace("CSCCathodeLCTProcessor")
                  << " bx = " << std::setw(2) << latch_bx << " --->"
                  << " halfstrip = " << std::setw(3) << hstrip << " best pid = " << std::setw(2) << best_pid[hstrip]
                  << " nhits = " << nhits[hstrip];
            }
          }
        }
      }
      // This trigger emulator does not have an active CFEB flag for DAQ (csc trigger hardware: AFF)
      // This is a fundamental difference with the firmware where the TMB prepares the DAQ to
      // read out the chamber

      // The pattern finder runs continuously, so another pre-trigger
      // could occur already at the next bx.

      // Quality for sorting.
      int quality[CSCConstants::NUM_HALF_STRIPS_7CFEBS];
      int best_halfstrip[CSCConstants::MAX_CLCTS_PER_PROCESSOR];
      int best_quality[CSCConstants::MAX_CLCTS_PER_PROCESSOR];

      for (int ilct = 0; ilct < CSCConstants::MAX_CLCTS_PER_PROCESSOR; ilct++) {
        best_halfstrip[ilct] = -1;
        best_quality[ilct] = 0;
      }

      // Calculate quality from pattern id and number of hits, and
      // simultaneously select best-quality LCT.
      if (hits_in_time) {
        for (int hstrip = stagger[CSCConstants::KEY_CLCT_LAYER - 1]; hstrip < maxHalfStrips; hstrip++) {
          // The bend-direction bit pid[0] is ignored (left and right
          // bends have equal quality).
          quality[hstrip] = (best_pid[hstrip] & 14) | (nhits[hstrip] << 5);
          if (quality[hstrip] > best_quality[0]) {
            best_halfstrip[0] = hstrip;
            best_quality[0] = quality[hstrip];
          }
          if (infoV > 1 && quality[hstrip] > 0) {
            LogTrace("CSCCathodeLCTProcessor")
                << " 1st CLCT: halfstrip = " << std::setw(3) << hstrip << " quality = " << std::setw(3)
                << quality[hstrip] << " nhits = " << std::setw(3) << nhits[hstrip] << " pid = " << std::setw(3)
                << best_pid[hstrip] << " best halfstrip = " << std::setw(3) << best_halfstrip[0]
                << " best quality = " << std::setw(3) << best_quality[0];
          }
        }
      }

      // If 1st best CLCT is found, look for other CLCTs!
      if (best_halfstrip[0] >= 0) {
        for (int ilct = 1; ilct < CSCConstants::MAX_CLCTS_PER_PROCESSOR; ilct++) {
          // Mark keys near best CLCT as busy by setting their quality to zero, and repeat the search.
          markBusyKeys(best_halfstrip[ilct - 1], best_pid[best_halfstrip[ilct - 1]], quality);

          for (int hstrip = stagger[CSCConstants::KEY_CLCT_LAYER - 1]; hstrip < maxHalfStrips; hstrip++) {
            if (quality[hstrip] > best_quality[ilct]) {
              best_halfstrip[ilct] = hstrip;
              best_quality[ilct] = quality[hstrip];
            }
            if (infoV > 1 && quality[hstrip] > 0) {
              LogTrace("CSCCathodeLCTProcessor")
                  << "CLCT " << ilct + 1 << ": halfstrip = " << std::setw(3) << hstrip << " quality = " << std::setw(3)
                  << quality[hstrip] << " nhits = " << std::setw(3) << nhits[hstrip] << " pid = " << std::setw(3)
                  << best_pid[hstrip] << " best halfstrip = " << std::setw(3) << best_halfstrip[1]
                  << " best quality = " << std::setw(3) << best_quality[1];
            }
          }
        }

        // Pattern finder.
        for (int ilct = 0; ilct < CSCConstants::MAX_CLCTS_PER_PROCESSOR; ilct++) {
          int best_hs = best_halfstrip[ilct];
          if (best_hs >= 0 && nhits[best_hs] >= nplanes_hit_pattern) {
            keystrip_data[ilct][CLCT_PATTERN] = best_pid[best_hs];
            keystrip_data[ilct][CLCT_BEND] =
                clct_pattern_[best_pid[best_hs]][CSCConstants::NUM_LAYERS - 1][CSCConstants::CLCT_PATTERN_WIDTH];
            // Remove stagger if any.
            keystrip_data[ilct][CLCT_STRIP] = best_hs - stagger[CSCConstants::KEY_CLCT_LAYER - 1];
            keystrip_data[ilct][CLCT_BX] = first_bx;
            keystrip_data[ilct][CLCT_STRIP_TYPE] = 1;  // obsolete
            keystrip_data[ilct][CLCT_QUALITY] = nhits[best_hs];
            keystrip_data[ilct][CLCT_CFEB] = keystrip_data[ilct][CLCT_STRIP] / CSCConstants::NUM_HALF_STRIPS_PER_CFEB;
            const uint16_t halfstrip_in_cfeb = keystrip_data[ilct][CLCT_STRIP] -
                                               CSCConstants::NUM_HALF_STRIPS_PER_CFEB * keystrip_data[ilct][CLCT_CFEB];

            CSCCLCTDigi thisLCT(1,
                                keystrip_data[ilct][CLCT_QUALITY],
                                keystrip_data[ilct][CLCT_PATTERN],
                                keystrip_data[ilct][CLCT_STRIP_TYPE],
                                keystrip_data[ilct][CLCT_BEND],
                                halfstrip_in_cfeb,
                                keystrip_data[ilct][CLCT_CFEB],
                                keystrip_data[ilct][CLCT_BX]);

            // get the comparator hits for this pattern
            auto compHits = hits_in_patterns[best_hs][keystrip_data[ilct][CLCT_PATTERN]];

            // purge the comparator digi collection from the obsolete "65535" entries...
            cleanComparatorContainer(compHits);

            // set the hit collection
            thisLCT.setHits(compHits);

            // useful debugging
            if (infoV > 1) {
              LogTrace("CSCCathodeLCTProcessor") << " Final selection: ilct " << ilct << " " << thisLCT << std::endl;
            }

            // put the CLCT into the collection
            lctList.push_back(thisLCT);
          }
        }
      }  //find CLCT, end of best_halfstrip[0] >= 0

      // If there is a trigger, CLCT pre-trigger state machine
      // checks the number of hits that lie within a pattern template
      // at every bx, and waits for it to drop below threshold.
      // The search for CLCTs resumes only when the number of hits
      // drops below threshold.
      start_bx = fifo_tbins;
      // Stop checking drift_delay bx's short of fifo_tbins since
      // at later bx's we won't have a full set of hits for a
      // pattern search anyway.
      unsigned int stop_time = fifo_tbins - drift_delay;
      for (unsigned int bx = latch_bx + 1; bx < stop_time; bx++) {
        bool return_to_idle = true;
        bool hits_in_time = patternFinding(pulse, maxHalfStrips, bx, hits_in_patterns);
        if (hits_in_time) {
          for (int hstrip = stagger[CSCConstants::KEY_CLCT_LAYER - 1]; hstrip < maxHalfStrips; hstrip++) {
            // the dead-time is done at the pre-trigger, not at the trigger
            if (nhits[hstrip] >= nplanes_hit_pretrig) {
              if (infoV > 1)
                LogTrace("CSCCathodeLCTProcessor") << " State machine busy at bx = " << bx;
              return_to_idle = false;
              break;
            }
          }
        }
        if (return_to_idle) {
          if (infoV > 1)
            LogTrace("CSCCathodeLCTProcessor") << " State machine returns to idle state at bx = " << bx;
          start_bx = bx;
          break;
        }
      }
    }  //pre_trig
    else {
      start_bx = first_bx + 1;  // no dead time
    }
  }

  return lctList;
}  // findLCTs -- TMB-07 version.

// Common to all versions.
void CSCCathodeLCTProcessor::pulseExtension(
    const std::vector<int> time[CSCConstants::NUM_LAYERS][CSCConstants::NUM_HALF_STRIPS_7CFEBS],
    const int nStrips,
    PulseArray pulse) {
  static const unsigned int bits_in_pulse = 8 * sizeof(pulse[0][0]);

  // Clear pulse array.  This array will be used as a bit representation of
  // hit times.  For example: if strip[1][2] has a value of 3, then 1 shifted
  // left 3 will be bit pattern of pulse[1][2].  This would make the pattern
  // look like 0000000000001000.  Then add on additional bits to signify
  // the duration of a signal (hit_persist, formerly bx_width) to simulate
  // the TMB's drift delay.  So for the same pulse[1][2] with a hit_persist
  // of 3 would look like 0000000000111000.  This is similating the digital
  // one-shot in the TMB.
  for (int i_layer = 0; i_layer < CSCConstants::NUM_LAYERS; i_layer++)
    for (int i_strip = 0; i_strip < nStrips; i_strip++)
      pulse[i_layer][i_strip] = 0;

  // Loop over all layers and halfstrips.
  for (int i_layer = 0; i_layer < CSCConstants::NUM_LAYERS; i_layer++) {
    for (int i_strip = 0; i_strip < nStrips; i_strip++) {
      // If there is a hit, simulate digital one-shot persistence starting
      // in the bx of the initial hit.  Fill this into pulse[][].
      if (!time[i_layer][i_strip].empty()) {
        std::vector<int> bx_times = time[i_layer][i_strip];
        for (unsigned int i = 0; i < bx_times.size(); i++) {
          // Check that min and max times are within the allowed range.
          if (bx_times[i] < 0 || bx_times[i] + hit_persist >= bits_in_pulse) {
            if (infoV > 0)
              edm::LogWarning("L1CSCTPEmulatorOutOfTimeDigi")
                  << "+++ BX time of comparator digi (halfstrip = " << i_strip << " layer = " << i_layer
                  << ") bx = " << bx_times[i] << " is not within the range (0-" << bits_in_pulse
                  << "] allowed for pulse extension.  Skip this digi! +++\n";
            continue;
          }
          if (bx_times[i] >= start_bx_shift) {
            for (unsigned int bx = bx_times[i]; bx < bx_times[i] + hit_persist; ++bx)
              pulse[i_layer][i_strip] = pulse[i_layer][i_strip] | (1 << bx);
          }
        }
      }
    }
  }
}  // pulseExtension.

// TMB-07 version.
bool CSCCathodeLCTProcessor::preTrigger(const PulseArray pulse, const int start_bx, int& first_bx) {
  if (infoV > 1)
    LogTrace("CSCCathodeLCTProcessor") << "....................PreTrigger...........................";

  // Max. number of half-strips for this chamber.
  const int nStrips = 2 * numStrips + 1;

  int nPreTriggers = 0;

  bool pre_trig = false;
  // Now do a loop over bx times to see (if/when) track goes over threshold
  for (unsigned int bx_time = start_bx; bx_time < fifo_tbins; bx_time++) {
    // For any given bunch-crossing, start at the lowest keystrip and look for
    // the number of separate layers in the pattern for that keystrip that have
    // pulses at that bunch-crossing time.  Do the same for the next keystrip,
    // etc.  Then do the entire process again for the next bunch-crossing, etc
    // until you find a pre-trigger.

    std::map<int, std::map<int, CSCCLCTDigi::ComparatorContainer>> hits_in_patterns;
    hits_in_patterns.clear();

    bool hits_in_time = patternFinding(pulse, nStrips, bx_time, hits_in_patterns);
    if (hits_in_time) {
      for (int hstrip = stagger[CSCConstants::KEY_CLCT_LAYER - 1]; hstrip < nStrips; hstrip++) {
        if (infoV > 1) {
          if (nhits[hstrip] > 0) {
            LogTrace("CSCCathodeLCTProcessor")
                << " bx = " << std::setw(2) << bx_time << " --->"
                << " halfstrip = " << std::setw(3) << hstrip << " best pid = " << std::setw(2) << best_pid[hstrip]
                << " nhits = " << nhits[hstrip];
          }
        }
        ispretrig[hstrip] = false;
        if (nhits[hstrip] >= nplanes_hit_pretrig && best_pid[hstrip] >= pid_thresh_pretrig) {
          pre_trig = true;
          ispretrig[hstrip] = true;

          // write each pre-trigger to output
          nPreTriggers++;
          const int bend =
              clct_pattern_[best_pid[hstrip]][CSCConstants::NUM_LAYERS - 1][CSCConstants::CLCT_PATTERN_WIDTH];
          const int halfstrip = hstrip % CSCConstants::NUM_HALF_STRIPS_PER_CFEB;
          const int cfeb = hstrip / CSCConstants::NUM_HALF_STRIPS_PER_CFEB;
          thePreTriggerDigis.push_back(CSCCLCTPreTriggerDigi(
              1, nhits[hstrip], best_pid[hstrip], 1, bend, halfstrip, cfeb, bx_time, nPreTriggers, 0));
        }
      }

      if (pre_trig) {
        first_bx = bx_time;  // bx at time of pretrigger
        return true;
      }
    }
  }  // end loop over bx times

  if (infoV > 1)
    LogTrace("CSCCathodeLCTProcessor") << "no pretrigger, returning \n";
  first_bx = fifo_tbins;
  return false;
}  // preTrigger -- TMB-07 version.

// TMB-07 version.
bool CSCCathodeLCTProcessor::patternFinding(
    const PulseArray pulse,
    const int nStrips,
    const unsigned int bx_time,
    std::map<int, std::map<int, CSCCLCTDigi::ComparatorContainer>>& hits_in_patterns) {
  if (bx_time >= fifo_tbins)
    return false;

  // This loop is a quick check of a number of layers hit at bx_time: since
  // most of the time it is 0, this check helps to speed-up the execution
  // substantially.
  unsigned int layers_hit = 0;
  for (int i_layer = 0; i_layer < CSCConstants::NUM_LAYERS; i_layer++) {
    for (int i_hstrip = 0; i_hstrip < nStrips; i_hstrip++) {
      if (((pulse[i_layer][i_hstrip] >> bx_time) & 1) == 1) {
        layers_hit++;
        break;
      }
    }
  }
  if (layers_hit < nplanes_hit_pretrig)
    return false;

  for (int key_hstrip = 0; key_hstrip < nStrips; key_hstrip++) {
    best_pid[key_hstrip] = 0;
    nhits[key_hstrip] = 0;
    first_bx_corrected[key_hstrip] = -999;
  }

  bool hit_layer[CSCConstants::NUM_LAYERS];

  // Loop over candidate key strips.
  for (int key_hstrip = stagger[CSCConstants::KEY_CLCT_LAYER - 1]; key_hstrip < nStrips; key_hstrip++) {
    // Loop over patterns and look for hits matching each pattern.
    for (unsigned int pid = clct_pattern_.size() - 1; pid >= pid_thresh_pretrig; pid--) {
      layers_hit = 0;
      // clear all layers
      for (int ilayer = 0; ilayer < CSCConstants::NUM_LAYERS; ilayer++) {
        hit_layer[ilayer] = false;
      }

      // clear a single pattern!
      CSCCLCTDigi::ComparatorContainer hits_single_pattern;
      hits_single_pattern.resize(6);
      for (auto& p : hits_single_pattern) {
        p.resize(CSCConstants::CLCT_PATTERN_WIDTH, CSCCathodeLCTProcessor::INVALID_HALFSTRIP);
      }

      // clear all medians
      double num_pattern_hits = 0., times_sum = 0.;
      std::multiset<int> mset_for_median;
      mset_for_median.clear();

      // Loop over halfstrips in trigger pattern mask and calculate the
      // "absolute" halfstrip number for each.
      for (int this_layer = 0; this_layer < CSCConstants::NUM_LAYERS; this_layer++) {
        for (int strip_num = 0; strip_num < CSCConstants::CLCT_PATTERN_WIDTH; strip_num++) {
          // ignore "0" half-strips in the pattern
          if (clct_pattern_[pid][this_layer][strip_num] == 0)
            continue;

          // the current strip is the key half-strip plus the offset (can be negative or positive)
          int this_strip = CSCPatternBank::clct_pattern_offset_[strip_num] + key_hstrip;

          // current strip should be valid of course
          if (this_strip >= 0 && this_strip < nStrips) {
            if (infoV > 3) {
              LogTrace("CSCCathodeLCTProcessor") << " In patternFinding: key_strip = " << key_hstrip << " pid = " << pid
                                                 << " layer = " << this_layer << " strip = " << this_strip << std::endl;
            }
            // Determine if "one shot" is high at this bx_time
            if (((pulse[this_layer][this_strip] >> bx_time) & 1) == 1) {
              if (hit_layer[this_layer] == false) {
                hit_layer[this_layer] = true;
                layers_hit++;  // determines number of layers hit
                // add this strip in this layer to the pattern we are currently considering
                hits_single_pattern[this_layer][strip_num] = this_strip;
              }

              // find at what bx did pulse on this halsfstrip & layer have started
              // use hit_persist constraint on how far back we can go
              int first_bx_layer = bx_time;
              for (unsigned int dbx = 0; dbx < hit_persist; dbx++) {
                if (((pulse[this_layer][this_strip] >> (first_bx_layer - 1)) & 1) == 1)
                  first_bx_layer--;
                else
                  break;
              }
              times_sum += (double)first_bx_layer;
              num_pattern_hits += 1.;
              mset_for_median.insert(first_bx_layer);
              if (infoV > 2)
                LogTrace("CSCCathodeLCTProcessor") << " 1st bx in layer: " << first_bx_layer << " sum bx: " << times_sum
                                                   << " #pat. hits: " << num_pattern_hits;
            }
          }
        }  // end loop over strips in pretrigger pattern
      }    // end loop over layers

      // save the pattern information when a trigger was formed!
      if (layers_hit >= nplanes_hit_pattern) {
        hits_in_patterns[key_hstrip][pid] = hits_single_pattern;
      }

      // determine the current best pattern!
      if (layers_hit > nhits[key_hstrip]) {
        best_pid[key_hstrip] = pid;
        nhits[key_hstrip] = layers_hit;

        // calculate median
        const int sz = mset_for_median.size();
        if (sz > 0) {
          std::multiset<int>::iterator im = mset_for_median.begin();
          if (sz > 1)
            std::advance(im, sz / 2 - 1);
          if (sz == 1)
            first_bx_corrected[key_hstrip] = *im;
          else if ((sz % 2) == 1)
            first_bx_corrected[key_hstrip] = *(++im);
          else
            first_bx_corrected[key_hstrip] = ((*im) + (*(++im))) / 2;

#if defined(EDM_ML_DEBUG)
          //LogTrace only ever prints if EDM_ML_DEBUG is defined
          if (infoV > 1) {
            auto lt = LogTrace("CSCCathodeLCTProcessor")
                      << "bx=" << bx_time << " bx_cor=" << first_bx_corrected[key_hstrip] << "  bxset=";
            for (im = mset_for_median.begin(); im != mset_for_median.end(); im++) {
              lt << " " << *im;
            }
          }
#endif
        }
        // Do not loop over the other (worse) patterns if max. numbers of
        // hits is found.
        if (nhits[key_hstrip] == CSCConstants::NUM_LAYERS)
          break;
      }
    }  // end loop over pid
  }    // end loop over candidate key strips

  // At this point there exists at least one halfstrip for which at least one pattern
  // has at least 3 layers --> definition of a pre-trigger
  return true;
}  // patternFinding -- TMB-07 version.

// TMB-07 version.
void CSCCathodeLCTProcessor::markBusyKeys(const int best_hstrip,
                                          const int best_patid,
                                          int quality[CSCConstants::NUM_HALF_STRIPS_7CFEBS]) {
  int nspan = min_separation;
  int pspan = min_separation;

  for (int hstrip = best_hstrip - nspan; hstrip <= best_hstrip + pspan; hstrip++) {
    if (hstrip >= 0 && hstrip < CSCConstants::NUM_HALF_STRIPS_7CFEBS) {
      quality[hstrip] = 0;
    }
  }
}  // markBusyKeys -- TMB-07 version.

void CSCCathodeLCTProcessor::cleanComparatorContainer(CSCCLCTDigi::ComparatorContainer& compHits) const {
  for (auto& p : compHits) {
    p.erase(std::remove_if(
                p.begin(), p.end(), [](unsigned i) -> bool { return i == CSCCathodeLCTProcessor::INVALID_HALFSTRIP; }),
            p.end());
  }
}

// --------------------------------------------------------------------------
// Auxiliary code.
// --------------------------------------------------------------------------
// Dump of configuration parameters.
void CSCCathodeLCTProcessor::dumpConfigParams() const {
  std::ostringstream strm;
  strm << "\n";
  strm << "++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n";
  strm << "+                  CLCT configuration parameters:                  +\n";
  strm << "++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n";
  strm << " fifo_tbins   [total number of time bins in DAQ readout] = " << fifo_tbins << "\n";
  strm << " fifo_pretrig [start time of cathode raw hits in DAQ readout] = " << fifo_pretrig << "\n";
  strm << " hit_persist  [duration of signal pulse, in 25 ns bins] = " << hit_persist << "\n";
  strm << " drift_delay  [time after pre-trigger before TMB latches LCTs] = " << drift_delay << "\n";
  strm << " nplanes_hit_pretrig [min. number of layers hit for pre-trigger] = " << nplanes_hit_pretrig << "\n";
  strm << " nplanes_hit_pattern [min. number of layers hit for trigger] = " << nplanes_hit_pattern << "\n";
  strm << " pid_thresh_pretrig [lower threshold on pattern id] = " << pid_thresh_pretrig << "\n";
  strm << " min_separation     [region of busy key strips] = " << min_separation << "\n";
  strm << "++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n";
  LogDebug("CSCCathodeLCTProcessor") << strm.str();
}

// Reasonably nice dump of digis on half-strips.
void CSCCathodeLCTProcessor::dumpDigis(
    const std::vector<int> strip[CSCConstants::NUM_LAYERS][CSCConstants::NUM_HALF_STRIPS_7CFEBS],
    const int nStrips) const {
  LogDebug("CSCCathodeLCTProcessor") << theCSCName_ << " strip type: half-strip,  nStrips " << nStrips;

  std::ostringstream strstrm;
  for (int i_strip = 0; i_strip < nStrips; i_strip++) {
    if (i_strip % 10 == 0) {
      if (i_strip < 100)
        strstrm << i_strip / 10;
      else
        strstrm << (i_strip - 100) / 10;
    } else
      strstrm << " ";
    if ((i_strip + 1) % CSCConstants::NUM_HALF_STRIPS_PER_CFEB == 0)
      strstrm << " ";
  }
  strstrm << "\n";
  for (int i_strip = 0; i_strip < nStrips; i_strip++) {
    strstrm << i_strip % 10;
    if ((i_strip + 1) % CSCConstants::NUM_HALF_STRIPS_PER_CFEB == 0)
      strstrm << " ";
  }
  for (int i_layer = 0; i_layer < CSCConstants::NUM_LAYERS; i_layer++) {
    strstrm << "\n";
    for (int i_strip = 0; i_strip < nStrips; i_strip++) {
      if (!strip[i_layer][i_strip].empty()) {
        std::vector<int> bx_times = strip[i_layer][i_strip];
        // Dump only the first in time.
        strstrm << std::hex << bx_times[0] << std::dec;
      } else {
        strstrm << "-";
      }
      if ((i_strip + 1) % CSCConstants::NUM_HALF_STRIPS_PER_CFEB == 0)
        strstrm << " ";
    }
  }
  LogTrace("CSCCathodeLCTProcessor") << strstrm.str();
}

// Check if the CLCT is valid
void CSCCathodeLCTProcessor::checkValid(const CSCCLCTDigi& clct, unsigned max_stubs) const {
  const unsigned max_strip = csctp::get_csc_max_halfstrip(theStation, theRing);
  const auto& [min_pattern, max_pattern] = csctp::get_csc_min_max_pattern(use_run3_patterns_);
  const auto& [min_cfeb, max_cfeb] = csctp::get_csc_min_max_cfeb(theStation, theRing);
  const unsigned max_quality = csctp::get_csc_clct_max_quality();
  unsigned errors = 0;

  // CLCT must be valid
  if (!clct.isValid()) {
    edm::LogError("CSCCathodeLCTProcessor") << "CSCLCTDigi with invalid bit set: " << clct.isValid();
    errors++;
  }

  // CLCT number is 1 or max
  if (clct.getTrknmb() < 1 or clct.getTrknmb() > max_stubs) {
    edm::LogError("CSCCathodeLCTProcessor")
        << "CSCLCTDigi with invalid track number: " << clct.getTrknmb() << "; allowed [1," << max_stubs << "]";
    errors++;
  }

  // CLCT quality must be valid
  // CLCTs require at least 4 layers hit
  // Run-3: ME1/1 CLCTs require only 3 layers
  // Run-4: ME2/1 CLCTs require only 3 layers
  if (clct.getQuality() < nplanes_hit_pattern or clct.getQuality() > max_quality) {
    edm::LogError("CSCCathodeLCTProcessor")
        << "CSCLCTDigi with invalid quality: " << clct.getQuality() << "; allowed [0," << max_quality << "]";
    ;
    errors++;
  }

  // CLCT half-strip must be within bounds
  if (clct.getStrip() >= CSCConstants::NUM_HALF_STRIPS_PER_CFEB) {
    edm::LogError("CSCCathodeLCTProcessor") << "CSCLCTDigi with invalid half-strip: " << clct.getStrip()
                                            << "; allowed [0, " << CSCConstants::NUM_HALF_STRIPS_PER_CFEB - 1 << "]";
    errors++;
  }

  // CLCT key half-strip must be within bounds
  if (clct.getKeyStrip() >= max_strip) {
    edm::LogError("CSCCathodeLCTProcessor")
        << "CSCLCTDigi with invalid key half-strip: " << clct.getKeyStrip() << "; allowed [0, " << max_strip - 1 << "]";
    errors++;
  }

  // CLCT with out-of-time BX
  if (clct.getBX() >= CSCConstants::MAX_CLCT_TBINS) {
    edm::LogError("CSCCathodeLCTProcessor") << "CSCLCTDigi with invalid BX: " << clct.getBX() << "; allowed [0, "
                                            << CSCConstants::MAX_CLCT_TBINS - 1 << "]";
    errors++;
  }

  // CLCT with neither left nor right bending
  if (clct.getBend() > 1) {
    edm::LogError("CSCCathodeLCTProcessor")
        << "CSCLCTDigi with invalid bending: " << clct.getBend() << "; allowed [0,1]";
    errors++;
  }

  // CLCT with an invalid pattern ID
  if (clct.getPattern() < min_pattern or clct.getPattern() > max_pattern) {
    edm::LogError("CSCCathodeLCTProcessor") << "CSCLCTDigi with invalid pattern ID: " << clct.getPattern()
                                            << "; allowed [" << min_pattern << ", " << max_pattern << "]";
    errors++;
  }

  // CLCT with an invalid CFEB ID
  if (clct.getCFEB() < min_cfeb or clct.getCFEB() > max_cfeb) {
    edm::LogError("CSCCathodeLCTProcessor") << "CSCLCTDigi with invalid CFEB ID: " << clct.getCFEB() << "; allowed ["
                                            << min_cfeb << ", " << max_cfeb << "]";
    errors++;
  }

  if (use_run3_patterns_) {
    // CLCT comparator code is invalid
    if (clct.getCompCode() < 0 or clct.getCompCode() >= std::pow(2, 12)) {
      edm::LogError("CSCCathodeLCTProcessor") << "CSCLCTDigi with invalid comparator code: " << clct.getCompCode()
                                              << "; allowed [0, " << std::pow(2, 12) - 1 << "]";
      errors++;
    }

    unsigned max_quartstrip = csctp::get_csc_max_quartstrip(theStation, theRing);
    unsigned max_eightstrip = csctp::get_csc_max_eightstrip(theStation, theRing);

    // CLCT key half-strip must be within bounds
    if (clct.getKeyStrip(4) >= max_quartstrip) {
      edm::LogError("CSCCathodeLCTProcessor") << "CSCLCTDigi with invalid key quart-strip: " << clct.getKeyStrip(4)
                                              << "; allowed [0, " << max_quartstrip - 1 << "]";
      errors++;
    }

    // CLCT key half-strip must be within bounds
    if (clct.getKeyStrip(8) >= max_eightstrip) {
      edm::LogError("CSCCathodeLCTProcessor") << "CSCLCTDigi with invalid key eight-strip: " << clct.getKeyStrip(8)
                                              << "; allowed [0, " << max_eightstrip - 1 << "]";
      errors++;
    }
  }

  if (errors > 0) {
    edm::LogError("CSCCathodeLCTProcessor") << "Faulty CLCT: " << cscId_ << " " << clct << "\n errors " << errors;
  }
}

// Returns vector of read-out CLCTs, if any.  Starts with the vector
// of all found CLCTs and selects the ones in the read-out time window.
std::vector<CSCCLCTDigi> CSCCathodeLCTProcessor::readoutCLCTs(int nMaxCLCTs) const {
  std::vector<CSCCLCTDigi> tmpV;

  // The start time of the L1A*CLCT coincidence window should be
  // related to the fifo_pretrig parameter, but I am not completely
  // sure how.  For now, just choose it such that the window is
  // centered at bx=7.  This may need further tweaking if the value of
  // tmb_l1a_window_size changes.

  // The number of CLCT bins in the read-out is given by the
  // tmb_l1a_window_size parameter, but made even by setting the LSB
  // of tmb_l1a_window_size to 0.
  //
  static std::atomic<int> lct_bins;
  lct_bins = (tmb_l1a_window_size % 2 == 0) ? tmb_l1a_window_size : tmb_l1a_window_size - 1;
  static std::atomic<int> late_tbins;
  late_tbins = early_tbins + lct_bins;

  static std::atomic<int> ifois{0};
  if (ifois == 0) {
    if (infoV >= 0 && early_tbins < 0) {
      edm::LogWarning("L1CSCTPEmulatorSuspiciousParameters")
          << "+++ early_tbins = " << early_tbins << "; in-time CLCTs are not getting read-out!!! +++"
          << "\n";
    }

    if (late_tbins > CSCConstants::MAX_CLCT_TBINS - 1) {
      if (infoV >= 0)
        edm::LogWarning("L1CSCTPEmulatorSuspiciousParameters")
            << "+++ Allowed range of time bins, [0-" << late_tbins << "] exceeds max allowed, "
            << CSCConstants::MAX_CLCT_TBINS - 1 << " +++\n"
            << "+++ Set late_tbins to max allowed +++\n";
      late_tbins = CSCConstants::MAX_CLCT_TBINS - 1;
    }
    ifois = 1;
  }

  // Start from the vector of all found CLCTs and select those within
  // the CLCT*L1A coincidence window.
  int bx_readout = -1;
  const std::vector<CSCCLCTDigi>& all_lcts = getCLCTs(nMaxCLCTs);
  for (const auto& p : all_lcts) {
    // only consider valid CLCTs
    if (!p.isValid())
      continue;

    const int bx = p.getBX();
    // Skip CLCTs found too early relative to L1Accept.
    if (bx <= early_tbins) {
      if (infoV > 1)
        LogDebug("CSCCathodeLCTProcessor")
            << " Do not report CLCT on key halfstrip " << p.getKeyStrip() << ": found at bx " << bx
            << ", whereas the earliest allowed bx is " << early_tbins + 1;
      continue;
    }

    // Skip CLCTs found too late relative to L1Accept.
    if (bx > late_tbins) {
      if (infoV > 1)
        LogDebug("CSCCathodeLCTProcessor")
            << " Do not report CLCT on key halfstrip " << p.getKeyStrip() << ": found at bx " << bx
            << ", whereas the latest allowed bx is " << late_tbins;
      continue;
    }

    // If (readout_earliest_2) take only CLCTs in the earliest bx in the read-out window:
    // in digi->raw step, LCTs have to be packed into the TMB header, and
    // currently there is room just for two.
    if (readout_earliest_2) {
      if (bx_readout == -1 || bx == bx_readout) {
        tmpV.push_back(p);
        if (bx_readout == -1)
          bx_readout = bx;
      }
    } else
      tmpV.push_back(p);
  }

  // remove the CLCTs with an index larger than nMaxCLCTs
  if (tmpV.size() > unsigned(nMaxCLCTs)) {
    tmpV.erase(tmpV.begin() + nMaxCLCTs, tmpV.end());
  }

  // do a final check on the CLCTs in readout
  for (const auto& clct : tmpV) {
    checkValid(clct, nMaxCLCTs);
  }

  return tmpV;
}

// Returns vector of read-out CLCTs, if any.  Starts with the vector
// of all found CLCTs and selects the ones in the read-out time window.
std::vector<CSCCLCTDigi> CSCCathodeLCTProcessor::readoutCLCTsME1a(int nMaxCLCTs) const {
  std::vector<CSCCLCTDigi> tmpV;
  if (not(theStation == 1 and (theRing == 1 or theRing == 4)))
    return tmpV;
  const std::vector<CSCCLCTDigi>& allCLCTs = readoutCLCTs(nMaxCLCTs);
  for (const auto& clct : allCLCTs)
    if (clct.getCFEB() >= 4)
      tmpV.push_back(clct);
  return tmpV;
}

// Returns vector of read-out CLCTs, if any.  Starts with the vector
// of all found CLCTs and selects the ones in the read-out time window.
std::vector<CSCCLCTDigi> CSCCathodeLCTProcessor::readoutCLCTsME1b(int nMaxCLCTs) const {
  std::vector<CSCCLCTDigi> tmpV;
  if (not(theStation == 1 and (theRing == 1 or theRing == 4)))
    return tmpV;
  const std::vector<CSCCLCTDigi>& allCLCTs = readoutCLCTs(nMaxCLCTs);
  for (const auto& clct : allCLCTs)
    if (clct.getCFEB() < 4)
      tmpV.push_back(clct);
  return tmpV;
}

std::vector<CSCCLCTPreTriggerDigi> CSCCathodeLCTProcessor::preTriggerDigisME1a() const {
  std::vector<CSCCLCTPreTriggerDigi> tmpV;
  if (not(theStation == 1 and (theRing == 1 or theRing == 4)))
    return tmpV;
  const std::vector<CSCCLCTPreTriggerDigi>& allPretriggerdigis = preTriggerDigis();
  for (const auto& preclct : allPretriggerdigis)
    if (preclct.getCFEB() >= 4)
      tmpV.push_back(preclct);
  return tmpV;
}

std::vector<CSCCLCTPreTriggerDigi> CSCCathodeLCTProcessor::preTriggerDigisME1b() const {
  std::vector<CSCCLCTPreTriggerDigi> tmpV;
  if (not(theStation == 1 and (theRing == 1 or theRing == 4)))
    return tmpV;
  const std::vector<CSCCLCTPreTriggerDigi>& allPretriggerdigis = preTriggerDigis();
  for (const auto& preclct : allPretriggerdigis)
    if (preclct.getCFEB() < 4)
      tmpV.push_back(preclct);
  return tmpV;
}

// Returns vector of all found CLCTs, if any.  Used for ALCT-CLCT matching.
std::vector<CSCCLCTDigi> CSCCathodeLCTProcessor::getCLCTs(unsigned nMaxCLCTs) const {
  std::vector<CSCCLCTDigi> tmpV;
  for (int bx = 0; bx < CSCConstants::MAX_CLCT_TBINS; bx++) {
    for (unsigned iCLCT = 0; iCLCT < CLCTContainer_.at(bx).size(); iCLCT++) {
      if (iCLCT < nMaxCLCTs and CLCTContainer_.at(bx).at(iCLCT).isValid()) {
        tmpV.push_back(CLCTContainer_.at(bx).at(iCLCT));
      }
    }
  }
  return tmpV;
}

// shift the BX from 7 to 8
// the unpacked real data CLCTs have central BX at bin 7
// however in simulation the central BX  is bin 8
// to make a proper comparison with ALCTs we need
// CLCT and ALCT to have the central BX in the same bin
CSCCLCTDigi CSCCathodeLCTProcessor::getBestCLCT(int bx) const {
  CSCCLCTDigi lct;
  // check that the container has at least one entry
  if (!CLCTContainer_.at(bx).empty()) {
    lct = CLCTContainer_.at(bx).at(0);
    lct.setBX(lct.getBX() + alctClctOffset_);
  }
  return lct;
}

CSCCLCTDigi CSCCathodeLCTProcessor::getSecondCLCT(int bx) const {
  CSCCLCTDigi lct;
  // check that the container has at least two entries
  if (CLCTContainer_.at(bx).size() >= 2) {
    lct = CLCTContainer_.at(bx).at(1);
    lct.setBX(lct.getBX() + alctClctOffset_);
  }
  return lct;
}
