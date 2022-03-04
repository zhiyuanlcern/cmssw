
/*----------------------------------------------------------------------

Toy EDAnalyzer for testing purposes only.

----------------------------------------------------------------------*/

#include "FWCore/Framework/interface/one/EDAnalyzer.h"
#include "FWCore/Framework/interface/Event.h"

#include "FWCore/Framework/interface/EventSetup.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"

#include <string>

namespace edmtest {
  class DTTimeUtility : public edm::one::EDAnalyzer<> {
  public:
    explicit DTTimeUtility(edm::ParameterSet const& p);
    explicit DTTimeUtility(int i);
    ~DTTimeUtility() override = default;
    void analyze(const edm::Event& e, const edm::EventSetup& c) override;

  private:
    int year;
    int month;
    int day;
    int hour;
    int min;
    int sec;
    long long int condTime;
    long long int coralTime;
  };
}  // namespace edmtest
