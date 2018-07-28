#include "remollGenericDetector.hh"

#include "G4OpticalPhoton.hh"
#include "G4SDManager.hh"
#include "G4GenericMessenger.hh"

#include "remollGenericDetectorHit.hh"
#include "remollGenericDetectorSum.hh"

#include <sstream>

std::list<remollGenericDetector*> remollGenericDetector::fGenericDetectors = std::list<remollGenericDetector*>();
G4GenericMessenger* remollGenericDetector::fStaticMessenger = 0;

remollGenericDetector::remollGenericDetector( G4String name, G4int detnum )
: G4VSensitiveDetector(name),fHitColl(0),fSumColl(0),fEnabled(true)
{
  fDetNo = detnum;
  assert( fDetNo > 0 );

  fDetectSecondaries = true;
  fDetectOpticalPhotons = false;
  fDetectLowEnergyNeutrals = false;

  std::stringstream genhit;
  genhit << "genhit_" << detnum;
  collectionName.insert(G4String(genhit.str()));

  std::stringstream gensum;
  gensum << "gensum_" << detnum;
  collectionName.insert(G4String(gensum.str()));

  fHCID = -1;
  fSCID = -1;

  // Create generic detector messenger
  std::stringstream ss;
  ss << fDetNo;
  fMessenger = new G4GenericMessenger(this,"/remoll/SD/det_" + ss.str() + "/","Remoll SD properties for " + name);
  fMessenger->DeclareProperty(
      "enable",
      fEnabled,
      "Enable recording of hits in this detector")
      .SetParameterName("flag",true).SetDefaultValue(true);

  // Create static messenger
  fStaticMessenger = new G4GenericMessenger(this,"/remoll/SD/","Remoll SD properties");
  fStaticMessenger->DeclareMethod(
    "enable_all",
    &remollGenericDetector::SetAllEnabled,
    "Enable recording of hits in all detectors");
  fStaticMessenger->DeclareMethod(
    "disable_all",
    &remollGenericDetector::SetAllDisabled,
    "Disable recording of hits in all detectors");
  fStaticMessenger->DeclareMethod(
    "print_all",
    &remollGenericDetector::PrintAll,
    "Print all detectors");

  // Add to static list
  InsertGenericDetector(this);
}

remollGenericDetector::~remollGenericDetector()
{
  EraseGenericDetector(this);
  delete fMessenger;
}

void remollGenericDetector::Initialize(G4HCofThisEvent *){

    fHitColl = new remollGenericDetectorHitCollection( SensitiveDetectorName, collectionName[0] );
    fSumColl = new remollGenericDetectorSumCollection( SensitiveDetectorName, collectionName[1] );

    fSumMap.clear();

}

///////////////////////////////////////////////////////////////////////

G4bool remollGenericDetector::ProcessHits( G4Step *step, G4TouchableHistory *){
    G4bool badedep = false;
    G4bool badhit  = false;

    // Ignore this detector if disabled
    if (! fEnabled) return false;

    // Ignore optical photons as hits (but still simulate them
    // so they can knock out electrons of the photocathode)
    if (! fDetectOpticalPhotons
        && step->GetTrack()->GetDefinition() == G4OpticalPhoton::OpticalPhotonDefinition()) {
      return false;
    }

    // Ignore neutral particles below 0.1 MeV
    G4double charge = step->GetTrack()->GetDefinition()->GetPDGCharge();
    if (! fDetectLowEnergyNeutrals
        && charge == 0.0 && step->GetTrack()->GetTotalEnergy() < 0.1*CLHEP::MeV) {
      return false;
    }

    // Get the step point and track
    G4StepPoint* prepoint = step->GetPreStepPoint();
    G4StepPoint* postpoint = step->GetPostStepPoint();
    G4Track*     track = step->GetTrack();

    // Get touchable volume info
    G4TouchableHistory *hist = (G4TouchableHistory*)(prepoint->GetTouchable());
    G4int  copyID = hist->GetVolume()->GetCopyNo();//return the copy id of the logical volume

    G4double edep = step->GetTotalEnergyDeposit();

    // We're just going to record primary particles and things
    // that have just entered our boundary
    badhit = true;
    if (track->GetCreatorProcess() == 0 ||
	(fDetectSecondaries && prepoint->GetStepStatus() == fGeomBoundary)) {
	badhit = false;
    }

    badedep = false;
    if (edep <= 0.0) {
        badedep = true;
    }

    /////////////////////////////////////////////////////

    // Do the actual data grabbing

    if (! badedep) {
        // Sum
        remollGenericDetectorSum* thissum = 0;
        if (! fSumMap.count(copyID)) {
	    thissum = new remollGenericDetectorSum(fDetNo, copyID);
	    fSumMap[copyID] = thissum;
	    fSumColl->insert(thissum);
        } else thissum = fSumMap[copyID];

        // Add energy deposit
        thissum->fEdep += edep;
    }

    if (! badhit) {
	// Hit
	remollGenericDetectorHit* thishit = new remollGenericDetectorHit(fDetNo, copyID);
	fHitColl->insert( thishit );

        // Which point do we store?
        G4StepPoint* point = 0;
        // optical absorption
        if (step->GetTrack()->GetDefinition() == G4OpticalPhoton::OpticalPhotonDefinition()
         && postpoint->GetStepStatus() == fGeomBoundary) {
          point = postpoint;
        // all other cases
        } else {
          point = prepoint;
        }

	// Positions
	G4ThreeVector global_position = point->GetPosition();
	G4ThreeVector local_position = point->GetTouchable()->GetHistory()->GetTopTransform().TransformPoint(global_position);
	thishit->f3X  = global_position;
	thishit->f3Xl = local_position;

	thishit->f3V  = track->GetVertexPosition();
	thishit->f3P  = track->GetMomentum();
	thishit->f3S  = track->GetPolarization();

        thishit->fTime = point->GetGlobalTime();

	thishit->fP = track->GetMomentum().mag();
	thishit->fE = track->GetTotalEnergy();
	thishit->fM = track->GetDefinition()->GetPDGMass();

	thishit->fTrID  = track->GetTrackID();
	thishit->fmTrID = track->GetParentID();
	thishit->fPID   = track->GetDefinition()->GetPDGEncoding();

	// FIXME - Enumerate encodings
	thishit->fGen   = (long int) track->GetCreatorProcess();

        thishit->fEdep  = step->GetTotalEnergyDeposit();
    }

    return !badedep && !badhit;
}

///////////////////////////////////////////////////////////////////////

void remollGenericDetector::EndOfEvent(G4HCofThisEvent*HCE) {
    G4SDManager *sdman = G4SDManager::GetSDMpointer();

    if(fHCID<0){ fHCID = sdman->GetCollectionID(collectionName[0]); }
    if(fSCID<0){ fSCID = sdman->GetCollectionID(collectionName[1]); }

    HCE->AddHitsCollection( fHCID, fHitColl );
    HCE->AddHitsCollection( fSCID, fSumColl );

    return;
}


