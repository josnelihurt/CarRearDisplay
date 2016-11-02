CC=gcc
LIBS=-lfftw3 -lasound


%.o: %.c
	$(CC) -c -o $@ $<


whistle_control: whistle_control.o
	$(CC) -o whistle_control whistle_control.o $(LIBS)


clean:
	rm -f *.o *~ whistle_control
