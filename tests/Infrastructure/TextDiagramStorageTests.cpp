#include "../TestSupport/TestSupport.h"

#include "Application/DiagramStorage.h"
#include "Domain/DiagramModel.h"
#include "Infrastructure/TextDiagramStorage.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace arteststudio::tests
{
	using namespace domain;
	using application::DiagramStorageLimits;
	using application::StorageError;
	using application::StorageResult;
	using infrastructure::TextDiagramStorage;
	using json = nlohmann::json;

	TEST(TextDiagramStorageTests, PersistsAndRestoresDiagram)
	{
		DiagramModel original;
		const std::wstring sourceLabel = L"Fuente ñ \"encendida\" \\ ruta\nsiguiente";
		const NodeId source = original.AddNode(NodeKind::Rectangle, {10, 20}, 180, 90, sourceLabel);
		const NodeId target = original.AddNode(NodeKind::Diamond, {420, 260}, 110, 80, L"Condición");
		const AddConnectionResult connection = original.AddConnection(
			{source, PortId::Right},
			{target, PortId::Top},
			{{240, 65}, {475, 65}});
		VerifyTestCondition(static_cast<bool>(connection), "The persisted connection must be valid.");

		TemporaryDiagramFile file;
		const TextDiagramStorage storage;
		const StorageResult saved = storage.Save(file.Path(), original);
		VerifyTestCondition(static_cast<bool>(saved), "A valid diagram must be saved.");

		DiagramModel restored;
		const StorageResult loaded = storage.Load(file.Path(), restored);
		VerifyTestCondition(static_cast<bool>(loaded), "A saved diagram must be loaded.");
		VerifyTestCondition(restored.Nodes().size() == 2, "All nodes must be restored.");
		VerifyTestCondition(restored.Connections().size() == 1, "All connections must be restored.");

		const Node& restoredSource = restored.Nodes()[0];
		const Node& restoredTarget = restored.Nodes()[1];
		VerifyTestCondition(restoredSource.kind == NodeKind::Rectangle, "The source kind must be restored.");
		VerifyTestCondition(restoredSource.position == Point{10, 20}, "The source position must be restored.");
		VerifyTestCondition(restoredSource.width == 180 && restoredSource.height == 90,
			"The source dimensions must be restored.");
		VerifyTestCondition(restoredSource.label == sourceLabel,
			"Unicode labels with JSON escapes and line breaks must round-trip.");
		VerifyTestCondition(restoredTarget.kind == NodeKind::Diamond, "The target kind must be restored.");
		VerifyTestCondition(restoredTarget.label == L"Condición", "Accented labels must round-trip.");

		const Connection& restoredConnection = restored.Connections().front();
		VerifyTestCondition(restoredConnection.from.nodeId == restoredSource.id,
			"The source endpoint must reference the restored source.");
		VerifyTestCondition(restoredConnection.to.nodeId == restoredTarget.id,
			"The target endpoint must reference the restored target.");
		VerifyTestCondition(restoredConnection.from.portId == PortId::Right &&
			restoredConnection.to.portId == PortId::Top,
			"Connection ports must be restored.");
		VerifyTestCondition(restoredConnection.intermediatePoints == std::vector<Point>{{240, 65}, {475, 65}},
			"The route points must be restored.");
	}

	TEST(TextDiagramStorageTests, SavesNewDocumentsAsVersionTwoJson)
	{
		DiagramModel diagram;
		(void)diagram.AddNode(NodeKind::Rectangle, {15, 25}, L"JSON document");
		TemporaryDiagramFile file;
		const TextDiagramStorage storage;

		VerifyTestCondition(static_cast<bool>(storage.Save(file.Path(), diagram)),
			"A valid diagram must be saved as JSON.");
		const json root = json::parse(ReadFileContents(file.Path()));
		VerifyTestCondition(root.is_object(), "The saved document root must be a JSON object.");
		VerifyTestCondition(root.at("format") == "ARTestStudio.Diagram",
			"The JSON format discriminator must be stable.");
		VerifyTestCondition(root.at("version") == 2, "New documents must use JSON schema version 2.");
		VerifyTestCondition(root.at("nodes").size() == 1 && root.at("connections").empty(),
			"The JSON document must contain the complete diagram collections.");
	}

	TEST(TextDiagramStorageTests, MigratesLegacyVersionOneToJsonWhenSaved)
	{
		TemporaryDiagramFile file;
		{
			std::ofstream output(file.Path(), std::ios::binary | std::ios::trunc);
			output << "ARTESTSTUDIO_DIAGRAM 1\n"
				<< "NODES 1\n"
				<< "NODE 42 0 10 20 150 100 \"Legacy step\"\n"
				<< "CONNECTIONS 0\n"
				<< "END\n";
		}

		const TextDiagramStorage storage;
		DiagramModel diagram;
		VerifyTestCondition(static_cast<bool>(storage.Load(file.Path(), diagram)),
			"A legacy version-one document must load before migration.");
		VerifyTestCondition(static_cast<bool>(storage.Save(file.Path(), diagram)),
			"Saving a loaded legacy document must migrate it to the current format.");

		const json migrated = json::parse(ReadFileContents(file.Path()));
		VerifyTestCondition(migrated.at("version") == 2,
			"A migrated legacy document must be written as version-two JSON.");
		VerifyTestCondition(migrated.at("nodes").at(0).at("id") == 42,
			"Migration must preserve stable identifiers.");
		VerifyTestCondition(migrated.at("nodes").at(0).at("label") == "Legacy step",
			"Migration must preserve content.");
	}

	TEST(TextDiagramStorageTests, RejectsUnsupportedJsonVersionsAtomically)
	{
		TemporaryDiagramFile file;
		{
			std::ofstream output(file.Path(), std::ios::binary | std::ios::trunc);
			output << R"json({
  "format": "ARTestStudio.Diagram",
  "version": 999,
  "nodes": [],
  "connections": []
})json";
		}

		DiagramModel existing;
		const NodeId original = existing.AddNode(NodeKind::Rectangle, {5, 5}, L"Preserve");
		const TextDiagramStorage storage;
		const StorageResult result = storage.Load(file.Path(), existing);

		VerifyTestCondition(result.error == StorageError::UnsupportedVersion,
			"A future JSON version must report UnsupportedVersion.");
		VerifyTestCondition(existing.Nodes().size() == 1 && existing.FindNode(original) != nullptr,
			"A future JSON version must not modify the active diagram.");
	}

	TEST(TextDiagramStorageTests, RejectsMalformedJsonAtomically)
	{
		TemporaryDiagramFile file;
		{
			std::ofstream output(file.Path(), std::ios::binary | std::ios::trunc);
			output << "{\"format\":\"ARTestStudio.Diagram\",\"version\":2,\"nodes\":[";
		}

		DiagramModel existing;
		const NodeId original = existing.AddNode(NodeKind::Diamond, {7, 9}, L"Keep");
		const TextDiagramStorage storage;
		const StorageResult result = storage.Load(file.Path(), existing);

		VerifyTestCondition(result.error == StorageError::InvalidFormat,
			"Truncated JSON must report InvalidFormat.");
		VerifyTestCondition(existing.Nodes().size() == 1 && existing.FindNode(original) != nullptr,
			"Malformed JSON must not partially replace the active diagram.");
	}

	TEST(TextDiagramStorageTests, RejectsInvalidJsonReferencesAtomically)
	{
		TemporaryDiagramFile file;
		{
			std::ofstream output(file.Path(), std::ios::binary | std::ios::trunc);
			output << R"json({
  "format": "ARTestStudio.Diagram",
  "version": 2,
  "nodes": [
    {"id": 1, "kind": "rectangle", "position": {"x": 0, "y": 0},
     "size": {"width": 150, "height": 100}, "label": "Only node"}
  ],
  "connections": [
    {"id": 1, "from": {"nodeId": 1, "port": "right"},
     "to": {"nodeId": 999, "port": "left"}, "route": []}
  ]
})json";
		}

		DiagramModel existing;
		const NodeId original = existing.AddNode(NodeKind::Rectangle, {11, 13}, L"Preserve");
		const TextDiagramStorage storage;
		const StorageResult result = storage.Load(file.Path(), existing);

		VerifyTestCondition(result.error == StorageError::InvalidData,
			"A JSON connection to a missing node must report InvalidData.");
		VerifyTestCondition(existing.Nodes().size() == 1 && existing.FindNode(original) != nullptr,
			"Invalid JSON references must not modify the active diagram.");
	}

	TEST(TextDiagramStorageTests, RejectsInvalidUtf8JsonAtomically)
	{
		TemporaryDiagramFile file;
		std::string invalid =
			"{\"format\":\"ARTestStudio.Diagram\",\"version\":2,\"nodes\":[],"
			"\"connections\":[],\"invalid\":\"";
		invalid.push_back(static_cast<char>(0xC3));
		invalid.push_back(static_cast<char>(0x28));
		invalid += "\"}";
		{
			std::ofstream output(file.Path(), std::ios::binary | std::ios::trunc);
			output.write(invalid.data(), static_cast<std::streamsize>(invalid.size()));
		}

		DiagramModel existing;
		const NodeId original = existing.AddNode(NodeKind::Rectangle, {17, 19}, L"Preserve");
		const TextDiagramStorage storage;
		const StorageResult result = storage.Load(file.Path(), existing);

		VerifyTestCondition(result.error == StorageError::InvalidEncoding,
			"Invalid UTF-8 JSON must report InvalidEncoding.");
		VerifyTestCondition(existing.Nodes().size() == 1 && existing.FindNode(original) != nullptr,
			"Invalid UTF-8 must not modify the active diagram.");
	}

	TEST(TextDiagramStorageTests, PreservesIdentifiersWithGaps)
	{
		DiagramModel original;
		const NodeId first = original.AddNode(NodeKind::Rectangle, {0, 0}, L"First");
		const NodeId removed = original.AddNode(NodeKind::Rectangle, {200, 0}, L"Removed");
		const NodeId third = original.AddNode(NodeKind::Diamond, {400, 0}, L"Third");
		const AddConnectionResult removedConnection = original.AddConnection(
			{first, PortId::Right}, {removed, PortId::Left});
		const AddConnectionResult survivingConnection = original.AddConnection(
			{first, PortId::Right}, {third, PortId::Left});
		VerifyTestCondition(static_cast<bool>(removedConnection) && static_cast<bool>(survivingConnection),
			"Both initial connections must be created.");
		VerifyTestCondition(original.RemoveNode(removed) == DiagramError::None,
			"Removing the middle node must create an identifier gap.");

		TemporaryDiagramFile file;
		const TextDiagramStorage storage;
		VerifyTestCondition(static_cast<bool>(storage.Save(file.Path(), original)),
			"A diagram with identifier gaps must be saved.");

		DiagramModel restored;
		VerifyTestCondition(static_cast<bool>(storage.Load(file.Path(), restored)),
			"A diagram with identifier gaps must be loaded.");
		VerifyTestCondition(restored.FindNode(first) != nullptr && restored.FindNode(third) != nullptr,
			"Existing node identifiers must be preserved exactly.");
		VerifyTestCondition(restored.FindNode(removed) == nullptr,
			"A removed node identifier must remain unused after loading.");
		VerifyTestCondition(restored.FindConnection(removedConnection.connectionId) == nullptr,
			"A removed connection identifier must remain unused after loading.");
		VerifyTestCondition(restored.FindConnection(survivingConnection.connectionId) != nullptr,
			"The surviving connection identifier must be preserved exactly.");

		const NodeId addedNode = restored.AddNode(NodeKind::Rectangle, {600, 0}, L"Added after load");
		VerifyTestCondition(addedNode == NodeId{4}, "The next node identifier must continue after the maximum restored ID.");
		const AddConnectionResult addedConnection = restored.AddConnection(
			{third, PortId::Right}, {addedNode, PortId::Left});
		VerifyTestCondition(addedConnection.connectionId == ConnectionId{3},
			"The next connection identifier must continue after the maximum restored ID.");
	}

	TEST(TextDiagramStorageTests, LoadsExistingVersionOneDocumentsWithStableIdentifiers)
	{
		TemporaryDiagramFile file;
		{
			std::ofstream output(file.Path(), std::ios::binary | std::ios::trunc);
			output << "ARTESTSTUDIO_DIAGRAM 1\n"
				<< "NODES 2\n"
				<< "NODE 42 0 10 20 150 100 \"High identifier\"\n"
				<< "NODE 7 1 400 200 100 80 \"Low identifier\"\n"
				<< "CONNECTIONS 1\n"
				<< "CONNECTION 77 42 1 7 3 1 250 70\n"
				<< "END\n";
		}

		DiagramModel diagram;
		const TextDiagramStorage storage;
		const StorageResult result = storage.Load(file.Path(), diagram);
		VerifyTestCondition(static_cast<bool>(result), "An existing version-one document must remain loadable.");
		VerifyTestCondition(diagram.FindNode(NodeId{42}) != nullptr && diagram.FindNode(NodeId{7}) != nullptr,
			"Version-one node identifiers must be preserved regardless of file order.");
		VerifyTestCondition(diagram.FindConnection(ConnectionId{77}) != nullptr,
			"Version-one connection identifiers must be preserved.");
		VerifyTestCondition(diagram.AddNode(NodeKind::Rectangle, {700, 0}, L"Next") == NodeId{43},
			"The next node identifier must use the restored maximum, not the last file entry.");
		const AddConnectionResult added = diagram.AddConnection(
			{NodeId{7}, PortId::Right}, {NodeId{43}, PortId::Left});
		VerifyTestCondition(added.connectionId == ConnectionId{78},
			"The next connection identifier must continue after a version-one restored maximum.");
	}

	TEST(TextDiagramStorageTests, RejectsCorruptFilesWithoutChangingTheDiagram)
	{
		TemporaryDiagramFile file;
		{
			std::ofstream output(file.Path(), std::ios::binary | std::ios::trunc);
			output << "NOT_A_DIAGRAM 1\n";
		}

		DiagramModel existing;
		const NodeId originalId = existing.AddNode(NodeKind::Rectangle, {5, 8}, L"Keep me");
		const TextDiagramStorage storage;
		const StorageResult result = storage.Load(file.Path(), existing);

		VerifyTestCondition(result.error == StorageError::InvalidFormat, "A corrupt header must be reported.");
		VerifyTestCondition(existing.Nodes().size() == 1, "A failed load must preserve the current diagram.");
		VerifyTestCondition(existing.FindNode(originalId) != nullptr, "The original node must remain after a failed load.");
	}

	TEST(TextDiagramStorageTests, RejectsConnectionsToMissingNodes)
	{
		TemporaryDiagramFile file;
		{
			std::ofstream output(file.Path(), std::ios::binary | std::ios::trunc);
			output << "ARTESTSTUDIO_DIAGRAM 1\n"
				<< "NODES 1\n"
				<< "NODE 1 0 10 20 150 100 \"Only node\"\n"
				<< "CONNECTIONS 1\n"
				<< "CONNECTION 1 1 1 999 3 0\n"
				<< "END\n";
		}

		DiagramModel diagram;
		const TextDiagramStorage storage;
		const StorageResult result = storage.Load(file.Path(), diagram);
		VerifyTestCondition(result.error == StorageError::InvalidData,
			"Connections to missing nodes must be rejected as invalid data.");
		VerifyTestCondition(diagram.Nodes().empty() && diagram.Connections().empty(),
			"A rejected document must not be partially loaded.");
	}

	TEST(TextDiagramStorageTests, RejectsUnsupportedFileVersions)
	{
		TemporaryDiagramFile file;
		{
			std::ofstream output(file.Path(), std::ios::binary | std::ios::trunc);
			output << "ARTESTSTUDIO_DIAGRAM 999\nNODES 0\nCONNECTIONS 0\nEND\n";
		}

		DiagramModel diagram;
		const TextDiagramStorage storage;
		const StorageResult result = storage.Load(file.Path(), diagram);
		VerifyTestCondition(result.error == StorageError::UnsupportedVersion,
			"Future file versions must be rejected explicitly.");
	}

	TEST(TextDiagramStorageTests, ReportsMissingDiagramFiles)
	{
		TemporaryDiagramFile file;
		DiagramModel diagram;
		const TextDiagramStorage storage;
		const StorageResult result = storage.Load(file.Path(), diagram);
		VerifyTestCondition(result.error == StorageError::FileNotFound, "Missing files must have a specific error.");
	}

	TEST(TextDiagramStorageTests, RejectsNonDiagramFileExtensions)
	{
		TemporaryDiagramFile projectFile{".atprj"};
		DiagramModel diagram;
		(void)diagram.AddNode(NodeKind::Rectangle, {0, 0}, L"Diagram only");
		const TextDiagramStorage storage;

		const StorageResult saveResult = storage.Save(projectFile.Path(), diagram);
		VerifyTestCondition(saveResult.error == StorageError::UnsupportedFileExtension,
			"The diagram storage must reject the reserved project extension.");
		VerifyTestCondition(!std::filesystem::exists(projectFile.Path()),
			"A rejected extension must not create a file.");

		const StorageResult loadResult = storage.Load(projectFile.Path(), diagram);
		VerifyTestCondition(loadResult.error == StorageError::UnsupportedFileExtension,
			"Loading a non-.atd path must report the extension error before accessing the file.");
	}

	TEST(TextDiagramStorageTests, RejectsOversizedDiagramFilesBeforeParsing)
	{
		TemporaryDiagramFile file;
		{
			std::ofstream output(file.Path(), std::ios::binary | std::ios::trunc);
			output.seekp(static_cast<std::streamoff>(DiagramStorageLimits::MaximumFileBytes));
			output.put('\0');
		}

		DiagramModel existing;
		const NodeId original = existing.AddNode(NodeKind::Rectangle, {10, 10}, L"Preserve");
		const TextDiagramStorage storage;
		const StorageResult result = storage.Load(file.Path(), existing);

		VerifyTestCondition(result.error == StorageError::FileTooLarge,
			"Files larger than 16 MB must be rejected before parsing.");
		VerifyTestCondition(existing.Nodes().size() == 1 && existing.FindNode(original) != nullptr,
			"An oversized file must not replace the active diagram.");
	}

	TEST(TextDiagramStorageTests, RejectsOversizedLabelsWithBoundedParsing)
	{
		TemporaryDiagramFile file;
		{
			std::ofstream output(file.Path(), std::ios::binary | std::ios::trunc);
			output << "ARTESTSTUDIO_DIAGRAM 1\nNODES 1\nNODE 1 0 0 0 150 100 \"";
			const std::string oversizedLabel(DiagramStorageLimits::MaximumLabelBytes + 1, 'A');
			output.write(oversizedLabel.data(), static_cast<std::streamsize>(oversizedLabel.size()));
			output << "\"\nCONNECTIONS 0\nEND\n";
		}

		DiagramModel existing;
		const NodeId original = existing.AddNode(NodeKind::Diamond, {20, 20}, L"Preserve");
		const TextDiagramStorage storage;
		const StorageResult result = storage.Load(file.Path(), existing);

		VerifyTestCondition(result.error == StorageError::DataLimitExceeded,
			"Labels larger than 64 KB must have a specific limit error.");
		VerifyTestCondition(existing.Nodes().size() == 1 && existing.FindNode(original) != nullptr,
			"An oversized label must not replace the active diagram.");
	}

	TEST(TextDiagramStorageTests, RejectsTruncatedLabelsAtomically)
	{
		TemporaryDiagramFile file;
		{
			std::ofstream output(file.Path(), std::ios::binary | std::ios::trunc);
			output << "ARTESTSTUDIO_DIAGRAM 1\n"
				<< "NODES 1\n"
				<< "NODE 1 0 0 0 150 100 \"unterminated";
		}

		DiagramModel existing;
		const NodeId original = existing.AddNode(NodeKind::Rectangle, {30, 30}, L"Preserve");
		const TextDiagramStorage storage;
		const StorageResult result = storage.Load(file.Path(), existing);

		VerifyTestCondition(result.error == StorageError::InvalidData,
			"A truncated quoted label must be rejected as invalid data.");
		VerifyTestCondition(existing.Nodes().size() == 1 && existing.FindNode(original) != nullptr,
			"A truncated file must not replace the active diagram.");
	}

	TEST(TextDiagramStorageTests, RemovesStaleTemporaryFilesOnSuccessfulSave)
	{
		TemporaryDiagramFile file;
		std::filesystem::path temporaryPath = file.Path();
		temporaryPath += L".tmp";
		{
			std::ofstream output(temporaryPath, std::ios::binary | std::ios::trunc);
			output << "stale temporary data";
		}

		DiagramModel diagram;
		(void)diagram.AddNode(NodeKind::Rectangle, {40, 40}, L"Saved after cleanup");
		const TextDiagramStorage storage;
		const StorageResult result = storage.Save(file.Path(), diagram);

		VerifyTestCondition(static_cast<bool>(result), "A stale temporary file must not block a valid save.");
		VerifyTestCondition(!std::filesystem::exists(temporaryPath),
			"The stale temporary file must be removed after a successful save.");
		DiagramModel restored;
		VerifyTestCondition(static_cast<bool>(storage.Load(file.Path(), restored)),
			"The destination written after cleanup must remain loadable.");
	}

	TEST(TextDiagramStorageTests, FailedReplacementPreservesThePreviousDocument)
	{
		TemporaryDiagramFile file;
		DiagramModel baseline;
		(void)baseline.AddNode(NodeKind::Rectangle, {50, 50}, L"Original content");
		const TextDiagramStorage realStorage;
		VerifyTestCondition(static_cast<bool>(realStorage.Save(file.Path(), baseline)),
			"The baseline document must be saved before simulating failure.");
		const std::string originalContents = ReadFileContents(file.Path());

		DiagramModel modified;
		(void)modified.AddNode(NodeKind::Diamond, {60, 60}, L"Replacement content");
		FailingAtomicFileWriter failingWriter{AtomicWriteError::ReplacementFailure};
		const TextDiagramStorage failingStorage{failingWriter};
		const StorageResult result = failingStorage.Save(file.Path(), modified);

		VerifyTestCondition(result.error == StorageError::ReplacementFailure,
			"Replacement failures must be reported specifically.");
		VerifyTestCondition(failingWriter.calls == 1 && failingWriter.lastDestination == file.Path(),
			"The diagram storage must delegate one atomic replacement attempt.");
		VerifyTestCondition(ReadFileContents(file.Path()) == originalContents,
			"A failed replacement must not alter the previous document.");
	}

	TEST(TextDiagramStorageTests, ReportsTemporaryWriteFailuresSpecifically)
	{
		TemporaryDiagramFile file;
		DiagramModel diagram;
		(void)diagram.AddNode(NodeKind::Rectangle, {70, 70}, L"Temporary failure");
		FailingAtomicFileWriter failingWriter{AtomicWriteError::TemporaryWriteFailure};
		const TextDiagramStorage storage{failingWriter};
		const StorageResult result = storage.Save(file.Path(), diagram);

		VerifyTestCondition(result.error == StorageError::TemporaryFileFailure,
			"Temporary write failures must not be collapsed into a generic IO error.");
		VerifyTestCondition(!std::filesystem::exists(file.Path()),
			"A temporary write failure must not create the destination document.");
	}

	TEST(TextDiagramStorageTests, OversizedSavePreservesThePreviousDocument)
	{
		TemporaryDiagramFile file;
		DiagramModel baseline;
		(void)baseline.AddNode(NodeKind::Rectangle, {80, 80}, L"Baseline");
		const TextDiagramStorage storage;
		VerifyTestCondition(static_cast<bool>(storage.Save(file.Path(), baseline)),
			"The baseline document must be saved before the size-limit test.");
		const std::string originalContents = ReadFileContents(file.Path());

		DiagramModel oversized;
		const std::wstring maximumLabel(DiagramStorageLimits::MaximumLabelBytes, L'X');
		for (int index = 0; index < 256; ++index)
		{
			(void)oversized.AddNode(
				NodeKind::Rectangle,
				{index * 10, index * 10},
				150,
				100,
				maximumLabel);
		}

		const StorageResult result = storage.Save(file.Path(), oversized);
		VerifyTestCondition(result.error == StorageError::FileTooLarge,
			"Serialized diagrams larger than 16 MB must be rejected.");
		VerifyTestCondition(ReadFileContents(file.Path()) == originalContents,
			"A size-limit failure must preserve the previous document byte for byte.");
	}


}
