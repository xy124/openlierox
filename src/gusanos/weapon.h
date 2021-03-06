#ifndef WEAPON_H
#define WEAPON_H

#include "netstream.h"
#include "timer_event.h"
#include "glua.h"
#include "util/FixedPointNumber.h"

struct ALLEGRO_BITMAP;
class CWorm;
class WeaponType;

class Weapon : public LuaObject
{
public:
	
	friend class CWorm;
	
	static LuaReference metaTable;
	//static int const luaID = 4;

	enum Actions
	{
		PRIMARY_TRIGGER,
		SECONDARY_TRIGGER
	};
	
	enum GameEvents
	{
		RELOADED = 0,
		OUTOFAMMO,
		SHOOT,
		OutOfAmmoCheck,
		AmmoCorrection,
		GameEventsCount
	};
		
	Weapon(WeaponType* type, CWorm* owner);
	~Weapon();
	
	void think( bool isFocused, size_t index );
	
	void reset();
	
#ifndef DEDICATED_ONLY
	void drawBottom(ALLEGRO_BITMAP* where,int x, int y);
	void drawTop(ALLEGRO_BITMAP* where,int x, int y);
#endif
	
	void actionStart( Actions action );
	void actionStop( Actions action );
	void recieveMessage( BitStream* data );
	
	void delay( int time );
	void useAmmo( int amount );
	CWorm* getOwner();
	
	WeaponType* getType() { return m_type; }
	
	int getReloadTime() { return reloadTime.asInt(); }
	int getAmmo() { return ammo; }
	
	virtual void makeReference();
	virtual void finalize();
	
	bool reloading;

private:
	
	void reload();
	void outOfAmmo();
	
	bool m_outOfAmmo;
	bool outOfAmmoCheck;
	bool sentOutOfAmmo;
	
	bool primaryShooting;
	int ammo;
	int inactiveTime;
	FixedPointNumber<100> reloadTime;
	
	std::vector< TimerEvent::State > timer;
	std::vector< TimerEvent::State > activeTimer;
	std::vector< TimerEvent::State > shootTimer;

	WeaponType* m_type;
	CWorm* m_owner;
};

#endif  // _WEAPON_H_
