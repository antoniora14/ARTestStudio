#include "Domain/DiagramModel.h"
#include "Domain/DiagramGeometry.h"
#include "Domain/OrthogonalRouter.h"
#include "Application/FaultService.h"
#include "Infrastructure/FileFaultReporter.h"
#include "Infrastructure/TextDiagramStorage.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <exception>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>
#include <stdexcept>
#include <utility>
#include <vector>

namespace
{
	using namespace arteststudio::domain;
	using arteststudio::application::StorageError;
	using arteststudio::application::StorageResult;
	using arteststudio::application::Fault;
	using arteststudio::application::FaultCategory;
	using arteststudio::application::FaultService;
	using arteststudio::application::FaultSeverity;
	using arteststudio::application::IFaultReporter;
	using arteststudio::infrastructure::FileFaultReporter;
	using arteststudio::infrastructure::TextDiagramStorage;

	struct TestCase
	{
		std::string_view name;
		std::function<void()> execute;
	};

	class TestFailure final : public std::exception
	{
	public:
		explicit TestFailure(const char* message) noexcept
			: m_message(message)
		{
		}

		[[nodiscard]] const char* what() const noexcept override
		{
			return m_message;
		}

	private:
		const char* m_message;
	};

	void Require(bool condition, const char* message)
	{
		if (!condition)
		{
			throw TestFailure(message);
		}
	}

	class TemporaryDiagramFile final
	{
	public:
		explicit TemporaryDiagramFile(std::string_view extension = ".atd")
		{
			const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
			m_path = std::filesystem::temp_directory_path() /
				("ARTestStudio.Tests." + std::to_string(suffix) + std::string{extension});
		}

		~TemporaryDiagramFile()
		{
			std::error_code error;
			std::filesystem::remove(m_path, error);
			std::filesystem::path temporaryPath = m_path;
			temporaryPath += L".tmp";
			std::filesystem::remove(temporaryPath, error);
			const std::filesystem::path previousPath = m_path.parent_path() /
				(m_path.stem().wstring() + L".previous" + m_path.extension().wstring());
			std::filesystem::remove(previousPath, error);
		}

		[[nodiscard]] const std::filesystem::path& Path() const noexcept
		{
			return m_path;
		}

	private:
		std::filesystem::path m_path;
	};

	class RecordingFaultReporter final : public IFaultReporter
	{
	public:
		void Report(const Fault& fault) override
		{
			++count;
			lastSeverity = fault.severity;
			lastCode = fault.code;
			lastOperation = fault.operation;
		}

		int count = 0;
		FaultSeverity lastSeverity = FaultSeverity::Information;
		std::wstring lastCode;
		std::wstring lastOperation;
	};

	class ThrowingFaultReporter final : public IFaultReporter
	{
	public:
		void Report(const Fault&) override
		{
			throw std::runtime_error("simulated reporter failure");
		}
	};

	class FaultReporterScope final
	{
	public:
		explicit FaultReporterScope(IFaultReporter* reporter) noexcept
		{
			FaultService::Configure(reporter);
		}

		~FaultReporterScope()
		{
			FaultService::Configure(nullptr);
		}

		FaultReporterScope(const FaultReporterScope&) = delete;
		FaultReporterScope& operator=(const FaultReporterScope&) = delete;
	};

	std::vector<Point> GetFullRoute(const DiagramModel& diagram, const Connection& connection)
	{
		const Node* source = diagram.FindNode(connection.from.nodeId);
		const Node* target = diagram.FindNode(connection.to.nodeId);
		Require(source != nullptr && target != nullptr, "Route endpoints must exist.");

		std::vector<Point> points{GetConnectionPoint(*source, connection.from.portId)};
		points.insert(points.end(), connection.intermediatePoints.begin(), connection.intermediatePoints.end());
		points.push_back(GetConnectionPoint(*target, connection.to.portId));
		return points;
	}

	void RequireOrthogonal(const std::vector<Point>& points)
	{
		for (std::size_t index = 0; index + 1 < points.size(); ++index)
		{
			Require(points[index].x == points[index + 1].x || points[index].y == points[index + 1].y,
				"Every route segment must be horizontal or vertical.");
		}

		for (std::size_t index = 1; index + 1 < points.size(); ++index)
		{
			const bool redundantVertical = points[index - 1].x == points[index].x &&
				points[index].x == points[index + 1].x;
			const bool redundantHorizontal = points[index - 1].y == points[index].y &&
				points[index].y == points[index + 1].y;
			Require(!redundantVertical && !redundantHorizontal,
				"Collinear route points must be simplified.");
		}
	}

	bool SegmentCrossesInterior(Point first, Point second, Rect rectangle)
	{
		if (first.x == second.x)
		{
			const int minimum = std::min(first.y, second.y);
			const int maximum = std::max(first.y, second.y);
			return first.x > rectangle.left && first.x < rectangle.right &&
				maximum > rectangle.top && minimum < rectangle.bottom;
		}

		if (first.y == second.y)
		{
			const int minimum = std::min(first.x, second.x);
			const int maximum = std::max(first.x, second.x);
			return first.y > rectangle.top && first.y < rectangle.bottom &&
				maximum > rectangle.left && minimum < rectangle.right;
		}
		return true;
	}

	void RequireAvoids(const std::vector<Point>& route, Rect obstacle)
	{
		for (std::size_t index = 0; index + 1 < route.size(); ++index)
		{
			Require(!SegmentCrossesInterior(route[index], route[index + 1], obstacle),
				"A route segment must not cross an obstacle.");
		}
	}

	void RequirePortDirection(Point port, Point adjacent, PortId portId)
	{
		switch (portId)
		{
		case PortId::Top:
			Require(adjacent.x == port.x && adjacent.y <= port.y, "Top port route must leave upward.");
			break;
		case PortId::Right:
			Require(adjacent.y == port.y && adjacent.x >= port.x, "Right port route must leave to the right.");
			break;
		case PortId::Bottom:
			Require(adjacent.x == port.x && adjacent.y >= port.y, "Bottom port route must leave downward.");
			break;
		case PortId::Left:
			Require(adjacent.y == port.y && adjacent.x <= port.x, "Left port route must leave to the left.");
			break;
		}
	}

	void AddsAndMovesNodes()
	{
		DiagramModel diagram;
		const NodeId first = diagram.AddNode(NodeKind::Rectangle, {10, 20}, L"PowerON");
		const NodeId second = diagram.AddNode(NodeKind::Diamond, {100, 120}, L"IF");

		Require(first != second, "Node IDs must be unique.");
		Require(diagram.Nodes().size() == 2, "Both nodes must be stored.");
		Require(diagram.MoveNodeBy(first, 5, -2) == DiagramError::None, "Existing node must move.");

		const Node* moved = diagram.FindNode(first);
		Require(moved != nullptr, "Moved node must still exist.");
		Require(moved->position == Point{15, 18}, "MoveNodeBy must update the node position.");
		Require(diagram.MoveNodeTo(second, {7, 9}) == DiagramError::None, "Existing node must move to a position.");
		Require(diagram.FindNode(second)->position == Point{7, 9}, "MoveNodeTo must replace the position.");
	}

	void CreatesValidConnections()
	{
		DiagramModel diagram;
		const NodeId source = diagram.AddNode(NodeKind::Rectangle, {0, 0}, L"Source");
		const NodeId target = diagram.AddNode(NodeKind::Rectangle, {200, 0}, L"Target");

		const AddConnectionResult result = diagram.AddConnection(
			{source, PortId::Right},
			{target, PortId::Left},
			{{100, 50}});

		Require(static_cast<bool>(result), "A connection between existing nodes must succeed.");
		Require(diagram.Connections().size() == 1, "The connection must be stored.");
		Require(diagram.Connections().front().intermediatePoints == std::vector<Point>{{100, 50}},
			"Intermediate points must be preserved.");
	}

	void RejectsInvalidConnections()
	{
		DiagramModel diagram;
		const NodeId existing = diagram.AddNode(NodeKind::Rectangle, {0, 0}, L"Existing");

		const AddConnectionResult missingNode = diagram.AddConnection(
			{existing, PortId::Right},
			{NodeId{999}, PortId::Left});
		Require(missingNode.error == DiagramError::NodeNotFound, "Unknown nodes must be rejected.");

		const AddConnectionResult invalidPort = diagram.AddConnection(
			{existing, static_cast<PortId>(99)},
			{existing, PortId::Left});
		Require(invalidPort.error == DiagramError::InvalidPort, "Unknown ports must be rejected.");
		Require(diagram.Connections().empty(), "Rejected connections must not modify the model.");
	}

	void RemovingNodeRemovesItsConnections()
	{
		DiagramModel diagram;
		const NodeId first = diagram.AddNode(NodeKind::Rectangle, {0, 0}, L"First");
		const NodeId second = diagram.AddNode(NodeKind::Rectangle, {200, 0}, L"Second");
		const NodeId third = diagram.AddNode(NodeKind::Rectangle, {400, 0}, L"Third");

		Require(static_cast<bool>(diagram.AddConnection({first, PortId::Right}, {second, PortId::Left})),
			"First connection must be created.");
		Require(static_cast<bool>(diagram.AddConnection({second, PortId::Right}, {third, PortId::Left})),
			"Second connection must be created.");

		Require(diagram.RemoveNode(second) == DiagramError::None, "Existing node must be removed.");
		Require(diagram.FindNode(second) == nullptr, "Removed node must not be found.");
		Require(diagram.Nodes().size() == 2, "Unrelated nodes must remain.");
		Require(diagram.Connections().empty(), "Connections referencing the removed node must be removed.");
	}

	void RoutesOrthogonallyAroundBlocks()
	{
		DiagramModel diagram;
		const NodeId source = diagram.AddNode(NodeKind::Rectangle, {0, 100}, L"Source");
		const NodeId obstacleId = diagram.AddNode(NodeKind::Rectangle, {250, 50}, 150, 200, L"Obstacle");
		const NodeId target = diagram.AddNode(NodeKind::Rectangle, {500, 100}, L"Target");
		const AddConnectionResult added = diagram.AddConnection(
			{source, PortId::Right}, {target, PortId::Left});

		Require(OrthogonalRouter::RouteConnection(diagram, added.connectionId),
			"Existing connection must be routed.");

		const Connection* connection = diagram.FindConnection(added.connectionId);
		Require(connection != nullptr, "Routed connection must still exist.");
		const std::vector<Point> route = GetFullRoute(diagram, *connection);
		RequireOrthogonal(route);
		const Node* obstacle = diagram.FindNode(obstacleId);
		Require(obstacle != nullptr, "Obstacle must exist.");
		RequireAvoids(route, Inflate(GetNodeBounds(*obstacle), 12));
	}

	void ReroutesWhenAnObstacleMoves()
	{
		DiagramModel diagram;
		const NodeId source = diagram.AddNode(NodeKind::Rectangle, {0, 100}, L"Source");
		const NodeId obstacleId = diagram.AddNode(NodeKind::Rectangle, {250, 350}, 150, 200, L"Obstacle");
		const NodeId target = diagram.AddNode(NodeKind::Rectangle, {500, 100}, L"Target");
		const AddConnectionResult added = diagram.AddConnection(
			{source, PortId::Right}, {target, PortId::Left});
		Require(OrthogonalRouter::RouteConnection(diagram, added.connectionId),
			"Initial connection must be routed.");

		const Connection* initialConnection = diagram.FindConnection(added.connectionId);
		Require(initialConnection != nullptr, "Initial connection must exist.");
		const std::vector<Point> initialRoute = GetFullRoute(diagram, *initialConnection);
		Require(initialConnection->intermediatePoints.empty(),
			"Aligned nodes without obstacles should use a direct route.");

		Require(diagram.MoveNodeTo(obstacleId, {250, 50}) == DiagramError::None,
			"Obstacle must move into the direct route.");
		OrthogonalRouter::RouteAll(diagram);

		const Connection* reroutedConnection = diagram.FindConnection(added.connectionId);
		Require(reroutedConnection != nullptr, "Rerouted connection must exist.");
		const std::vector<Point> rerouted = GetFullRoute(diagram, *reroutedConnection);
		Require(rerouted != initialRoute, "The route must change after an obstacle moves into it.");
		RequireOrthogonal(rerouted);
		RequireAvoids(rerouted, Inflate(GetNodeBounds(*diagram.FindNode(obstacleId)), 12));
	}

	void RoutesEveryPortCombination()
	{
		constexpr std::array<PortId, 4> ports{
			PortId::Top, PortId::Right, PortId::Bottom, PortId::Left};

		for (const PortId sourcePort : ports)
		{
			for (const PortId targetPort : ports)
			{
				DiagramModel diagram;
				const NodeId sourceId = diagram.AddNode(NodeKind::Rectangle, {0, 0}, L"Source");
				const NodeId targetId = diagram.AddNode(NodeKind::Rectangle, {400, 300}, L"Target");
				const AddConnectionResult added = diagram.AddConnection(
					{sourceId, sourcePort}, {targetId, targetPort});
				Require(OrthogonalRouter::RouteConnection(diagram, added.connectionId),
					"Every valid port combination must be routable.");

				const Connection* connection = diagram.FindConnection(added.connectionId);
				Require(connection != nullptr, "Port combination connection must exist.");
				const std::vector<Point> route = GetFullRoute(diagram, *connection);
				Require(route.size() >= 2, "A routed connection must contain both endpoints.");
				RequireOrthogonal(route);
				RequirePortDirection(route.front(), route[1], sourcePort);
				RequirePortDirection(route.back(), route[route.size() - 2], targetPort);
			}
		}
	}

	void PersistsAndRestoresDiagram()
	{
		DiagramModel original;
		const NodeId source = original.AddNode(NodeKind::Rectangle, {10, 20}, 180, 90, L"Fuente ñ");
		const NodeId target = original.AddNode(NodeKind::Diamond, {420, 260}, 110, 80, L"Condición");
		const AddConnectionResult connection = original.AddConnection(
			{source, PortId::Right},
			{target, PortId::Top},
			{{240, 65}, {475, 65}});
		Require(static_cast<bool>(connection), "The persisted connection must be valid.");

		TemporaryDiagramFile file;
		const TextDiagramStorage storage;
		const StorageResult saved = storage.Save(file.Path(), original);
		Require(static_cast<bool>(saved), "A valid diagram must be saved.");

		DiagramModel restored;
		const StorageResult loaded = storage.Load(file.Path(), restored);
		Require(static_cast<bool>(loaded), "A saved diagram must be loaded.");
		Require(restored.Nodes().size() == 2, "All nodes must be restored.");
		Require(restored.Connections().size() == 1, "All connections must be restored.");

		const Node& restoredSource = restored.Nodes()[0];
		const Node& restoredTarget = restored.Nodes()[1];
		Require(restoredSource.kind == NodeKind::Rectangle, "The source kind must be restored.");
		Require(restoredSource.position == Point{10, 20}, "The source position must be restored.");
		Require(restoredSource.width == 180 && restoredSource.height == 90,
			"The source dimensions must be restored.");
		Require(restoredSource.label == L"Fuente ñ", "UTF-8 labels must round-trip.");
		Require(restoredTarget.kind == NodeKind::Diamond, "The target kind must be restored.");
		Require(restoredTarget.label == L"Condición", "Accented labels must round-trip.");

		const Connection& restoredConnection = restored.Connections().front();
		Require(restoredConnection.from.nodeId == restoredSource.id,
			"The source endpoint must reference the restored source.");
		Require(restoredConnection.to.nodeId == restoredTarget.id,
			"The target endpoint must reference the restored target.");
		Require(restoredConnection.from.portId == PortId::Right &&
			restoredConnection.to.portId == PortId::Top,
			"Connection ports must be restored.");
		Require(restoredConnection.intermediatePoints == std::vector<Point>{{240, 65}, {475, 65}},
			"The route points must be restored.");
	}

	void PreservesIdentifiersWithGaps()
	{
		DiagramModel original;
		const NodeId first = original.AddNode(NodeKind::Rectangle, {0, 0}, L"First");
		const NodeId removed = original.AddNode(NodeKind::Rectangle, {200, 0}, L"Removed");
		const NodeId third = original.AddNode(NodeKind::Diamond, {400, 0}, L"Third");
		const AddConnectionResult removedConnection = original.AddConnection(
			{first, PortId::Right}, {removed, PortId::Left});
		const AddConnectionResult survivingConnection = original.AddConnection(
			{first, PortId::Right}, {third, PortId::Left});
		Require(static_cast<bool>(removedConnection) && static_cast<bool>(survivingConnection),
			"Both initial connections must be created.");
		Require(original.RemoveNode(removed) == DiagramError::None,
			"Removing the middle node must create an identifier gap.");

		TemporaryDiagramFile file;
		const TextDiagramStorage storage;
		Require(static_cast<bool>(storage.Save(file.Path(), original)),
			"A diagram with identifier gaps must be saved.");

		DiagramModel restored;
		Require(static_cast<bool>(storage.Load(file.Path(), restored)),
			"A diagram with identifier gaps must be loaded.");
		Require(restored.FindNode(first) != nullptr && restored.FindNode(third) != nullptr,
			"Existing node identifiers must be preserved exactly.");
		Require(restored.FindNode(removed) == nullptr,
			"A removed node identifier must remain unused after loading.");
		Require(restored.FindConnection(removedConnection.connectionId) == nullptr,
			"A removed connection identifier must remain unused after loading.");
		Require(restored.FindConnection(survivingConnection.connectionId) != nullptr,
			"The surviving connection identifier must be preserved exactly.");

		const NodeId addedNode = restored.AddNode(NodeKind::Rectangle, {600, 0}, L"Added after load");
		Require(addedNode == NodeId{4}, "The next node identifier must continue after the maximum restored ID.");
		const AddConnectionResult addedConnection = restored.AddConnection(
			{third, PortId::Right}, {addedNode, PortId::Left});
		Require(addedConnection.connectionId == ConnectionId{3},
			"The next connection identifier must continue after the maximum restored ID.");
	}

	void LoadsExistingVersionOneDocumentsWithStableIdentifiers()
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
		Require(static_cast<bool>(result), "An existing version-one document must remain loadable.");
		Require(diagram.FindNode(NodeId{42}) != nullptr && diagram.FindNode(NodeId{7}) != nullptr,
			"Version-one node identifiers must be preserved regardless of file order.");
		Require(diagram.FindConnection(ConnectionId{77}) != nullptr,
			"Version-one connection identifiers must be preserved.");
		Require(diagram.AddNode(NodeKind::Rectangle, {700, 0}, L"Next") == NodeId{43},
			"The next node identifier must use the restored maximum, not the last file entry.");
		const AddConnectionResult added = diagram.AddConnection(
			{NodeId{7}, PortId::Right}, {NodeId{43}, PortId::Left});
		Require(added.connectionId == ConnectionId{78},
			"The next connection identifier must continue after a version-one restored maximum.");
	}

	void RejectsDuplicateNodeIdentifiersAtomically()
	{
		DiagramModel diagram;
		const NodeId existing = diagram.AddNode(NodeKind::Rectangle, {5, 5}, L"Existing");
		DiagramSnapshot snapshot;
		snapshot.nodes = {
			{NodeId{7}, NodeKind::Rectangle, {0, 0}, 150, 100, L"First"},
			{NodeId{7}, NodeKind::Diamond, {200, 0}, 100, 80, L"Duplicate"}};

		const RestoreSnapshotResult result = diagram.RestoreSnapshot(std::move(snapshot));
		Require(result.error == DiagramSnapshotError::DuplicateNodeId,
			"Duplicate node identifiers must be rejected explicitly.");
		Require(result.identifier == 7, "The duplicate identifier must be reported.");
		Require(diagram.Nodes().size() == 1 && diagram.FindNode(existing) != nullptr,
			"A rejected snapshot must not replace the active diagram.");
	}

	void RejectsDuplicateConnectionIdentifiersAtomically()
	{
		DiagramModel diagram;
		const NodeId existing = diagram.AddNode(NodeKind::Rectangle, {5, 5}, L"Existing");
		DiagramSnapshot snapshot;
		snapshot.nodes = {
			{NodeId{10}, NodeKind::Rectangle, {0, 0}, 150, 100, L"First"},
			{NodeId{20}, NodeKind::Rectangle, {300, 0}, 150, 100, L"Second"}};
		snapshot.connections = {
			{ConnectionId{30}, {NodeId{10}, PortId::Right}, {NodeId{20}, PortId::Left}, {}},
			{ConnectionId{30}, {NodeId{20}, PortId::Left}, {NodeId{10}, PortId::Right}, {}}};

		const RestoreSnapshotResult result = diagram.RestoreSnapshot(std::move(snapshot));
		Require(result.error == DiagramSnapshotError::DuplicateConnectionId,
			"Duplicate connection identifiers must be rejected explicitly.");
		Require(result.identifier == 30, "The duplicate connection identifier must be reported.");
		Require(diagram.Nodes().size() == 1 && diagram.FindNode(existing) != nullptr,
			"A rejected connection snapshot must preserve the active diagram.");
	}

	void RejectsDanglingSnapshotConnectionsAtomically()
	{
		DiagramModel diagram;
		const NodeId existing = diagram.AddNode(NodeKind::Rectangle, {5, 5}, L"Existing");
		DiagramSnapshot snapshot;
		snapshot.nodes = {
			{NodeId{10}, NodeKind::Rectangle, {0, 0}, 150, 100, L"Only node"}};
		snapshot.connections = {
			{ConnectionId{20}, {NodeId{10}, PortId::Right}, {NodeId{99}, PortId::Left}, {}}};

		const RestoreSnapshotResult result = diagram.RestoreSnapshot(std::move(snapshot));
		Require(result.error == DiagramSnapshotError::NodeNotFound,
			"Connections to nodes outside the snapshot must be rejected.");
		Require(result.identifier == 99, "The missing endpoint identifier must be reported.");
		Require(diagram.Nodes().size() == 1 && diagram.FindNode(existing) != nullptr,
			"A dangling snapshot must not partially replace the active diagram.");
	}

	void RejectsCorruptFilesWithoutChangingTheDiagram()
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

		Require(result.error == StorageError::InvalidFormat, "A corrupt header must be reported.");
		Require(existing.Nodes().size() == 1, "A failed load must preserve the current diagram.");
		Require(existing.FindNode(originalId) != nullptr, "The original node must remain after a failed load.");
	}

	void RejectsConnectionsToMissingNodes()
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
		Require(result.error == StorageError::InvalidData,
			"Connections to missing nodes must be rejected as invalid data.");
		Require(diagram.Nodes().empty() && diagram.Connections().empty(),
			"A rejected document must not be partially loaded.");
	}

	void RejectsUnsupportedFileVersions()
	{
		TemporaryDiagramFile file;
		{
			std::ofstream output(file.Path(), std::ios::binary | std::ios::trunc);
			output << "ARTESTSTUDIO_DIAGRAM 999\nNODES 0\nCONNECTIONS 0\nEND\n";
		}

		DiagramModel diagram;
		const TextDiagramStorage storage;
		const StorageResult result = storage.Load(file.Path(), diagram);
		Require(result.error == StorageError::UnsupportedVersion,
			"Future file versions must be rejected explicitly.");
	}

	void ReportsMissingDiagramFiles()
	{
		TemporaryDiagramFile file;
		DiagramModel diagram;
		const TextDiagramStorage storage;
		const StorageResult result = storage.Load(file.Path(), diagram);
		Require(result.error == StorageError::FileNotFound, "Missing files must have a specific error.");
	}

	void RejectsNonDiagramFileExtensions()
	{
		TemporaryDiagramFile projectFile{".atprj"};
		DiagramModel diagram;
		(void)diagram.AddNode(NodeKind::Rectangle, {0, 0}, L"Diagram only");
		const TextDiagramStorage storage;

		const StorageResult saveResult = storage.Save(projectFile.Path(), diagram);
		Require(saveResult.error == StorageError::UnsupportedFileExtension,
			"The diagram storage must reject the reserved project extension.");
		Require(!std::filesystem::exists(projectFile.Path()),
			"A rejected extension must not create a file.");

		const StorageResult loadResult = storage.Load(projectFile.Path(), diagram);
		Require(loadResult.error == StorageError::UnsupportedFileExtension,
			"Loading a non-.atd path must report the extension error before accessing the file.");
	}

	void RoutesFaultsThroughTheConfiguredReporter()
	{
		RecordingFaultReporter reporter;
		const FaultReporterScope scope{&reporter};
		FaultService::Report(Fault{
			FaultSeverity::Error,
			FaultCategory::Storage,
			L"TEST_STORAGE_FAILURE",
			L"Regression test",
			L"Expected test fault",
			L"No external side effect"});

		Require(reporter.count == 1, "The configured reporter must receive each fault.");
		Require(reporter.lastSeverity == FaultSeverity::Error, "Fault severity must be preserved.");
		Require(reporter.lastCode == L"TEST_STORAGE_FAILURE", "Fault code must be preserved.");
		Require(reporter.lastOperation == L"Regression test", "Fault operation must be preserved.");
	}

	void WritesStructuredFaultLogs()
	{
		TemporaryDiagramFile file{".log"};
		FileFaultReporter reporter{file.Path()};
		reporter.Report(Fault{
			FaultSeverity::Warning,
			FaultCategory::Storage,
			L"TEST_LOG_ENTRY",
			L"Write regression log",
			L"The logger must write UTF-8",
			L"Technical detail 42"});

		std::ifstream input(file.Path(), std::ios::binary);
		const std::string contents{
			std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
		Require(input.good() || input.eof(), "The generated fault log must be readable.");
		Require(contents.find("[WARNING]") != std::string::npos, "The log must contain severity.");
		Require(contents.find("[STORAGE]") != std::string::npos, "The log must contain category.");
		Require(contents.find("TEST_LOG_ENTRY") != std::string::npos, "The log must contain the stable code.");
		Require(contents.find("Technical detail 42") != std::string::npos,
			"The log must contain the technical detail.");
	}

	void FaultReportingFailuresNeverEscape()
	{
		ThrowingFaultReporter reporter;
		const FaultReporterScope scope{&reporter};
		FaultService::Report(Fault{
			FaultSeverity::Critical,
			FaultCategory::Unexpected,
			L"TEST_THROWING_REPORTER",
			L"Fault boundary regression",
			L"The reporter intentionally throws",
			{}});
		Require(true, "A reporter exception must not escape FaultService.");
	}

	void RotatesOversizedFaultLogs()
	{
		TemporaryDiagramFile file{".log"};
		{
			std::ofstream output(file.Path(), std::ios::binary | std::ios::trunc);
			const std::string block(1024, 'x');
			for (int index = 0; index < 2048; ++index)
			{
				output.write(block.data(), static_cast<std::streamsize>(block.size()));
			}
		}

		FileFaultReporter reporter{file.Path()};
		reporter.Report(Fault{
			FaultSeverity::Error,
			FaultCategory::Application,
			L"TEST_ROTATED_ENTRY",
			L"Rotate regression log",
			L"This entry belongs to the new log",
			{}});

		const std::filesystem::path previousPath = file.Path().parent_path() /
			(file.Path().stem().wstring() + L".previous" + file.Path().extension().wstring());
		Require(std::filesystem::exists(previousPath), "An oversized log must be preserved as previous.log.");
		Require(std::filesystem::file_size(previousPath) >= 2 * 1024 * 1024,
			"The rotated file must contain the previous log contents.");

		std::ifstream input(file.Path(), std::ios::binary);
		const std::string contents{
			std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
		Require(contents.find("TEST_ROTATED_ENTRY") != std::string::npos,
			"The fault that triggered rotation must be written to the new log.");
	}
}

int main()
{
	const std::vector<TestCase> tests{
		{"adds and moves nodes", AddsAndMovesNodes},
		{"creates valid connections", CreatesValidConnections},
		{"rejects invalid connections", RejectsInvalidConnections},
		{"removing node removes its connections", RemovingNodeRemovesItsConnections},
		{"routes orthogonally around blocks", RoutesOrthogonallyAroundBlocks},
		{"reroutes when an obstacle moves", ReroutesWhenAnObstacleMoves},
		{"routes every port combination", RoutesEveryPortCombination},
		{"persists and restores diagrams", PersistsAndRestoresDiagram},
		{"preserves identifiers with gaps", PreservesIdentifiersWithGaps},
		{"loads existing version-one documents with stable identifiers", LoadsExistingVersionOneDocumentsWithStableIdentifiers},
		{"rejects duplicate node identifiers atomically", RejectsDuplicateNodeIdentifiersAtomically},
		{"rejects duplicate connection identifiers atomically", RejectsDuplicateConnectionIdentifiersAtomically},
		{"rejects dangling snapshot connections atomically", RejectsDanglingSnapshotConnectionsAtomically},
		{"rejects corrupt files without changing the diagram", RejectsCorruptFilesWithoutChangingTheDiagram},
		{"rejects connections to missing nodes", RejectsConnectionsToMissingNodes},
		{"rejects unsupported file versions", RejectsUnsupportedFileVersions},
		{"reports missing diagram files", ReportsMissingDiagramFiles},
		{"rejects non-diagram file extensions", RejectsNonDiagramFileExtensions},
		{"routes faults through the configured reporter", RoutesFaultsThroughTheConfiguredReporter},
		{"writes structured fault logs", WritesStructuredFaultLogs},
		{"fault reporting failures never escape", FaultReportingFailuresNeverEscape},
		{"rotates oversized fault logs", RotatesOversizedFaultLogs},
	};

	int failures = 0;
	for (const auto& test : tests)
	{
		try
		{
			test.execute();
			std::cout << "[PASS] " << test.name << '\n';
		}
		catch (const std::exception& error)
		{
			++failures;
			std::cerr << "[FAIL] " << test.name << ": " << error.what() << '\n';
		}
		catch (...)
		{
			++failures;
			std::cerr << "[FAIL] " << test.name << ": unknown error\n";
		}
	}

	std::cout << tests.size() - static_cast<std::size_t>(failures)
		<< '/' << tests.size() << " tests passed\n";
	return failures == 0 ? 0 : 1;
}
