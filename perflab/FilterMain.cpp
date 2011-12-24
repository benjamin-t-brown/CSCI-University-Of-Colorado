#include <stdio.h>
#include "cs1300bmp.h"
#include <iostream>
#include <fstream>
#include "Filter.h"

using namespace std;

#include "rtdsc.h"

//
// Forward declare the functions
//
Filter * readFilter(string filename);
double applyFilter(Filter *filter, cs1300bmp *input, cs1300bmp *output);

int
main(int argc, char **argv)
{

  if ( argc < 2) {
    fprintf(stderr,"Usage: %s filter inputfile1 inputfile2 .... \n", argv[0]);
  }

  //
  // Convert to C++ strings to simplify manipulation
  //
  string filtername = argv[1];

  //
  // remove any ".filter" in the filtername
  //
  string filterOutputName = filtername;
  string::size_type loc = filterOutputName.find(".filter");
  if (loc != string::npos) {
    //
    // Remove the ".filter" name, which should occur on all the provided filters
    //
    filterOutputName = filtername.substr(0, loc);
  }

  Filter *filter = readFilter(filtername);

  double sum = 0.0;
  int samples = 0;

  for (int inNum = 2; inNum < argc; inNum++) {
    string inputFilename = argv[inNum];
    string outputFilename = "filtered-" + filterOutputName + "-" + inputFilename;
    struct cs1300bmp *input = new struct cs1300bmp;
    struct cs1300bmp *output = new struct cs1300bmp;
    int ok = cs1300bmp_readfile( (char *) inputFilename.c_str(), input);

    if ( ok ) {
      double sample = applyFilter(filter, input, output);
      sum += sample;
      samples++;
      cs1300bmp_writefile((char *) outputFilename.c_str(), output);
    }
  }
  fprintf(stdout, "Average cycles per sample is %f\n", sum / samples);

}

struct Filter *
readFilter(string filename)
{
  ifstream input(filename.c_str());

  if ( ! input.bad() ) {
    int size = 0;
    input >> size;
    Filter *filter = new Filter(size);
    int div;
    input >> div;
    filter -> setDivisor(div);
    for (int i=0; i < size; i++) {
      for (int j=0; j < size; j++) {
	int value;
	input >> value;
	filter -> set(i,j,value);
      }
    }
    return filter;
  }
}


double
applyFilter(struct Filter *filter, cs1300bmp *input, cs1300bmp *output)
{

  long long cycStart, cycStop;

  cycStart = rdtscll();

  output -> width = input -> width;
  output -> height = input -> height;

  int img_width_minus_one = input -> width - 1;
  int img_height_minus_one = input -> height - 1;
  int filter_size = filter -> getSize();
  int filt[filter_size][filter_size];
  int div = filter -> getDivisor();
  int i, j, row, col, plane, tempfilt;
  int value, temprow, tempcol;
  int val1, val2, val3, val4, val5, val6;

  for (i = 0; i < filter_size; ++i) {
    for (j = 0; j < filter_size; ++j) {
      filt[i][j] = filter ->get(i,j);
    }
  }
  printf("THE IMAGE WIDTH MINUS ONE%d", img_width_minus_one);

  for(row = 1; row < img_height_minus_one-1; ++row) {
    for(col = 1; col < img_width_minus_one-3; col+=2) {

      val1 = 0; val2 = 0; val3 = 0; val4 = 0; val5 = 0; val6 = 0;
      for (i = 0; i < filter_size; i++) {
	  temprow = row+i-1;
	  for (j = 0; j < filter_size; j++) {

	    tempcol = col+j-1;
	    tempfilt = filt[i][j];
	    val1 += input -> color[0][temprow][tempcol]
	    * tempfilt;
	    val2 += input -> color[1][temprow][tempcol]
	    * tempfilt;
	    val3 += input -> color[2][temprow][tempcol]
	    * tempfilt;
	    val4 += input -> color[0][temprow][tempcol+1]
	      * tempfilt;
	    val5 += input -> color[1][temprow][tempcol+1]
	      * tempfilt;
	    val6 += input -> color[2][temprow][tempcol+1]
	      * tempfilt;
	  }
	}
      if( div == 1 ){}
      else{
	val1 = val1/div;
	val2 = val2/div;
	val3 = val3/div;
	val4 = val4/div;
	val5 = val5/div;
	val6 = val6/div;
      }


	if( val1 < 256 && val1 > -1) {}
	else if ( val1  > 255 ) { val1 = 255; }
	else { val1 = 0; }
	if( val2 < 256 && val2 > -1) {}
	else if ( val2  > 255 ) { val2 = 255; }
	else{ val2 = 0; }
	if( val3 < 256 && val3 > -1) {}
	else if ( val3  > 255 ) { val3 = 255; }
	else{ val3 = 0; }
	if( val4 < 256 && val5 > -1) {}
	else if ( val4  > 255 ) { val4 = 255; }
	else { val4 = 0; }
	if( val5 < 256 && val5 > -1) {}
	else if ( val5  > 255 ) { val5 = 255; }
	else{ val5 = 0; }
	if( val6 < 256 && val6 > -1) {}
	else if ( val6  > 255 ) { val6 = 255; }
	else{ val6 = 0; }
	output -> color[0][row][col] = val1;
	output -> color[1][row][col] = val2;
	output -> color[2][row][col] = val3;
	output -> color[0][row][col+1] = val4;
	output -> color[1][row][col+1] = val5;
	output -> color[2][row][col+1] = val6;
    }
  }
  row = 1022; col = 1022;
      val1 = 0; val2 = 0; val3 = 0;
      for (i = 0; i < filter_size; i++) {
	  temprow = 1022+i-1;
	  for (j = 0; j < filter_size; j++) {

	    tempcol = 1022+j-1;
	    tempfilt = filt[i][j];
	    val1 += input -> color[0][temprow][tempcol]
	    * tempfilt;
	    val2 += input -> color[1][temprow][tempcol]
	    * tempfilt;
	    val3 += input -> color[2][temprow][tempcol]
	      * tempfilt;
	  }
	}
      if( div == 1 ){}
      else{
	val1 = val1/div;
	val2 = val2/div;
	val3 = val3/div;
      }


	if( val1 < 256 && val1 > -1) {}
	else if ( val1  > 255 ) { val1 = 255; }
	else { val1 = 0; }
	if( val2 < 256 && val2 > -1) {}
	else if ( val2  > 255 ) { val2 = 255; }
	else{ val2 = 0; }
	if( val3 < 256 && val3 > -1) {}
	else if ( val3  > 255 ) { val3 = 255; }
	else{ val3 = 0; }
	output -> color[0][row][col] = val1;
	output -> color[1][row][col] = val2;
	output -> color[2][row][col] = val3;



  cycStop = rdtscll();
  double diff = cycStop - cycStart;
  double diffPerPixel = diff / (output -> width * output -> height);
  fprintf(stderr, "Took %f cycles to process, or %f cycles per pixel\n",
	  diff, diff / (output -> width * output -> height));
  return diffPerPixel;
}
