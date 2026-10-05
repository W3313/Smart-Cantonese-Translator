# Tests for sct_tts - add with sct_add_test().
# All run headless: no TTS backend, voices or audio device are required.

sct_add_test(tst_ssmlbuilder SOURCES tst_ssmlbuilder.cpp LIBS sct_tts)
sct_add_test(tst_voicematch SOURCES tst_voicematch.cpp LIBS sct_tts)
sct_add_test(tst_audiocache SOURCES tst_audiocache.cpp LIBS sct_tts)
sct_add_test(tst_azurespeech SOURCES tst_azurespeech.cpp FakeAzureServer.h LIBS sct_tts)
sct_add_test(tst_speechservice SOURCES tst_speechservice.cpp FakeAzureServer.h LIBS sct_tts)
sct_add_test(tst_systemspeech SOURCES tst_systemspeech.cpp fake_tts_sapi.json fake_tts_winrt.json LIBS sct_tts)
