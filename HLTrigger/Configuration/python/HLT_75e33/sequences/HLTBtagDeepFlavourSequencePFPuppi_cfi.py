import FWCore.ParameterSet.Config as cms

from ..modules.hltDeepBLifetimeTagInfosPFPuppi_cfi import *
from ..modules.hltDeepCombinedSecondaryVertexBJetTagsInfosPuppi_cfi import *
from ..modules.hltDeepInclusiveMergedVerticesPF_cfi import *
from ..modules.hltDeepInclusiveSecondaryVerticesPF_cfi import *
from ..modules.hltDeepInclusiveVertexFinderPF_cfi import *
from ..modules.hltDeepSecondaryVertexTagInfosPFPuppi_cfi import *
from ..modules.hltDeepTrackVertexArbitratorPF_cfi import *
from ..modules.hltPfDeepFlavourJetTags_cfi import *
from ..modules.hltPfDeepFlavourTagInfos_cfi import *
from ..modules.hltPFPuppiJetForBtagEta2p4_cfi import *
from ..modules.hltPFPuppiJetForBtagEta4p0_cfi import *
from ..modules.hltPFPuppiJetForBtagSelectorEta2p4_cfi import *
from ..modules.hltPFPuppiJetForBtagSelectorEta4p0_cfi import *
from ..modules.hltPrimaryVertexAssociation_cfi import *

HLTBtagDeepFlavourSequencePFPuppi = cms.Sequence(hltPFPuppiJetForBtagSelectorEta2p4+hltPFPuppiJetForBtagSelectorEta4p0+hltPFPuppiJetForBtagEta2p4+hltPFPuppiJetForBtagEta4p0+hltDeepBLifetimeTagInfosPFPuppi+hltDeepInclusiveVertexFinderPF+hltDeepInclusiveSecondaryVerticesPF+hltDeepTrackVertexArbitratorPF+hltDeepInclusiveMergedVerticesPF+hltDeepSecondaryVertexTagInfosPFPuppi+hltPrimaryVertexAssociation+hltDeepCombinedSecondaryVertexBJetTagsInfosPuppi+hltPfDeepFlavourTagInfos+hltPfDeepFlavourJetTags)
