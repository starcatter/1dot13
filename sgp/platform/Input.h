#pragma once

#include "types.h"

namespace Platform::Input
{
	// Starts/stops the selected host's event source. The Windows implementation
	// installs the legacy thread-local mouse hook; portable hosts inject events.
	bool InitializeEventSource() noexcept;
	void ShutdownEventSource() noexcept;

	// All positions and rectangles crossing this boundary are expressed in the
	// game's logical client coordinates, never desktop/output coordinates.
	SGPPoint GetCursorPosition() noexcept;
	SGPPoint ScreenToLogicalPosition(SGPPoint position) noexcept;
	void SetCursorPosition(SGPPoint position) noexcept;

	void RestrictCursor(const SGPRect& rectangle) noexcept;
	void FreeCursor() noexcept;
	void RestoreCursorRestriction() noexcept;
	bool IsCursorRestricted() noexcept;
	SGPRect GetCursorRestriction() noexcept;

	// Legacy configurable keys retain their Win32 virtual-key numeric values.
	bool IsLegacyKeyPressed(UINT8 key) noexcept;

	// Event-driven hosts maintain the physical-key snapshot used by configurable
	// key bindings. Polling hosts may implement these as no-ops.
	void SetLegacyKeyPressed(UINT8 key, bool pressed) noexcept;
	void ClearLegacyKeyState() noexcept;

	// Flushes queued native keyboard events before the engine queue is cleared.
	void FlushPendingKeyboardEvents() noexcept;
}
