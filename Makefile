CFLAGS += -Wall -Wextra -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE -O3 -g -fopenmp

OBJS = main.o sha256.o rc4.o md5.o pdfcrack.o pdfparser.o passwords.o common.o pattern.o \
	benchmark.o
OBJS_PDFREADER = pdfparser.o pdfreader.o common.o

all: pdfcrack

pdfcrack: $(OBJS)
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) -o $@ $(OBJS)

pdfreader: $(OBJS_PDFREADER)
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) -o $@ $(OBJS_PDFREADER)

clean:
	rm -f pdfcrack pdfreader testreader savedstate.sav *.o test*.txt

%.o: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) -c -o $@ $+
		
testall: test3 test4

test1: pdfcrack
	./pdfcrack -o -c Jmy831 -n 6 -m 6 ./testpdfs/TestPDF1.pdf
	./pdfcrack -o -t 5 -e [Jmy][Jmy][y][Jmy1234567890][831][123458] ./testpdfs/TestPDF1.pdf

test3: pdfcrack
	./pdfcrack -c Niy546 -n 6 -m 6 ./testpdfs/TestPDF3.pdf
	./pdfcrack -t 5 -e [Niy][Niy][y][Niy1234567890][546][123546] ./testpdfs/TestPDF3.pdf
	
perftest3: pdfcrack
	./pdfcrack -c Niy1234567890 -n 6 -m 6 ./testpdfs/TestPDF3.pdf
	./pdfcrack -t 1 -e [Niy1234567890][Niy1234567890][Niy1234567890][Niy1234567890][Niy1234567890][Niy1234567890] ./testpdfs/TestPDF3.pdf
	
test4: pdfcrack
	./pdfcrack -c Ktw810 -n 6 -m 6 ./testpdfs/TestPDF4.pdf
	./pdfcrack -t 5 -e [Ktw][Ktw][w][Ktw1234567890][810][423810] ./testpdfs/TestPDF4.pdf
	

	
debugold: pdfcrack
	gdb --args ./pdfcrack -c Niy546 -n 6 -m 6 ./testpdfs/TestPDF3.pdf
	
debugnew: pdfcrack
	gdb --args ./pdfcrack -t 1 -e [Niy][Niy][Niy][Niy1234567890][546][123546] ./testpdfs/TestPDF3.pdf
	
