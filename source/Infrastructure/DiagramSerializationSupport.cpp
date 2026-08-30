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
			return Failure(StorageError::InvalidEncoding, L"La etiqueta es demasiado grande.");
		}

		const int required = WideCharToMultiByte(
			CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
			nullptr, 0, nullptr, nullptr);
		if (required <= 0)
		{
			return Failure(StorageError::InvalidEncoding, L"No se pudo convertir una etiqueta a UTF-8.");
		}

		output.resize(static_cast<std::size_t>(required));
		if (WideCharToMultiByte(
			CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
			output.data(), required, nullptr, nullptr) != required)
		{
			output.clear();
			return Failure(StorageError::InvalidEncoding, L"No se pudo convertir una etiqueta a UTF-8.");
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
			return Failure(StorageError::InvalidEncoding, L"La etiqueta es demasiado grande.");
		}

		const int required = MultiByteToWideChar(
			CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0);
		if (required <= 0)
		{
			return Failure(StorageError::InvalidEncoding, L"El archivo contiene una etiqueta UTF-8 invalida.");
		}

		output.resize(static_cast<std::size_t>(required));
		if (MultiByteToWideChar(
			CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
			output.data(), required) != required)
		{
			output.clear();
			return Failure(StorageError::InvalidEncoding, L"El archivo contiene una etiqueta UTF-8 invalida.");
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
			return Failure(StorageError::InvalidEncoding, L"El contenido UTF-8 es demasiado grande.");
		}
		if (MultiByteToWideChar(
			CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0) <= 0)
		{
			return Failure(StorageError::InvalidEncoding, L"El documento contiene bytes que no forman UTF-8 valido.");
		}
		return {};
	}

	std::wstring DescribeSnapshotFailure(const RestoreSnapshotResult& result)
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

	StorageResult ValidateSnapshotForStorage(const domain::DiagramSnapshot& snapshot)
	{
		const RestoreSnapshotResult validation = DiagramModel::ValidateSnapshot(snapshot);
		if (!validation)
		{
			return Failure(StorageError::InvalidData, DescribeSnapshotFailure(validation));
		}
		if (snapshot.nodes.size() > kMaximumNodes || snapshot.connections.size() > kMaximumConnections)
		{
			return Failure(StorageError::DataLimitExceeded, L"El diagrama contiene demasiados elementos.");
		}

		for (const domain::Node& node : snapshot.nodes)
		{
			if (!IsCoordinateValid(node.position.x) || !IsCoordinateValid(node.position.y) ||
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
		}

		for (const domain::Connection& connection : snapshot.connections)
		{
			if (connection.intermediatePoints.size() > kMaximumRoutePoints)
			{
				return Failure(StorageError::DataLimitExceeded, L"Una conexion contiene demasiados puntos de ruta.");
			}
			for (const domain::Point point : connection.intermediatePoints)
			{
				if (!IsCoordinateValid(point.x) || !IsCoordinateValid(point.y))
				{
					return Failure(StorageError::InvalidData, L"Una ruta contiene coordenadas invalidas.");
				}
			}
		}
		return {};
	}
}
