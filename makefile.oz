makefile(
    bin: ['testZmq.exe']
    lib: ['z14.so' 'ZeroMQ.ozf']

    rules: o(
	'z14.so': ld('z14.o' [library(zmq)])
    )
)

