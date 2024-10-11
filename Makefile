
SRCDIR=$(PROJDIR)../../thirdparty/owemdjee/libpathutils/

GPERF=$(BINDIR)/gperf.exe

# https://www.gnu.org/software/make/manual/html_node/Automatic-Variables.html#:~:text=In%20a%20pattern%20rule%20that%20has%20multiple%20targets%20(see%20Introduction

all: $(SRCDIR)system_channels.hashcheck.cpp

$(SRCDIR)system_channels.hashcheck.cpp : $(SRCDIR)system_channels.gperf
	$(GPERF) --output-file=$@ $<

.PHONY: all
