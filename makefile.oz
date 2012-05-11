makefile(
    lib: ['z14.so' 'ZeroMQ.ozf']
    rules: o(
	'z14.so': ld('z14.o' [library(zmq)])
    )
    depends: o(
	'z14.o': ['z14.cc' 'ozcommon.hh' 'm14/bytedata.hh' 'm14/am.hh']
    )
)

