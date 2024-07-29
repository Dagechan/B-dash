CC = cc
LDLIBS = -lncursesw

SOURCES = bdash.c

EXECUTABLE = bdash

$(EXECUTABLE): $(SOURCES)
	$(CC) $(SOURCES) $(LDLIBS)  -o $(EXECUTABLE)

clean:
	rm -f $(EXECUTABLE)
