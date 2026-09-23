#include "EnemyGroupBattleState.h"

#include <cstdlib>
#include <iostream>

namespace
{
	void Require(bool condition, const char* message)
	{
		if (!condition)
		{
			std::cerr << "FAIL: " << message << '\n';
			std::exit(1);
		}
	}
}

int main()
{
	using namespace ja2::strategic;
	const EnemyGroupBattleCounts fighting{ 0, 9, 0, 0, 0, 0 };
	const EnemyGroupBattleCounts idle{};

	Require(shouldHoldArrivalForLoadedBattle(
		true, true, true, true, true, 5, 3, 0, 5, 3, 0, fighting),
		"an engaged mobile enemy group must not leave the loaded battle sector");
	Require(!shouldHoldArrivalForLoadedBattle(
		true, true, true, true, true, 5, 3, 0, 5, 3, 0, idle),
		"an idle group may complete its arrival");
	Require(!shouldHoldArrivalForLoadedBattle(
		true, true, true, true, true, 6, 3, 0, 5, 3, 0, fighting),
		"an unrelated group must not be held by another sector's battle");
	Require(!shouldHoldArrivalForLoadedBattle(
		true, true, true, false, true, 5, 3, 0, 5, 3, 0, fighting),
		"stale counters alone must not freeze strategic movement");
	Require(!shouldHoldArrivalForLoadedBattle(
		true, true, true, true, false, 5, 3, 0, 5, 3, 0, fighting),
		"a different tactical enemy must not freeze a group with stale counters");

	Require(shouldRepairAdvancedLoadedBattleGroup(
		true, true, true, true, 6, 3, 0, 5, 3, 5, 3, 0, fighting),
		"a group advanced from the loaded battle sector must be rejoined");
	Require(!shouldRepairAdvancedLoadedBattleGroup(
		true, true, true, true, 5, 3, 0, 5, 3, 5, 3, 0, fighting),
		"a group already in the loaded sector is not corrupt");
	Require(!shouldRepairAdvancedLoadedBattleGroup(
		true, true, true, false, 6, 3, 0, 5, 3, 5, 3, 0, fighting),
		"an adjacent reinforcement without a matching tactical soldier is not moved");
	Require(!shouldRepairAdvancedLoadedBattleGroup(
		true, true, true, true, 6, 3, 0, 6, 2, 5, 3, 0, fighting),
		"an unrelated group's previous sector must not be rewritten");

	std::cout << "enemy group battle-state tests passed\n";
	return 0;
}
