CONTIKI_PROJECT = udp-client udp-server
all: $(CONTIKI_PROJECT)

CONTIKI=../..

include $(CONTIKI)/Makefile.include
CONTIKIDIRS += $(CONTIKI)/Bulldog/rpl-udp/
APP += powertrace
PROJECT_SOURCEFILES += powertrace.c
