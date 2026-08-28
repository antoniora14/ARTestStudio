#include "TextDiagramStorage.h"
#include "WindowsAtomicFileWriter.h"

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <filesystem>
#include <fstream>
#include <cwchar>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace arteststudio::infrastructure
{
	namespace
	{
		using application::AtomicWriteError;
		using application::AtomicWriteResult;
		using application::DiagramStorageLimits;
		using application::StorageError;
		using application::StorageResult;
		using application::kDiagramFileExtension;
		using domain::Connection;
		using domain::ConnectionId;
		using domain::DiagramModel;
		using domain::DiagramSnapshot;
		using domain::DiagramSnapshotError;
		using domain::Node;
		using domain::NodeId;
		using domain::NodeKind;
		using domain::Point;
		using domain::PortId;
		using domain::RestoreSnapshotResult;

		constexpr int kFormatVersion = 1;
		constexpr std::uint64_t kMaximumNodes = 10000;
		constexpr std::uint64_t kMaximumConnections = 20000;
		constexpr std::uint64_t kMaximumRoutePoints = 1000;
		constexpr int kMaximumCoordinate = 10000000;
		constexpr int kMaximumNodeDimension = 100000;

		[[nodiscard]] StorageResult Failure(StorageError error, std::wstring detail = {})
		{
			return {error, std::move(detail)};
		}

		[[nodiscard]] bool IsCoordinateValid(int value) noexcept
		{
			return value >= -kMaximumCoordinate && value <= kMaximumCoordinate;
		}

		[[nodiscard]] bool IsDimensionValid(int value) noexcept
		{
			return value > 0 && value <= kMaximumNodeDimension;
		}

		[[nodiscard]] bool IsPortValid(int value) noexcept
		{
			return value >= static_cast<int>(PortId::Top) && value <= static_cast<int>(PortId::Left);
		}

		[[nodiscard]] bool HasDiagramFileExtension(const std::filesystem::path& path) noexcept
		{
			const std::wstring extension = path.extension().native();
			return _wcsicmp(extension.c_str(), kDiagramFileExtension.data()) == 0;
		}

		[[nodiscard]] bool ExceedsMaximumFileSize(std::ostringstream& output) noexcept
		{
			const std::streampos position = output.tellp();
			return position < 0 ||
				static_cast<std::uintmax_t>(position) > DiagramStorageLimits::MaximumFileBytes;
		}

		enum class QuotedReadResult
		{
			Success,
			Invalid,
			LimitExceeded
		};

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

		[[nodiscard]] std::wstring DescribeSnapshotFailure(const RestoreSnapshotResult& result)
		{
			std::wstring detail;
			switch (result.error)
			{
			case DiagramSnapshotError::InvalidNodeId:
				detail = L"El snapshot contiene un identificador de bloque invalido.";
				break;
			case DiagramSnapshotError::DuplicateNodeId:
				detail = L"El snapshot contiene un identificador de bloque duplicado.";
				break;
			case DiagramSnapshotError::InvalidNodeKind:
				detail = L"El snapshot contiene un tipo de bloque invalido.";
				break;
			case DiagramSnapshotError::InvalidNodeDimensions:
				detail = L"El snapshot contiene dimensiones de bloque invalidas.";
				break;
			case DiagramSnapshotError::InvalidConnectionId:
				detail = L"El snapshot contiene un identificador de conexion invalido.";
				break;
			case DiagramSnapshotError::DuplicateConnectionId:
				detail = L"El snapshot contiene un identificador de conexion duplicado.";
				break;
			case DiagramSnapshotError::NodeNotFound:
				detail = L"Una conexion del snapshot referencia un bloque inexistente.";
				break;
			case DiagramSnapshotError::InvalidPort:
				detail = L"Una conexion del snapshot utiliza un puerto invalido.";
				break;
			case DiagramSnapshotError::IdentifierOverflow:
				detail = L"Un identificador no permite generar el siguiente valor de forma segura.";
				break;
			case DiagramSnapshotError::AllocationFailure:
				detail = L"No hay memoria suficiente para validar el snapshot.";
				break;
			case DiagramSnapshotError::UnexpectedFailure:
				detail = L"Ocurrio un error inesperado al validar el snapshot.";
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

		[[nodiscard]] StorageResult ToUtf8(std::wstring_view value, std::string& output)
		{
			output.clear();
			if (value.empty())
			{
				return {};
			}

			if (value.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
			{
				return Failure(StorageError::InvalidEncoding, L"La etiqueta es demasiado grande.");
			}

			const int required = WideCharToMultiByte(
				CP_UTF8,
				WC_ERR_INVALID_CHARS,
				value.data(),
				static_cast<int>(value.size()),
				nullptr,
				0,
				nullptr,
				nullptr);
			if (required <= 0)
			{
				return Failure(StorageError::InvalidEncoding, L"No se pudo convertir una etiqueta a UTF-8.");
			}

			output.resize(static_cast<std::size_t>(required));
			if (WideCharToMultiByte(
				CP_UTF8,
				WC_ERR_INVALID_CHARS,
				value.data(),
				static_cast<int>(value.size()),
				output.data(),
				required,
				nullptr,
				nullptr) != required)
			{
				output.clear();
				return Failure(StorageError::InvalidEncoding, L"No se pudo convertir una etiqueta a UTF-8.");
			}

			return {};
		}

		[[nodiscard]] StorageResult FromUtf8(std::string_view value, std::wstring& output)
		{
			output.clear();
			if (value.empty())
			{
				return {};
			}

			if (value.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
			{
				return Failure(StorageError::InvalidEncoding, L"La etiqueta es demasiado grande.");
			}

			const int required = MultiByteToWideChar(
				CP_UTF8,
				MB_ERR_INVALID_CHARS,
				value.data(),
				static_cast<int>(value.size()),
				nullptr,
				0);
			if (required <= 0)
			{
				return Failure(StorageError::InvalidEncoding, L"El archivo contiene una etiqueta UTF-8 invalida.");
			}

			output.resize(static_cast<std::size_t>(required));
			if (MultiByteToWideChar(
				CP_UTF8,
				MB_ERR_INVALID_CHARS,
				value.data(),
				static_cast<int>(value.size()),
				output.data(),
				required) != required)
			{
				output.clear();
				return Failure(StorageError::InvalidEncoding, L"El archivo contiene una etiqueta UTF-8 invalida.");
			}

			return {};
		}

		[[nodiscard]] StorageResult SerializeDiagram(const DiagramModel& diagram, std::string& serialized)
		{
			const DiagramSnapshot snapshot = diagram.CaptureSnapshot();
			const RestoreSnapshotResult validation = DiagramModel::ValidateSnapshot(snapshot);
			if (!validation)
			{
				return Failure(StorageError::InvalidData, DescribeSnapshotFailure(validation));
			}
			if (snapshot.nodes.size() > kMaximumNodes || snapshot.connections.size() > kMaximumConnections)
			{
				return Failure(StorageError::DataLimitExceeded, L"El diagrama contiene demasiados elementos.");
			}

			std::ostringstream output;
			output.imbue(std::locale::classic());
			output << "ARTESTSTUDIO_DIAGRAM " << kFormatVersion << '\n';
			output << "NODES " << snapshot.nodes.size() << '\n';

			for (const Node& node : snapshot.nodes)
			{
				if (!node.id || !IsCoordinateValid(node.position.x) || !IsCoordinateValid(node.position.y) ||
					!IsDimensionValid(node.width) || !IsDimensionValid(node.height) ||
					(node.kind != NodeKind::Rectangle && node.kind != NodeKind::Diamond))
				{
					return Failure(StorageError::InvalidData, L"El modelo contiene un bloque invalido.");
				}

				std::string label;
				const StorageResult conversion = ToUtf8(node.label, label);
				if (!conversion)
				{
					return conversion;
				}
				if (label.size() > DiagramStorageLimits::MaximumLabelBytes)
				{
					return Failure(StorageError::DataLimitExceeded, L"Una etiqueta excede el limite de 64 KB.");
				}

				output << "NODE " << node.id.value << ' ' << static_cast<int>(node.kind) << ' '
					<< node.position.x << ' ' << node.position.y << ' '
					<< node.width << ' ' << node.height << ' ' << std::quoted(label) << '\n';
				if (ExceedsMaximumFileSize(output))
				{
					return Failure(StorageError::FileTooLarge, L"El contenido serializado excede 16 MB.");
				}
			}

			output << "CONNECTIONS " << snapshot.connections.size() << '\n';
			for (const Connection& connection : snapshot.connections)
			{
				if (!connection.id || !connection.from.nodeId || !connection.to.nodeId ||
					!IsPortValid(static_cast<int>(connection.from.portId)) ||
					!IsPortValid(static_cast<int>(connection.to.portId)) ||
					connection.intermediatePoints.size() > kMaximumRoutePoints)
				{
					return Failure(StorageError::InvalidData, L"El modelo contiene una conexion invalida.");
				}

				output << "CONNECTION " << connection.id.value << ' '
					<< connection.from.nodeId.value << ' ' << static_cast<int>(connection.from.portId) << ' '
					<< connection.to.nodeId.value << ' ' << static_cast<int>(connection.to.portId) << ' '
					<< connection.intermediatePoints.size();

				for (const Point point : connection.intermediatePoints)
				{
					if (!IsCoordinateValid(point.x) || !IsCoordinateValid(point.y))
					{
						return Failure(StorageError::InvalidData, L"Una ruta contiene coordenadas invalidas.");
					}
					output << ' ' << point.x << ' ' << point.y;
				}
				output << '\n';
				if (ExceedsMaximumFileSize(output))
				{
					return Failure(StorageError::FileTooLarge, L"El contenido serializado excede 16 MB.");
				}
			}

			output << "END\n";
			if (!output)
			{
				return Failure(StorageError::IoFailure, L"No se pudo generar el contenido del documento.");
			}
			if (ExceedsMaximumFileSize(output))
			{
				return Failure(StorageError::FileTooLarge, L"El contenido serializado excede 16 MB.");
			}

			serialized = output.str();
			return {};
		}

		[[nodiscard]] StorageResult DeserializeDiagram(std::istream& input, DiagramModel& diagram)
		{
			input.imbue(std::locale::classic());
			std::string token;
			int version = 0;
			if (!(input >> token >> version) || token != "ARTESTSTUDIO_DIAGRAM")
			{
				return Failure(StorageError::InvalidFormat, L"Falta el encabezado ARTESTSTUDIO_DIAGRAM.");
			}
			if (version != kFormatVersion)
			{
				return Failure(StorageError::UnsupportedVersion, L"Version encontrada: " + std::to_wstring(version));
			}

			std::uint64_t nodeCount = 0;
			if (!(input >> token >> nodeCount) || token != "NODES" || nodeCount > kMaximumNodes)
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
					!IsCoordinateValid(x) || !IsCoordinateValid(y) ||
					!IsDimensionValid(width) || !IsDimensionValid(height))
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
				const StorageResult conversion = FromUtf8(labelBytes, label);
				if (!conversion)
				{
					return conversion;
				}

				snapshot.nodes.push_back(Node{
					NodeId{storedId}, static_cast<NodeKind>(kind), {x, y}, width, height, std::move(label)});
			}

			std::uint64_t connectionCount = 0;
			if (!(input >> token >> connectionCount) || token != "CONNECTIONS" ||
				connectionCount > kMaximumConnections)
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
					token != "CONNECTION" ||
					!IsPortValid(fromPort) || !IsPortValid(toPort) || pointCount > kMaximumRoutePoints)
				{
					return Failure(StorageError::InvalidData, L"Se encontro una conexion invalida.");
				}

				std::vector<Point> points;
				points.reserve(static_cast<std::size_t>(pointCount));
				for (std::uint64_t pointIndex = 0; pointIndex < pointCount; ++pointIndex)
				{
					Point point;
					if (!(input >> point.x >> point.y) ||
						!IsCoordinateValid(point.x) || !IsCoordinateValid(point.y))
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
			const RestoreSnapshotResult restored = loaded.RestoreSnapshot(std::move(snapshot));
			if (!restored)
			{
				return Failure(StorageError::InvalidData, DescribeSnapshotFailure(restored));
			}

			diagram = std::move(loaded);
			return {};
		}

		[[nodiscard]] StorageResult MapAtomicWriteFailure(const AtomicWriteResult& result)
		{
			switch (result.error)
			{
			case AtomicWriteError::None:
				return {};
			case AtomicWriteError::InvalidPath:
				return Failure(StorageError::InvalidPath, result.detail);
			case AtomicWriteError::AccessDenied:
				return Failure(StorageError::AccessDenied, result.detail);
			case AtomicWriteError::TemporaryCleanupFailure:
			case AtomicWriteError::TemporaryWriteFailure:
				return Failure(StorageError::TemporaryFileFailure, result.detail);
			case AtomicWriteError::ReplacementFailure:
				return Failure(StorageError::ReplacementFailure, result.detail);
			}

			return Failure(StorageError::IoFailure, result.detail);
		}
	}

	TextDiagramStorage::TextDiagramStorage() noexcept
		: m_writer([]() -> const application::IAtomicFileWriter*
			{
				static const WindowsAtomicFileWriter writer;
				return &writer;
			}())
	{
	}

	TextDiagramStorage::TextDiagramStorage(const application::IAtomicFileWriter& writer) noexcept
		: m_writer(&writer)
	{
	}

	application::StorageResult TextDiagramStorage::Load(
		const std::filesystem::path& path,
		domain::DiagramModel& diagram) const noexcept
	{
		try
		{
			if (path.empty())
			{
				return Failure(StorageError::InvalidPath);
			}
			if (!HasDiagramFileExtension(path))
			{
				return Failure(StorageError::UnsupportedFileExtension, L"Se esperaba un archivo con extension .atd.");
			}

			std::error_code existsError;
			const bool exists = std::filesystem::exists(path, existsError);
			if (existsError)
			{
				return Failure(StorageError::IoFailure, L"No se pudo comprobar la existencia del archivo.");
			}
			if (!exists)
			{
				return Failure(StorageError::FileNotFound);
			}

			std::error_code sizeError;
			const std::uintmax_t fileSize = std::filesystem::file_size(path, sizeError);
			if (sizeError)
			{
				return Failure(StorageError::IoFailure, L"No se pudo determinar el tamano del archivo.");
			}
			if (fileSize > DiagramStorageLimits::MaximumFileBytes)
			{
				return Failure(StorageError::FileTooLarge, L"Tamano detectado: " + std::to_wstring(fileSize) + L" bytes.");
			}

			std::ifstream input(path, std::ios::binary);
			if (!input)
			{
				return Failure(StorageError::AccessDenied, L"No se pudo abrir el archivo para lectura.");
			}

			return DeserializeDiagram(input, diagram);
		}
		catch (const std::bad_alloc&)
		{
			return Failure(StorageError::IoFailure, L"No hay memoria suficiente para cargar el documento.");
		}
		catch (const std::exception&)
		{
			return Failure(StorageError::IoFailure, L"Se produjo una excepcion al cargar el documento.");
		}
		catch (...)
		{
			return Failure(StorageError::IoFailure, L"Se produjo un error desconocido al cargar el documento.");
		}
	}

	application::StorageResult TextDiagramStorage::Save(
		const std::filesystem::path& path,
		const domain::DiagramModel& diagram) const noexcept
	{
		try
		{
			if (path.empty() || path.filename().empty())
			{
				return Failure(StorageError::InvalidPath);
			}
			if (!HasDiagramFileExtension(path))
			{
				return Failure(StorageError::UnsupportedFileExtension, L"Se esperaba un archivo con extension .atd.");
			}

			std::string serialized;
			const StorageResult serialization = SerializeDiagram(diagram, serialized);
			if (!serialization)
			{
				return serialization;
			}

			return MapAtomicWriteFailure(m_writer->Write(path, serialized));
		}
		catch (const std::bad_alloc&)
		{
			return Failure(StorageError::IoFailure, L"No hay memoria suficiente para guardar el documento.");
		}
		catch (const std::exception&)
		{
			return Failure(StorageError::IoFailure, L"Se produjo una excepcion al guardar el documento.");
		}
		catch (...)
		{
			return Failure(StorageError::IoFailure, L"Se produjo un error desconocido al guardar el documento.");
		}
	}
}
