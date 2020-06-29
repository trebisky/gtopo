OBJS = gpx.o utils.o reader.o

TARGET = gpx

all:	$(TARGET)

clean:
	rm -f $(TARGET) $(OBJS)

.c.o:	
	cc -c -g $<

gpx:	$(OBJS)
	cc -o gpx $(OBJS) -lm

# THE END
