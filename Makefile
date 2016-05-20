all : test

test : ext
	py.test .

ext :
	pip install -e .

clean :
	rm -rf *.o *.so tktest.c
