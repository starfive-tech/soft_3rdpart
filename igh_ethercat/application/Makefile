INCLUDE += -I ../ethercat/include
CFLAGS  += -g -Wall -O2 $(DEFINES) $(INCLUDE)
LIBS    +=
LDFLAGS := -L ../lib/.libs -static 
CXXFLAGS:= $(CFLAGS)
SOURCE  := $(wildcard main_profile_velocity_2.c)
OBJS    := $(patsubst %.c,%.o,$(patsubst %.cpp,%.o,$(SOURCE)))
TARGET  := ectest_PV

.PHONY : everything objs clean distclean rebuild

all : $(TARGET)

objs : $(OBJS)

rebuild: distclean all

clean :
	rm -rf *~
	rm -rf *.o

distclean : clean
	rm -rf $(TARGET)

$(TARGET) : $(OBJS)
	$(CC) $(CXXFLAGS) -o $@ $(OBJS) $(LDFLAGS) $(LIBS) ../ethercat/lib/.libs/libethercat.a

