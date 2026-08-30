#include "JsonDiagramSerializer.h"
#include "DiagramDocumentDto.h"
#include "DiagramSerializationSupport.h"

#include <nlohmann/json.hpp>

#include <cctype>
#include <limits>
#include <string>
#include <utility>

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
		using dto::ConnectionDto;
		using dto::DiagramDocumentDto;
		using dto::EndpointDto;
		using dto::NodeDto;
		using dto::PointDto;
		using json = nlohmann::json;
		using serialization::Failure;

		[[nodiscard]] std::string_view TrimJsonPrefix(std::string_view value) noexcept
		{
			if (value.starts_with("\xEF\xBB\xBF"))
			{
				value.remove_prefix(3);
			}
			while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())) != 0)
			{
				value.remove_prefix(1);
			}
			return value;
		}

		[[nodiscard]] const char* NodeKindName(NodeKind kind) noexcept
		{
			return kind == NodeKind::Diamond ? "diamond" : "rectangle";
		}

		[[nodiscard]] bool TryParseNodeKind(std::string_view value, NodeKind& kind) noexcept
		{
			if (value == "rectangle")
			{
				kind = NodeKind::Rectangle;
				return true;
			}
			if (value == "diamond")
			{
				kind = NodeKind::Diamond;
				return true;
			}
			return false;
		}

		[[nodiscard]] const char* PortName(PortId port) noexcept
		{
			switch (port)
			{
			case PortId::Top: return "top";
			case PortId::Right: return "right";
			case PortId::Bottom: return "bottom";
			case PortId::Left: return "left";
			}
			return "invalid";
		}

		[[nodiscard]] bool TryParsePort(std::string_view value, PortId& port) noexcept
		{
			if (value == "top") port = PortId::Top;
			else if (value == "right") port = PortId::Right;
			else if (value == "bottom") port = PortId::Bottom;
			else if (value == "left") port = PortId::Left;
			else return false;
			return true;
		}

		[[nodiscard]] StorageResult ReadUnsigned(
			const json& object,
			const char* field,
			std::uint64_t& output)
		{
			const auto iterator = object.find(field);
			if (iterator == object.end() ||
				(!iterator->is_number_unsigned() && !iterator->is_number_integer()))
			{
				return Failure(StorageError::InvalidData, L"Falta un identificador numerico valido en el JSON.");
			}
			if (iterator->is_number_unsigned())
			{
				output = iterator->get<std::uint64_t>();
				return {};
			}
			const std::int64_t signedValue = iterator->get<std::int64_t>();
			if (signedValue < 0)
			{
				return Failure(StorageError::InvalidData, L"Un identificador JSON no puede ser negativo.");
			}
			output = static_cast<std::uint64_t>(signedValue);
			return {};
		}

		[[nodiscard]] StorageResult ReadInt(const json& object, const char* field, int& output)
		{
			const auto iterator = object.find(field);
			if (iterator == object.end() ||
				(!iterator->is_number_integer() && !iterator->is_number_unsigned()))
			{
				return Failure(StorageError::InvalidData, L"Falta un valor entero valido en el JSON.");
			}

			if (iterator->is_number_unsigned())
			{
				const std::uint64_t value = iterator->get<std::uint64_t>();
				if (value > static_cast<std::uint64_t>((std::numeric_limits<int>::max)()))
				{
					return Failure(StorageError::InvalidData, L"Un entero JSON esta fuera del rango permitido.");
				}
				output = static_cast<int>(value);
				return {};
			}

			const std::int64_t value = iterator->get<std::int64_t>();
			if (value < (std::numeric_limits<int>::min)() || value > (std::numeric_limits<int>::max)())
			{
				return Failure(StorageError::InvalidData, L"Un entero JSON esta fuera del rango permitido.");
			}
			output = static_cast<int>(value);
			return {};
		}

		[[nodiscard]] StorageResult ReadString(
			const json& object,
			const char* field,
			std::string& output,
			std::size_t maximumBytes)
		{
			const auto iterator = object.find(field);
			if (iterator == object.end() || !iterator->is_string())
			{
				return Failure(StorageError::InvalidData, L"Falta un texto requerido en el JSON.");
			}
			output = iterator->get<std::string>();
			if (output.size() > maximumBytes)
			{
				return Failure(StorageError::DataLimitExceeded, L"Un texto JSON excede el limite permitido.");
			}
			return {};
		}

		[[nodiscard]] StorageResult ReadPoint(const json& value, PointDto& point)
		{
			if (!value.is_object())
			{
				return Failure(StorageError::InvalidData, L"Un punto JSON no es un objeto.");
			}
			StorageResult result = ReadInt(value, "x", point.x);
			if (!result) return result;
			result = ReadInt(value, "y", point.y);
			if (!result) return result;
			if (!serialization::IsCoordinateValid(point.x) || !serialization::IsCoordinateValid(point.y))
			{
				return Failure(StorageError::InvalidData, L"Un punto JSON contiene coordenadas invalidas.");
			}
			return {};
		}

		[[nodiscard]] StorageResult ReadEndpoint(const json& value, EndpointDto& endpoint)
		{
			if (!value.is_object())
			{
				return Failure(StorageError::InvalidData, L"Un extremo de conexion JSON no es un objeto.");
			}
			StorageResult result = ReadUnsigned(value, "nodeId", endpoint.nodeId);
			if (!result) return result;
			return ReadString(value, "port", endpoint.port, 16);
		}

		[[nodiscard]] StorageResult SnapshotToDto(
			const DiagramSnapshot& snapshot,
			DiagramDocumentDto& document)
		{
			const StorageResult validation = serialization::ValidateSnapshotForStorage(snapshot);
			if (!validation)
			{
				return validation;
			}

			document.nodes.reserve(snapshot.nodes.size());
			for (const Node& node : snapshot.nodes)
			{
				NodeDto dtoNode;
				dtoNode.id = node.id.value;
				dtoNode.kind = NodeKindName(node.kind);
				dtoNode.position = {node.position.x, node.position.y};
				dtoNode.width = node.width;
				dtoNode.height = node.height;
				const StorageResult conversion = serialization::ToUtf8(node.label, dtoNode.label);
				if (!conversion) return conversion;
				document.nodes.push_back(std::move(dtoNode));
			}

			document.connections.reserve(snapshot.connections.size());
			for (const Connection& connection : snapshot.connections)
			{
				ConnectionDto dtoConnection;
				dtoConnection.id = connection.id.value;
				dtoConnection.from = {connection.from.nodeId.value, PortName(connection.from.portId)};
				dtoConnection.to = {connection.to.nodeId.value, PortName(connection.to.portId)};
				dtoConnection.route.reserve(connection.intermediatePoints.size());
				for (const Point point : connection.intermediatePoints)
				{
					dtoConnection.route.push_back({point.x, point.y});
				}
				document.connections.push_back(std::move(dtoConnection));
			}
			return {};
		}

		[[nodiscard]] json DtoToJson(const DiagramDocumentDto& document)
		{
			json nodes = json::array();
			for (const NodeDto& node : document.nodes)
			{
				nodes.push_back({
					{"id", node.id},
					{"kind", node.kind},
					{"position", {{"x", node.position.x}, {"y", node.position.y}}},
					{"size", {{"width", node.width}, {"height", node.height}}},
					{"label", node.label}});
			}

			json connections = json::array();
			for (const ConnectionDto& connection : document.connections)
			{
				json route = json::array();
				for (const PointDto& point : connection.route)
				{
					route.push_back({{"x", point.x}, {"y", point.y}});
				}
				connections.push_back({
					{"id", connection.id},
					{"from", {{"nodeId", connection.from.nodeId}, {"port", connection.from.port}}},
					{"to", {{"nodeId", connection.to.nodeId}, {"port", connection.to.port}}},
					{"route", std::move(route)}});
			}

			return {
				{"format", document.format},
				{"version", document.version},
				{"nodes", std::move(nodes)},
				{"connections", std::move(connections)}};
		}

		[[nodiscard]] StorageResult JsonToDto(const json& root, DiagramDocumentDto& document)
		{
			if (!root.is_object())
			{
				return Failure(StorageError::InvalidFormat, L"La raiz del documento JSON debe ser un objeto.");
			}

			std::string format;
			StorageResult result = ReadString(root, "format", format, 64);
			if (!result || format != dto::kDiagramFormat)
			{
				return Failure(StorageError::InvalidFormat, L"El identificador de formato JSON no corresponde a ARTestStudio.Diagram.");
			}

			int version = 0;
			result = ReadInt(root, "version", version);
			if (!result) return result;
			if (version != dto::kJsonFormatVersion)
			{
				return Failure(StorageError::UnsupportedVersion, L"Version JSON encontrada: " + std::to_wstring(version));
			}

			const auto nodes = root.find("nodes");
			const auto connections = root.find("connections");
			if (nodes == root.end() || !nodes->is_array() || connections == root.end() || !connections->is_array())
			{
				return Failure(StorageError::InvalidData, L"El JSON debe contener arreglos nodes y connections.");
			}
			if (nodes->size() > serialization::kMaximumNodes ||
				connections->size() > serialization::kMaximumConnections)
			{
				return Failure(StorageError::DataLimitExceeded, L"El JSON contiene demasiados elementos.");
			}

			document.nodes.reserve(nodes->size());
			for (const json& value : *nodes)
			{
				if (!value.is_object())
				{
					return Failure(StorageError::InvalidData, L"Un bloque JSON no es un objeto.");
				}
				NodeDto node;
				result = ReadUnsigned(value, "id", node.id);
				if (!result) return result;
				result = ReadString(value, "kind", node.kind, 32);
				if (!result) return result;
				const auto position = value.find("position");
				const auto size = value.find("size");
				if (position == value.end() || size == value.end() || !size->is_object())
				{
					return Failure(StorageError::InvalidData, L"Un bloque JSON no contiene position o size validos.");
				}
				result = ReadPoint(*position, node.position);
				if (!result) return result;
				result = ReadInt(*size, "width", node.width);
				if (!result) return result;
				result = ReadInt(*size, "height", node.height);
				if (!result) return result;
				if (!serialization::IsDimensionValid(node.width) || !serialization::IsDimensionValid(node.height))
				{
					return Failure(StorageError::InvalidData, L"Un bloque JSON contiene dimensiones invalidas.");
				}
				result = ReadString(value, "label", node.label, DiagramStorageLimits::MaximumLabelBytes);
				if (!result) return result;
				document.nodes.push_back(std::move(node));
			}

			document.connections.reserve(connections->size());
			for (const json& value : *connections)
			{
				if (!value.is_object())
				{
					return Failure(StorageError::InvalidData, L"Una conexion JSON no es un objeto.");
				}
				ConnectionDto connection;
				result = ReadUnsigned(value, "id", connection.id);
				if (!result) return result;
				const auto from = value.find("from");
				const auto to = value.find("to");
				const auto route = value.find("route");
				if (from == value.end() || to == value.end() || route == value.end() || !route->is_array())
				{
					return Failure(StorageError::InvalidData, L"Una conexion JSON no contiene from, to o route validos.");
				}
				if (route->size() > serialization::kMaximumRoutePoints)
				{
					return Failure(StorageError::DataLimitExceeded, L"Una ruta JSON contiene demasiados puntos.");
				}
				result = ReadEndpoint(*from, connection.from);
				if (!result) return result;
				result = ReadEndpoint(*to, connection.to);
				if (!result) return result;
				connection.route.reserve(route->size());
				for (const json& pointValue : *route)
				{
					PointDto point;
					result = ReadPoint(pointValue, point);
					if (!result) return result;
					connection.route.push_back(point);
				}
				document.connections.push_back(std::move(connection));
			}
			return {};
		}

		[[nodiscard]] StorageResult DtoToSnapshot(
			const DiagramDocumentDto& document,
			DiagramSnapshot& snapshot)
		{
			snapshot.nodes.reserve(document.nodes.size());
			for (const NodeDto& dtoNode : document.nodes)
			{
				NodeKind kind;
				if (!TryParseNodeKind(dtoNode.kind, kind))
				{
					return Failure(StorageError::InvalidData, L"Un bloque JSON contiene un kind desconocido.");
				}
				std::wstring label;
				const StorageResult conversion = serialization::FromUtf8(dtoNode.label, label);
				if (!conversion) return conversion;
				snapshot.nodes.push_back(Node{
					NodeId{dtoNode.id}, kind, {dtoNode.position.x, dtoNode.position.y},
					dtoNode.width, dtoNode.height, std::move(label)});
			}

			snapshot.connections.reserve(document.connections.size());
			for (const ConnectionDto& dtoConnection : document.connections)
			{
				PortId fromPort;
				PortId toPort;
				if (!TryParsePort(dtoConnection.from.port, fromPort) ||
					!TryParsePort(dtoConnection.to.port, toPort))
				{
					return Failure(StorageError::InvalidData, L"Una conexion JSON contiene un puerto desconocido.");
				}
				std::vector<Point> route;
				route.reserve(dtoConnection.route.size());
				for (const PointDto& point : dtoConnection.route)
				{
					route.push_back({point.x, point.y});
				}
				snapshot.connections.push_back(Connection{
					ConnectionId{dtoConnection.id},
					{NodeId{dtoConnection.from.nodeId}, fromPort},
					{NodeId{dtoConnection.to.nodeId}, toPort},
					std::move(route)});
			}
			return serialization::ValidateSnapshotForStorage(snapshot);
		}
	}

	bool JsonDiagramSerializer::CanDeserialize(std::string_view content) const noexcept
	{
		content = TrimJsonPrefix(content);
		return !content.empty() && content.front() == '{';
	}

	application::StorageResult JsonDiagramSerializer::Serialize(
		const domain::DiagramModel& diagram,
		std::string& content) const noexcept
	{
		try
		{
			DiagramDocumentDto document;
			const StorageResult mapping = SnapshotToDto(diagram.CaptureSnapshot(), document);
			if (!mapping) return mapping;

			content = DtoToJson(document).dump(2);
			content.push_back('\n');
			if (content.size() > DiagramStorageLimits::MaximumFileBytes)
			{
				content.clear();
				return Failure(StorageError::FileTooLarge, L"El contenido JSON serializado excede 16 MB.");
			}
			return {};
		}
		catch (const std::bad_alloc&)
		{
			return Failure(StorageError::IoFailure, L"No hay memoria suficiente para generar el documento JSON.");
		}
		catch (...)
		{
			return Failure(StorageError::IoFailure, L"No se pudo generar el documento JSON.");
		}
	}

	application::StorageResult JsonDiagramSerializer::Deserialize(
		std::string_view content,
		domain::DiagramModel& diagram) const noexcept
	{
		try
		{
			content = TrimJsonPrefix(content);
			const StorageResult encoding = serialization::ValidateUtf8(content);
			if (!encoding) return encoding;
			const json root = json::parse(content, nullptr, false);
			if (root.is_discarded())
			{
				return Failure(StorageError::InvalidFormat, L"El contenido JSON esta truncado o tiene sintaxis invalida.");
			}

			DiagramDocumentDto document;
			StorageResult result = JsonToDto(root, document);
			if (!result) return result;

			DiagramSnapshot snapshot;
			result = DtoToSnapshot(document, snapshot);
			if (!result) return result;

			DiagramModel loaded;
			const domain::RestoreSnapshotResult restored = loaded.RestoreSnapshot(std::move(snapshot));
			if (!restored)
			{
				return Failure(StorageError::InvalidData, serialization::DescribeSnapshotFailure(restored));
			}
			diagram = std::move(loaded);
			return {};
		}
		catch (const nlohmann::json::exception&)
		{
			return Failure(StorageError::InvalidData, L"El JSON contiene un tipo o valor fuera de rango.");
		}
		catch (const std::bad_alloc&)
		{
			return Failure(StorageError::IoFailure, L"No hay memoria suficiente para cargar el documento JSON.");
		}
		catch (...)
		{
			return Failure(StorageError::InvalidData, L"El documento JSON contiene datos no validos.");
		}
	}
}
