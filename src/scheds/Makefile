

CC=gcc

CFLAGS          = -Wall -O2 -std=gnu99 -g
LDFLAGS         = -lpthread -lcpuset -lbitmask -lnuma -lm -lrt
SCHEDS          = b_gang.o batch.o gang.o hugh.o simple.o ss.o


%.o: %.c 
	$(CC) $(LDFLAGS) -o $@ $(CFLAGS) $<

schedulers: $(SCHEDS)
	$(CC) $(LDFLAGS) -o $@ $(CFLAGS) $(SCHEDS)
