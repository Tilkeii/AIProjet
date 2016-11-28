#include "Weapon_Knife.h"
#include "Raven_Bot.h"
#include "misc/Cgdi.h"
#include "Raven_Game.h"
#include "Raven_Map.h"
#include "lua/Raven_Scriptor.h"
#include "misc/utils.h"
#include "fuzzy/FuzzyOperators.h"

#include "Raven_Messages.h"
#include "Messaging\MessageDispatcher.h"


//--------------------------- ctor --------------------------------------------
//-----------------------------------------------------------------------------
Knife::Knife(Raven_Bot*   owner):

                      Raven_Weapon(type_knife,
                                   script->GetInt("Knife_DefaultRounds"),
                                   script->GetInt("Knife_MaxRoundsCarried"),
                                   script->GetDouble("Knife_FiringFreq"),
                                   script->GetDouble("Knife_IdealRange"),
                                   script->GetDouble("Knife_MaxSpeed"),
                                   owner)

{

    //setup the vertex buffer
  const int NumWeaponVerts = 4;
  const Vector2D weapon[NumWeaponVerts] = {Vector2D(0, -1),
                                           Vector2D(5, -1),
                                           Vector2D(5, 1),
                                           Vector2D(0, 1)
                                           };

  
  for (int vtx=0; vtx<NumWeaponVerts; ++vtx)
  {
    m_vecWeaponVB.push_back(weapon[vtx]);
  }

  //setup the fuzzy module
  InitializeFuzzyModule();

}

//------------------------------ ShootAt --------------------------------------

inline void Knife::ShootAt(Vector2D pos)
{ 
	if (isReadyForNextShot())
  {
    //fire!
    m_pOwner->GetWorld()->AddKnife(m_pOwner, pos);

    UpdateTimeWeaponIsNextAvailable();
  }
}

//---------------------------- Desirability -----------------------------------
//
//-----------------------------------------------------------------------------
inline double Knife::GetDesirability(double DistToTarget)
{

    //fuzzify distance and amount of ammo
    m_FuzzyModule.Fuzzify("DistanceToTarget", DistToTarget);
    //m_FuzzyModule.Fuzzify("AmmoStatus", (double)m_iNumRoundsLeft);

    m_dLastDesirabilityScore = m_FuzzyModule.DeFuzzify("Desirability", FuzzyModule::max_av);

  return m_dLastDesirabilityScore;
}

//--------------------------- InitializeFuzzyModule ---------------------------
//
//  set up some fuzzy variables and rules
//-----------------------------------------------------------------------------
void Knife::InitializeFuzzyModule()
{  
  FuzzyVariable& DistanceToTarget = m_FuzzyModule.CreateFLV("DistanceToTarget");

  FzSet& Target_Close = DistanceToTarget.AddLeftShoulderSet("Target_Close", 0, 15, 30);
  FzSet& Target_Medium = DistanceToTarget.AddTriangularSet("Target_Medium", 15, 150, 300);
  FzSet& Target_Far = DistanceToTarget.AddRightShoulderSet("Target_Far", 150, 300, 1000);

  FuzzyVariable& Desirability = m_FuzzyModule.CreateFLV("Desirability");
  
  FzSet& VeryDesirable = Desirability.AddRightShoulderSet("VeryDesirable", 50, 75, 100);
  FzSet& Desirable = Desirability.AddTriangularSet("Desirable", 25, 50, 75);
  FzSet& Undesirable = Desirability.AddLeftShoulderSet("Undesirable", 0, 25, 50);

  m_FuzzyModule.AddRule(Target_Close, VeryDesirable);
  m_FuzzyModule.AddRule(Target_Medium, FzVery(Undesirable));
  m_FuzzyModule.AddRule(Target_Far, FzVery(Undesirable));
}

//-------------------------------- Render -------------------------------------
//-----------------------------------------------------------------------------
void Knife::Render()
{
  m_vecWeaponVBTrans = WorldTransform(m_vecWeaponVB,
                                   m_pOwner->Pos(),
                                   m_pOwner->Facing(),
                                   m_pOwner->Facing().Perp(),
                                   m_pOwner->Scale());

  gdi->BlackPen();

  gdi->PolyLine(m_vecWeaponVBTrans);

}