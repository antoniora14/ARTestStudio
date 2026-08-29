#include "EditCommand.h"

#include <utility>

namespace arteststudio::application
{
	SnapshotEditCommand::SnapshotEditCommand(
		std::wstring description,
		domain::DiagramSnapshot before,
		domain::DiagramSnapshot after)
		: m_description(std::move(description)),
		  m_before(std::move(before)),
		  m_after(std::move(after))
	{
	}

	bool SnapshotEditCommand::Undo(domain::DiagramModel& diagram)
	{
		return static_cast<bool>(diagram.RestoreSnapshot(m_before));
	}

	bool SnapshotEditCommand::Redo(domain::DiagramModel& diagram)
	{
		return static_cast<bool>(diagram.RestoreSnapshot(m_after));
	}

	std::wstring_view SnapshotEditCommand::Description() const noexcept
	{
		return m_description;
	}

	void CommandHistory::RecordExecuted(
		std::wstring description,
		domain::DiagramSnapshot before,
		domain::DiagramSnapshot after)
	{
		m_undo.push_back(std::make_unique<SnapshotEditCommand>(
			std::move(description),
			std::move(before),
			std::move(after)));
		m_redo.clear();
	}

	bool CommandHistory::Undo(domain::DiagramModel& diagram)
	{
		if (m_undo.empty() || !m_undo.back()->Undo(diagram))
		{
			return false;
		}

		m_redo.push_back(std::move(m_undo.back()));
		m_undo.pop_back();
		return true;
	}

	bool CommandHistory::Redo(domain::DiagramModel& diagram)
	{
		if (m_redo.empty() || !m_redo.back()->Redo(diagram))
		{
			return false;
		}

		m_undo.push_back(std::move(m_redo.back()));
		m_redo.pop_back();
		return true;
	}

	bool CommandHistory::CanUndo() const noexcept
	{
		return !m_undo.empty();
	}

	bool CommandHistory::CanRedo() const noexcept
	{
		return !m_redo.empty();
	}

	std::wstring_view CommandHistory::UndoDescription() const noexcept
	{
		return m_undo.empty() ? std::wstring_view{} : m_undo.back()->Description();
	}

	std::wstring_view CommandHistory::RedoDescription() const noexcept
	{
		return m_redo.empty() ? std::wstring_view{} : m_redo.back()->Description();
	}

	void CommandHistory::Clear() noexcept
	{
		m_undo.clear();
		m_redo.clear();
	}
}
