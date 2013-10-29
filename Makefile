CFLAGS = -Wall -O3

BIN = avinfo
SRC = avinfo.c
DEPS = avi.h audio.h video.h

CC = gcc
STRIP = strip

WINCC = i386-mingw32-gcc
WINSTRIP = i386-mingw32-strip
UPX = upx

default:	$(BIN) $(BIN).exe

$(BIN):	$(SRC) $(DEPS)
	$(CC) $(CFLAGS) $(SRC) -o $(BIN)
	$(STRIP) $(BIN)

avinfo.exe:	$(DEPS)
	$(WINCC) -mwindows $(CFLAGS) $(SRC) -o $(BIN).exe -lcomdlg32
	$(WINSTRIP) $(BIN).exe
	$(UPX) $(BIN).exe

.PHONY:	clean

clean:
	rm -f $(BIN) $(BIN).exe

