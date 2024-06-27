all: PideShop HungryVeryMuch 

circularqueue: circularqueue.c
	gcc -o circularqueue circularqueue.c -pthread

PideShop: PideShop.c circularqueue.h
	gcc -g -o PideShop PideShop.c circularqueue.c -pthread

HungryVeryMuch: HungryVeryMuch.c
	gcc -o HungryVeryMuch HungryVeryMuch.c -pthread 

clean:
	rm -f PideShop HungryVeryMuch
