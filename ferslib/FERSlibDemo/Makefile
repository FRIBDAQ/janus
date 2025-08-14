TARGET = CAEN_FERSlibDemo
CXX = g++
CXXFLAGS = -Wall -Wno-write-strings -Dlinux
INCLUDE = /usr/local/lib
LDFERS = -lcaenferslib
INCDIR = -Iinclude/
SRCDIR = src
BINDIR = bin


.PHONY: default all clean

default: $(TARGET)
all: default  

##############################
# Demo make
##############################	

OBJDEMO = $(DEMODIR)/FERSlibDemo.o

#.PRECIOUS: $(TARGET) $(OBJDEMO)

$(TARGET): $(SRCDIR)/FERSlibDemo.cpp
	$(CXX) $(CXXFLAGS) -o $(BINDIR)/$@ $< -L$(INCLUDE) $(LDFERS)
	

clean:
	-rm -f $(SRCDIR)/*.o
	-rm -f $(BINDIR)/$(TARGET)	
	

