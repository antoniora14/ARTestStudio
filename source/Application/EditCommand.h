#pragma once

#include "../Domain/DiagramModel.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace arteststudio::application
{
	class IEditCommand
	{
	public:
		virtual ~IEditCommand() = default;

		[[nodiscard]] virtual bool Undo(domain::DiagramModel& diagram) = 0;
		[[nodiscard]] virtual bool Redo(domain::DiagramModel& diagram) = 0;
		[[nodiscard]] virtual std::wstring_view Description() const noexcept = 0;
	};

	class SnapshotEditCommand final : public IEditCommand
	{
	public:
		SnapshotEditCommand(
			std::wstring description,
			domain::DiagramSnapshot before,
			domain::DiagramSnapshot after);

		[[nodiscard]] bool Undo(domain::DiagramModel& diagram) override;
		[[nodiscard]] bool Redo(domain::DiagramModel& diagram) override;
		[[nodiscard]] std::wstring_view Description() const noexcept override;

	private:
		std::wstring m_description;
		domain::DiagramSnapshot m_before;
		domain::DiagramSnapshot m_after;
	};

	class CommandHistory
	{
	public:
		void RecordExecuted(
			std::wstring description,
			domain::DiagramSnapshot before,
			domain::DiagramSnapshot after);

		[[nodiscard]] bool Undo(domain::DiagramModel& diagram);
		[[nodiscard]] bool Redo(domain::DiagramModel& diagram);
		[[nodiscard]] bool CanUndo() const noexcept;
		[[nodiscard]] bool CanRedo() const noexcept;
		[[nodiscard]] std::wstring_view UndoDescription() const noexcept;
		[[nodiscard]] std::wstring_view RedoDescription() const noexcept;
		void Clear() noexcept;

	private:
		std::vector<std::unique_ptr<IEditCommand>> m_undo;
		std::vector<std::unique_ptr<IEditCommand>> m_redo;
	};
}
