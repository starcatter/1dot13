#pragma once

#include <cstdint>

namespace ja2::strategic
{
	struct EnemyGroupBattleCounts
	{
		std::uint16_t admins = 0;
		std::uint16_t troops = 0;
		std::uint16_t elites = 0;
		std::uint16_t robots = 0;
		std::uint16_t tanks = 0;
		std::uint16_t jeeps = 0;
	};

	constexpr bool hasMembersInBattle(const EnemyGroupBattleCounts& counts)
	{
		return counts.admins != 0 || counts.troops != 0 || counts.elites != 0 ||
			counts.robots != 0 || counts.tanks != 0 || counts.jeeps != 0;
	}

	constexpr bool shouldHoldArrivalForLoadedBattle(
		bool isEnemyGroup,
		bool isBetweenSectors,
		bool worldLoaded,
		bool tacticalEnemiesPresent,
		bool hasMatchingTacticalSoldier,
		std::uint8_t groupSectorX,
		std::uint8_t groupSectorY,
		std::int8_t groupSectorZ,
		std::int16_t worldSectorX,
		std::int16_t worldSectorY,
		std::int8_t worldSectorZ,
		const EnemyGroupBattleCounts& counts)
	{
		return isEnemyGroup && isBetweenSectors && worldLoaded && tacticalEnemiesPresent &&
			hasMatchingTacticalSoldier &&
			groupSectorX == worldSectorX && groupSectorY == worldSectorY &&
			groupSectorZ == worldSectorZ && hasMembersInBattle(counts);
	}

	constexpr bool shouldRepairAdvancedLoadedBattleGroup(
		bool isEnemyGroup,
		bool isBetweenSectors,
		bool tacticalEnemiesPresent,
		bool hasMatchingTacticalSoldier,
		std::uint8_t groupSectorX,
		std::uint8_t groupSectorY,
		std::int8_t groupSectorZ,
		std::uint8_t groupPreviousX,
		std::uint8_t groupPreviousY,
		std::int16_t worldSectorX,
		std::int16_t worldSectorY,
		std::int8_t worldSectorZ,
		const EnemyGroupBattleCounts& counts)
	{
		return isEnemyGroup && isBetweenSectors && tacticalEnemiesPresent &&
			hasMatchingTacticalSoldier && hasMembersInBattle(counts) &&
			groupSectorZ == worldSectorZ &&
			(groupSectorX != worldSectorX || groupSectorY != worldSectorY) &&
			groupPreviousX == worldSectorX && groupPreviousY == worldSectorY;
	}
}
