#pragma once

#include "../Domain/DiagramModel.h"

#include <filesystem>
#include <string>
#include <string_view>

namespace arteststudio::application
{
	inline constexpr std::wstring_view kDiagramFileExtension = L".atd";
	inline constexpr std::wstring_view kProjectFileExtension = L".atprj";

	enum class StorageError
	{
		None,
		InvalidPath,
		UnsupportedFileExtension,
		FileNotFound,
		AccessDenied,
		IoFailure,
		InvalidFormat,
		UnsupportedVersion,
		InvalidData,
		InvalidEncoding
	};

	struct StorageResult
	{
		StorageError error = StorageError::None;
		std::wstring detail;

		[[nodiscard]] explicit operator bool() const noexcept
		{
			return error == StorageError::None;
		}
	};

	[[nodiscard]] constexpr std::wstring_view DescribeStorageError(StorageError error) noexcept
	{
		switch (error)
		{
		case StorageError::None:
			return L"La operacion se completo correctamente.";
		case StorageError::InvalidPath:
			return L"La ruta del documento no es valida.";
		case StorageError::UnsupportedFileExtension:
			return L"La extension no corresponde a un diagrama de ARTestStudio (.atd).";
		case StorageError::FileNotFound:
			return L"No se encontro el archivo solicitado.";
		case StorageError::AccessDenied:
			return L"No hay permisos suficientes para acceder al archivo.";
		case StorageError::IoFailure:
			return L"Ocurrio un error de entrada o salida.";
		case StorageError::InvalidFormat:
			return L"El archivo no tiene un formato de ARTestStudio valido.";
		case StorageError::UnsupportedVersion:
			return L"La version del archivo no es compatible con esta aplicacion.";
		case StorageError::InvalidData:
			return L"El archivo contiene datos de diagrama invalidos.";
		case StorageError::InvalidEncoding:
			return L"El archivo contiene texto que no es UTF-8 valido.";
		}

		return L"Ocurrio un error desconocido.";
	}

	class IDiagramStorage
	{
	public:
		virtual ~IDiagramStorage() = default;

		[[nodiscard]] virtual StorageResult Load(
			const std::filesystem::path& path,
			domain::DiagramModel& diagram) const noexcept = 0;

		[[nodiscard]] virtual StorageResult Save(
			const std::filesystem::path& path,
			const domain::DiagramModel& diagram) const noexcept = 0;
	};
}
