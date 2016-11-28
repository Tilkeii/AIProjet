#ifndef Knife_H
#define Knife_H
#pragma warning (disable:4786)
//-----------------------------------------------------------------------------
//
//  Name:   Weapon_Knife.h
//
//  Author: Mat Buckland (www.ai-junkie.com)
//
//  Desc:   class to implement a shot gun
//-----------------------------------------------------------------------------
#include "armory\Raven_Weapon.h"


class  Raven_Bot;



class Knife : public Raven_Weapon
{
private:

  void     InitializeFuzzyModule();


public:

  Knife(Raven_Bot* owner);

  void  Render();

  void  ShootAt(Vector2D pos);

  double GetDesirability(double DistToTarget);
};



#endif