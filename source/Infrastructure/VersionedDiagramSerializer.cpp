#include "VersionedDiagramSerializer.h"
#include "DiagramSerializationSupport.h"
#include "JsonDiagramSerializer.h"
#include "LegacyDiagramSerializer.h"

namespace arteststudio::infrastructure
{
	namespace
	{
		[[nodiscard]] const JsonDiagramSerializer& JsonSerializer() noexcept
		{
			static const JsonDiagramSerializer serializer;
			return serializer;
		}

		[[nodiscard]] const LegacyDiagramSerializer& LegacySerializer() noexcept
		{
			static const LegacyDiagramSerializer serializer;
			return serializer;
		}
	}

	application::StorageResult VersionedDiagramSerializer::Serialize(
		const domain::DiagramModel& diagram,
		std::string& content) const noexcept
	{
		return JsonSerializer().Serialize(diagram, content);
	}

	application::StorageResult VersionedDiagramSerializer::Deserialize(
		std::string_view content,
		domain::DiagramModel& diagram) const noexcept
	{
		if (JsonSerializer().CanDeserialize(content))
		{
			return JsonSerializer().Deserialize(content, diagram);
		}
		if (LegacySerializer().CanDeserialize(content))
		{
			return LegacySerializer().Deserialize(content, diagram);
		}
		return serialization::Failure(
			application::StorageError::InvalidFormat,
			L"El archivo no contiene un documento JSON v2 ni un diagrama legacy v1 reconocido.");
	}
}
