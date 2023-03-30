TARGET = JanusC
CC = g++
CFLAGS = -Wall -Wno-write-strings
LDFLAGS = -pthread -lrt 
INCLUDE = -I/usr/include/libusb-1.0
LDADD = -lusb-1.0
SRCDIR = src
BINDIR = bin
MYDEF = FERS_5202

.PHONY: default all clean

default: $(TARGET)
all: default

OBJECTS = $(SRCDIR)/configure.o $(SRCDIR)/console.o $(SRCDIR)/FERS_LLeth.o $(SRCDIR)/FERS_LLusb.o $(SRCDIR)/FERS_LLtdl.o $(SRCDIR)/FERS_readout.o $(SRCDIR)/FERSlib.o $(SRCDIR)/FERSutils.o $(SRCDIR)/JanusC.o $(SRCDIR)/MultiPlatform.o $(SRCDIR)/outputfiles.o $(SRCDIR)/paramparser.o $(SRCDIR)/plot.o $(SRCDIR)/Statistics.o

$(SRCDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -D$(MYDEF) -c $< -o $@
	
$(SRCDIR)/%.o: $(SRCDIR)/%.cpp
	$(CC) $(CFLAGS) -D$(MYDEF) -c $< -o $@ $(INCLUDE)
	
#OBJECT = $(OBJECT) FERS_LLusb.o
	
.PRECIOUS: $(TARGET) $(OBJECTS)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $(BINDIR)/$@ $(LDADD)
	
clean:
	-rm -f $(SRCDIR)/*.o
	-rm -f $(BINDIR)/$(TARGET)
