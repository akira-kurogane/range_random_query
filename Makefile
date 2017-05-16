#libmongoc_flags=$(pkg-config --cflags --libs libmongoc-1.0)
libmongoc_flags=-I/usr/include/libmongoc-1.0 -I/usr/include/libbson-1.0 -lmongoc-1.0 -lsasl2 -lssl -lcrypto -lrt -lbson-1.0

random_range_query:	random_range_query.c random_range_query_opts.c
	gcc -c -g random_range_query_opts.c
	gcc -c -g random_range_query.c ${libmongoc_flags}
	gcc -g -o random_range_query random_range_query.o random_range_query_opts.o ${libmongoc_flags}

clean:
	rm -f random_range_query *.o
