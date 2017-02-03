// Priyanka Chordia.

#include <stdio.h>
#include "cs1300bmp.h"
#include <iostream>
#include <stdint.h>
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
		delete input;
		delete output;
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

#if defined(__arm__)
static inline unsigned int get_cyclecount (void)
{
	unsigned int value;
	// Read CCNT Register
	asm volatile ("MRC p15, 0, %0, c9, c13, 0\t\n": "=r"(value)); 
	return value;
}

static inline void init_perfcounters (int32_t do_reset, int32_t enable_divider)
{
	// in general enable all counters (including cycle counter)
	int32_t value = 1;

	// peform reset: 
	if (do_reset)
	{
		value |= 2;     // reset all counters to zero.
		value |= 4;     // reset cycle counter to zero.
	}

	if (enable_divider)
		value |= 8;     // enable "by 64" divider for CCNT.

	value |= 16;

	// program the performance-counter control-register:
	asm volatile ("MCR p15, 0, %0, c9, c12, 0\t\n" :: "r"(value)); 

	// enable all counters: 
	asm volatile ("MCR p15, 0, %0, c9, c12, 1\t\n" :: "r"(0x8000000f)); 

	// clear overflows:
	asm volatile ("MCR p15, 0, %0, c9, c12, 3\t\n" :: "r"(0x8000000f));
}



#endif



	double
applyFilter(struct Filter *filter, cs1300bmp *input, cs1300bmp *output)
{
#if defined(__arm__)
	init_perfcounters (1, 1);
#endif

	long long cycStart, cycStop;
	double start,stop;
#if defined(__arm__)
	cycStart = get_cyclecount();
#else
	cycStart = rdtscll();
#endif
	output -> width = input -> width;
	output -> height = input -> height;

	/*int i, j, c, k, width, height, filterDivisor, temp, temp2, temp3;
	i = j = c = k = temp = temp2 = temp3 = filterDivisor = 0;
	int filterVal[9];

	for(i = 0; i < 3; i++)
	{
		for(j = 0; j < 3; j++)
		{
			filterVal[k] = filter->get(i, j);
			k++;
		}
	}

	width =  ((input->width) - 1);
	height = ((input->height) - 1);

	filterDivisor = filter -> getDivisor();

	for(c = 0; c < 3; c++)
	{
		for(i = 1; i < height; i++)
		{
			int r1, r2;

			r1 = i - 1;
			r2 = i + 1;

			for(j = 1; j < width; j++)
			{
				int c1, c2;

				c1 = j - 1;
				c2 = j + 1;

				temp = ((input->color[c][r1][c1]) * (filterVal[0]));
				temp2 = ((input->color[c][r1][j]) * (filterVal[1]));
				temp3 = ((input->color[c][r1][c2]) * (filterVal[2]));
				temp += ((input->color[c][i][c1]) * (filterVal[3]));
				temp2 += ((input->color[c][i][j]) * (filterVal[4]));
				temp3 += ((input->color[c][i][c2]) * (filterVal[5]));
				temp += ((input->color[c][r2][c1]) * (filterVal[6]));
				temp2 += ((input->color[c][r2][j]) * (filterVal[7]));
				temp3 += ((input->color[c][r2][c2]) * (filterVal[8]));

				output->color[c][i][j] = temp + temp2 + temp3;

				if(filterDivisor > 1)
					output->color[c][i][j] /= filterDivisor;

				if( output->color[c][i][j] < 0)
					output->color[c][i][j] = 0;

				else if( output->color[c][i][j] > 255)
					output->color[c][i][j] = 255;
			}
		}
	}
*/

	int w1 = (input -> width) -1;
    int h1 = (input -> height) -1; //instead of doing calculations in the loop, go ahead and calculate the width and height before the loop and just use the variables
    int plane, row, col, sum;
    int div = filter -> getDivisor(); //same thing
    
    int fArray[8]; //Localizing filter -> get(x,x)
    fArray[0] = filter -> get(0,0);
    fArray[1] = filter -> get(0,1);
    fArray[2] = filter -> get(0,2);
    fArray[3] = filter -> get(1,0);
    fArray[4] = filter -> get(1,1);
    fArray[5] = filter -> get(1,2);
    fArray[6] = filter -> get(2,0);
    fArray[7] = filter -> get(2,1);
    fArray[8] = filter -> get(2,2);
    
    for(plane = 0; plane < 3; ++plane) { //switching the order of the loops helps us with locality
        for(row = 1; row < h1 ; ++row) {
            for(col = 1; col < w1; ++col) {
                
                int sum = 0; //took out original code; redeclared it as sum outside the loop to stop in loop calculations
                

                
                sum += (input -> color[plane][row     - 1][col     - 1] * fArray[0]); //filter -> get(x,x)
                sum += (input -> color[plane][row     - 1][col        ] * fArray[1]);
                sum += (input -> color[plane][row     - 1][col     + 1] * fArray[2]);
                sum += (input -> color[plane][row        ][col     - 1] * fArray[3]);
                sum += (input -> color[plane][row        ][col        ] * fArray[4]);
                sum += (input -> color[plane][row        ][col     + 1] * fArray[5]);
                sum += (input -> color[plane][row     + 1][col     - 1] * fArray[6]);
                sum += (input -> color[plane][row     + 1][col        ] * fArray[7]);
                sum += (input -> color[plane][row     + 1][col     + 1] * fArray[8]);
                
                
                output -> color[plane][row][col] = sum;
                
                sum = sum / div;
                
                if ( sum  < 0 ) {
                    sum = 0;
                }
                
                if ( sum  > 255 ) { 
                    sum = 255;
                }
                output -> color[plane][row][col] = sum;
            } 
        }
        
    }
    
#if defined(__arm__)
	cycStop = get_cyclecount();
#else
	cycStop = rdtscll();
#endif

	double diff = cycStop-cycStart;
#if defined(__arm__)
  diff=diff*64;
#endif
	double diffPerPixel = diff / (output -> width * output -> height);
	fprintf(stderr, "Took %f cycles to process, or %f cycles per pixel\n",
			diff, diff / (output -> width * output -> height));
	return diffPerPixel;
}
