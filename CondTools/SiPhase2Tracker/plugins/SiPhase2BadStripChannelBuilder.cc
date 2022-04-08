// system include files
#include <vector>
#include <memory>
#include <iostream>
#include <fstream>

// user include files
#include "CommonTools/ConditionDBWriter/interface/ConditionDBWriter.h"
#include "CondFormats/SiStripObjects/interface/SiStripBadStrip.h"
#include "DataFormats/SiStripDetId/interface/StripSubdetector.h"
#include "DataFormats/TrackerCommon/interface/TrackerTopology.h"
#include "DataFormats/Phase2TrackerDigi/interface/Phase2TrackerDigi.h"
#include "FWCore/MessageLogger/interface/MessageLogger.h"
#include "FWCore/ParameterSet/interface/ConfigurationDescriptions.h"
#include "FWCore/ParameterSet/interface/FileInPath.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/ParameterSet/interface/ParameterSetDescription.h"
#include "FWCore/ServiceRegistry/interface/Service.h"
#include "FWCore/Utilities/interface/Exception.h"
#include "FWCore/Utilities/interface/RandomNumberGenerator.h"
#include "Geometry/CommonDetUnit/interface/PixelGeomDetUnit.h"
#include "Geometry/CommonTopologies/interface/PixelTopology.h"
#include "Geometry/Records/interface/TrackerDigiGeometryRecord.h"
#include "Geometry/Records/interface/TrackerTopologyRcd.h"
#include "Geometry/TrackerGeometryBuilder/interface/TrackerGeometry.h"

#include "CLHEP/Random/RandFlat.h"
#include "CLHEP/Random/RandGauss.h"

/** 
 * enum to decide which algorithm use to populate the conditions
 */
namespace {
  enum badChannelAlgo { NAIVE = 1, RANDOM = 2, NONE = 99 };
}

class SiPhase2BadStripChannelBuilder : public ConditionDBWriter<SiStripBadStrip> {
public:
  explicit SiPhase2BadStripChannelBuilder(const edm::ParameterSet&);
  ~SiPhase2BadStripChannelBuilder() override = default;

  static void fillDescriptions(edm::ConfigurationDescriptions& descriptions);

private:
  std::unique_ptr<SiStripBadStrip> getNewObject() override;

  void algoBeginRun(const edm::Run& run, const edm::EventSetup& es) override {
    if (!tTopo_) {
      tTopo_ = std::make_unique<TrackerTopology>(es.getData(topoToken_));
      tGeom_ = std::make_unique<TrackerGeometry>(es.getData(geomToken_));
    }
  };

  void algoAnalyze(const edm::Event& event, const edm::EventSetup& es) override {
    edm::Service<edm::RandomNumberGenerator> rng;
    if (!engine_) {
      engine_ = &rng->getEngine(event.streamID());
    }
  }

  std::map<unsigned short, unsigned short> clusterizeBadChannels(
      const std::vector<Phase2TrackerDigi::PackedDigiType>& maskedChannels);

  // ----------member data ---------------------------
  std::unique_ptr<TrackerTopology> tTopo_;
  std::unique_ptr<TrackerGeometry> tGeom_;
  CLHEP::HepRandomEngine* engine_;

  const edm::ESGetToken<TrackerTopology, TrackerTopologyRcd> topoToken_;
  const edm::ESGetToken<TrackerGeometry, TrackerDigiGeometryRecord> geomToken_;
  const bool printdebug_;
  const unsigned int popConAlgo_;
  const float badComponentsFraction_;
  badChannelAlgo theBCAlgo_;
};

//__________________________________________________________________________________________________
SiPhase2BadStripChannelBuilder::SiPhase2BadStripChannelBuilder(const edm::ParameterSet& iConfig)
    : ConditionDBWriter<SiStripBadStrip>(iConfig),
      topoToken_(esConsumes<edm::Transition::BeginRun>()),
      geomToken_(esConsumes<edm::Transition::BeginRun>()),
      printdebug_(iConfig.getUntrackedParameter<bool>("printDebug", false)),
      popConAlgo_(iConfig.getParameter<unsigned int>("popConAlgo")),
      badComponentsFraction_(iConfig.getParameter<double>("badComponentsFraction")) {
  if (badComponentsFraction_ > 1. || badComponentsFraction_ < 0.) {
    throw cms::Exception("Inconsistent configuration")
        << "[SiPhase2BadStripChannelBuilder::c'tor] the requested fraction of bad components is unphysical. \n";
  }
  theBCAlgo_ = static_cast<badChannelAlgo>(popConAlgo_);
}

//__________________________________________________________________________________________________
std::unique_ptr<SiStripBadStrip> SiPhase2BadStripChannelBuilder::getNewObject() {
  using Phase2TrackerGeomDetUnit = PixelGeomDetUnit;
  edm::LogInfo("SiPhase2BadStripChannelBuilder") << "... creating dummy SiStripBadStrip Data" << std::endl;

  auto obj = std::make_unique<SiStripBadStrip>();

  // early return with nullptr if fraction is ==0.
  if (badComponentsFraction_ == 0.f) {
    return obj;
  }

  edm::LogInfo("SiPhase2BadStripChannelBuilder")
      << " There are " << tGeom_->detUnits().size() << " modules in this geometry." << std::endl;

  for (auto const& det_u : tGeom_->detUnits()) {
    const DetId detid = det_u->geographicalId();
    uint32_t rawId = detid.rawId();
    int subid = detid.subdetId();
    if (detid.det() == DetId::Detector::Tracker) {
      const Phase2TrackerGeomDetUnit* pixdet = dynamic_cast<const Phase2TrackerGeomDetUnit*>(det_u);
      assert(pixdet);
      LogDebug("SiPhase2BadStripChannelBuilder") << rawId << " is a " << subid << " det" << std::endl;
      if (subid == StripSubdetector::TOB || subid == StripSubdetector::TID) {
        if (tGeom_->getDetectorType(rawId) == TrackerGeometry::ModuleType::Ph2PSS ||
            tGeom_->getDetectorType(rawId) == TrackerGeometry::ModuleType::Ph2SS) {
          const PixelTopology& topol(pixdet->specificTopology());

          const int nrows = topol.nrows();
          const int ncols = topol.ncolumns();

          LogDebug("SiPhase2BadStripChannelBuilder")
              << "DetId: " << rawId << " subdet: " << subid << " nrows: " << nrows << " ncols: " << ncols << std::endl;

          std::vector<unsigned int> theSiStripVector;

          switch (theBCAlgo_) {
            case NAIVE: {
              LogDebug("SiPhase2BadStripChannelBuilder") << "using the NAIVE algorithm" << std::endl;

              unsigned short firstBadStrip = std::floor(CLHEP::RandFlat::shoot(engine_, 0, nrows));
              unsigned short NconsecutiveBadStrips = std::floor(CLHEP::RandFlat::shoot(engine_, 1, 10));

              // if the interval exceeds the end of the module
              if (firstBadStrip + NconsecutiveBadStrips > nrows) {
                NconsecutiveBadStrips = nrows - firstBadStrip;
              }

              unsigned int theBadStripRange;
              theBadStripRange = obj->encodePhase2(firstBadStrip, NconsecutiveBadStrips);

              if (printdebug_)
                edm::LogInfo("SiPhase2BadStripChannelBuilder")
                    << "detid " << rawId << " \t"
                    << " firstBadStrip " << firstBadStrip << "\t "
                    << " NconsecutiveBadStrips " << NconsecutiveBadStrips << "\t "
                    << " packed integer " << std::hex << theBadStripRange << std::dec << std::endl;

              theSiStripVector.push_back(theBadStripRange);
              break;
            }
            case RANDOM: {
              LogDebug("SiPhase2BadStripChannelBuilder") << "using the RANDOM algorithm" << std::endl;

              // auxilliary vector to check if the channels were already used
              std::vector<Phase2TrackerDigi::PackedDigiType> usedChannels;

              size_t nmaxBadStrips = std::floor(nrows * ncols * badComponentsFraction_);

              LogDebug("SiPhase2BadStripChannelBuilder")
                  << __FUNCTION__ << " " << __LINE__ << " will mask: " << nmaxBadStrips << " strips" << std::endl;

              while (usedChannels.size() < nmaxBadStrips) {
                unsigned short badStripRow = std::floor(CLHEP::RandFlat::shoot(engine_, 0, nrows));
                unsigned short badStripCol = std::floor(CLHEP::RandFlat::shoot(engine_, 0, ncols));
                const auto& badChannel = Phase2TrackerDigi::pixelToChannel(badStripRow, badStripCol);

                LogDebug("SiPhase2BadStripChannelBuilder")
                    << __FUNCTION__ << " " << __LINE__ << ": masking channel " << badChannel << " (" << badStripRow
                    << "," << badStripCol << ")" << std::endl;

                if (std::find(usedChannels.begin(), usedChannels.end(), badChannel) == usedChannels.end()) {
                  usedChannels.push_back(badChannel);
                }
              }

              const auto badChannelsGroups = this->clusterizeBadChannels(usedChannels);
              // loop over the groups of bad strips
              for (const auto& [first, consec] : badChannelsGroups) {
                unsigned int theBadChannelsRange;
                theBadChannelsRange = obj->encodePhase2(first, consec);

                if (printdebug_) {
                  edm::LogInfo("SiPhase2BadStripChannelBuilder")
                      << "detid " << rawId << " \t"
                      << " firstBadStrip " << first << "\t "
                      << " NconsecutiveBadStrips " << consec << "\t "
                      << " packed integer " << std::hex << theBadChannelsRange << std::dec << std::endl;
                }
                theSiStripVector.push_back(theBadChannelsRange);
              }
              break;
            }
            case NONE:
              [[fallthrough]];
            default:
              throw cms::Exception("Inconsistent configuration") << "Did not specifiy the right algorithm to be run";
          }

          SiStripBadStrip::Range range(theSiStripVector.begin(), theSiStripVector.end());
          if (!obj->put(rawId, range))
            edm::LogError("SiPhase2BadStripChannelBuilder")
                << "[SiPhase2BadStripChannelBuilder::getNewObject] detid already exists" << std::endl;

        }  // if it's a Strip module
      }    // if it's OT
    }      // if it's Tracker
  }        // loop of geomdets

  //End now write sistripbadChannel data in DB
  edm::Service<cond::service::PoolDBOutputService> mydbservice;

  if (mydbservice.isAvailable()) {
    if (mydbservice->isNewTagRequest("SiStripBadStripRcd")) {
      mydbservice->createOneIOV<SiStripBadStrip>(*obj, mydbservice->beginOfTime(), "SiStripBadStripRcd");
    } else {
      mydbservice->appendOneIOV<SiStripBadStrip>(*obj, mydbservice->currentTime(), "SiStripBadStripRcd");
    }
  } else {
    edm::LogError("SiPhase2BadStripChannelBuilder") << "Service is unavailable" << std::endl;
  }

  tGeom_.release();
  return obj;
}

// poor-man clusterizing algorithm
std::map<unsigned short, unsigned short> SiPhase2BadStripChannelBuilder::clusterizeBadChannels(
    const std::vector<Phase2TrackerDigi::PackedDigiType>& maskedChannels) {
  // Here we will store the result
  std::map<unsigned short, unsigned short> result{};
  std::map<int, std::string> printresult{};

  // Sort and remove duplicates.
  std::set data(maskedChannels.begin(), maskedChannels.end());

  // We will start the evaluation at the beginning of our data
  auto startOfSequence = data.begin();

  // Find all sequences
  while (startOfSequence != data.end()) {
    // FInd first value that is not greate than one
    auto endOfSequence =
        std::adjacent_find(startOfSequence, data.end(), [](const auto& v1, const auto& v2) { return v2 != v1 + 1; });
    if (endOfSequence != data.end())
      std::advance(endOfSequence, 1);

    auto consecutiveStrips = std::distance(startOfSequence, endOfSequence);
    result[*startOfSequence] = consecutiveStrips;

    if (printdebug_) {
      // Build resulting string
      std::ostringstream oss{};
      bool writeDash = false;
      for (auto it = startOfSequence; it != endOfSequence; ++it) {
        oss << (writeDash ? "-" : "") << std::to_string(*it);
        writeDash = true;
      }

      // Copy result to map
      for (auto it = startOfSequence; it != endOfSequence; ++it)
        printresult[*it] = oss.str();
    }

    // Continue to search for the next sequence
    startOfSequence = endOfSequence;
  }

  if (printdebug_) {
    // Show result on the screen. Or use the map in whichever way you want.
    for (const auto& [value, text] : printresult)
      edm::LogInfo("SiPhase2BadStripChannelBuilder") << std::left << std::setw(2) << value << " -> " << text << "\n";
  }

  return result;
}

// ------------ method fills 'descriptions' with the allowed parameters for the module  ------------
void SiPhase2BadStripChannelBuilder::fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
  edm::ParameterSetDescription desc;
  desc.setComment("Module to build SiStripBadStrip Payloads for the Phase-2 Outer Tracker");
  ConditionDBWriter::fillPSetDescription(desc);  // inherited from mother class
  desc.addUntracked<bool>("printDebug", false)->setComment("maximum amount of print-outs");
  desc.add<unsigned int>("popConAlgo", 1)->setComment("algorithm to populate the payload: 1=NAIVE,2=RANDOM");
  desc.add<double>("badComponentsFraction", 0.01)->setComment("fraction of bad components to populate the payload");
  descriptions.addWithDefaultLabel(desc);
}

#include "FWCore/PluginManager/interface/ModuleDef.h"
#include "FWCore/Framework/interface/MakerMacros.h"

DEFINE_FWK_MODULE(SiPhase2BadStripChannelBuilder);
