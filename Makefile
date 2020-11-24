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
		
testall: test perftest

test: test2 test5 test1 test3 test4 test6

perftest: perftest2 perftest5 perftest1 perftest3 perftest4 perftest6

test1: pdfcrack
	@echo ""
	@echo "---------------------------------------------"
	@echo "Testing Rev3 owner password"
	@echo "---------------------------------------------"
	@echo ""
	@echo "Generative method"
	@echo "-----------------"
	./pdfcrack -o -c Jmy831 -n 6 -m 6 ./testpdfs/TestPDF1.pdf
	@echo ""
	@echo "Pattern method"
	@echo "--------------"
	./pdfcrack -o -t 5 -e [Jmy][Jmy][y][Jmy1234567890][831][123458] ./testpdfs/TestPDF1.pdf


perftest1: pdfcrack
	@echo ""
	@echo "============================================="
	@echo "Performance testing Rev3 owner password"
	@echo "============================================="
	@echo ""
	@echo "Generative method"
	@echo "================="
	./pdfcrack -o -c Jmy1234567890 -n 6 -m 6 ./testpdfs/TestPDF1.pdf
	@echo ""
	@echo "Pattern method"
	@echo "=============="
	./pdfcrack -o -t 5 -e [Jmy1234567890][Jmy1234567890][Jmy1234567890][Jmy1234567890][Jmy1234567890][Jmy1234567890] ./testpdfs/TestPDF1.pdf



test2: pdfcrack
	@echo ""
	@echo "---------------------------------------------"
	@echo "Testing Rev2 user password"
	@echo "---------------------------------------------"
	@echo ""
	@echo "Generative method"
	@echo "-----------------"
	./pdfcrack -c Jhe238 -n 6 -m 6 ./testpdfs/TestPDF2.pdf
	@echo ""
	@echo "Pattern method"
	@echo "--------------"
	./pdfcrack -t 5 -e [Jhe][Jhe][e][Jhe1234567890][123][123458] ./testpdfs/TestPDF2.pdf


perftest2: pdfcrack
	@echo ""
	@echo "================================================"
	@echo "Performance testing Rev2 user password"
	@echo "================================================"
	@echo ""
	@echo "Generative method"
	@echo "================="
	./pdfcrack -c Jhe1234567890zyxwvutsrq -n 6 -m 6 ./testpdfs/TestPDF2.pdf
	@echo ""
	@echo "Pattern method"
	@echo "=============="
	./pdfcrack -t 5 -e [Jhe1234567890zyxwvutsrq][Jhe1234567890zyxwvutsrq][Jhe1234567890zyxwvutsrq][Jhe1234567890zyxwvutsrq][Jhe1234567890zyxwvutsrq][Jhe1234567890zyxwvutsrq] ./testpdfs/TestPDF2.pdf



test3: pdfcrack
	@echo ""
	@echo "---------------------------------------------"
	@echo "Testing Rev3 (AES) user password"
	@echo "---------------------------------------------"
	@echo ""
	@echo "Generative method"
	@echo "-----------------"
	./pdfcrack -c Niy546 -n 6 -m 6 ./testpdfs/TestPDF3.pdf
	@echo ""
	@echo "Pattern method"
	@echo "--------------"
	./pdfcrack -t 5 -e [Niy][Niy][y][Niy1234567890][546][123546] ./testpdfs/TestPDF3.pdf

	
perftest3: pdfcrack
	@echo ""
	@echo "================================================"
	@echo "Performance testing Rev3 (AES) user password"
	@echo "================================================"
	@echo ""
	@echo "Generative method"
	@echo "================="
	./pdfcrack -c Niy1234567890 -n 6 -m 6 ./testpdfs/TestPDF3.pdf
	@echo ""
	@echo "Pattern method"
	@echo "=============="
	./pdfcrack -t 5 -e [Niy1234567890][Niy1234567890][Niy1234567890][Niy1234567890][Niy1234567890][Niy1234567890] ./testpdfs/TestPDF3.pdf


	
test4: pdfcrack
	@echo ""
	@echo "---------------------------------------------"
	@echo "Testing Rev3 (RC4) user password"
	@echo "---------------------------------------------"
	@echo ""
	@echo "Generative method"
	@echo "-----------------"
	./pdfcrack -c Ktw810 -n 6 -m 6 ./testpdfs/TestPDF4.pdf
	@echo ""
	@echo "Pattern method"
	@echo "--------------"
	./pdfcrack -t 5 -e [Ktw][Ktw][w][Ktw1234567890][810][423810] ./testpdfs/TestPDF4.pdf


perftest4: pdfcrack
	@echo ""
	@echo "================================================"
	@echo "Performance testing Rev3 (RC4) user password"
	@echo "================================================"
	@echo ""
	@echo "Generative method"
	@echo "================="
	./pdfcrack -c Ktw1234567890 -n 6 -m 6 ./testpdfs/TestPDF4.pdf
	@echo ""
	@echo "Pattern method"
	@echo "=============="
	./pdfcrack -t 5 -e [Ktw1234567890][Ktw1234567890][Ktw1234567890][Ktw1234567890][Ktw1234567890][Ktw1234567890] ./testpdfs/TestPDF4.pdf



test5: pdfcrack
	@echo ""
	@echo "---------------------------------------------"
	@echo "Testing Rev2 owner password"
	@echo "---------------------------------------------"
	@echo ""
	@echo "Generative method"
	@echo "-----------------"
	./pdfcrack -o -c Rkz573 -n 6 -m 6 ./testpdfs/TestPDF5.pdf
	@echo ""
	@echo "Pattern method"
	@echo "--------------"
	./pdfcrack -o -t 5 -e [Rkz][Rkz][z][Rkz1234567890][127][123456] ./testpdfs/TestPDF5.pdf


perftest5: pdfcrack
	@echo ""
	@echo "================================================"
	@echo "Performance testing Rev2 owner password"
	@echo "================================================"
	@echo ""
	@echo "Generative method"
	@echo "================="
	./pdfcrack -o -c Rkz1234567890abcdefijl -n 6 -m 6 ./testpdfs/TestPDF5.pdf
	@echo ""
	@echo "Pattern method"
	@echo "=============="
	./pdfcrack -o -t 5 -e [Rkz1234567890abcdefijl][Rkz1234567890abcdefijl][Rkz1234567890abcdefijl][Rkz1234567890abcdefijl][Rkz1234567890abcdefijl][Rkz1234567890abcdefijl] ./testpdfs/TestPDF5.pdf

test6: pdfcrack
	@echo ""
	@echo "---------------------------------------------"
	@echo "Testing Rev5 (AES 256) user password"
	@echo "---------------------------------------------"
	@echo ""
	@echo "Generative method"
	@echo "-----------------"
	./pdfcrack -c Mnv731 -n 6 -m 6 ./testpdfs/TestPDF6.pdf
	@echo ""
	@echo "Pattern method"
	@echo "--------------"
	./pdfcrack -t 5 -e [Mnv][Mnv][v][Mnv1234567890][731][731245] ./testpdfs/TestPDF6.pdf	

perftest6: pdfcrack
	@echo ""
	@echo "================================================"
	@echo "Performance testing Rev5 (AES 256) user password"
	@echo "================================================"
	@echo ""
	@echo "Generative method"
	@echo "================="
	./pdfcrack -c mnopqrstuvwxyz1234567890M -n 6 -m 6 ./testpdfs/TestPDF6.pdf
	@echo ""
	@echo "Pattern method"
	@echo "=============="
	./pdfcrack -t 5 -e [mnopqrstuvwxyz1234567890M][mnopqrstuvwxyz1234567890M][mnopqrstuvwxyz1234567890M][mnopqrstuvwxyz1234567890M][mnopqrstuvwxyz1234567890M][mnopqrstuvwxyz1234567890M] ./testpdfs/TestPDF6.pdf	

	
debugold: pdfcrack
	gdb --args ./pdfcrack -c Mnv731 -n 6 -m 6 ./testpdfs/TestPDF6.pdf
	
debugnew: pdfcrack
	gdb --args ./pdfcrack -t 1 -e [Mnv][Mnv][v][Mnv1234567890][731][731245] ./testpdfs/TestPDF6.pdf

