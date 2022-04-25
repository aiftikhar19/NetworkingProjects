/*Filename: count.c
  Created by: Aisha Iftikhar
  Creation date: 1/7/20
  Synopsis: write a program in C called count to read a binary file and print the following statistics 
            on the screen as well to an output file:
		the size of the file in bytes
		number of times the search string specified in the second argument appeared in the file
	    run the program using count <input-filename> <search-string> <output-filename>
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*Main*/
int main(int argc, char *argv[]){
	/*variable declarations*/
	FILE *input, *output;		/*input and output files*/
	char *inFileName, *outFileName, *search;	/*input and output file names, and search string*/
	int size, matchCount;		/*vars to store file size and count number of matches*/
	const int MAXBUFFER = 100;	/*can only read file in chunks of 100 bytes or smaller*/
	char arr[MAXBUFFER];		/*array to store input from file*/

	/*Error check for correct input*/
	if (argc != 4){
		printf("ERROR: Incorrect number of arguments.\n");
		printf("Use the format: count <inputfile> <searchstring> <outputfile> \n");
		exit(1);
	}

	/*Assign input to variables*/
	inFileName = argv[1];
	search = argv[2];
	outFileName = argv[3];

	/*Error checking; opening input and output files*/
	/*open input file*/
	if ((input = fopen(inFileName, "rb")) == NULL) {
		printf ("ERROR: Cannot open the input file %s\n", inFileName);
		exit(1);
	}
	/*open output file*/
	if ((output = fopen(outFileName, "wb")) == NULL) {
		printf ("ERROR: Cannot open the output file %s\n", outFileName);
		exit(1);
	}

	/*Calculate file size*/
	fseek(input, 0, SEEK_END);
	size = ftell(input);
	fseek(input, 0, SEEK_SET);
	
	/*print statement size of file to console*/
	printf("Size of file: %zu\n", size);
	/*print to output file*/
	fprintf(output, "\nSize of file: %zu\n", size);

	/*count number of matches*/
	while (fgets (arr, MAXBUFFER, input) != NULL) {
		char *match = arr;
		while ((match = (strstr(match, search))) != NULL) {
			matchCount++;
			++match;
		}
	}

	/*print statement number of matches*/
	printf("Number of matches: %d\n", matchCount);
	/*print to output file*/
	fprintf(output, "Number of matches: %d\n", matchCount);

	/*Close files*/
	fclose(input);
	fclose(output);

return 0;
}
