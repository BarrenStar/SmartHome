armcc = arm-linux-gcc
cc = gcc
prom = server
source = server_udp.c v4l2.c m0.c uart_device.c voc.c
$(prom): $(source)
	$(cc) -pthread -o $(prom) $(source)
clean:
	rm *~	
	rm *.o
