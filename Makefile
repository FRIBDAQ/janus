NSCLDAQ=$(DAQROOT)

TARGET = JanusC
CC = g++
CFLAGS = -Wall -Wno-write-strings -std=c++11 -Dlinux -I$(NSCLDAQ)/include
LDFLAGS = -pthread -lrt -L$(NSCLDAQ)/lib -Wl,-rpath=$(NSCLDAQ)/lib -ldataformat -lDataFlow -lException
INCLUDE = -I/usr/include/libusb-1.0
LDADD = -lusb-1.0
SRCDIR = src
BINDIR = bin
MYDEF = FERS_5202

TARGETMACRO = BinToCsv
MACRODIR = macros

.PHONY: default conversion all clean

default: $(TARGET)
conversion: $(TARGETMACRO)
all: default conversion

OBJECTS = $(SRCDIR)/configure.o $(SRCDIR)/console.o $(SRCDIR)/FERS_LLeth.o $(SRCDIR)/FERS_LLusb.o $(SRCDIR)/FERS_LLtdl.o $(SRCDIR)/FERS_readout.o $(SRCDIR)/FERSlib.o $(SRCDIR)/FERSutils.o $(SRCDIR)/JanusC.o $(SRCDIR)/MultiPlatform.o $(SRCDIR)/outputfiles.o $(SRCDIR)/paramparser.o $(SRCDIR)/plot.o $(SRCDIR)/Statistics.o $(SRCDIR)/RingBufferHandler.o $(SRCDIR)/RBHWrapper.o

$(SRCDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -D$(MYDEF) -c $< -o $@
	
$(SRCDIR)/%.o: $(SRCDIR)/%.cpp
	$(CC) $(CFLAGS) -D$(MYDEF) -c $< -o $@ $(INCLUDE)

$(SRCDIR)/RingBufferHandler.o: $(SRCDIR)/RingBufferHandler.cpp
	$(CC) $(CFLAGS) -std=c++11 -c $< -o $@ $(INCLUDE) $(LDFLAGS)


#OBJECT = $(OBJECT) FERS_LLusb.o
	
.PRECIOUS: $(TARGET) $(OBJECTS)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $(BINDIR)/$@ $(LDADD)
	
# BintoCsv make
OBJMACRO = $(MACRODIR)/BinToCsv.o $(MACRODIR)/BinaryDataFERS.o $(MACRODIR)/BinaryData_5202.o $(MACRODIR)/BinaryData_5203.o

$(MACRODIR)/%.o: $(MACRODIR)/%.cpp
	$(CC) $(CFLAGS) -c $< -o $@

.PRECIOUS: $(TARGETMACRO) $(OBJMACRO)

$(TARGETMACRO):  $(OBJMACRO)
	$(CC) $(OBJMACRO) -o $(BINDIR)/$@

# $(TARGETMACRO):
# 	$(CC) -o $(BINDIR)/$@ $(CFLAGS) $(MACRODIR)/$(TARGETMACRO).cpp

cleanjanus:
	-rm -f $(SRCDIR)/*.o
	-rm -f $(BINDIR)/$(TARGET)

cleanbintocsv:
	-rm -f $(BINDIR)/$(TARGETMACRO)

clean: cleanjanus cleanbintocsv
