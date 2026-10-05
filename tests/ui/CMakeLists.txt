# Tests for sct_ui - add with sct_add_test().
# Headless (offscreen) and offline: the AI provider is a scriptable fake.
# Set SCT_SCREENSHOT_DIR=<dir> when running tst_mainwindow to save PNGs of
# every major UI state for visual review.

sct_add_test(tst_mainwindow SOURCES tst_mainwindow.cpp LIBS sct_ui)
