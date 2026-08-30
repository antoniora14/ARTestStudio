#pragma once

#include "../Application/DiagramStorage.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace arteststudio::infrastructure::serialization
{
	inline constexpr std::uint64_t kMaximumNodes = 10000;
	inline constexpr std::uint64_t kMaximumConnections = 20000;
	inline constexpr std::uint64_t kMaximumRoutePoints = 1000;
	inline constexpr int kMaximumCoordinate = 10000000;
	inline constexpr int kMaximumNodeDimension = 100000;

	[[nodiscard]] application::StorageResult Failure(
		application::StorageError error,
		std::wstring detail = {});

	[[nodiscard]] bool IsCoordinateValid(int value) noexcept;
	[[nodiscard]] bool IsDimensionValid(int value) noexcept;
	[[nodiscard]] bool IsPortValid(int value) noexcept;

	[[nodiscard]] application::StorageResult ToUtf8(
		std::wstring_view value,
		std::string& output);

	[[nodiscard]] application::StorageResult FromUtf8(
		std::string_view value,
		std::wstring& output);

	[[nodiscard]] application::StorageResult ValidateUtf8(std::string_view value);

	[[nodiscard]] std::wstring DescribeSnapshotFailure(
		const domain::RestoreSnapshotResult& result);

	[[nodiscard]] application::StorageResult ValidateSnapshotForStorage(
		const domain::DiagramSnapshot& snapshot);
}
