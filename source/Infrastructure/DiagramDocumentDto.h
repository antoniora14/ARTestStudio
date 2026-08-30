#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace arteststudio::infrastructure::dto
{
	inline constexpr char kDiagramFormat[] = "ARTestStudio.Diagram";
	inline constexpr int kJsonFormatVersion = 2;

	struct PointDto
	{
		int x = 0;
		int y = 0;
	};

	struct NodeDto
	{
		std::uint64_t id = 0;
		std::string kind;
		PointDto position;
		int width = 0;
		int height = 0;
		std::string label;
	};

	struct EndpointDto
	{
		std::uint64_t nodeId = 0;
		std::string port;
	};

	struct ConnectionDto
	{
		std::uint64_t id = 0;
		EndpointDto from;
		EndpointDto to;
		std::vector<PointDto> route;
	};

	struct DiagramDocumentDto
	{
		std::string format = kDiagramFormat;
		int version = kJsonFormatVersion;
		std::vector<NodeDto> nodes;
		std::vector<ConnectionDto> connections;
	};
}
