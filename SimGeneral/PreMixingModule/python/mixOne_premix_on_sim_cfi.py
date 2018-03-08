# This is the PreMixing config. Not only does it do a RawToDigi
# conversion to the secondary input source, it also holds its own
# instances of an EcalDigiProducer and an HcalDigitizer. It also
# replicates the noise adding functions in the SiStripDigitizer.
#
# Adapted from DataMixingModule


import FWCore.ParameterSet.Config as cms
from SimCalorimetry.HcalSimProducers.hcalUnsuppressedDigis_cfi import hcalSimBlock
from SimGeneral.MixingModule.SiStripSimParameters_cfi import SiStripSimBlock
from SimGeneral.MixingModule.SiPixelSimParameters_cfi import SiPixelSimBlock
from SimGeneral.MixingModule.ecalDigitizer_cfi import ecalDigitizer

import EventFilter.EcalRawToDigi.EcalUnpackerData_cfi
import EventFilter.ESRawToDigi.esRawToDigi_cfi
import EventFilter.HcalRawToDigi.HcalRawToDigi_cfi
import EventFilter.DTRawToDigi.dtunpacker_cfi
import EventFilter.RPCRawToDigi.rpcUnpacker_cfi
import EventFilter.CSCRawToDigi.cscUnpacker_cfi
import EventFilter.SiStripRawToDigi.SiStripDigis_cfi
import EventFilter.SiPixelRawToDigi.SiPixelRawToDigi_cfi

# content from Configuration/StandardSequences/DigiToRaw_cff.py

ecalDigis = EventFilter.EcalRawToDigi.EcalUnpackerData_cfi.ecalEBunpacker.clone()

ecalPreshowerDigis = EventFilter.ESRawToDigi.esRawToDigi_cfi.esRawToDigi.clone()

hcalDigis = EventFilter.HcalRawToDigi.HcalRawToDigi_cfi.hcalDigis.clone()

muonCSCDigis = EventFilter.CSCRawToDigi.cscUnpacker_cfi.muonCSCDigis.clone()

muonDTDigis = EventFilter.DTRawToDigi.dtunpacker_cfi.muonDTDigis.clone()

siStripDigis = EventFilter.SiStripRawToDigi.SiStripDigis_cfi.siStripDigis.clone()

siPixelDigis = EventFilter.SiPixelRawToDigi.SiPixelRawToDigi_cfi.siPixelDigis.clone()

siPixelDigis.InputLabel = 'rawDataCollector'
ecalDigis.InputLabel = 'rawDataCollector'
ecalPreshowerDigis.sourceTag = 'rawDataCollector'
hcalDigis.InputLabel = 'rawDataCollector'
muonCSCDigis.InputObjects = 'rawDataCollector'
muonDTDigis.inputLabel = 'rawDataCollector'

hcalDigis.FilterDataQuality = cms.bool(False)
hcalSimBlock.HcalPreMixStage2 = cms.bool(True)

mixData = cms.EDProducer("PreMixingModule",
    input = cms.SecSource("EmbeddedRootSource",
        producers = cms.VPSet(cms.convertToVPSet(
                ecalDigis = ecalDigis,
                ecalPreshowerDigis = ecalPreshowerDigis,
                hcalDigis = hcalDigis,
                muonDTDigis = muonDTDigis,
                muonCSCDigis = muonCSCDigis,
                siStripDigis = siStripDigis,
                siPixelDigis = siPixelDigis,
        )),
        nbPileupEvents = cms.PSet(
            averageNumber = cms.double(1.0)
        ),
        seed = cms.int32(1234567),
        type = cms.string('fixed'),
        sequential = cms.untracked.bool(False), # set to true for sequential reading of pileup
        fileNames = cms.untracked.vstring('file:DMPreProcess_RAW2DIGI.root')
    ),
    # Mixing Module parameters
    bunchspace = cms.int32(25),
    minBunch = cms.int32(0),
    maxBunch = cms.int32(0),
    mixProdStep1 = cms.bool(False),
    mixProdStep2 = cms.bool(False),
    # Workers
    workers = cms.PSet(
        pileup = cms.PSet(
            PileupInfoInputTag = cms.InputTag("addPileupInfo"),
            BunchSpacingInputTag = cms.InputTag("addPileupInfo","bunchSpacing"),
            CFPlaybackInputTag = cms.InputTag("mix"),
            GenPUProtonsInputTags = cms.VInputTag("genPUProtons"),
        ),
        # Note: elements with "@MIXING" in the input tag are generated by
        pixel = cms.PSet(
            SiPixelSimBlock,
            #
            workerType = cms.string("PreMixingSiPixelWorker"),
            #
            pixeldigiCollectionSig = cms.InputTag("simSiPixelDigis"),
            pixeldigiCollectionPile = cms.InputTag("siPixelDigis","","@MIXING"),
            PixelDigiCollectionDM = cms.string('siPixelDigisDM'),                   
        ),
        strip = cms.PSet(
            SiStripSimBlock,
            #
            workerType = cms.string("PreMixingSiStripWorker"),
            #
            SistripLabelSig = cms.InputTag("simSiStripDigis","ZeroSuppressed"),
            SiStripPileInputTag = cms.InputTag("siStripDigis","ZeroSuppressed","@MIXING"),
            # Dead APV Vector
            SistripAPVPileInputTag = cms.InputTag("mix","AffectedAPVList"),
            SistripAPVLabelSig = cms.InputTag("mix","AffectedAPVList"),
            # output
            SiStripDigiCollectionDM = cms.string('siStripDigisDM'),
            SiStripAPVListDM = cms.string('SiStripAPVList'),
        ),
        ecal = cms.PSet(
            ecalDigitizer.clone(accumulatorType = None, makeDigiSimLinks=None),
            #
            workerType = cms.string("PreMixingEcalWorker"),
            #
            EBdigiProducerSig = cms.InputTag("simEcalUnsuppressedDigis"),
            EEdigiProducerSig = cms.InputTag("simEcalUnsuppressedDigis"),
            ESdigiProducerSig = cms.InputTag("simEcalPreshowerDigis"),
            #
            EBPileInputTag = cms.InputTag("ecalDigis","ebDigis","@MIXING"),
            EEPileInputTag = cms.InputTag("ecalDigis","eeDigis","@MIXING"),
            ESPileInputTag = cms.InputTag("ecalPreshowerDigis","","@MIXING"),
            #
            EBDigiCollectionDM   = cms.string(''),
            EEDigiCollectionDM   = cms.string(''),
            ESDigiCollectionDM   = cms.string(''),
        ),
        hcal = cms.PSet(
            hcalSimBlock,
            #
            workerType = cms.string("PreMixingHcalWorker"),
            #
            HBHEdigiCollectionSig  = cms.InputTag("simHcalUnsuppressedDigis"),
            HOdigiCollectionSig    = cms.InputTag("simHcalUnsuppressedDigis"),
            HFdigiCollectionSig    = cms.InputTag("simHcalUnsuppressedDigis"),
            QIE10digiCollectionSig = cms.InputTag("simHcalUnsuppressedDigis"),
            QIE11digiCollectionSig = cms.InputTag("simHcalUnsuppressedDigis"),
            ZDCdigiCollectionSig   = cms.InputTag("simHcalUnsuppressedDigis"),
            #
            HBHEPileInputTag = cms.InputTag("hcalDigis","","@MIXING"),
            HOPileInputTag   = cms.InputTag("hcalDigis","","@MIXING"),
            HFPileInputTag   = cms.InputTag("hcalDigis","","@MIXING"),
            QIE10PileInputTag   = cms.InputTag("hcalDigis","","@MIXING"),
            QIE11PileInputTag   = cms.InputTag("hcalDigis","","@MIXING"),
            ZDCPileInputTag  = cms.InputTag(""),
            #
            HBHEDigiCollectionDM = cms.string(''),
            HODigiCollectionDM   = cms.string(''),
            HFDigiCollectionDM   = cms.string(''),
            QIE10DigiCollectionDM   = cms.string(''),
            QIE11DigiCollectionDM   = cms.string(''),
            ZDCDigiCollectionDM  = cms.string('')
        ),
        dt = cms.PSet(
            workerType = cms.string("PreMixingDTWorker"),
            #
            digiTagSig = cms.InputTag("simMuonDTDigis"),
            pileInputTag = cms.InputTag("muonDTDigis","","@MIXING"),
            collectionDM = cms.string(''),
        ),
        rpc = cms.PSet(
            workerType = cms.string("PreMixingRPCWorker"),
            #
            digiTagSig = cms.InputTag("simMuonRPCDigis"),                   
            pileInputTag = cms.InputTag("simMuonRPCDigis",""),
            collectionDM = cms.string(''),
        ),
        csc = cms.PSet(
            workerType = cms.string("PreMixingCSCWorker"),
            #
            strip = cms.PSet(
                digiTagSig = cms.InputTag("simMuonCSCDigis","MuonCSCStripDigi"),
                pileInputTag = cms.InputTag("muonCSCDigis","MuonCSCStripDigi","@MIXING"),
                collectionDM = cms.string('MuonCSCStripDigisDM'),
            ),
            wire = cms.PSet(
                digiTagSig = cms.InputTag("simMuonCSCDigis","MuonCSCWireDigi"),
                pileInputTag = cms.InputTag("muonCSCDigis","MuonCSCWireDigi","@MIXING"),
                collectionDM = cms.string('MuonCSCWireDigisDM'),
            ),
            comparator = cms.PSet(
                digiTagSig = cms.InputTag("simMuonCSCDigis","MuonCSCComparatorDigi"),
                pileInputTag = cms.InputTag("muonCSCDigis","MuonCSCComparatorDigi","@MIXING"),
                collectionDM = cms.string('MuonCSCComparatorDigisDM'),
            ),
        ),
        trackingTruth = cms.PSet(
            workerType = cms.string("PreMixingTrackingParticleWorker"),
            #
            labelSig = cms.InputTag("mix","MergedTrackTruth"),
            pileInputTag = cms.InputTag("mix","MergedTrackTruth"),
            collectionDM = cms.string('MergedTrackTruth'),
        ),
        pixelSimLink = cms.PSet(
            workerType = cms.string("PreMixingPixelDigiSimLinkWorker"),
            #
            labelSig = cms.InputTag("simSiPixelDigis"),
            pileInputTag = cms.InputTag("simSiPixelDigis"),
            collectionDM = cms.string('PixelDigiSimLink'),
        ),
        stripSimLink = cms.PSet(
            workerType = cms.string("PreMixingStripDigiSimLinkWorker"),
            #
            labelSig = cms.InputTag("simSiStripDigis"),
            pileInputTag = cms.InputTag("simSiStripDigis"),
            collectionDM = cms.string('StripDigiSimLink'),
        ),
        dtSimLink = cms.PSet(
            workerType = cms.string("PreMixingDTDigiSimLinkWorker"),
            #
            labelSig = cms.InputTag("simMuonDTDigis"),
            pileInputTag = cms.InputTag("simMuonDTDigis"),
            collectionDM = cms.string('simMuonDTDigis'),
        ),
        rpcSimLink = cms.PSet(
            workerType = cms.string("PreMixingRPCDigiSimLinkWorker"),
            #
            labelSig = cms.InputTag("simMuonRPCDigis","RPCDigiSimLink"),
            pileInputTag = cms.InputTag("simMuonRPCDigis","RPCDigiSimLink"),
            collectionDM = cms.string('RPCDigiSimLink'),
        ),
        cscStripSimLink = cms.PSet(
            workerType = cms.string("PreMixingStripDigiSimLinkWorker"),
            #
            labelSig = cms.InputTag("simMuonCSCDigis","MuonCSCStripDigiSimLinks"),
            pileInputTag = cms.InputTag("simMuonCSCDigis","MuonCSCStripDigiSimLinks"),
            collectionDM = cms.string('MuonCSCStripDigiSimLinks'),
        ),
        cscWireSimLink = cms.PSet(
            workerType = cms.string("PreMixingStripDigiSimLinkWorker"),
            #
            labelSig = cms.InputTag("simMuonCSCDigis","MuonCSCWireDigiSimLinks"),
            pileInputTag = cms.InputTag("simMuonCSCDigis","MuonCSCWireDigiSimLinks"),
            collectionDM = cms.string('MuonCSCWireDigiSimLinks'),
        ),
    ),
)


from Configuration.Eras.Modifier_fastSim_cff import fastSim
from FastSimulation.Tracking.recoTrackAccumulator_cfi import recoTrackAccumulator as _recoTrackAccumulator
fastSim.toModify(mixData,
    # from signal: mix tracks not strip or pixel digis
    workers = dict(
        pixel = None,
        strip = None,
        pixelSimLink = None,
        stripSimLink = None,
        tracks = cms.PSet(
            workerType = cms.string("PreMixingDigiAccumulatorWorker"),
            accumulator = _recoTrackAccumulator.clone(
                pileUpTracks = "mix:generalTracks"
            )
        )
    ),
)

from Configuration.Eras.Modifier_phase2_common_cff import phase2_common
from Configuration.Eras.Modifier_phase2_tracker_cff import phase2_tracker
from Configuration.Eras.Modifier_phase2_ecal_cff import phase2_ecal
from Configuration.Eras.Modifier_phase2_hcal_cff import phase2_hcal
from Configuration.Eras.Modifier_phase2_hgcal_cff import phase2_hgcal
from Configuration.Eras.Modifier_phase2_muon_cff import phase2_muon
phase2_common.toModify(mixData, input = dict(producers = [])) # we use digis directly, no need for raw2digi producers

# Tracker
phase2_tracker.toModify(mixData,
    workers = dict(
        # Disable SiStrip
        strip = None,
        stripSimLink = None,
        # Add Phase2 OT digiSimLink
        # TODO: Add digis, but needs code first
        pixelSimLink = dict(
            labelSig = "simSiPixelDigis:Pixel",
            pileInputTag = "simSiPixelDigis:Pixel",
        ),
        phase2OTSimLink = cms.PSet(
            workerType = cms.string("PreMixingPixelDigiSimLinkWorker"),
            #
            labelSig = cms.InputTag("simSiPixelDigis:Tracker"),
            pileInputTag = cms.InputTag("simSiPixelDigis:Tracker"),
            collectionDM = cms.string("Phase2OTDigiSimLink"),
        ),
    ),
)

# ECAL
phase2_ecal.toModify(mixData,
    workers = dict(
        ecal = dict(
            doES = False,
            EBPileInputTag = "simEcalDigis:ebDigis",
            EEPileInputTag = "simEcalDigis:eeDigis",
        )
    )
)
phase2_hgcal.toModify(mixData, workers=dict(ecal=dict(doEE=False)))

# HCAL
phase2_hcal.toModify(mixData,
    workers = dict(
        hcal = dict(
            HBHEPileInputTag = "simHcalDigis",
            HOPileInputTag = "simHcalDigis",
            HFPileInputTag = "simHcalDigis",
            QIE10PileInputTag = "simHcalDigis:HFQIE10DigiCollection",
            QIE11PileInputTag = "simHcalDigis:HBHEQIE11DigiCollection",
            ZDCPileInputTag = "simHcalUnsuppressedDigis",
        )
    )
)

# TODO: Add HGCAL, but needs code first

# Muon
phase2_muon.toModify(mixData,
    workers = dict(
        dt = dict(pileInputTag = "simMuonDTDigis"),
        rpc = dict(pileInputTag = "simMuonRPCDigis"),
        csc = dict(
            strip = dict(pileInputTag = "simMuonCSCDigis:MuonCSCStripDigi"),
            wire = dict(pileInputTag = "simMuonCSCDigis:MuonCSCWireDigi"),
            comparator = dict(pileInputTag = "simMuonCSCDigis:MuonCSCComparatorDigi"),
        )
    )
)
# TODO: Add GEM and ME0, but needs code first
