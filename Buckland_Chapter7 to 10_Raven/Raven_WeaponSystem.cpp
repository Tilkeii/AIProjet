#include "Raven_WeaponSystem.h"
#include "armory/Weapon_RocketLauncher.h"
#include "armory/Weapon_RailGun.h"
#include "armory/Weapon_ShotGun.h"
#include "armory/Weapon_Blaster.h"
#include "Raven_Bot.h"
#include "misc/utils.h"
#include "lua/Raven_Scriptor.h"
#include "Raven_Game.h"
#include "Raven_UserOptions.h"
#include "2D/transformations.h"
#include "fuzzy/FuzzyOperators.h"
#include "Weapon_Knife.h"

//uncomment to write object creation/deletion to debug console
#define  LOG_CREATIONAL_STUFF
#include "debug/DebugConsole.h"

//------------------------- ctor ----------------------------------------------
//-----------------------------------------------------------------------------
Raven_WeaponSystem::Raven_WeaponSystem(Raven_Bot* owner,
                                       double ReactionTime,
                                       double AimAccuracy,
                                       double AimPersistance):m_pOwner(owner),
                                                          m_dReactionTime(ReactionTime),
                                                          m_dAimAccuracy(AimAccuracy),
                                                          m_dAimPersistance(AimPersistance)
{
  Initialize();
}

//------------------------- dtor ----------------------------------------------
//-----------------------------------------------------------------------------
Raven_WeaponSystem::~Raven_WeaponSystem()
{
  for (unsigned int w=0; w<m_WeaponMap.size(); ++w)
  {
    delete m_WeaponMap[w];
  }
}

//------------------------------ Initialize -----------------------------------
//
//  initializes the weapons
//-----------------------------------------------------------------------------
void Raven_WeaponSystem::Initialize()
{
  //delete any existing weapons
  WeaponMap::iterator curW;
  for (curW = m_WeaponMap.begin(); curW != m_WeaponMap.end(); ++curW)
  {
    delete curW->second;
  }

  m_WeaponMap.clear();

  //set up the container
  m_pCurrentWeapon = new Blaster(m_pOwner);

  m_WeaponMap[type_blaster]         = m_pCurrentWeapon;
  m_WeaponMap[type_shotgun]         = 0;
  m_WeaponMap[type_rail_gun]        = 0;
  m_WeaponMap[type_rocket_launcher] = 0;
  m_WeaponMap[type_knife]			= 0;

  AddWeapon(type_knife);

  InitializeFuzzyModule();
}

//-------------------------------- SelectWeapon -------------------------------
//
//-----------------------------------------------------------------------------
void Raven_WeaponSystem::SelectWeapon()
{ 
  //if a target is present use fuzzy logic to determine the most desirable 
  //weapon.
  if (m_pOwner->GetTargetSys()->isTargetPresent())
  {
    //calculate the distance to the target
    double DistToTarget = Vec2DDistance(m_pOwner->Pos(), m_pOwner->GetTargetSys()->GetTarget()->Pos());

    //for each weapon in the inventory calculate its desirability given the 
    //current situation. The most desirable weapon is selected
    double BestSoFar = MinDouble;

    WeaponMap::const_iterator curWeap;
    for (curWeap=m_WeaponMap.begin(); curWeap != m_WeaponMap.end(); ++curWeap)
    {
      //grab the desirability of this weapon (desirability is based upon
      //distance to target and ammo remaining)
      if (curWeap->second)
      {
        double score = curWeap->second->GetDesirability(DistToTarget);

        //if it is the most desirable so far select it
        if (score > BestSoFar)
        {
          BestSoFar = score;

          //place the weapon in the bot's hand.
          m_pCurrentWeapon = curWeap->second;
        }
      }
    }
  }

  else
  {
    m_pCurrentWeapon = m_WeaponMap[type_blaster];
  }
}

//--------------------  AddWeapon ------------------------------------------
//
//  this is called by a weapon affector and will add a weapon of the specified
//  type to the bot's inventory.
//
//  if the bot already has a weapon of this type then only the ammo is added
//-----------------------------------------------------------------------------
void  Raven_WeaponSystem::AddWeapon(unsigned int weapon_type)
{
  //create an instance of this weapon
  Raven_Weapon* w = 0;

  switch(weapon_type)
  {
  case type_rail_gun:

    w = new RailGun(m_pOwner); break;

  case type_shotgun:

    w = new ShotGun(m_pOwner); break;

  case type_rocket_launcher:

    w = new RocketLauncher(m_pOwner); break;

  case type_knife:
	w = new Knife(m_pOwner); break;

  }//end switch
  

  //if the bot already holds a weapon of this type, just add its ammo
  Raven_Weapon* present = GetWeaponFromInventory(weapon_type);

  if (present)
  {
    present->IncrementRounds(w->NumRoundsRemaining());

    delete w;
  }
  
  //if not already holding, add to inventory
  else
  {
    m_WeaponMap[weapon_type] = w;
  }
}


//------------------------- GetWeaponFromInventory -------------------------------
//
//  returns a pointer to any matching weapon.
//
//  returns a null pointer if the weapon is not present
//-----------------------------------------------------------------------------
Raven_Weapon* Raven_WeaponSystem::GetWeaponFromInventory(int weapon_type)
{
  return m_WeaponMap[weapon_type];
}

//----------------------- ChangeWeapon ----------------------------------------
void Raven_WeaponSystem::ChangeWeapon(unsigned int type)
{
  Raven_Weapon* w = GetWeaponFromInventory(type);

  if (w) m_pCurrentWeapon = w;
}

//--------------------------- TakeAimAndShoot ---------------------------------
//
//  this method aims the bots current weapon at the target (if there is a
//  target) and, if aimed correctly, fires a round
//-----------------------------------------------------------------------------
void Raven_WeaponSystem::TakeAimAndShoot()
{
  //aim the weapon only if the current target is shootable or if it has only
  //very recently gone out of view (this latter condition is to ensure the 
  //weapon is aimed at the target even if it temporarily dodges behind a wall
  //or other cover)


  if ( m_pOwner->GetTargetSys()->GetTarget() && m_pOwner->GetEquipe() != m_pOwner->GetTargetBot()->GetEquipe() && (m_pOwner->GetTargetSys()->isTargetShootable() ||
      (m_pOwner->GetTargetSys()->GetTimeTargetHasBeenOutOfView() < 
	  m_dAimPersistance)))
  {

#ifdef LOG_CREATIONAL_STUFF
	  debug_con << "Bot ID : " << m_pOwner->ID() << " Bot target ID : " << m_pOwner->GetTargetBot()->ID() << " Shootable : " << m_pOwner->GetTargetSys()->isTargetShootable() << "";
#endif

    //the position the weapon will be aimed at
    Vector2D AimingPos = m_pOwner->GetTargetBot()->Pos();

	double time = m_pOwner->GetTargetSys()->GetTimeTargetHasBeenVisible();
	Vector2D velocity = m_pOwner->GetTargetSys()->GetTarget()->Velocity();
	Vector2D PosOwner = m_pOwner->GetTargetBot()->Pos();
	Vector2D PosBot = m_pOwner->GetTargetSys()->GetTarget()->Pos();
	double distToTarget = (PosBot - PosOwner).Length();

	double precision = GetPrecision(distToTarget, velocity, time); // const enlev�
    
    //if the current weapon is not an instant hit type gun the target position
    //must be adjusted to take into account the predicted movement of the 
    //target
    if (GetCurrentWeapon()->GetType() == type_rocket_launcher ||
        GetCurrentWeapon()->GetType() == type_blaster)
    {
      //AimingPos = PredictFuturePositionOfTarget();

      //if the weapon is aimed correctly, there is line of sight between the
      //bot and the aiming position and it has been in view for a period longer
      //than the bot's reaction time, shoot the weapon
      if ( m_pOwner->RotateFacingTowardPosition(AimingPos) &&
           (m_pOwner->GetTargetSys()->GetTimeTargetHasBeenVisible() >
            m_dReactionTime) &&
           m_pOwner->hasLOSto(AimingPos) )
      {

        AddNoiseToAim(AimingPos, precision);

        GetCurrentWeapon()->ShootAt(AimingPos);
      }
    }

	else if(GetCurrentWeapon()->GetType() == type_knife){
		GetCurrentWeapon()->ShootAt(AimingPos);
	}

    //no need to predict movement, aim directly at target
    else
    {
      //if the weapon is aimed correctly and it has been in view for a period
      //longer than the bot's reaction time, shoot the weapon
      if ( m_pOwner->RotateFacingTowardPosition(AimingPos) &&
           (m_pOwner->GetTargetSys()->GetTimeTargetHasBeenVisible() >
            m_dReactionTime) )
      {
        AddNoiseToAim(AimingPos, precision);
        
        GetCurrentWeapon()->ShootAt(AimingPos);
      }
    }

  }
  
  //no target to shoot at so rotate facing to be parallel with the bot's
  //heading direction
  else
  {
    m_pOwner->RotateFacingTowardPosition(m_pOwner->Pos()+ m_pOwner->Heading());
  }
}

void Raven_WeaponSystem::InitializeFuzzyModule(){
  FuzzyVariable& DistToTarget = m_FuzzyModuleAim.CreateFLV("DistToTarget");
  FzSet& Target_Close = DistToTarget.AddLeftShoulderSet("Target_Close", 0, 15, 45);
  FzSet& Target_Medium = DistToTarget.AddTriangularSet("Target_Medium", 15, 75, 150);
  FzSet& Target_Far = DistToTarget.AddRightShoulderSet("Target_Far", 75, 150, 10000);
  
  FuzzyVariable& TimeVisible = m_FuzzyModuleAim.CreateFLV("TimeVisible");
  FzSet& TimeVisible_fast = TimeVisible.AddRightShoulderSet("TimeVisible_fast", 4, 6, 100);
  FzSet& TimeVisible_medium = TimeVisible.AddTriangularSet("TimeVisible_medium", 2, 4, 6);
  FzSet& TimeVisible_slow = TimeVisible.AddLeftShoulderSet("TimeVisible_slow", 0, 2, 4);

  FuzzyVariable& Velocity = m_FuzzyModuleAim.CreateFLV("Velocity");
  FzSet& Velocity_fast = Velocity.AddRightShoulderSet("Velocity_fast", 75, 150, 300);
  FzSet& Velocity_medium = Velocity.AddTriangularSet("Velocity_medium", 15, 75, 150);
  FzSet& Velocity_slow = Velocity.AddLeftShoulderSet("Velocity_slow", 0, 15, 45);

  FuzzyVariable& Deviation = m_FuzzyModuleAim.CreateFLV("Deviation"); 
  FzSet& BigDeviation = Deviation.AddRightShoulderSet("BigDeviation", 0.15, 0.25, 0.3);
  FzSet& DeviationMedium = Deviation.AddTriangularSet("DeviationMedium", 0.05, 0.15, 0.2);
  FzSet& SmallDeviation = Deviation.AddLeftShoulderSet("SmallDeviation", 0, 0.05, 0.1);


  // Target Close //
  m_FuzzyModuleAim.AddRule(FzAND(Target_Close, TimeVisible_fast, Velocity_fast ), BigDeviation);
  m_FuzzyModuleAim.AddRule(FzAND(Target_Close, TimeVisible_fast, Velocity_medium ), BigDeviation );
  m_FuzzyModuleAim.AddRule(FzAND(Target_Close, TimeVisible_fast, Velocity_slow ), DeviationMedium );

  m_FuzzyModuleAim.AddRule(FzAND(Target_Close, TimeVisible_medium, Velocity_fast ), BigDeviation);
  m_FuzzyModuleAim.AddRule(FzAND(Target_Close, TimeVisible_medium, Velocity_medium ), DeviationMedium );
  m_FuzzyModuleAim.AddRule(FzAND(Target_Close, TimeVisible_medium, Velocity_slow ), SmallDeviation );

  m_FuzzyModuleAim.AddRule(FzAND(Target_Close, TimeVisible_slow, Velocity_fast ), DeviationMedium);
  m_FuzzyModuleAim.AddRule(FzAND(Target_Close, TimeVisible_slow, Velocity_medium ), SmallDeviation );
  m_FuzzyModuleAim.AddRule(FzAND(Target_Close, TimeVisible_slow, Velocity_slow ), SmallDeviation );

  // Target Medium
  m_FuzzyModuleAim.AddRule(FzAND(Target_Medium, TimeVisible_fast, Velocity_fast ), BigDeviation);
  m_FuzzyModuleAim.AddRule(FzAND(Target_Medium, TimeVisible_fast, Velocity_medium ), BigDeviation );
  m_FuzzyModuleAim.AddRule(FzAND(Target_Medium, TimeVisible_fast, Velocity_slow ), DeviationMedium );

  m_FuzzyModuleAim.AddRule(FzAND(Target_Medium, TimeVisible_medium, Velocity_fast ), BigDeviation);
  m_FuzzyModuleAim.AddRule(FzAND(Target_Medium, TimeVisible_medium, Velocity_medium ), DeviationMedium );
  m_FuzzyModuleAim.AddRule(FzAND(Target_Medium, TimeVisible_medium, Velocity_slow ), SmallDeviation );

  m_FuzzyModuleAim.AddRule(FzAND(Target_Medium, TimeVisible_slow, Velocity_fast ), DeviationMedium);
  m_FuzzyModuleAim.AddRule(FzAND(Target_Medium, TimeVisible_slow, Velocity_medium ), SmallDeviation );
  m_FuzzyModuleAim.AddRule(FzAND(Target_Medium, TimeVisible_slow, Velocity_slow ), SmallDeviation );

  //Target Far
  m_FuzzyModuleAim.AddRule(FzAND(Target_Far, TimeVisible_fast, Velocity_fast ), BigDeviation);
  m_FuzzyModuleAim.AddRule(FzAND(Target_Far, TimeVisible_fast, Velocity_medium ), BigDeviation );
  m_FuzzyModuleAim.AddRule(FzAND(Target_Far, TimeVisible_fast, Velocity_slow ), DeviationMedium );

  m_FuzzyModuleAim.AddRule(FzAND(Target_Far, TimeVisible_medium, Velocity_fast ), BigDeviation);
  m_FuzzyModuleAim.AddRule(FzAND(Target_Far, TimeVisible_medium, Velocity_medium ), DeviationMedium );
  m_FuzzyModuleAim.AddRule(FzAND(Target_Far, TimeVisible_medium, Velocity_slow ), SmallDeviation );

  m_FuzzyModuleAim.AddRule(FzAND(Target_Far, TimeVisible_slow, Velocity_fast ), DeviationMedium);
  m_FuzzyModuleAim.AddRule(FzAND(Target_Far, TimeVisible_slow, Velocity_medium ), DeviationMedium );
  m_FuzzyModuleAim.AddRule(FzAND(Target_Far, TimeVisible_slow, Velocity_slow ), DeviationMedium );
  
}

//---------------------------- GetPrecision -----------------------------------
//
//-----------------------------------------------------------------------------
double Raven_WeaponSystem::GetPrecision(double distToTarget, Vector2D velocity, double timeVisibility)
{
	speed = sqrt(std::pow(velocity.x, 2) + std::pow(velocity.y, 2));

	m_FuzzyModuleAim.Fuzzify("DistToTarget", distToTarget);
	m_FuzzyModuleAim.Fuzzify("Velocity", speed);
	m_FuzzyModuleAim.Fuzzify("TimeVisible", timeVisibility);

	m_dLastDeviationScore = m_FuzzyModuleAim.DeFuzzify("Deviation", FuzzyModule::max_av);

	return m_dLastDeviationScore;
}

//---------------------------- AddNoiseToAim ----------------------------------
//
//  adds a random deviation to the firing angle not greater than m_dAimAccuracy 
//  rads
//-----------------------------------------------------------------------------
void Raven_WeaponSystem::AddNoiseToAim(Vector2D& AimingPos, double precision)const
{
  Vector2D toPos = AimingPos - m_pOwner->Pos();

  Vec2DRotateAroundOrigin(toPos, RandInRange(-precision, precision));

  AimingPos = toPos + m_pOwner->Pos();
}

//-------------------------- PredictFuturePositionOfTarget --------------------
//
//  predicts where the target will be located in the time it takes for a
//  projectile to reach it. This uses a similar logic to the Pursuit steering
//  behavior.
//-----------------------------------------------------------------------------
Vector2D Raven_WeaponSystem::PredictFuturePositionOfTarget()const
{
  double MaxSpeed = GetCurrentWeapon()->GetMaxProjectileSpeed();
  
  //if the target is ahead and facing the agent shoot at its current pos
  Vector2D ToEnemy = m_pOwner->GetTargetBot()->Pos() - m_pOwner->Pos();
 
  //the lookahead time is proportional to the distance between the enemy
  //and the pursuer; and is inversely proportional to the sum of the
  //agent's velocities
  double LookAheadTime = ToEnemy.Length() / 
                        (MaxSpeed + m_pOwner->GetTargetBot()->MaxSpeed());
  
  //return the predicted future position of the enemy
  return m_pOwner->GetTargetBot()->Pos() + 
         m_pOwner->GetTargetBot()->Velocity() * LookAheadTime;
}


//------------------ GetAmmoRemainingForWeapon --------------------------------
//
//  returns the amount of ammo remaining for the specified weapon. Return zero
//  if the weapon is not present
//-----------------------------------------------------------------------------
int Raven_WeaponSystem::GetAmmoRemainingForWeapon(unsigned int weapon_type)
{
  if (m_WeaponMap[weapon_type])
  {
    return m_WeaponMap[weapon_type]->NumRoundsRemaining();
  }

  return 0;
}

//---------------------------- ShootAt ----------------------------------------
//
//  shoots the current weapon at the given position
//-----------------------------------------------------------------------------
void Raven_WeaponSystem::ShootAt(Vector2D pos)const
{
  GetCurrentWeapon()->ShootAt(pos);
}

//-------------------------- RenderCurrentWeapon ------------------------------
//-----------------------------------------------------------------------------
void Raven_WeaponSystem::RenderCurrentWeapon()const
{
  GetCurrentWeapon()->Render();
}

void Raven_WeaponSystem::RenderDesirabilities()const
{
  Vector2D p = m_pOwner->Pos();

  int num = 0;
  
  WeaponMap::const_iterator curWeap;
  for (curWeap=m_WeaponMap.begin(); curWeap != m_WeaponMap.end(); ++curWeap)
  {
    if (curWeap->second) num++;
  }

  int offset = 15 * num;

    for (curWeap=m_WeaponMap.begin(); curWeap != m_WeaponMap.end(); ++curWeap)
    {
      if (curWeap->second)
      {
        double score = curWeap->second->GetLastDesirabilityScore();
        std::string type = GetNameOfType(curWeap->second->GetType());

        gdi->TextAtPos(p.x+10.0, p.y-offset, ttos(score) + " " + type);

        offset+=15;
      }
    }
}
