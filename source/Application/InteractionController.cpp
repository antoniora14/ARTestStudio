#include "InteractionController.h"

#include "../Domain/DiagramGeometry.h"
#include "../Domain/DiagramHitTesting.h"
#include "../Domain/OrthogonalRouter.h"

#include <utility>

namespace arteststudio::application
{
	using domain::DiagramHitTesting;
	using domain::DiagramSnapshot;
	using domain::OrthogonalRouter;

	InteractionController::InteractionController(domain::DiagramModel& diagram) noexcept
		: m_diagram(diagram)
	{
	}

	void InteractionController::Reset() noexcept
	{
		FinishInteraction();
		m_selection.Clear();
		m_history.Clear();
		m_clipboardNode.reset();
		m_contextPoint = {};
	}

	InteractionState InteractionController::State() const noexcept
	{
		return m_state;
	}

	bool InteractionController::IsPointerActionActive() const noexcept
	{
		return m_state != InteractionState::Idle;
	}

	const SelectionModel& InteractionController::Selection() const noexcept
	{
		return m_selection;
	}

	std::optional<domain::Point> InteractionController::ConnectionPreviewStart() const noexcept
	{
		return m_previewStart;
	}

	std::optional<domain::Point> InteractionController::ConnectionPreviewEnd() const noexcept
	{
		return m_previewEnd;
	}

	void InteractionController::BeginPrimaryAction(domain::Point point)
	{
		CancelPrimaryAction();
		if (const auto port = DiagramHitTesting::FindPortAt(m_diagram, point); port.has_value())
		{
			m_state = InteractionState::CreatingConnection;
			m_connectionStart = domain::ConnectionEndpoint{port->nodeId, port->portId};
			m_previewStart = port->point;
			m_previewEnd = port->point;
			m_selection.SelectNode(port->nodeId);
			return;
		}

		if (const auto nodeId = DiagramHitTesting::FindNodeAt(m_diagram, point); nodeId.has_value())
		{
			m_state = InteractionState::DraggingNode;
			m_draggingNodeId = nodeId;
			m_dragBefore = m_diagram.CaptureSnapshot();
			m_lastPointer = point;
			m_dragChanged = false;
			m_selection.SelectNode(*nodeId);
			return;
		}

		if (const auto connectionId = DiagramHitTesting::FindConnectionAt(m_diagram, point); connectionId.has_value())
		{
			m_selection.SelectConnection(*connectionId);
		}
		else
		{
			m_selection.Clear();
		}
	}

	bool InteractionController::UpdatePrimaryAction(domain::Point point)
	{
		if (m_state == InteractionState::CreatingConnection)
		{
			m_previewEnd = point;
			return false;
		}

		if (m_state != InteractionState::DraggingNode || !m_draggingNodeId.has_value())
		{
			return false;
		}

		const int dx = point.x - m_lastPointer.x;
		const int dy = point.y - m_lastPointer.y;
		m_lastPointer = point;
		if ((dx == 0 && dy == 0) ||
			m_diagram.MoveNodeBy(*m_draggingNodeId, dx, dy) != domain::DiagramError::None)
		{
			return false;
		}

		OrthogonalRouter::RouteAll(m_diagram);
		m_dragChanged = true;
		return true;
	}

	bool InteractionController::EndPrimaryAction(domain::Point point)
	{
		if (m_state == InteractionState::CreatingConnection && m_connectionStart.has_value())
		{
			const auto target = DiagramHitTesting::FindPortAt(m_diagram, point);
			const domain::ConnectionEndpoint source = *m_connectionStart;
			FinishInteraction();
			if (!target.has_value())
			{
				return false;
			}

			DiagramSnapshot before = m_diagram.CaptureSnapshot();
			const auto added = m_diagram.AddConnection(
				source,
				domain::ConnectionEndpoint{target->nodeId, target->portId});
			if (!added)
			{
				return false;
			}

			(void)OrthogonalRouter::RouteConnection(m_diagram, added.connectionId);
			m_history.RecordExecuted(
				L"Create connection",
				std::move(before),
				m_diagram.CaptureSnapshot());
			m_selection.SelectConnection(added.connectionId);
			return true;
		}

		if (m_state == InteractionState::DraggingNode)
		{
			const bool changed = m_dragChanged && m_dragBefore.has_value();
			std::optional<DiagramSnapshot> before = std::move(m_dragBefore);
			FinishInteraction();
			if (changed)
			{
				m_history.RecordExecuted(
					L"Move node",
					std::move(*before),
					m_diagram.CaptureSnapshot());
			}
			return changed;
		}

		FinishInteraction();
		return false;
	}

	void InteractionController::CancelPrimaryAction() noexcept
	{
		if (m_state == InteractionState::DraggingNode && m_dragChanged && m_dragBefore.has_value())
		{
			(void)m_diagram.RestoreSnapshot(std::move(*m_dragBefore));
		}
		FinishInteraction();
	}

	void InteractionController::SelectAt(domain::Point point) noexcept
	{
		m_contextPoint = point;
		if (const auto nodeId = DiagramHitTesting::FindNodeAt(m_diagram, point); nodeId.has_value())
		{
			m_selection.SelectNode(*nodeId);
			return;
		}
		if (const auto connectionId = DiagramHitTesting::FindConnectionAt(m_diagram, point); connectionId.has_value())
		{
			m_selection.SelectConnection(*connectionId);
			return;
		}
		m_selection.Clear();
	}

	void InteractionController::SetContextPoint(domain::Point point) noexcept
	{
		m_contextPoint = point;
	}

	bool InteractionController::AddNode(
		domain::NodeKind kind,
		domain::Point position,
		std::wstring label,
		int width,
		int height)
	{
		DiagramSnapshot before = m_diagram.CaptureSnapshot();
		const domain::NodeId nodeId = m_diagram.AddNode(
			kind,
			position,
			width,
			height,
			std::move(label));
		OrthogonalRouter::RouteAll(m_diagram);
		m_history.RecordExecuted(
			L"Add node",
			std::move(before),
			m_diagram.CaptureSnapshot());
		m_selection.SelectNode(nodeId);
		return true;
	}

	bool InteractionController::CopySelection()
	{
		if (!m_selection.NodeId().has_value())
		{
			return false;
		}
		const domain::Node* node = m_diagram.FindNode(*m_selection.NodeId());
		if (node == nullptr)
		{
			ReconcileSelection();
			return false;
		}
		m_clipboardNode = *node;
		return true;
	}

	bool InteractionController::CutSelection()
	{
		return CopySelection() && DeleteSelection();
	}

	bool InteractionController::Paste()
	{
		if (!m_clipboardNode.has_value())
		{
			return false;
		}
		const domain::Node copied = *m_clipboardNode;
		return AddNode(
			copied.kind,
			m_contextPoint,
			copied.label,
			copied.width,
			copied.height);
	}

	bool InteractionController::DeleteSelection()
	{
		if (!CanDelete())
		{
			return false;
		}

		DiagramSnapshot before = m_diagram.CaptureSnapshot();
		domain::DiagramError result = domain::DiagramError::None;
		if (m_selection.NodeId().has_value())
		{
			result = m_diagram.RemoveNode(*m_selection.NodeId());
		}
		else
		{
			result = m_diagram.RemoveConnection(*m_selection.ConnectionId());
		}
		if (result != domain::DiagramError::None)
		{
			ReconcileSelection();
			return false;
		}

		OrthogonalRouter::RouteAll(m_diagram);
		m_history.RecordExecuted(
			L"Delete selection",
			std::move(before),
			m_diagram.CaptureSnapshot());
		m_selection.Clear();
		return true;
	}

	bool InteractionController::Undo()
	{
		CancelPrimaryAction();
		if (!m_history.Undo(m_diagram))
		{
			return false;
		}
		ReconcileSelection();
		return true;
	}

	bool InteractionController::Redo()
	{
		CancelPrimaryAction();
		if (!m_history.Redo(m_diagram))
		{
			return false;
		}
		ReconcileSelection();
		return true;
	}

	bool InteractionController::CanUndo() const noexcept
	{
		return m_history.CanUndo();
	}

	bool InteractionController::CanRedo() const noexcept
	{
		return m_history.CanRedo();
	}

	bool InteractionController::CanCopy() const noexcept
	{
		return m_selection.NodeId().has_value() &&
			m_diagram.FindNode(*m_selection.NodeId()) != nullptr;
	}

	bool InteractionController::CanPaste() const noexcept
	{
		return m_clipboardNode.has_value();
	}

	bool InteractionController::CanDelete() const noexcept
	{
		return (m_selection.NodeId().has_value() &&
			m_diagram.FindNode(*m_selection.NodeId()) != nullptr) ||
			(m_selection.ConnectionId().has_value() &&
			m_diagram.FindConnection(*m_selection.ConnectionId()) != nullptr);
	}

	std::wstring_view InteractionController::UndoDescription() const noexcept
	{
		return m_history.UndoDescription();
	}

	std::wstring_view InteractionController::RedoDescription() const noexcept
	{
		return m_history.RedoDescription();
	}

	void InteractionController::FinishInteraction() noexcept
	{
		m_state = InteractionState::Idle;
		m_connectionStart.reset();
		m_previewStart.reset();
		m_previewEnd.reset();
		m_draggingNodeId.reset();
		m_dragBefore.reset();
		m_dragChanged = false;
	}

	void InteractionController::ReconcileSelection() noexcept
	{
		if ((m_selection.NodeId().has_value() &&
			m_diagram.FindNode(*m_selection.NodeId()) == nullptr) ||
			(m_selection.ConnectionId().has_value() &&
			m_diagram.FindConnection(*m_selection.ConnectionId()) == nullptr))
		{
			m_selection.Clear();
		}
	}
}
