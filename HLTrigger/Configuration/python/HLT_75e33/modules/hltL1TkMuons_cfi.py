import FWCore.ParameterSet.Config as cms

hltL1TkMuons = cms.EDProducer("L1TkMuonProducer",
    CHI2MAX = cms.double(100.0),
    DRmax = cms.double(0.5),
    ETABARRELOVERLAP = cms.double(0.83),
    ETAMAX = cms.double(5.0),
    ETAMIN = cms.double(0),
    ETAOVERLAPENDCAP = cms.double(1.24),
    L1BMTFInputTag = cms.InputTag("simKBmtfDigis","BMTF"),
    L1EMTFInputTag = cms.InputTag("simEmtfDigis","EMTF"),
    L1EMTFTrackCollectionInputTag = cms.InputTag("simEmtfDigis"),
    L1OMTFInputTag = cms.InputTag("simOmtfDigis","OMTF"),
    L1TrackInputTag = cms.InputTag("TTTracksFromTrackletEmulation","Level1TTTracks"),
    PTMINTRA = cms.double(2.0),
    ZMAX = cms.double(25.0),
    applyQualityCuts = cms.bool(True),
    bmtfMatchAlgoVersion = cms.string('TP'),
    correctGMTPropForTkZ = cms.bool(True),
    do_relax_factors = cms.bool(True),
    emtfMatchAlgoVersion = cms.string('MAnTra'),
    emtfcorr_boundaries = cms.FileInPath('L1Trigger/L1TMuon/data/MAnTra_data/matching_windows_endcap/matching_windows_boundaries.root'),
    emtfcorr_phi_windows = cms.FileInPath('L1Trigger/L1TMuon/data/MAnTra_data/matching_windows_endcap/matching_windows_phi_q99.root'),
    emtfcorr_theta_windows = cms.FileInPath('L1Trigger/L1TMuon/data/MAnTra_data/matching_windows_endcap/matching_windows_theta_q99.root'),
    final_window_factor = cms.double(0.5),
    initial_window_factor = cms.double(0.0),
    mantra_bmtfcorr_boundaries = cms.FileInPath('L1Trigger/L1TMuon/data/MAnTra_data/matching_windows_barrel/matching_windows_boundaries.root'),
    mantra_bmtfcorr_phi_windows = cms.FileInPath('L1Trigger/L1TMuon/data/MAnTra_data/matching_windows_barrel/matching_windows_phi_q99.root'),
    mantra_bmtfcorr_theta_windows = cms.FileInPath('L1Trigger/L1TMuon/data/MAnTra_data/matching_windows_barrel/matching_windows_theta_q99.root'),
    mantra_emtfcorr_boundaries = cms.FileInPath('L1Trigger/L1TMuon/data/MAnTra_data/matching_windows_endcap/matching_windows_boundaries.root'),
    mantra_emtfcorr_phi_windows = cms.FileInPath('L1Trigger/L1TMuon/data/MAnTra_data/matching_windows_endcap/matching_windows_phi_q99.root'),
    mantra_emtfcorr_theta_windows = cms.FileInPath('L1Trigger/L1TMuon/data/MAnTra_data/matching_windows_endcap/matching_windows_theta_q99.root'),
    mantra_n_trk_par = cms.int32(4),
    mantra_omtfcorr_boundaries = cms.FileInPath('L1Trigger/L1TMuon/data/MAnTra_data/matching_windows_overlap/matching_windows_boundaries.root'),
    mantra_omtfcorr_phi_windows = cms.FileInPath('L1Trigger/L1TMuon/data/MAnTra_data/matching_windows_overlap/matching_windows_phi_q99.root'),
    mantra_omtfcorr_theta_windows = cms.FileInPath('L1Trigger/L1TMuon/data/MAnTra_data/matching_windows_overlap/matching_windows_theta_q99.root'),
    max_trk_aeta = cms.double(2.5),
    max_trk_chi2 = cms.double(100.0),
    min_trk_nstubs = cms.int32(4),
    min_trk_p = cms.double(3.5),
    nStubsmin = cms.int32(4),
    n_trk_par = cms.int32(4),
    omtfMatchAlgoVersion = cms.string('MAnTra'),
    pt_end_relax = cms.double(6.0),
    pt_start_relax = cms.double(2.0),
    use5ParameterFit = cms.bool(False),
    useRegionEtaMatching = cms.bool(True),
    useTPMatchWindows = cms.bool(True)
)
