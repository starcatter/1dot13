#pragma once

#include <string_view>

namespace Platform
{
	enum class DialogKind
	{
		information,
		warning,
		error
	};

	// Dialog boundaries use UTF-8. The native pre-window fallback writes to
	// stderr; questions default to no so consent is never inferred.
	void ShowDialog(
		std::string_view titleUtf8, std::string_view messageUtf8, DialogKind kind) noexcept;
	bool AskYesNo(std::string_view titleUtf8, std::string_view messageUtf8) noexcept;
}
