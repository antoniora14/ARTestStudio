#pragma once

#include "EditCommand.h"
#include "SelectionModel.h"
#include "../Domain/DiagramModel.h"

#include <optional>
#include <string>

namespace arteststudio::application
{
	enum class InteractionState
	{
		Idle,
		DraggingNode,
		CreatingConnection
	};

	class InteractionController
	{
	public:
		explicit InteractionController(domain::DiagramModel& diagram) noexcept;

		void Reset() noexcept;
		[[nodiscard]] InteractionState State() const noexcept;
		[[nodiscard]] bool IsPointerActionActive() const noexcept;
		[[nodiscard]] const SelectionModel& Selection() const noexcept;
		[[nodiscard]] std::optional<domain::Point> ConnectionPreviewStart() const noexcept;
		[[nodiscard]] std::optional<domain::Point> ConnectionPreviewEnd() const noexcept;

		void BeginPrimaryAction(domain::Point point);
		[[nodiscard]] bool UpdatePrimaryAction(domain::Point point);
		[[nodiscard]] bool EndPrimaryAction(domain::Point point);
		void CancelPrimaryAction() noexcept;

		void SelectAt(domain::Point point) noexcept;
		void SetContextPoint(domain::Point point) noexcept;

		[[nodiscard]] bool AddNode(
			domain::NodeKind kind,
			domain::Point position,
			std::wstring label,
			int width = 150,
			int height = 100);
		[[nodiscard]] bool CopySelection();
		[[nodiscard]] bool CutSelection();
		[[nodiscard]] bool Paste();
		[[nodiscard]] bool DeleteSelection();

		[[nodiscard]] bool Undo();
		[[nodiscard]] bool Redo();
		[[nodiscard]] bool CanUndo() const noexcept;
		[[nodiscard]] bool CanRedo() const noexcept;
		[[nodiscard]] bool CanCopy() const noexcept;
		[[nodiscard]] bool CanPaste() const noexcept;
		[[nodiscard]] bool CanDelete() const noexcept;
		[[nodiscard]] std::wstring_view UndoDescription() const noexcept;
		[[nodiscard]] std::wstring_view RedoDescription() const noexcept;

	private:
		void FinishInteraction() noexcept;
		void ReconcileSelection() noexcept;

		domain::DiagramModel& m_diagram;
		SelectionModel m_selection;
		CommandHistory m_history;
		InteractionState m_state = InteractionState::Idle;
		std::optional<domain::ConnectionEndpoint> m_connectionStart;
		std::optional<domain::Point> m_previewStart;
		std::optional<domain::Point> m_previewEnd;
		std::optional<domain::NodeId> m_draggingNodeId;
		std::optional<domain::DiagramSnapshot> m_dragBefore;
		domain::Point m_lastPointer;
		domain::Point m_contextPoint;
		bool m_dragChanged = false;
		std::optional<domain::Node> m_clipboardNode;
	};
}
