
MPICC       = mpicc

CFLAGS      = -Ofast
INCLUDES    =
LDFLAGS     =
LIBS        =

.c.o:
	$(MPICC) $(CFLAGS) $(INCLUDES) -c $<

isendrecv: isendrecv.o
	$(MPICC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)

alltoallw: alltoallw.o
	$(MPICC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)

clean:
	rm -f core.* *.o isendrecv alltoallw

.PHONY: clean

