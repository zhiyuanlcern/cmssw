import FWCore.ParameterSet.Config as cms
def customise(process):

    #Add the configuration for JeProfService running to dump Jemalloc heap profile snapshots:
    process.IgProfService = cms.Service("JeProfService",
        reportFirstEvent            = cms.untracked.int32(1),  #Dump first event for baseline studies
        reportEventInterval         = cms.untracked.int32( ((process.maxEvents.input.value()-1)//2) ), # dump in the middle of the run
        reportToFileAtPostEvent     = cms.untracked.string("| gzip -c > Jeprof.heap.%I.gz")
        )
    
    return(process)
