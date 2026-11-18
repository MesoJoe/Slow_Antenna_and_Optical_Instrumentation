
.PHONY: all clean

NAME       := c_vid_w_gps
PYLON_ROOT ?= /home/pi/Desktop/opt/pylon

CXX        := g++
LD         := $(CXX)

CPPFLAGS   := -I$(PYLON_ROOT)/include \
              -I$(PYLON_ROOT)/include/pylon

CXXFLAGS   := -O2 -std=c++17 -pthread

LDFLAGS    := -L$(PYLON_ROOT)/lib \
              -Wl,-rpath,$(PYLON_ROOT)/lib

LDLIBS     := -lpylonbase \
              -lpylonutility \
              -lpylonc \
              -lGenApi_gcc_v3_1_Basler_pylon \
              -lGCBase_gcc_v3_1_Basler_pylon \
              -pthread

all: $(NAME)

$(NAME): $(NAME).o
	$(LD) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(NAME).o: $(NAME).cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -f $(NAME).o $(NAME)

