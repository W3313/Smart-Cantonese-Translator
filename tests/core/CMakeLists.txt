# Tests for sct_core - add with sct_add_test().
# None of these tests touch the real network: HTTP tests talk to a local mock
# server (MockHttpServer.h) on 127.0.0.1.

sct_add_test(tst_translationtypes SOURCES tst_translationtypes.cpp LIBS sct_core)
sct_add_test(tst_secretstore SOURCES tst_secretstore.cpp LIBS sct_core)
sct_add_test(tst_appsettings SOURCES tst_appsettings.cpp LIBS sct_core)
sct_add_test(tst_historystore SOURCES tst_historystore.cpp LIBS sct_core)
sct_add_test(tst_promptbuilder SOURCES tst_promptbuilder.cpp LIBS sct_core)
sct_add_test(tst_responseparser SOURCES tst_responseparser.cpp LIBS sct_core)
sct_add_test(tst_claudeapi SOURCES tst_claudeapi.cpp LIBS sct_core)
sct_add_test(tst_openaiapi SOURCES tst_openaiapi.cpp LIBS sct_core)
sct_add_test(tst_httpprovider SOURCES tst_httpprovider.cpp MockHttpServer.h LIBS sct_core)
sct_add_test(tst_translationservice SOURCES tst_translationservice.cpp LIBS sct_core)
