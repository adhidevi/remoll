/*!
  remoll - 12 GeV Moller Simluation

  Seamus Riordan, et al.
  riordan@jlab.org

*/

#include "CLHEP/Random/Random.h"

#include "remollRunAction.hh"
#include "remollRun.hh"
#include "remollRunData.hh"
#include "remollPrimaryGeneratorAction.hh"
#include "remollEventAction.hh"
#include "remollSteppingAction.hh"

#include "G4StepLimiterBuilder.hh"

#include "remollDetectorConstruction.hh"

#include "remollIO.hh"
#include "remollMessenger.hh"

//  Standard physics list
#include "LHEP.hh"
#include "G4PhysListFactory.hh"

#include "G4UImanager.hh"
#include "G4RunManager.hh"
#include "G4RunManagerKernel.hh"

#include "G4UnitsTable.hh"

#ifdef G4VIS_USE
#include "G4VisExecutive.hh"
#endif

#ifdef G4UI_USE
#include "G4UIExecutive.hh"
#endif

#include <sys/types.h>
#include <sys/stat.h>

int main(int argc, char** argv){

    // Initialize the CLHEP random engine used by
    // "shoot" type functions

    unsigned int seed = time(0);

    CLHEP::HepRandom::createInstance();
    CLHEP::HepRandom::setTheSeed(seed);

    remollRun::GetRun()->GetData()->SetSeed(seed);

    remollIO *io = new remollIO();

    //-------------------------------
    // Initialization of Run manager
    //-------------------------------
    G4cout << "RunManager construction starting...." << G4endl;
    G4RunManager * runManager = new G4RunManager;

    remollMessenger *rmmess = new remollMessenger();
    rmmess->SetIO(io);

    // Detector geometry
    G4VUserDetectorConstruction* detector = new remollDetectorConstruction();
    runManager->SetUserInitialization(detector);
    rmmess->SetDetCon( ((remollDetectorConstruction *) detector) );
    rmmess->SetMagField( ((remollDetectorConstruction *) detector)->GetGlobalField() );

    ((remollDetectorConstruction *) detector)->SetIO(io);

    // Physics we want to use
    G4int verbose = 0;
    G4PhysListFactory factory;
    G4VModularPhysicsList* physlist = factory.GetReferencePhysList("LHEP");
    physlist->SetVerboseLevel(verbose);
    runManager->SetUserInitialization(physlist);

    //-------------------------------
    // UserAction classes
    //-------------------------------
    G4UserRunAction* run_action = new remollRunAction;
    ((remollRunAction *) run_action)->SetIO(io);
    runManager->SetUserAction(run_action);

    G4VUserPrimaryGeneratorAction* gen_action = new remollPrimaryGeneratorAction;
    ((remollPrimaryGeneratorAction *) gen_action)->SetIO(io);
    rmmess->SetPriGen((remollPrimaryGeneratorAction *)gen_action);
    runManager->SetUserAction(gen_action);

    G4UserEventAction* event_action = new remollEventAction;
    ((remollEventAction *) event_action)->SetIO(io);

    runManager->SetUserAction(event_action);
    G4UserSteppingAction* stepping_action = new remollSteppingAction;
    runManager->SetUserAction(stepping_action);
    rmmess->SetStepAct((remollSteppingAction *) stepping_action);

    // New units


    //----------------
    // Visualization:
    //----------------
#ifdef G4VIS_USE
    G4cout << "Instantiating Visualization Manager......." << G4endl;
    G4VisManager* visManager = new G4VisExecutive;
    visManager -> Initialize ();
#endif

    // Setup commands
    //
    G4UImanager * UImanager = G4UImanager::GetUIpointer();
    /*
       UImanager->ApplyCommand("/Step/Verbose 0");
       UImanager->ApplyCommand("/tracking/Verbose 1");
       UImanager->ApplyCommand("/gun/particle e-");
       UImanager->ApplyCommand("/gun/energy 100 MeV");
       UImanager->ApplyCommand("/gun/direction 0 0 1");
       UImanager->ApplyCommand("/gun/position 0 0 0");
       UImanager->ApplyCommand("/gun/direction 0 .3 1.");
       */

    remollRunData *rundata = remollRun::GetRun()->GetData();

    runManager->SetVerboseLevel(0);

    if(argc==1)
    {
	//--------------------------
	// Define (G)UI
	//--------------------------
#ifdef G4UI_USE
	G4UIExecutive * ui = new G4UIExecutive(argc,argv);
	ui->SessionStart();

	delete ui;
#endif
    }
    else
    {

	G4String command = "/control/execute ";
	G4String fileName = argv[1];


	/* Copy contents of macro into buffer to be written out
	 * into ROOT file
	 * */
	rundata->SetMacroFile(argv[1]);

	UImanager->ApplyCommand(command+fileName);

	G4UIExecutive * ui = new G4UIExecutive(argc,argv);
	ui->SessionStart();


    }

    // Initialize Run manager
    // runManager->Initialize();

    return 0;
}
