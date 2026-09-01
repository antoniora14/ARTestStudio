#include "DiagramSerializationSupport.h"

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <limits>
#include <utility>

namespace arteststudio::infrastructure::serialization
{
	using application::DiagramStorageLimits;
	using application::StorageError;
	using application::StorageResult;
	using domain::DiagramModel;
	using domain::DiagramSnapshotError;
	using domain::NodeKind;
	using domain::PortId;
	using domain::RestoreSnapshotResult;

	StorageResult Failure(StorageError error, std::wstring detail)
	{
		return {error, std::move(detail)};
	}

	bool IsCoordinateValid(int value) noexcept
	{
		return value >= -kMaximumCoordinate && value <= kMaximumCoordinate;
	}

	bool IsDimensionValid(int value) noexcept
	{
		return value > 0 && value <= kMaximumNodeDimension;
	}

	bool IsPortValid(int value) noexcept
	{
		return value >= static_cast<int>(PortId::Top) && value <= static_cast<int>(PortId::Left);
	}

	StorageResult ToUtf8(std::wstring_view value, std::string& output)
	{
		output.clear();
		if (value.empty())
		{
			return {};
		}
		if (value.size() > static_cast<std::size_t>((std::numeric_limits<int>::max)()))
		{
			return Failure(StorageError::InvalidEncoding, L"The label is too large.");
		}

		const int required = WideCharToMultiByte(
			CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
			nullptr, 0, nullptr, nullptr);
		if (required <= 0)
		{
			return Failure(StorageError::InvalidEncoding, L"A label could not be converted to UTF-8.");
		}

		output.resize(static_cast<std::size_t>(required));
		if (WideCharToMultiByte(
			CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
			output.data(), required, nullptr, nullptr) != required)
		{
			output.clear();
			return Failure(StorageError::InvalidEncoding, L"A label could not be converted to UTF-8.");
		}
		return {};
	}

	StorageResult FromUtf8(std::string_view value, std::wstring& output)
	{
		output.clear();
		if (value.empty())
		{
			return {};
		}
		if (value.size() > static_cast<std::size_t>((std::numeric_limits<int>::max)()))
		{
			return Failure(StorageError::InvalidEncoding, L"The label is too large.");
		}

		const int required = MultiByteToWideChar(
			CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0);
		if (required <= 0)
		{
			return Failure(StorageError::InvalidEncoding, L"The file contains an invalid UTF-8 label.");
		}

		output.resize(static_cast<std::size_t>(required));
		if (MultiByteToWideChar(
			CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
			output.data(), required) != required)
		{
			output.clear();
			return Failure(StorageError::InvalidEncoding, L"The file contains an invalid UTF-8 label.");
		}
		return {};
	}

	StorageResult ValidateUtf8(std::string_view value)
	{
		if (value.empty())
		{
			return {};
		}
		if (value.size() > static_cast<std::size_t>((std::numeric_limits<int>::max)()))
		{
			return Failure(StorageError::InvalidEncoding, L"The UTF-8 content is too large.");
		}
		if (MultiByteToWideChar(
			CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0) <= 0)
		{
			return Failure(StorageError::InvalidEncoding, L"The document contains an invalid UTF-8 byte sequence.");
		}
		return {};
	}

	std::wstring DescribeSnapshotFailure(const RestoreSnapshotResult& result)
	{
		std::wstring detail;
		switch (result.error)
		{
		case DiagramSnapshotError::InvalidNodeId:
			detail = L"The snapshot contains an invalid node identifier.";
			break;
		case DiagramSnapshotError::DuplicateNodeId:
			detail = L"The snapshot contains a duplicate node identifier.";
			break;
		case DiagramSnapshotError::InvalidNodeKind:
			detail = L"The snapshot contains an invalid node kind.";
			break;
		case DiagramSnapshotError::InvalidNodeDimensions:
			detail = L"The snapshot contains invalid node dimensions.";
			break;
		case DiagramSnapshotError::InvalidConnectionId:
			detail = L"The snapshot contains an invalid connection identifier.";
			break;
		case DiagramSnapshotError::DuplicateConnectionId:
			detail = L"The snapshot contains a duplicate connection identifier.";
			break;
		case DiagramSnapshotError::NodeNotFound:
			detail = L"A snapshot connection references a missing node.";
			break;
		case DiagramSnapshotError::InvalidPort:
			detail = L"A snapshot connection uses an invalid port.";
			break;
		case DiagramSnapshotError::IdentifierOverflow:
			detail = L"An identifier does not allow the next value to be generated safely.";
			break;
		case DiagramSnapshotError::AllocationFailure:
			detail = L"Insufficient memory to validate the snapshot.";
			break;
		case DiagramSnapshotError::UnexpectedFailure:
			detail = L"An unexpected error occurred while validating the snapshot.";
			break;
		case DiagramSnapshotError::None:
			return {};
		}

		if (result.identifier != 0)
		{
			detail += L" Identificador: ";
			detail += std::to_wstring(result.identifier);
			detail += L".";
		}
		return detail;
	}

	StorageResult ValidateSnapshotForStorage(const domain::DiagramSnapshot& snapshot)
	{
		const RestoreSnapshotResult validation = DiagramModel::ValidateSnapshot(snapshot);
		if (!validation)
		{
			return Failure(StorageError::InvalidData, DescribeSnapshotFailure(validation));
		}
		if (snapshot.nodes.size() > kMaximumNodes || snapshot.connections.size() > kMaximumConnections)
		{
			return Failure(StorageError::DataLimitExceeded, L"The diagram contains too many elements.");
		}

		for (const domain::Node& node : snapshot.nodes)
		{
			if (!IsCoordinateValid(node.position.x) || !IsCoordinateValid(node.position.y) ||
				!IsDimensionValid(node.width) || !IsDimensionValid(node.height) ||
				(node.kind != NodeKind::Rectangle && node.kind != NodeKind::Diamond))
			{
				return Failure(StorageError::InvalidData, L"The model contains an invalid node.");
			}

			std::string label;
			const StorageResult conversion = ToUtf8(node.label, label);
			if (!conversion)
			{
				return conversion;
			}
			if (label.size() > DiagramStorageLimits::MaximumLabelBytes)
			{
				return Failure(StorageError::DataLimitExceeded, L"A label exceeds the 64 KB limit.");
			}
		}

		for (const domain::Connection& connection : snapshot.connections)
		{
			if (connection.intermediatePoints.size() > kMaximumRoutePoints)
			{
				return Failure(StorageError::DataLimitExceeded, L"A connection contains too many route points.");
			}
			for (const domain::Point point : connection.intermediatePoints)
			{
				if (!IsCoordinateValid(point.x) || !IsCoordinateValid(point.y))
				{
					return Failure(StorageError::InvalidData, L"A route contains invalid coordinates.");
				}
			}
		}
		return {};
	}
}
