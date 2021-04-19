CFLAGS=-Wall -pedantic -std=c11 -I.

OFILES=sr.o hw.o

# all:  sr s_hw fakeClient
all:  s_hw 

%.o:	%.c
			gcc $(CFLAGS) -c $<

sr:		$(OFILES)
			gcc $(OFILES) -o sr

s_hw:	hw.o s_hw.o
			gcc $^ -o s_hw

fakeClient:
		gcc fakeClient.c -o fakeClient

run:
			./sr

clean:
			rm -f *~ *.o fakeClient s_hw
