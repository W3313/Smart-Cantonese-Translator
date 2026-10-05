#pragma once

class QWidget;

namespace sct::ui {

// About box: version, credits and the privacy note.
void showAboutDialog(QWidget *parent);

// Cheat sheet of the keyboard shortcuts.
void showShortcutsDialog(QWidget *parent);

} // namespace sct::ui
