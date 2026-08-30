#include "LegacyDiagramSerializer.h"
#include "DiagramSerializationSupport.h"

#include <cctype>
#include <locale>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace arteststudio::infrastructure
{
	namespace
	{
		using application::DiagramStorageLimits;
		using application::StorageError;
		using application::StorageResult;
		using domain::Connection;
		using domain::ConnectionId;
		using domain::DiagramModel;
		using domain::DiagramSnapshot;
		using domain::Node;
		using domain::NodeId;
		using domain::NodeKind;
		using domain::Point;
		using domain::PortId;
		using serialization::Failure;

		constexpr int kLegacyFormatVersion = 1;
		constexpr std::string_view kLegacyHeader = "ARTESTSTUDIO_DIAGRAM";

		enum class QuotedReadResult
		{
			Success,
			Invalid,
			LimitExceeded
		};

		[[nodiscard]] std::string_view TrimStart(std::string_view value) noexcept
		{
			while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())) != 0)
			{
				value.remove_prefix(1);
			}
			return value;
		}

		[[nodiscard]] QuotedReadResult ReadBoundedQuotedString(
			std::istream& input,
			std::string& output,
			std::size_t maximumBytes)
		{
			output.clear();
			input >> std::ws;
			char character = 0;
			if (!input.get(character) || character != '"')
			{
				return QuotedReadResult::Invalid;
			}

			while (input.get(character))
			{
				if (character == '"')
				{
					return QuotedReadResult::Success;
				}
				if (character == '\\' && !input.get(character))
				{
					return QuotedReadResult::Invalid;
				}
				if (output.size() >= maximumBytes)
				{
					return QuotedReadResult::LimitExceeded;
				}
				output.push_back(character);
			}
			return QuotedReadResult::Invalid;
		}
	}

	bool LegacyDiagramSerializer::CanDeserialize(std::string_view content) const noexcept
	{
		content = TrimStart(content);
		return content.starts_with(kLegacyHeader);
	}

	application::StorageResult LegacyDiagramSerializer::Deserialize(
		std::string_view content,
		domain::DiagramModel& diagram) const noexcept
	{
		try
		{
			std::istringstream input(std::string{content});
			input.imbue(std::locale::classic());

			std::string token;
			int version = 0;
			if (!(input >> token >> version) || token != kLegacyHeader)
			{
				return Failure(StorageError::InvalidFormat, L"Falta el encabezado ARTESTSTUDIO_DIAGRAM.");
			}
			if (version != kLegacyFormatVersion)
			{
				return Failure(StorageError::UnsupportedVersion, L"Version encontrada: " + std::to_wstring(version));
			}

			std::uint64_t nodeCount = 0;
			if (!(input >> token >> nodeCount) || token != "NODES" ||
				nodeCount > serialization::kMaximumNodes)
			{
				return Failure(StorageError::InvalidData, L"La seccion NODES es invalida.");
			}

			DiagramSnapshot snapshot;
			snapshot.nodes.reserve(static_cast<std::size_t>(nodeCount));
			for (std::uint64_t index = 0; index < nodeCount; ++index)
			{
				std::uint64_t storedId = 0;
				int kind = 0;
				int x = 0;
				int y = 0;
				int width = 0;
				int height = 0;
				std::string labelBytes;
				if (!(input >> token >> storedId >> kind >> x >> y >> width >> height) ||
					token != "NODE" ||
					(kind != static_cast<int>(NodeKind::Rectangle) && kind != static_cast<int>(NodeKind::Diamond)) ||
					!serialization::IsCoordinateValid(x) || !serialization::IsCoordinateValid(y) ||
					!serialization::IsDimensionValid(width) || !serialization::IsDimensionValid(height))
				{
					return Failure(StorageError::InvalidData, L"Se encontro un bloque invalido.");
				}

				const QuotedReadResult labelRead = ReadBoundedQuotedString(
					input, labelBytes, DiagramStorageLimits::MaximumLabelBytes);
				if (labelRead == QuotedReadResult::LimitExceeded)
				{
					return Failure(StorageError::DataLimitExceeded, L"Una etiqueta excede el limite de 64 KB.");
				}
				if (labelRead != QuotedReadResult::Success)
				{
					return Failure(StorageError::InvalidData, L"La etiqueta de un bloque esta truncada o es invalida.");
				}

				std::wstring label;
				const StorageResult conversion = serialization::FromUtf8(labelBytes, label);
				if (!conversion)
				{
					return conversion;
				}
				snapshot.nodes.push_back(Node{
					NodeId{storedId}, static_cast<NodeKind>(kind), {x, y}, width, height, std::move(label)});
			}

			std::uint64_t connectionCount = 0;
			if (!(input >> token >> connectionCount) || token != "CONNECTIONS" ||
				connectionCount > serialization::kMaximumConnections)
			{
				return Failure(StorageError::InvalidData, L"La seccion CONNECTIONS es invalida.");
			}

			snapshot.connections.reserve(static_cast<std::size_t>(connectionCount));
			for (std::uint64_t index = 0; index < connectionCount; ++index)
			{
				std::uint64_t storedId = 0;
				std::uint64_t fromNodeId = 0;
				std::uint64_t toNodeId = 0;
				int fromPort = 0;
				int toPort = 0;
				std::uint64_t pointCount = 0;
				if (!(input >> token >> storedId >> fromNodeId >> fromPort >> toNodeId >> toPort >> pointCount) ||
					token != "CONNECTION" || !serialization::IsPortValid(fromPort) ||
					!serialization::IsPortValid(toPort) || pointCount > serialization::kMaximumRoutePoints)
				{
					return Failure(StorageError::InvalidData, L"Se encontro una conexion invalida.");
				}

				std::vector<Point> points;
				points.reserve(static_cast<std::size_t>(pointCount));
				for (std::uint64_t pointIndex = 0; pointIndex < pointCount; ++pointIndex)
				{
					Point point;
					if (!(input >> point.x >> point.y) || !serialization::IsCoordinateValid(point.x) ||
						!serialization::IsCoordinateValid(point.y))
					{
						return Failure(StorageError::InvalidData, L"Una conexion contiene una ruta invalida.");
					}
					points.push_back(point);
				}

				snapshot.connections.push_back(Connection{
					ConnectionId{storedId},
					{NodeId{fromNodeId}, static_cast<PortId>(fromPort)},
					{NodeId{toNodeId}, static_cast<PortId>(toPort)},
					std::move(points)});
			}

			if (!(input >> token) || token != "END")
			{
				return Failure(StorageError::InvalidData, L"El documento esta truncado o no contiene END.");
			}
			if (input >> token)
			{
				return Failure(StorageError::InvalidData, L"El documento contiene datos inesperados despues de END.");
			}

			DiagramModel loaded;
			const domain::RestoreSnapshotResult restored = loaded.RestoreSnapshot(std::move(snapshot));
			if (!restored)
			{
				return Failure(StorageError::InvalidData, serialization::DescribeSnapshotFailure(restored));
			}

			diagram = std::move(loaded);
			return {};
		}
		catch (const std::bad_alloc&)
		{
			return Failure(StorageError::IoFailure, L"No hay memoria suficiente para cargar el documento legacy.");
		}
		catch (...)
		{
			return Failure(StorageError::InvalidData, L"El documento legacy contiene datos no validos.");
		}
	}
}
