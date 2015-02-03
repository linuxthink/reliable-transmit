GCC=g++
COPTS=-g3 -fpermissive 

all: mp3_sender mp3_receiver

.PHONY: clean
FILES=*.o mp3_receiver mp3_sender

mp3common.o: mp3common.h mp3common.c
	${GCC} ${COPTS} -L. -c -o mp3common.o mp3common.c

receiver.o: receiver.c
	${GCC} ${COPTS} -L. -c -o receiver.o receiver.c

sender.o: sender.c
	${GCC} ${COPTS} -L. -c -o sender.o sender.c

mp3_sender: mp3_sender.c sender.o mp3common.o
	${GCC} ${COPTS} -L. -lmp3channel -lpthread -lm -o mp3_sender sender.o mp3common.o mp3_sender.c

mp3_receiver: mp3_receiver.c receiver.o	mp3common.o
	${GCC} ${COPTS} -L. -lmp3channel -lpthread -lm -o mp3_receiver receiver.o mp3common.o mp3_receiver.c

clean:
	@-rm ${FILES}
