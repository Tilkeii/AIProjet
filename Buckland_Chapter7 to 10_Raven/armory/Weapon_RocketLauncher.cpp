#include "Weapon_RocketLauncher.h"
#include "../Raven_Bot.h"
#include "misc/Cgdi.h"
#include "../Raven_Game.h"
#include "../Raven_Map.h"
#include "../lua/Raven_Scriptor.h"
#include "fuzzy/FuzzyOperators.h"


//--------------------------- ctor --------------------------------------------
//-----------------------------------------------------------------------------
RocketLauncher::RocketLauncher(Raven_Bot*   owner):

                      Raven_Weapon(type_rocket_launcher,
                                   script->GetInt("RocketLauncher_DefaultRounds"),
                                   script->GetInt("RocketLauncher_MaxRoundsCarried"),
                                   script->GetDouble("RocketLauncher_FiringFreq"),
                                   script->GetDouble("RocketLauncher_IdealRange"),
                                   script->GetDouble("Rocket_MaxSpeed"),
                                   owner)
{
    //setup the vertex buffer
  const int NumWeaponVerts = 8;
  const Vector2D weapon[NumWeaponVerts] = {Vector2D(0, -3),
                                           Vector2D(6, -3),
                                           Vector2D(6, -1),
                                           Vector2D(15, -1),
                                           Vector2D(15, 1),
                                           Vector2D(6, 1),
                                           Vector2D(6, 3),
                                           Vector2D(0, 3)
                                           };
  for (int vtx=0; vtx<NumWeaponVerts; ++vtx)
  {
    m_vecWeaponVB.push_back(weapon[vtx]);
  }

  //setup the fuzzy module
  InitializeFuzzyModule();

}


//------------------------------ ShootAt --------------------------------------
//-----------------------------------------------------------------------------
inline void RocketLauncher::ShootAt(Vector2D pos)
{ 
  if (NumRoundsRemaining() > 0 && isReadyForNextShot())
  {
    //fire off a rocket!
    m_pOwner->GetWorld()->AddRocket(m_pOwner, pos);

    m_iNumRoundsLeft--;

    UpdateTimeWeaponIsNextAvailable();

    //add a trigger to the game so that the other bots can hear this shot
    //(provided they are within range)
    m_pOwner->GetWorld()->GetMap()->AddSoundTrigger(m_pOwner, script->GetDouble("RocketLauncher_SoundRange"));
  }
}

//---------------------------- Desirability -----------------------------------
//
//-----------------------------------------------------------------------------
double RocketLauncher::GetDesirability(double DistToTarget)
{
  if (m_iNumRoundsLeft == 0)
  {
    m_dLastDesirabilityScore = 0;
  }
  else
  {
    //fuzzify distance and amount of ammo
    m_FuzzyModule.Fuzzify("DistToTarget", DistToTarget);
    m_FuzzyModule.Fuzzify("AmmoStatus", (double)m_iNumRoundsLeft);

    m_dLastDesirabilityScore = m_FuzzyModule.DeFuzzify("Desirability", FuzzyModule::max_av);
  }

  return m_dLastDesirabilityScore;
}

//-------------------------  InitializeFuzzyModule ----------------------------
//
//  set up some fuzzy variables and rules
//-----------------------------------------------------------------------------
void RocketLauncher::InitializeFuzzyModule()
{
  FuzzyVariable& DistToTarget = m_FuzzyModule.CreateFLV("DistToTarget");
  FzSet& Target_Close = DistToTarget.AddLeftShoulderSet("Target_Close",0,15,75);
  FzSet& Target_Medium = DistToTarget.AddTriangularSet("Target_Medium",15,75,150);
  FzSet& Target_Far = DistToTarget.AddTriangularSet("Target_Far",75,150,300);
  FzSet& Target_VeryFar = DistToTarget.AddTriangularSet("Target_VeryFar",150, 300, 600);
  FzSet& Target_InUniverse = DistToTarget.AddRightShoulderSet("Target_InUniverse",300, 600, 1000);


  FuzzyVariable& Desirability = m_FuzzyModule.CreateFLV("Desirability"); 
  FzSet& IChooseYou = Desirability.AddRightShoulderSet("IChooseYou", 70, 90, 100);
  FzSet& VeryDesirable = Desirability.AddTriangularSet("VeryDesirable", 50, 70, 90);
  FzSet& Desirable = Desirability.AddTriangularSet("Desirable", 30, 50, 70);
  FzSet& Undesirable = Desirability.AddTriangularSet("Undesirable", 0, 10, 30);
  FzSet& ForgetIt = Desirability.AddTriangularSet("ForgetIt", 0, 0, 10);


  FuzzyVariable& AmmoStatus = m_FuzzyModule.CreateFLV("AmmoStatus");
  FzSet& Ammo_Loads = AmmoStatus.AddRightShoulderSet("Ammo_Loads", 50, 80, 100);
  FzSet& Ammo_AlmostFull = AmmoStatus.AddTriangularSet("Ammo_AlmostFull", 30, 50, 70);
  FzSet& Ammo_Okay = AmmoStatus.AddTriangularSet("Ammo_Okay", 10, 30, 60);
  FzSet& Ammo_Low = AmmoStatus.AddTriangularSet("Ammo_Low", 0, 10, 30);
  FzSet& Ammo_AlmostOut = AmmoStatus.AddTriangularSet("Ammo_AlmostOut", 0, 0, 10);


  m_FuzzyModule.AddRule(FzAND(Target_Close, Ammo_Loads), ForgetIt);
  m_FuzzyModule.AddRule(FzAND(Target_Close, Ammo_AlmostFull), ForgetIt);
  m_FuzzyModule.AddRule(FzAND(Target_Close, Ammo_Okay), ForgetIt);
  m_FuzzyModule.AddRule(FzAND(Target_Close, Ammo_Low), ForgetIt);
  m_FuzzyModule.AddRule(FzAND(Target_Close, Ammo_AlmostOut), ForgetIt);

  m_FuzzyModule.AddRule(FzAND(Target_Medium, Ammo_Loads), Desirable);
  m_FuzzyModule.AddRule(FzAND(Target_Medium, Ammo_AlmostFull), Desirable);
  m_FuzzyModule.AddRule(FzAND(Target_Medium, Ammo_Okay), Undesirable);
  m_FuzzyModule.AddRule(FzAND(Target_Medium, Ammo_Low), ForgetIt);
  m_FuzzyModule.AddRule(FzAND(Target_Medium, Ammo_AlmostOut), ForgetIt);

  m_FuzzyModule.AddRule(FzAND(Target_Far, Ammo_Loads), IChooseYou);
  m_FuzzyModule.AddRule(FzAND(Target_Far, Ammo_AlmostFull), VeryDesirable);
  m_FuzzyModule.AddRule(FzAND(Target_Far, Ammo_Okay), Desirable);
  m_FuzzyModule.AddRule(FzAND(Target_Far, Ammo_Low), Desirable);
  m_FuzzyModule.AddRule(FzAND(Target_Far, Ammo_AlmostOut), Undesirable);

  m_FuzzyModule.AddRule(FzAND(Target_VeryFar, Ammo_Loads), IChooseYou);
  m_FuzzyModule.AddRule(FzAND(Target_VeryFar, Ammo_AlmostFull), IChooseYou);
  m_FuzzyModule.AddRule(FzAND(Target_VeryFar, Ammo_Okay), VeryDesirable);
  m_FuzzyModule.AddRule(FzAND(Target_VeryFar, Ammo_Low), Desirable);
  m_FuzzyModule.AddRule(FzAND(Target_VeryFar, Ammo_AlmostOut), Undesirable);

  m_FuzzyModule.AddRule(FzAND(Target_InUniverse, Ammo_Loads), Desirable);
  m_FuzzyModule.AddRule(FzAND(Target_InUniverse, Ammo_AlmostFull), Undesirable);
  m_FuzzyModule.AddRule(FzAND(Target_InUniverse, Ammo_Okay), Undesirable);
  m_FuzzyModule.AddRule(FzAND(Target_InUniverse, Ammo_Low), ForgetIt);
  m_FuzzyModule.AddRule(FzAND(Target_InUniverse, Ammo_AlmostOut), ForgetIt);

}


//-------------------------------- Render -------------------------------------
//-----------------------------------------------------------------------------
void RocketLauncher::Render()
{
    m_vecWeaponVBTrans = WorldTransform(m_vecWeaponVB,
                                   m_pOwner->Pos(),
                                   m_pOwner->Facing(),
                                   m_pOwner->Facing().Perp(),
                                   m_pOwner->Scale());

  gdi->RedPen();

  gdi->ClosedShape(m_vecWeaponVBTrans);
}