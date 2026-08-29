#include "../TestSupport/TestSupport.h"

#include "Application/InteractionController.h"
#include "Domain/DiagramGeometry.h"

#include <gtest/gtest.h>

namespace arteststudio::tests
{
	using namespace application;
	using namespace domain;

	TEST(InteractionControllerTests, AddsNodesAndSupportsUndoRedo)
	{
		DiagramModel diagram;
		InteractionController controller{diagram};

		VerifyTestCondition(
			controller.AddNode(NodeKind::Rectangle, {100, 100}, L"Step"),
			"Adding a node through the controller must succeed.");
		VerifyTestCondition(diagram.Nodes().size() == 1, "The new node must be stored.");
		VerifyTestCondition(controller.CanUndo(), "Adding a node must create an undo command.");

		VerifyTestCondition(controller.Undo(), "The add command must be undoable.");
		VerifyTestCondition(diagram.Nodes().empty(), "Undo must remove the added node.");
		VerifyTestCondition(controller.CanRedo(), "Undo must make redo available.");

		VerifyTestCondition(controller.Redo(), "The add command must be redoable.");
		VerifyTestCondition(diagram.Nodes().size() == 1, "Redo must restore the node.");
	}

	TEST(InteractionControllerTests, TreatsAnEntireDragAsOneCommand)
	{
		DiagramModel diagram;
		const NodeId nodeId = diagram.AddNode(NodeKind::Rectangle, {100, 100}, L"Step");
		InteractionController controller{diagram};

		controller.BeginPrimaryAction({100, 100});
		VerifyTestCondition(
			controller.State() == InteractionState::DraggingNode,
			"Pressing a node must enter the dragging state.");
		VerifyTestCondition(controller.UpdatePrimaryAction({120, 110}), "The first pointer move must change the model.");
		VerifyTestCondition(controller.UpdatePrimaryAction({150, 140}), "The second pointer move must change the model.");
		VerifyTestCondition(controller.EndPrimaryAction({150, 140}), "Ending a changed drag must commit it.");
		VerifyTestCondition(
			diagram.FindNode(nodeId)->position == Point{150, 140},
			"The node must finish at the final pointer position.");

		VerifyTestCondition(controller.Undo(), "The complete drag must require one undo.");
		VerifyTestCondition(
			diagram.FindNode(nodeId)->position == Point{100, 100},
			"One undo must restore the position from before the drag.");
		VerifyTestCondition(!controller.CanUndo(), "The drag must not create one command per mouse move.");

		VerifyTestCondition(controller.Redo(), "The drag must be redoable.");
		VerifyTestCondition(
			diagram.FindNode(nodeId)->position == Point{150, 140},
			"Redo must restore the final drag position.");
	}

	TEST(InteractionControllerTests, CancellingADragRestoresTheOriginalDiagram)
	{
		DiagramModel diagram;
		const NodeId nodeId = diagram.AddNode(NodeKind::Rectangle, {100, 100}, L"Step");
		InteractionController controller{diagram};

		controller.BeginPrimaryAction({100, 100});
		VerifyTestCondition(controller.UpdatePrimaryAction({180, 160}), "Dragging must move the node.");
		controller.CancelPrimaryAction();

		VerifyTestCondition(
			diagram.FindNode(nodeId)->position == Point{100, 100},
			"Cancelling must restore the diagram snapshot.");
		VerifyTestCondition(
			controller.State() == InteractionState::Idle,
			"Cancelling must return to the idle state.");
		VerifyTestCondition(!controller.CanUndo(), "A cancelled drag must not enter command history.");
	}

	TEST(InteractionControllerTests, CreatesConnectionsThroughTheStateMachine)
	{
		DiagramModel diagram;
		const NodeId source = diagram.AddNode(NodeKind::Rectangle, {100, 100}, L"Source");
		const NodeId target = diagram.AddNode(NodeKind::Rectangle, {400, 100}, L"Target");
		InteractionController controller{diagram};
		const Point sourcePort = GetConnectionPoint(*diagram.FindNode(source), PortId::Right);
		const Point targetPort = GetConnectionPoint(*diagram.FindNode(target), PortId::Left);

		controller.BeginPrimaryAction(sourcePort);
		VerifyTestCondition(
			controller.State() == InteractionState::CreatingConnection,
			"Pressing a port must enter connection creation.");
		VerifyTestCondition(controller.ConnectionPreviewStart() == sourcePort, "Preview must start at the source port.");
		(void)controller.UpdatePrimaryAction({250, 160});
		VerifyTestCondition(
			controller.ConnectionPreviewEnd() == Point{250, 160},
			"Preview must follow the pointer.");
		VerifyTestCondition(controller.EndPrimaryAction(targetPort), "Releasing on a target port must create a connection.");
		VerifyTestCondition(diagram.Connections().size() == 1, "The connection must be stored.");
		VerifyTestCondition(
			controller.Selection().ConnectionId().has_value(),
			"The created connection must become the central selection.");

		VerifyTestCondition(controller.Undo(), "Connection creation must be undoable.");
		VerifyTestCondition(diagram.Connections().empty(), "Undo must remove the connection.");
		VerifyTestCondition(controller.Redo(), "Connection creation must be redoable.");
		VerifyTestCondition(diagram.Connections().size() == 1, "Redo must restore the connection.");
	}

	TEST(InteractionControllerTests, DeletingANodeAndItsConnectionsIsUndoable)
	{
		DiagramModel diagram;
		const NodeId source = diagram.AddNode(NodeKind::Rectangle, {100, 100}, L"Source");
		const NodeId target = diagram.AddNode(NodeKind::Rectangle, {400, 100}, L"Target");
		VerifyTestCondition(
			static_cast<bool>(diagram.AddConnection({source, PortId::Right}, {target, PortId::Left})),
			"Test setup must create a connection.");
		InteractionController controller{diagram};

		controller.SelectAt({100, 100});
		VerifyTestCondition(controller.DeleteSelection(), "The selected node must be deleted.");
		VerifyTestCondition(diagram.FindNode(source) == nullptr, "The selected node must be absent.");
		VerifyTestCondition(diagram.Connections().empty(), "Deleting a node must also remove its connections.");

		VerifyTestCondition(controller.Undo(), "Deletion must be undoable.");
		VerifyTestCondition(diagram.FindNode(source) != nullptr, "Undo must restore the node.");
		VerifyTestCondition(diagram.Connections().size() == 1, "Undo must restore dependent connections.");
	}

	TEST(InteractionControllerTests, OwnsClipboardAndSelectionForCutAndPaste)
	{
		DiagramModel diagram;
		const NodeId original = diagram.AddNode(NodeKind::Diamond, {100, 100}, 120, 80, L"Decision");
		InteractionController controller{diagram};

		controller.SelectAt({100, 100});
		VerifyTestCondition(controller.CanCopy(), "A selected node must be copyable.");
		VerifyTestCondition(controller.CutSelection(), "Cut must copy and delete the selected node.");
		VerifyTestCondition(diagram.FindNode(original) == nullptr, "Cut must remove the original node.");
		VerifyTestCondition(controller.CanPaste(), "Cut must populate the controller clipboard.");

		controller.SetContextPoint({500, 300});
		VerifyTestCondition(controller.Paste(), "Clipboard content must be pasteable.");
		VerifyTestCondition(diagram.Nodes().size() == 1, "Paste must create one replacement node.");
		const Node& pasted = diagram.Nodes().front();
		VerifyTestCondition(pasted.id != original, "Paste must allocate a new stable identifier.");
		VerifyTestCondition(pasted.position == Point{500, 300}, "Paste must use the context position.");
		VerifyTestCondition(
			pasted.kind == NodeKind::Diamond && pasted.width == 120 && pasted.height == 80 &&
				pasted.label == L"Decision",
			"Paste must preserve the copied node properties.");
	}

	TEST(InteractionControllerTests, ANewEditClearsTheRedoBranch)
	{
		DiagramModel diagram;
		InteractionController controller{diagram};
		VerifyTestCondition(controller.AddNode(NodeKind::Rectangle, {100, 100}, L"First"), "First add must succeed.");
		VerifyTestCondition(controller.Undo(), "First add must be undoable.");
		VerifyTestCondition(controller.CanRedo(), "Redo must exist after undo.");

		VerifyTestCondition(controller.AddNode(NodeKind::Rectangle, {300, 100}, L"Replacement"), "New add must succeed.");
		VerifyTestCondition(!controller.CanRedo(), "A new command must clear the abandoned redo branch.");
	}

	TEST(InteractionControllerTests, ResetClearsTransientStateSelectionAndHistory)
	{
		DiagramModel diagram;
		InteractionController controller{diagram};
		VerifyTestCondition(controller.AddNode(NodeKind::Rectangle, {100, 100}, L"Step"), "Add must succeed.");
		controller.BeginPrimaryAction({100, 100});
		VerifyTestCondition(controller.IsPointerActionActive(), "Drag must be active before reset.");

		controller.Reset();
		VerifyTestCondition(controller.State() == InteractionState::Idle, "Reset must cancel transient interaction.");
		VerifyTestCondition(!controller.Selection().NodeId().has_value(), "Reset must clear node selection.");
		VerifyTestCondition(!controller.Selection().ConnectionId().has_value(), "Reset must clear connection selection.");
		VerifyTestCondition(!controller.CanUndo() && !controller.CanRedo(), "Reset must clear command history.");
	}
}
