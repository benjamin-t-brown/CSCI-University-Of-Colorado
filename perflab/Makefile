##
##

CXX	=g++
CXXFLAGS= -m32 -static -O3

goals: run
	echo "Done"

filter: FilterMain.cpp Filter.cpp cs1300bmp.cc
	$(CXX) $(CXXFLAGS) -o filter FilterMain.cpp Filter.cpp cs1300bmp.cc
##
## Parameters for the test run
##
FILTERS = gauss.filter vline.filter hline.filter emboss.filter
IMAGES = boats.bmp blocks-small.bmp
TRIALS = 1 2 3 4

run: filter blocks-small.bmp
	for image in $(IMAGES); do for filter in $(FILTERS); do \
	  echo "-------- Do $$image with $$filter ------- " ; \
	     ./filter $$filter $$image $$image $$image $$image; \
          done; done

clean:
	-rm *.o
	-rm filter
	-rm filtered-*.bmp
