/**
 * 
 * \brief Computing Eigen values with QR decomposition using MPI
 * Uses Housholder's QR decomposition      
 *
 * \author Ondrej Zjevik, (c) 2012
 *
 *
 *   
 * call example: mpiexec -n 1 ./program -file file_name [-silent]
*/

#include "mpi.h"
#include <cblas.h>  //BLAS
#include <iostream>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h> 
#include <time.h>
#include <math.h>
#include <sys/time.h>
#include "cTimer.h"      

void computeQR(int argc, char* argv[], bool verbose, bool definition, float **matrixR, float **matrixQ, int rank, int lines);
                                                                    
#define MAX_PROCESSES 200

//creating continuous 2d array
float** create_matrix( int numrows, int numcols, bool verbose){
	float *buffer=new float[numrows*numcols];
	float **data=new float*[numrows];
	for(int i=0;i<numrows;++i) data[i]=buffer+i*numcols;
	
	return *&data;
}
int main(int argc, char* argv[])
{ 
  bool verbose = true, definition = true, solution = false; 
  float **matrixA,**matrixR,**matrixQ;
  double time;
  int rank=0,i,j,k,l,m,lines;
  FILE *file;
  arg::cTimer timer;    
  
  MPI_Init( &argc, &argv );  
  MPI_Comm_rank( MPI_COMM_WORLD, &rank );  
  if(rank != 0) verbose = false;
  if(rank == 0){
    //using extra arguments from command line
    for(i = 0; i < argc; i++){
      if(strcmp(argv[i],"-silent") == 0) verbose = false;
      if(strcmp(argv[i],"-solution") == 0) solution = true;
      if(strcmp(argv[i],"-file") == 0){
        if((file=fopen(argv[++i], "r")) == NULL) {
          printf("Cannot open file.\n");
          MPI_Finalize();
          return 0;
        }
        else{
          definition = false;
        }
      }
    }
    
    //the input isn't complete
    if(definition){
      printf("Not enought input parameters.\nUse call like: mpiexec -n 1 ./program -file file_name [-silent]\n");
      MPI_Finalize();
      return 0;
    }
    
    //scanning file for number of lines
    fscanf(file,"%d,\n",&lines);
  }  
  MPI_Bcast((void *)&lines, 1, MPI_INT, 0, MPI_COMM_WORLD);
  matrixQ = create_matrix(lines,lines, verbose);
  matrixR = create_matrix(lines,lines, verbose);
  matrixA = create_matrix(lines,lines, verbose);  
  if(rank == 0)
  {
    //loading of matrixR from file to array
    for(i = 0; i < lines; i++){
      for(j = 0; j < lines; j++){
        fscanf(file,"%f,",&matrixR[i][j]);
      }
    }
    for(i = 0; i < lines; i++){
      for(j = 0; j < lines; j++){
        matrixA[i][j] = matrixR[i][j];
      }
    }
    if(verbose){
      printf("matrix was sucesfuly loaded to memory.\nMartix:\n");
      for(i = 0; i < lines; i++){
        for(j = 0; j < lines; j++){
          if(verbose) printf("%f ",matrixR[i][j]);
        }
        if(verbose) printf("\n");
      }
    }
  }
  int a = 20;
  int b = (int)ceil(sqrt( lines ));
  //start time  
  if(rank == 0) timer.CpuStart();   
  for(i = 0; i < std::min(a,b); i++)
  {
    for(j = 0; j < lines; j++){
      for(k = 0; k < lines; k++){
        matrixR[j][k] = matrixA[j][k];
        matrixQ[j][k] = 0;
      }
    }
    computeQR(argc, argv, verbose, definition, matrixR, matrixQ, rank, lines);      
      
    //martix multiplication Q'*A*Q
    // A = Q'*R
    for(k = 0; k < lines; k++){
      for(l = 0; l < lines; l++){
        float tm = 0;
        for(m = 0; m < lines; m++){
          tm += matrixQ[m][k]*matrixA[m][l];
        }
        matrixR[k][l] = tm;
      }
    }      
         
    // R = A*Q
    for(k = 0; k < lines; k++){
      for(l = 0; l < lines; l++){
        float tm = 0;
        for(m = 0; m < lines; m++){
          tm += matrixR[k][m]*matrixQ[m][l];
        }
        matrixA[k][l] = tm;
      }
    }
    
    if(verbose){
      printf("Matrix A in %d round.\n",i);
      for(k = 0; k < lines; k++){
        for(j = 0; j < lines; j++){
          if(verbose) printf("%f ",matrixA[k][j]);
        }
        if(verbose) printf("\n");
      
      }
    }
  }           
  if(rank == 0)
  {
    time = timer.CpuStop().CpuSeconds();//end time
  }
  if(solution && verbose){
    printf("Eig values:\n--------------------------------------\n",i);
    for(k = 0; k < lines; k++){
      printf("%f\n",matrixA[k][k]);
    }
    printf("--------------------------------------\n");
  }
  //Print time
  if(rank == 0)printf("%f\n",time);
   
  MPI_Finalize();
  return 0;
}

void computeQR(int argc, char* argv[], bool verbose, bool definition, float **matrixR, float **matrixQ, int rank, int lines)
{ 
  bool root = false;   
  int i,j,size;
  int displs[MAX_PROCESSES], send_counts[MAX_PROCESSES], displs2[MAX_PROCESSES], send_counts2[MAX_PROCESSES];
  float **mat,**p,**matTmp,**matTmp2;
  float *vec, coef;
  if(rank == 0) root = true;
  
  MPI_Comm_rank( MPI_COMM_WORLD, &rank );
  if(rank == 0) root = true;

  MPI_Comm_size( MPI_COMM_WORLD, &size );

  

  for(int i = 0; i < lines; i++){
    matrixQ[i][i] = 1;
  }

  
  int k,l,m,tmpLines,tmpLines2;
  
   
  for(i = 0; i < lines; i++){ 
    tmpLines = (lines-i)/size;
    if(rank == (size-1) && size > 1) tmpLines += (lines-i)%size; 
    tmpLines2 = (lines)/size;
    if(rank == (size-1) && size > 1) tmpLines2 += (lines)%size;  
    //tmp matrix for parallel computing  
    if(i > 0 && i < lines-size){
      delete [] mat[0];
    }
    mat = create_matrix(tmpLines,lines-i, false);
    
    MPI_Bcast((void *)&matrixR[0][0], lines*lines, MPI_FLOAT, 0, MPI_COMM_WORLD);
    MPI_Bcast((void *)&matrixQ[0][0], lines*lines, MPI_FLOAT, 0, MPI_COMM_WORLD);      
    // nastaveni pro posilani matice o rozmeru [lines-i][lines-i]
    int piece = (lines-i)/size;
    int radek = lines-i;
    send_counts[0] = (piece)*(radek);
    displs[0] = 0;
    if(size > 1){
      for(k = 1; k < size-1; k++){
        send_counts[k] = piece*(radek);
        displs[k] = displs[k-1] + piece*(radek);
      }
      displs[size-1] = displs[size-2] + piece*(radek);
      send_counts[size-1] = (((lines-i)*(radek)) - displs[size-1]);
    }
    // nastaveni pro posilani matice o rozmeru [lines][lines-i]
    piece = (lines)/size;
    radek = lines-i;
    send_counts2[0] = (piece)*(radek);
    displs2[0] = 0;
    if(size > 1){
      for(k = 1; k < size-1; k++){
        send_counts2[k] = piece*(radek);
        displs2[k] = displs2[k-1] + piece*(radek);
      }
      displs2[size-1] = displs2[size-2] + piece*(radek);
      send_counts2[size-1] = (((lines)*(radek)) - displs2[size-1]);
    }
            
    float x = 0;         
    if(i > 0 && i < lines-size){
      delete [] vec;
      vec = NULL;
    } 
    vec = new float[lines-i]; 
    if(rank == 0){
      for(j = 0; j < lines-i; j++){
        vec[j] = -matrixR[j+i][i];
        x += vec[j]*vec[j];
      }
    
      x = sqrt(x);
      
       
      if(vec[0] > 0) x = -x; 
      vec[0] = vec[0] + x;
      x = 0;
      for(j = 0; j < lines-i; j++){
        x += vec[j]*vec[j];
      }
      x = sqrt(x); 
    }       
    MPI_Bcast((void *)&x, 1, MPI_FLOAT, 0, MPI_COMM_WORLD);
    MPI_Bcast((void *)&vec[0], lines-i, MPI_FLOAT, 0, MPI_COMM_WORLD);      
    if(x > 0){
      if(rank == 0){
        //normalizovat vec  
        for(j = 0; j < lines-i; j++){
          vec[j] /= x;
        }
      }
       
       
      MPI_Bcast((void *)&vec[0], lines-i, MPI_FLOAT, 0, MPI_COMM_WORLD);  
      //sestavit matici P
      if(i > 0 && i < lines-size){
        delete [] p[0];
        p = NULL;
      }
      p = create_matrix(lines-i,lines-i, false);
      
      MPI_Scatterv(&p[0][0],send_counts,displs,MPI_FLOAT,&mat[0][0],tmpLines*(lines-i),MPI_FLOAT,0,MPI_COMM_WORLD);
      for(k = 0; k < send_counts[rank]/(lines-i); k++){
        for(l = 0; l < lines-i; l++){                              
          if((k+(displs[rank]/(lines-i))) == l) mat[k][l] = 1 - 2*vec[k+displs[rank]/(lines-i)]*vec[l];
          else mat[k][l] = -2*vec[k+displs[rank]/(lines-i)]*vec[l];
          //if(mat[k][l] == 0) printf("%d -- \n",k+displs[rank]/(lines-i));
        }
      }                            
      MPI_Gatherv(&mat[0][0],send_counts[rank],MPI_FLOAT,&p[0][0],send_counts,displs,MPI_FLOAT,0,MPI_COMM_WORLD);
      MPI_Bcast((void *)&p[0][0], (lines-i)*(lines-i), MPI_FLOAT, 0, MPI_COMM_WORLD); 
                           
      //nasobeni matic (paralelizace)
      //R      
      for(k = 0; k < send_counts[rank]/(lines-i); k++){
        for(l = 0; l < lines-i; l++){    
          float tm = 0;
          for(m = i; m < lines; m++){                
            tm += p[k+displs[rank]/(lines-i)][m-i]*matrixR[m][l+i];   
          }
          mat[k][l] = tm;
        }
      }
      if(i > 0 && i < lines-size){
        delete [] matTmp[0];
        matTmp = NULL;
      }
      matTmp = create_matrix(lines-i,lines-i, false);
      MPI_Gatherv(&mat[0][0],send_counts[rank],MPI_FLOAT,&matTmp[0][0],send_counts,displs,MPI_FLOAT,0,MPI_COMM_WORLD);
      if(rank == 0){
        for(k = i; k < lines; k++){
          for(l = i; l < lines; l++){   
            matrixR[k][l] = matTmp[k-i][l-i];
          }                                
        } 
      }
      
      //Q
      if(i > 0 && i < lines-size){
        delete [] mat[0];
        mat = NULL;
      }
      mat = create_matrix(tmpLines2,lines-i, false);
      for(k = 0; k < send_counts2[rank]/(lines-i); k++){
        for(l = 0; l < lines-i; l++){
          float tm = 0;
          for(m = i; m < lines; m++){
            tm += matrixQ[k+displs2[rank]/(lines-i)][m]*p[m-i][l];
          }
          mat[k][l] = tm;
        }
      }

      if(i > 0 && i < lines-size){
        delete [] matTmp[0];
        matTmp = NULL;
      }
      matTmp = create_matrix(lines,lines-i, false);
      MPI_Gatherv(&mat[0][0],send_counts2[rank],MPI_FLOAT,&matTmp[0][0],send_counts2,displs2,MPI_FLOAT,0,MPI_COMM_WORLD);
      if(rank == 0){
        for(k = 0; k < lines; k++){
          for(l = i; l < lines; l++){
            matrixQ[k][l] = matTmp[k][l-i];
          }
        } 
      } 
    }
  }      
    
    //Print solution 
    if(verbose){
      printf("\nSolution is:\n");
      if(verbose) printf("Matrix Q.\n");
      for(k = 0; k < lines; k++){
        for(j = 0; j < lines; j++){
          if(verbose) printf("%f ",matrixQ[k][j]);
        }
        if(verbose) printf("\n");
      } 
      printf("\n");    
      if(verbose) printf("Matrix R.\n");
      for(k = 0; k < lines; k++){
        for(j = 0; j < lines; j++){
          if(verbose) printf("%f ",matrixR[k][j]);
        }
        if(verbose) printf("\n");
      }   
    } 
                      
  return;                                                                                                
}
