# makefile created automaticaly by LFMakeMaker
# LFMakeMaker 1.2 (May 30 2015 17:53:45) (c)LFSoft 1997

gotoall: all


#The compiler (may be customized for compiler's options).
cc=gcc -Wall -D_GNU_SOURCE -std=c99 -g -DFREEBOX -DUPS -DMETEO -DONEWIRE -lcurl -lpthread -lmosquitto -ldl `pkg-config --libs libssl` -DLUA  `pkg-config --cflags libconfig` `pkg-config --libs libconfig` `pkg-config --cflags json-c` `pkg-config --libs json-c`

OBJS=Marcel.o MQTT_tools.o Alerting.o DList.o

Alerting.o : Alerting.c Marcel.h Alerting.h DList.h Makefile 
	$(cc) -c -o Alerting.o Alerting.c 
Script.o : Script.c Marcel.h Script.h DList.h Makefile 
	$(cc) -c -o Script.o Script.c 

DeadPublisherDetection.o : DeadPublisherDetection.c \
  DeadPublisherDetection.h Marcel.h MQTT_tools.h Makefile 
	$(cc) -c -o DeadPublisherDetection.o DeadPublisherDetection.c `pkg-config --cflags lua5.2`

DList.o : DList.c DList.h Makefile 
	$(cc) -c -o DList.o DList.c 

Every.o : Every.c Every.h Marcel.h Makefile 
	$(cc) -c -o Every.o Every.c `pkg-config --cflags lua5.2`
	
OneWire.o : OneWire.c OneWire.h Marcel.h Makefile 
	$(cc) -c -o OneWire.o OneWire.c 

Freebox.o : Freebox.c Freebox.h Marcel.h MQTT_tools.h Makefile 
	$(cc) -c -o Freebox.o Freebox.c `pkg-config --cflags lua5.2`


Lua.o : Lua.c Marcel.h Alerting.h DList.h MQTT_tools.h Makefile 
	$(cc) -c -o Lua.o Lua.c `pkg-config --cflags lua5.2` 

Marcel.o : Marcel.c Marcel.h Freebox.h UPS.h DeadPublisherDetection.h \
  MQTT_tools.h Alerting.h DList.h Every.h Meteo.h Makefile 
	$(cc) -c -o Marcel.o Marcel.c 

Meteo.o : Meteo.c Meteo.h Marcel.h MQTT_tools.h Makefile 
	$(cc) -c -o Meteo.o Meteo.c 
	
Broker.o : Broker.c MQTT_tools.h Makefile 
	$(cc) -c -o Broker.o Broker.c 

MQTT_tools.o : MQTT_tools.c MQTT_tools.h Makefile 
	$(cc) -c -o MQTT_tools.o MQTT_tools.c 
	

UPS.o : UPS.c UPS.h Marcel.h MQTT_tools.h Makefile 
	$(cc) -c -o UPS.o UPS.c
	
../plugins/Broker.so : Broker.o MQTT_tools.o Makefile 
	$(cc) -shared -ldl -o $@ Broker.o MQTT_tools.o 
../plugins/FreeboxOS.so : Freebox.o Makefile 
	$(cc) -shared -ldl -o $@ Freebox.o 
../plugins/Ups.so : UPS.o  Makefile 
	$(cc) -shared -ldl -o $@ UPS.o -lupsclient
../plugins/OneWire.so : OneWire.o  Makefile 
	$(cc) -shared -ldl -o $@ OneWire.o -lownet
../plugins/Scripts.so : Script.o Every.o DeadPublisherDetection.o Alerting.o Lua.o DList.o Makefile 
	$(cc) -shared -ldl -o $@ Script.o Every.o DeadPublisherDetection.o Alerting.o DList.o Lua.o  `pkg-config --libs lua5.2` 
../plugins/Meteo.so : Meteo.o  Makefile 
	$(cc) -shared -ldl -o $@ Meteo.o 
	
../Marcel : $(OBJS) Makefile 
	 $(cc) -rdynamic -Wl,-rpath -Wl,. -o ../Marcel $(OBJS)

all: ../Marcel \
	../plugins/FreeboxOS.so \
	../plugins/Ups.so \
	../plugins/FreeboxOS.so \
	../plugins/Scripts.so \
	../plugins/OneWire.so \
	../plugins/Meteo.so \


clean: 
	rm *.o ../Marcel ../plugins/*.so