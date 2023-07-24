/** \class MuonBeamspotConstraintValueMapProducer
 * Compute muon pt and ptErr after beamspot constraint.
 *
 */
#include "FWCore/Framework/interface/Frameworkfwd.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/Framework/interface/Event.h"
#include "DataFormats/PatCandidates/interface/Muon.h"
#include "DataFormats/Common/interface/ValueMap.h"
#include "FWCore/Framework/interface/global/EDProducer.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "TrackingTools/TransientTrack/interface/TransientTrackBuilder.h"
#include "TrackingTools/Records/interface/TransientTrackRecord.h"
#include "TrackingTools/TransientTrack/interface/TransientTrack.h"
#include "RecoVertex/KalmanVertexFit/interface/SingleTrackVertexConstraint.h"

class MuonBeamspotConstraintValueMapProducer : public edm::global::EDProducer<> {
public:
  explicit MuonBeamspotConstraintValueMapProducer(const edm::ParameterSet& config)
      : muonToken_(consumes<pat::MuonCollection>(config.getParameter<edm::InputTag>("src"))),
        beamSpotToken_(consumes<reco::BeamSpot>(config.getParameter<edm::InputTag>("beamspot"))),
        ttbToken_(esConsumes(edm::ESInputTag("", "TransientTrackBuilder"))) {
    produces<edm::ValueMap<float>>("muonBSConstrainedPt");
    produces<edm::ValueMap<float>>("muonBSConstrainedPtErr");
  }

  ~MuonBeamspotConstraintValueMapProducer() override = default;

  static void fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
    edm::ParameterSetDescription desc;
    desc.add<edm::InputTag>("src", edm::InputTag("muons"))->setComment("Muon collection");
    desc.add<edm::InputTag>("beamspot", edm::InputTag("offlineBeamSpot"))->setComment("Beam spot collection");

    descriptions.addWithDefaultLabel(desc);
  }

private:
  void produce(edm::StreamID streamID, edm::Event& event, const edm::EventSetup& setup) const override {
    edm::Handle<pat::MuonCollection> muons;
    event.getByToken(muonToken_, muons);

    edm::Handle<reco::BeamSpot> beamSpotHandle;
    event.getByToken(beamSpotToken_, beamSpotHandle);

    edm::ESHandle<TransientTrackBuilder> ttkb = setup.getHandle(ttbToken_);

    std::vector<float> pts, ptErrs;
    pts.reserve(muons->size());
    ptErrs.reserve(muons->size());

    for (const auto& muon : *muons) {
      bool tbd = true;
      if (beamSpotHandle.isValid()) {
        SingleTrackVertexConstraint::BTFtuple btft = stvc.constrain(ttkb->build(muon.muonBestTrack()), *beamSpotHandle);
        if (std::get<0>(btft)) {
          // chi2 = std::get<2>(btft)); // should apply a cut, or store this as well?
          const reco::Track& trkBS = std::get<1>(btft).track();
          pts.push_back(trkBS.pt());
          ptErrs.push_back(trkBS.ptError());
          tbd = false;
        }
      }
      if (tbd) {  //FIXME fallback case if constrain fails; to be implemented
                  // 	pts.push_back(muon.pt());
                  // 	ptErrs.push_back(muon.bestTrack()->ptError());
        pts.push_back(-1.);
        ptErrs.push_back(-1.);
      }
    }

    {
      std::unique_ptr<edm::ValueMap<float>> valueMap(new edm::ValueMap<float>());
      edm::ValueMap<float>::Filler filler(*valueMap);
      filler.insert(muons, pts.begin(), pts.end());
      filler.fill();
      event.put(std::move(valueMap), "muonBSConstrainedPt");
    }

    {
      std::unique_ptr<edm::ValueMap<float>> valueMap(new edm::ValueMap<float>());
      edm::ValueMap<float>::Filler filler(*valueMap);
      filler.insert(muons, ptErrs.begin(), ptErrs.end());
      filler.fill();
      event.put(std::move(valueMap), "muonBSConstrainedPtErr");
    }
  }

  edm::EDGetTokenT<pat::MuonCollection> muonToken_;
  edm::EDGetTokenT<reco::BeamSpot> beamSpotToken_;
  edm::ESGetToken<TransientTrackBuilder, TransientTrackRecord> ttbToken_;
  SingleTrackVertexConstraint stvc;
};

DEFINE_FWK_MODULE(MuonBeamspotConstraintValueMapProducer);
