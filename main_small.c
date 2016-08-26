/*
 ============================================================================
 Project    : Greedy Algorithm for Small Phylogeny Problem
 File       : main.c
 Authors    : Jose Luis Soncco-Alvarez and Mauricio Ayala-Rincon
                Group of Theory of Computation
                Universidade de Brasilia (UnB) - Brazil
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "my_structs.h"
#include "condense.h"
#include "tree.h"
#include "iterate_tree.h"
#include "measure_time.h"
#include "random.h"

#include "dcjdist.h"

void initParameters( ParametersPtr paramsPtr);
void readCommandLine( int argc, char *argv[], ParametersPtr paramsPtr );
int readNumberGenomes( char *filename );
void readNumberGenesAndChromosomes( char *filename, RawDatasetPtr rdatasetPtr );
void readRawData( char *filename, RawDatasetPtr rdatasetPtr );

int main( int argc, char **argv )
{
    struct timeval t_ini, t_fin;
    gettimeofday( &t_ini, NULL );//---------------------------------take start time--
    
    Tree            phyloTree;
    RawDataset      rdataset;
    SetKeys         setkeys;  /* set of keys for condensing genomes */
    int             score;
    Parameters      params;

    initParameters( &params );
    readCommandLine( argc, argv, &params );
    srand( params.seed ); if ( DEBUG ) { printf( "\nseed: %d\n", params.seed ); }

    /* read raw data */
    rdataset.numberGenomes = readNumberGenomes( params.testsetName );
    allocateMemoryForRawData( &rdataset );//--from condense.c
    readNumberGenesAndChromosomes( params.testsetName, &rdataset );
    allocateMemoryForRawGenomes( &rdataset );//--from condense.c
    readRawData( params.testsetName, &rdataset );

    /* condense raw data */
    allocateMemoryForKeys( rdataset.numberGenes, &setkeys );//--from condense.c
    findSetKeysForCondensing( &rdataset, &setkeys );//--from condense.c
    condenseGenomes( &rdataset, &setkeys );//--from condense.c

    /* read genomes from raw data into phylogenetic tree */
    phyloTree.numberLeaves = rdataset.numberGenomes;
    phyloTree.numberGenes = rdataset.numberGenes; 
    allocateMemoryForNodes( &phyloTree, &params );//--from tree.c
    readGenomesFromRawData( &phyloTree, &params, &rdataset );//--from tree.c 

    createTopologyFromNewickFormat( &phyloTree, &params );//from tree.c
    score = labelOptimizeTree( &phyloTree, &params );//--iterate tree.c
    showTreeNewickFormat( phyloTree.startingNodePtr, SHOW_BY_NAME );//from tree.c

    return 0;

    gettimeofday( &t_fin, NULL );//---------------------------------take final time--
    double timediff = timeval_diff( &t_fin, &t_ini );//--from measure_time.h
    
    /* show results */
    if ( SHOW_JUST_SCORE == TRUE ) printf( "%d %.2f\n", score, timediff );
    //else showResults( &phyloTree, params.distanceType, score, timediff );//--from vns.c


    /* free memory */
    freeKeys( rdataset.numberGenes, &setkeys );//--from condense.c
    freeRawDataset( &rdataset );//--from condense.c
    freeTree( &phyloTree, &params );//--from tree.c

	return 0;
}

void initParameters( ParametersPtr paramsPtr )
{
    paramsPtr->seed             = time( NULL);
    paramsPtr->testsetName      = "";
    paramsPtr->newickFile       = "";
    paramsPtr->unichromosomes   = TRUE;
    paramsPtr->circular         = TRUE; //Not used here. Must be updated in readGenome() - tree.c
    paramsPtr->distanceType     = INVERSION_DIST;
    paramsPtr->solver           = CAPRARA_INV_MEDIAN;
    paramsPtr->useOutgroup      = FALSE;
    paramsPtr->outgroup         = "";
    paramsPtr->penaltyType      = -1;
    paramsPtr->initMethod       = R_LEAF_1BEST_EDGE;
    paramsPtr->opt              = BLANCHETTE; // (*)
    //opt = KOVAC (rev dist); //super slow, not good results (worst than BLANCHETTE)
    //opt = GREEDY_CANDIDATES (rev dist); // is slow, "almost" the same results as BLANCHETTE
}

void readCommandLine( int argc, char *argv[], ParametersPtr paramsPtr )
{   
    int i;
    char option;

    /* show parameter options */
    if ( argc == 1 ) {
        fprintf( stdout, "\nParameter Options:\n" );
        fprintf( stdout, "\t-d : evolutionary distance \n");
        fprintf( stdout, "\t\t -d rev : reversal\n\t\t -d dcj : double-cut-join\n");
        fprintf( stdout, "\t-f : dataset filename\n" );
        fprintf( stdout, "\t\t -f filename\n" );
        fprintf( stdout, "\t-k : topology in Newick format\n" );
        fprintf( stdout, "\t\t -k filename\n" );
        fprintf( stdout, "\t-m : use multiple-chromosomes [optional]\n" );
        fprintf( stdout, "\t\t(single-chromosomes is default if option is omitted)\n" );
        fprintf( stdout, "\t-s : seed [optional]\n" );
        fprintf( stdout, "\t\t -s some_seed\n" );
        fprintf( stdout, "\t\t(seed taken from system time by default if option is omitted)\n" );
        fprintf( stdout, "\t-z : penalize dcj [optional]\n" );
        fprintf( stdout, "\t\t -z 0 : penalize multiple chromosomes\n" );
        fprintf( stdout, "\t\t -z 1 : penalize multiple circular chromosomes\n" );
        fprintf( stdout, "\t\t -z 2 : penalize linear chrs., and multiple circular chrs.\n" );    
        fprintf( stdout, "\t\t -z 3 : penalize combinations of linear and circular chrs.\n" );
        fprintf( stdout, "\t\t(penalize is not used by default if option is omitted)\n\n" );
        
        //fprintf( stdout, " using the default testset: testsets/camp05_cond\n" );
        //fprintf( stdout, " try other >> ./main -t testsets/camp07_cond\n" );
        exit( EXIT_FAILURE );
    }

    /* read parameters from command line */
    i = 1;
    while ( i < argc) {
        if ( argv[ i ][ 0 ] == '-' && (i + 1) < argc ) { 
            option = argv[ i ][ 1 ]; 
            switch ( option ) {
                case 'd':
                    if ( strcmp( argv[ i + 1 ], "rev" ) == 0 ) {
                        paramsPtr->distanceType = INVERSION_DIST;
                        paramsPtr->opt = BLANCHETTE;
                    }
                    else if ( strcmp( argv[ i + 1 ], "dcj" ) == 0 ) {
                        paramsPtr->distanceType = DCJ_DIST;
                        paramsPtr->opt = GREEDY_CANDIDATES;
                    }
                    else {
                        fprintf( stderr, " stderr: incorrect distance (-d).\n" );
                        exit( EXIT_FAILURE );
                    }
                    break;
                case 'f':
                    paramsPtr->testsetName = argv[ i + 1 ]; 
                    break;
                case 'k':
                    paramsPtr->newickFile = argv[ i + 1 ]; 
                    break;
                case 'm':
                    paramsPtr->unichromosomes = FALSE;
                    break;
                case 's':
                    paramsPtr->seed = atoi( argv[ i + 1 ] );
                    break;
                case 'z':
                    if ( strcmp( argv[ i + 1 ], "0" ) == 0 ) {
                        paramsPtr->penaltyType = MULTIPLE_CH;
                    }
                    if ( strcmp( argv[ i + 1 ], "1" ) == 0 ) {
                        paramsPtr->penaltyType = MULT_CIRCULAR_CH;
                    }
                    else if ( strcmp( argv[ i + 1 ], "2" ) == 0 ) {
                        paramsPtr->penaltyType = LIN_CH_MULT_CIRC_CH;
                    }
                    else if ( strcmp( argv[ i + 1 ], "3" ) == 0 ) {
                        paramsPtr->penaltyType = COMB_LIN_CIRC_CH;
                    }
                    else {
                        fprintf( stderr, " stderr: incorrect penalty type (-z).\n" );
                        exit( EXIT_FAILURE ); 
                    }
                    break;
                default:
                    fprintf( stderr, " stderr: incorrect option: %c.\n", option );
                    exit( EXIT_FAILURE );
            }
            i = i + 2;  
        }
        else{
            fprintf( stderr, " stderr: incorrect options or parameters.\n" );
            exit( EXIT_FAILURE );
        }
    }

    /* discard some undesired cases */
    if ( paramsPtr->distanceType == INVERSION_DIST && 
            paramsPtr->unichromosomes == FALSE ) {
        fprintf( stderr, " stderr: the program does not support using the reversal" );
        fprintf( stderr, " distance and multiple-chromosome genomes.\n" );
        exit( EXIT_FAILURE );
    }
    if ( paramsPtr->distanceType == INVERSION_DIST && 
            paramsPtr->penaltyType != -1 ) {
        fprintf( stderr, " stderr: the program does not support using the reversal" );
        fprintf( stderr, " distance and penalize multiple chromosomes.\n" );
        exit( EXIT_FAILURE );
    }
}

int readNumberGenomes( char *filename )
{   
    FILE *filePtr;
    int c; /* use int (not char) for the EOF */
    int charCounter = 0;
    int newlineCounter = 0;

    if ( ( filePtr = fopen( filename , "r" ) ) == NULL ) {
        fprintf( stderr, " stderr: %s file could not be opened\n", filename  );
        exit( EXIT_FAILURE );
    }
    else {
        /* count the newline characters */
        while ( ( c = fgetc( filePtr ) ) != EOF ) {
            if ( c == '\n' ) { 
                /* if there exist at least one char before '\n'
                * increment the line counter. */
                if (charCounter > 0) { 
                    newlineCounter++;
                    charCounter=0;
                }
            }
            else { /* c is any char different from '\n' */
                /* dont count white spaces*/
                if ( c != ' ' ) {
                    charCounter++;
                }
            }
        }
        /* if last char before EOF was not '\n' and
        chars different from ' ' were found */
        if (charCounter > 0) { 
            newlineCounter++;
            charCounter = 0;
        }
    }

    fclose( filePtr );

    //printf("newlineCounter: %d\n", newlineCounter); 
    if ( newlineCounter == 0 || newlineCounter % 2 != 0 ) {
        fprintf( stderr, " stderr: data of %s file has incorrect format\n", filename  );
        exit( EXIT_FAILURE );
    }

    if ( newlineCounter / 2 <= 3) {
        fprintf( stderr, " stderr: the number of genomes of %s file must be  greater than 3.\n", filename );
        exit( EXIT_FAILURE );
    }
    return newlineCounter / 2;
}

/* NOTE: before calling this function initialize with zero: numberGenes, 
        and elements of numberChromosomesArray */
void readNumberGenesAndChromosomes( char *filename, RawDatasetPtr rdatasetPtr )  
{
    FILE *filePtr;
    int c, lastc; /* use int (not char) for the EOF */
    int count, i, k;

    if ( ( filePtr = fopen( filename, "r" ) ) == NULL ) {
        fprintf( stderr, 
            " stderr: %s file could not be opened\n", filename );
        exit( EXIT_FAILURE );
    }
    else {              
        /* read first line and discard */
        while ( ( c = fgetc( filePtr ) ) != EOF ) {
            if ( c == '\n' )
                break;
        }
        /* read second line and count number of genes */
        i = 0; /* digit counter*/
        while ( ( c = fgetc( filePtr ) ) != EOF ) {
            if ( c == ' ' || c == '@' || c == '$' || c == '\n' ) {
                /* if the digit counter "i" has at least one digit 
                * increment the genes counter */
                if ( i > 0 ) {
                    rdatasetPtr->numberGenes++; 
                    //( *numberGenes )++;
                }
                i = 0; /* re-start digit counter */

                if ( c == '@' || c == '$' ) {
                    rdatasetPtr->numberChromosomesArray[ 0 ]++;
                }

                if ( c == '\n' )
                    break;
            }
            else { /* c should be a digit */
                i++;
            }
        }

        /* verify if the other genomes have the same number of genes */
        for ( k = 1; k < rdatasetPtr->numberGenomes; k++ ) {
            count = 0;
            /* read name_of_genome line and discard */
            while ( ( c = fgetc( filePtr ) ) != EOF ) {
                if ( c == '\n' )
                    break;
            }
            /* read next line and count number of genes */
            i = 0;
            c = fgetc( filePtr );
            lastc = EOF;

            while ( c != EOF ) {
                if ( c != ' '  && c != '\n' ) {  
                    lastc = c;
                }
                if ( c == ' ' || c == '@' || c == '$' || c == '\n' ) {
                    if ( i > 0 ) {
                        count++;
                    }
                    i = 0;

                    if ( c == '@' || c == '$' ) {
                        rdatasetPtr->numberChromosomesArray[ k ]++;
                    }

                    if ( c == '\n' )
                        break;
                }
                else { /* c should be a number */
                    i++;
                }
                c = fgetc( filePtr );
            }

            if ( lastc != '@' && lastc != '$' ) {
                fprintf( stderr, 
                    " stderr: there is a genome whose last char is not @ or $ in %s\n", filename ); 
                exit( EXIT_FAILURE );
            }

            if ( rdatasetPtr->numberGenes != count ) {
                fprintf( stderr, 
                    " stderr: number of genes are not the same in %s\n", filename ); 
                exit( EXIT_FAILURE );
            } 
        }
    }

    fclose( filePtr );
}

/* Read "Raw" genomes, that is, genomes before condensation */
void readRawData( char *filename, RawDatasetPtr rdatasetPtr )
{
    FILE *filePtr;
    int c; /* use int (not char) for the EOF */
    int i, j, k, h;
    char buffer[ MAX_STRING_LEN ];

    if ( ( filePtr = fopen( filename, "r" ) ) == NULL ) {
        fprintf( stderr, " stderr: %s file could not be opened.\n", filename );
        exit( EXIT_FAILURE );
    }
    else {
        /* read genomes from file into leaves */
        for( i = 0; i < rdatasetPtr->numberGenomes; i++ ) {
            /* verify that char symbol '>' exists */
            if ( ( c = fgetc( filePtr ) ) != EOF ) {
                if ( c != '>' ) {
                    fprintf( stderr, " stderr: incorrect format of organism name in %s.\n", filename );
                    exit( EXIT_FAILURE );
                }
            }
            /* read organism name into buffer */
            k = 0;
            while ( ( c = fgetc( filePtr ) ) != EOF ) {
                if ( c == '\n' ) {
                    buffer[ k ] = '\0';
                    break;
                }
                else {
                    buffer[ k ] = c;
                    k++;

                    if ( k + 1 > MAX_STRING_LEN) {
                        fprintf( stderr, " stderr: increment MAX_STRING_LEN value!\n" );
                        exit( EXIT_FAILURE );
                    }
                }
            }

            /* copy buffer into leaf node*/
            rdatasetPtr->rgenomePtrArray[ i ]->organism = malloc( ( k + 1 ) * sizeof( char ) );
            strcpy( rdatasetPtr->rgenomePtrArray[ i ]->organism, buffer ); 

            /* read genes from next line*/
            k = 0; j = 0; h = 0;
            while ( ( c = fgetc( filePtr ) ) != EOF ) {
                if ( c == ' ' || c == '@' || c == '$' || c == '\n' ){
                    buffer[ k ] = '\0';
                    if ( k > 0 ) {
                        rdatasetPtr->rgenomePtrArray[ i ]->genome[ j ] = atoi( buffer );
                        j++;
                        //printf("%d,", atoi(buffer));    
                    }
                    k = 0;

                    if ( c == '@' || c == '$') {
                        rdatasetPtr->rgenomePtrArray[ i ]->genome[ j ] = SPLIT;
                        rdatasetPtr->rgenomePtrArray[ i ]->chromosomeType[ h ] = 
                                        ( c == '@' ? CIRCULAR_SYM : LINEAR_SYM );
                        j++;
                        h++;
                    }

                    if ( c == '\n' )
                        break;
                }
                else { // c should be a digit
                    buffer[ k ] = c;
                    k++;
                }
            }

        }//end for
    }

    fclose( filePtr );
}

