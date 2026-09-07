#include "WeaponDataTypes.h"

bool FWeaponDataRow::HasValidBalanceData() const
{
	return GroggyDamage >= 0.0f &&
		KnockbackPower >= 0.0f &&
		AttackCooldown >= 0.0f &&
		Weight > 0.0f;
}
