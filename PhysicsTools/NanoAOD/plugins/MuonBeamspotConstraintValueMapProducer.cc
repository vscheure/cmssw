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
#include "DataFormats/VertexReco/interface/Vertex.h"
#include "DataFormats/VertexReco/interface/VertexFwd.h"

class MuonBeamspotConstraintValueMapProducer : public edm::global::EDProducer<> {
public:
  explicit MuonBeamspotConstraintValueMapProducer(const edm::ParameterSet& config)
      : muonToken_(consumes<pat::MuonCollection>(config.getParameter<edm::InputTag>("src"))),
        beamSpotToken_(consumes<reco::BeamSpot>(config.getParameter<edm::InputTag>("beamspot"))),
        PrimaryVertexToken_(consumes<reco::VertexCollection>(config.getParameter<edm::InputTag>("vertices"))), //new
   	PrimaryVertexValueToken_(consumes<edm::ValueMap<float> >(config.getParameter<edm::InputTag>("vertices"))), //new
        ttbToken_(esConsumes(edm::ESInputTag("", "TransientTrackBuilder"))) {
    produces<edm::ValueMap<float>>("muonBSConstrainedPt");
    produces<edm::ValueMap<float>>("muonBSConstrainedPtErr");
  }

  ~MuonBeamspotConstraintValueMapProducer() override = default;

  static void fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
    edm::ParameterSetDescription desc;
    desc.add<edm::InputTag>("src", edm::InputTag("muons"))->setComment("Muon collection");
    desc.add<edm::InputTag>("beamspot", edm::InputTag("offlineBeamSpot"))->setComment("Beam spot collection");
    desc.add<edm::InputTag>("vertices", edm::InputTag("offlineSlimmedPrimaryVertices"))->setComment("Primary vertex collection");  //new

    descriptions.addWithDefaultLabel(desc);
  }

private:
  void produce(edm::StreamID streamID, edm::Event& event, const edm::EventSetup& setup) const override {
    edm::Handle<pat::MuonCollection> muons;
    event.getByToken(muonToken_, muons);

    edm::Handle<reco::BeamSpot> beamSpotHandle;
    event.getByToken(beamSpotToken_, beamSpotHandle);
    
    // Get primary vertices and score values
    edm::Handle<reco::VertexCollection> PrimaryVertices;  //new
    edm::Handle<edm::ValueMap<float> > PrimaryVertexValues;  //new
    event.getByToken(PrimaryVertexToken_, PrimaryVertices);  //new
    event.getByToken(PrimaryVertexValueToken_, PrimaryVertexValues);  //new
    
    GlobalError error;  //new
    GlobalPoint PVPos;  //new
    VertexState  PV; //new
    float score_tmp=0;  //new
    int i=0; //new
    for (reco::VertexCollection::const_iterator vite = PrimaryVertices->begin(); vite != PrimaryVertices->end(); ++vite) { // Select the correct Primary vertex  //new
   	float score = PrimaryVertexValues->get(i);	    //new
    	if (score > score_tmp){     //new
      		error = GlobalError(vite->covariance()); //new
      		PVPos = GlobalPoint(Basic3DVector<float>( vite->position())); //new
      		PV = VertexState(PVPos, error); //new
      		score_tmp = score; //new
      	}    //new
    	i++; //new
    }  //new

    edm::ESHandle<TransientTrackBuilder> ttkb = setup.getHandle(ttbToken_);
    
    float BeamWidthX = beamSpotHandle->BeamWidthX(); //new 
    float BeamWidthXError = beamSpotHandle->BeamWidthXError(); //new 
    float BeamWidthY = beamSpotHandle->BeamWidthY(); //new 
    float BeamWidthYError = beamSpotHandle->BeamWidthYError(); //new 

    std::vector<float> pts, ptErrs;
    pts.reserve(muons->size());
    ptErrs.reserve(muons->size());

    for (const auto& muon : *muons) {
      bool tbd = true;
      if (beamSpotHandle.isValid() && !((BeamWidthXError/BeamWidthX>0.3) | (BeamWidthYError/BeamWidthY>0.3))) {  //new (changed)
        SingleTrackVertexConstraint::BTFtuple btft = stvc.constrain(ttkb->build(muon.muonBestTrack()), *beamSpotHandle);
        if (std::get<0>(btft)) {
          // chi2 = std::get<2>(btft)); // should apply a cut, or store this as well?
          const reco::Track& trkBS = std::get<1>(btft).track();
          pts.push_back(trkBS.pt());
          ptErrs.push_back(trkBS.ptError());
          tbd = false;
        }
      }
      else{  //new
      	SingleTrackVertexConstraint::BTFtuple btft = stvc.constrain(ttkb->build(muon.muonBestTrack()), PV); //new
        if (std::get<0>(btft)) {  //new
          // chi2 = std::get<2>(btft)); // should apply a cut, or store this as well?  //new
          const reco::Track& trkBS = std::get<1>(btft).track(); //new
          pts.push_back(trkBS.pt()); //new
          ptErrs.push_back(trkBS.ptError()); //new
          tbd = false; //new
        } //new
      }   //new
      if (tbd) {  //FIXME fallback case if constrain fails; to be implemented
                   	pts.push_back(muon.pt());  //new (uncommented)
                   	ptErrs.push_back(muon.bestTrack()->ptError());//new (uncommented)
        //pts.push_back(-1.); //new (commented)
        //ptErrs.push_back(-1.); //new (commented)
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
  edm::EDGetTokenT<reco::VertexCollection> PrimaryVertexToken_; //new
  edm::EDGetTokenT<edm::ValueMap<float> > PrimaryVertexValueToken_; //new
  edm::ESGetToken<TransientTrackBuilder, TransientTrackRecord> ttbToken_;
  SingleTrackVertexConstraint stvc;
};

DEFINE_FWK_MODULE(MuonBeamspotConstraintValueMapProducer);
