all:
	doxygen Doxyfile
	sphinx-build -b html . build/html
	cp -r build/html/* ../public
	rm -rf build

clean:
	rm -rf build latex html
